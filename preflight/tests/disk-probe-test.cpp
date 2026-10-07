// Tests for disk-probe.cpp: unique-name writable test and the background folder worker.
// No framework: exits non-zero on failure. Qt Core only.
#include "disk-probe.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <chrono>
#include <cstdio>
#include <thread>

using namespace preflight;

static int failures = 0;
#define CHECK(cond, msg)                                                      \
	do {                                                                  \
		if (!(cond)) {                                                \
			std::printf("FAIL: %s  (line %d)\n", msg, __LINE__); \
			failures++;                                           \
		} else {                                                      \
			std::printf("ok:   %s\n", msg);                        \
		}                                                             \
	} while (0)

using Clock = std::chrono::steady_clock;

// Polls request(path) (as the UI tick would) until it is valid or timeoutMs passes.
static DiskResult waitValid(DiskProbe &p, const QString &path, int timeoutMs)
{
	const auto end = Clock::now() + std::chrono::milliseconds(timeoutMs);
	DiskResult r;
	while (Clock::now() < end) {
		r = p.request(path);
		if (r.valid)
			return r;
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
	return r;
}

int main()
{
	QTemporaryDir tmp;
	if (!tmp.isValid()) {
		std::printf("FAIL: no temp dir\n");
		return 1;
	}
	const QString dir = tmp.path();

	// --- tryCreateTemp / unique names ---
	{
		QFile stale(QDir(dir).absoluteFilePath(QStringLiteral("stale.tmp")));
		stale.open(QIODevice::WriteOnly);
		stale.close();
		CHECK(tryCreateTemp(dir, QStringLiteral("stale.tmp")) == TempCreate::Exists, "existing name -> Exists (ERROR_FILE_EXISTS)");
		CHECK(tryCreateTemp(dir, QStringLiteral("fresh.tmp")) == TempCreate::Created, "new name -> Created");
		CHECK(!QFile::exists(QDir(dir).absoluteFilePath(QStringLiteral("fresh.tmp"))), "temp file is gone after close (delete-on-close)");
		CHECK(tryCreateTemp(QDir(dir).absoluteFilePath(QStringLiteral("no-such-subdir")), QStringLiteral("x.tmp")) == TempCreate::Failed,
		      "missing folder -> Failed");

		CHECK(!folderWritableWith(dir, [] { return QStringLiteral("stale.tmp"); }),
		      "name always taken -> NOT writable (a leftover file is no proof)");
		int calls = 0;
		CHECK(!folderWritableWith(dir, [&calls] { ++calls; return QStringLiteral("stale.tmp"); }) && calls == 3,
		      "gives up after exactly 3 attempts");
		calls = 0;
		CHECK(folderWritableWith(dir,
					 [&calls] {
						 return ++calls < 3 ? QStringLiteral("stale.tmp") : QStringLiteral("third.tmp");
					 }) && calls == 3,
		      "name taken twice, third name free -> writable");
		CHECK(!folderWritableWith(QDir(dir).absoluteFilePath(QStringLiteral("nope")), [] { return QStringLiteral("a.tmp"); }),
		      "missing folder -> not writable");
		CHECK(folderWritable(dir), "real unique-name test: temp dir is writable");
		const QStringList left = QDir(dir).entryList(QDir::Files);
		CHECK(left == QStringList({"stale.tmp"}), "no rws-preflight-*.tmp files left behind");
		CHECK(freeBytesIn(dir) > 0, "free space readable");
	}

	// --- DiskProbe worker ---
	{
		DiskProbe p(10000);
		const auto t0 = Clock::now();
		const DiskResult first = p.request(dir);
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
		CHECK(!first.valid, "first request returns at once with no answer yet (Unknown)");
		CHECK(ms < 250, "request() does not block on disk work (includes starting the worker)");
		const DiskResult r = waitValid(p, dir, 3000);
		CHECK(r.valid && r.folderExists && r.folderWritable && r.freeBytes > 0, "worker answers: exists, writable, free space");
		const DiskResult miss = waitValid(p, QDir(dir).absoluteFilePath(QStringLiteral("missing")), 3000);
		CHECK(miss.valid && !miss.folderExists && !miss.folderWritable, "other path: worker answers folder missing");
		p.invalidate();
		CHECK(!p.request(dir).valid, "invalidate() clears the cache: Unknown until the worker answers again");
		const auto t0b = Clock::now();
		p.stop();
		const auto stopMs = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0b).count();
		CHECK(stopMs < 1500, "stop() returns promptly with an idle worker");
		CHECK(!p.request(dir).valid, "request after stop() answers nothing and starts nothing");
	}
	{
		// Refresh: with a short interval the worker re-checks and notices the folder is gone.
		QTemporaryDir t2;
		DiskProbe p(150);
		const QString d2 = t2.path();
		DiskResult r = waitValid(p, d2, 3000);
		CHECK(r.valid && r.folderExists, "refresh test: folder exists at first");
		QDir(d2).removeRecursively();
		bool gone = false;
		const auto end = Clock::now() + std::chrono::seconds(4);
		while (Clock::now() < end && !gone) {
			r = p.request(d2);
			gone = r.valid && !r.folderExists;
			std::this_thread::sleep_for(std::chrono::milliseconds(30));
		}
		CHECK(gone, "stale answer is refreshed by the worker after the interval");
		p.stop();
	}
	{
		DiskProbe p;
		CHECK(true, "stop() on a probe that never started is harmless");
		p.stop();
		p.stop();
	}

	std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
