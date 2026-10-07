#include "disk-probe.hpp"

#include <QDir>
#include <QFileInfo>
#include <QUuid>

#include <chrono>
#include <condition_variable>
#include <mutex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace preflight {

TempCreate tryCreateTemp(const QString &dir, const QString &name)
{
#ifdef _WIN32
	const std::wstring p = QDir::toNativeSeparators(QDir(dir).absoluteFilePath(name)).toStdWString();
	HANDLE h = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
			       FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
	if (h == INVALID_HANDLE_VALUE) {
		const DWORD e = GetLastError();
		return (e == ERROR_FILE_EXISTS || e == ERROR_ALREADY_EXISTS) ? TempCreate::Exists : TempCreate::Failed;
	}
	CloseHandle(h); // DELETE_ON_CLOSE removes it now
	return TempCreate::Created;
#else
	Q_UNUSED(name);
	return QFileInfo(dir).isWritable() ? TempCreate::Created : TempCreate::Failed;
#endif
}

bool folderWritableWith(const QString &dir, const std::function<QString()> &nameGen, int maxAttempts)
{
	for (int i = 0; i < maxAttempts; ++i) {
		switch (tryCreateTemp(dir, nameGen())) {
		case TempCreate::Created:
			return true;
		case TempCreate::Failed:
			return false;
		case TempCreate::Exists:
			break; // name taken (stale file, or a race): that proves nothing. Try a new name.
		}
	}
	return false;
}

bool folderWritable(const QString &dir)
{
	return folderWritableWith(dir, [] {
		return QStringLiteral("rws-preflight-%1.tmp").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
	});
}

qint64 freeBytesIn(const QString &dir)
{
#ifdef _WIN32
	const std::wstring p = QDir::toNativeSeparators(dir).toStdWString();
	ULARGE_INTEGER avail{};
	if (!GetDiskFreeSpaceExW(p.c_str(), &avail, nullptr, nullptr))
		return -1;
	return static_cast<qint64>(avail.QuadPart);
#else
	Q_UNUSED(dir);
	return -1;
#endif
}

struct DiskProbe::State {
	std::mutex m;
	std::condition_variable cv;
	bool stop = false;
	bool want = false;     // a refresh is requested
	bool busy = false;     // the worker is inside a disk call
	bool finished = false; // the worker loop has ended
	QString wantPath;
	DiskResult result;
	std::chrono::steady_clock::time_point resultAt;
	int refreshMs = 10000;
};

namespace {
void workerMain(std::shared_ptr<DiskProbe::State> st)
{
	std::unique_lock<std::mutex> lk(st->m);
	for (;;) {
		st->cv.wait(lk, [&] { return st->stop || st->want; });
		if (st->stop)
			break;
		const QString path = st->wantPath;
		st->want = false;
		st->busy = true;
		lk.unlock();

		DiskResult r;
		r.path = path;
		r.folderExists = !path.isEmpty() && QDir(path).exists();
		bool stopNow;
		{
			std::lock_guard<std::mutex> g(st->m);
			stopNow = st->stop; // do not start the temp-file work once we are shutting down
		}
		if (r.folderExists && !stopNow) {
			r.folderWritable = folderWritable(path);
			r.freeBytes = freeBytesIn(path);
		}
		r.valid = true;

		lk.lock();
		st->busy = false;
		st->result = r;
		st->resultAt = std::chrono::steady_clock::now();
	}
	st->finished = true;
	st->cv.notify_all();
}
} // namespace

DiskProbe::DiskProbe(int refreshMs) : st_(std::make_shared<State>())
{
	st_->refreshMs = refreshMs;
}

DiskProbe::~DiskProbe()
{
	stop();
}

DiskResult DiskProbe::request(const QString &path)
{
	if (stopped_)
		return DiskResult();
	if (!started_) {
		started_ = true;
		auto st = st_;
		th_ = std::thread([st] { workerMain(st); });
	}
	std::lock_guard<std::mutex> lk(st_->m);
	const bool have = st_->result.valid && st_->result.path == path;
	const bool stale = !have || std::chrono::steady_clock::now() - st_->resultAt >=
					     std::chrono::milliseconds(st_->refreshMs);
	if (stale && !st_->busy && !st_->want) {
		st_->want = true;
		st_->wantPath = path;
		st_->cv.notify_all();
	}
	return have ? st_->result : DiskResult();
}

void DiskProbe::invalidate()
{
	std::lock_guard<std::mutex> lk(st_->m);
	st_->result = DiskResult(); // Unknown until the worker answers again
}

void DiskProbe::stop(int waitMs)
{
	if (stopped_)
		return;
	stopped_ = true;
	if (!started_)
		return;
	bool finished;
	{
		std::unique_lock<std::mutex> lk(st_->m);
		st_->stop = true;
		st_->cv.notify_all();
		finished = st_->cv.wait_for(lk, std::chrono::milliseconds(waitMs), [this] { return st_->finished; });
	}
	if (!th_.joinable())
		return;
	if (finished)
		th_.join();
	else
		th_.detach(); // stuck in a slow disk call (dead network share?): the State outlives us via shared_ptr
}

} // namespace preflight
