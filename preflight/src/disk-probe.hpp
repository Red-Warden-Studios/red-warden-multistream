// Recording-folder checks (exists / writable / free space) on a background worker, so the UI thread
// never does disk I/O. Qt Core + Win32 only: no libobs, so it is unit-testable.
#pragma once

#include <QString>
#include <QtGlobal>

#include <functional>
#include <memory>
#include <thread>

namespace preflight {

enum class TempCreate { Created, Exists, Failed };

// One attempt to create <dir>\<name> with CREATE_NEW + FILE_FLAG_DELETE_ON_CLOSE (Windows deletes it
// the moment the handle closes). Exists = that name was already taken (ERROR_FILE_EXISTS).
TempCreate tryCreateTemp(const QString &dir, const QString &name);

// True only if a throwaway file could really be created. A name that already exists is never proof
// of writability: it is retried with a fresh name from nameGen (max maxAttempts tries), then false.
bool folderWritableWith(const QString &dir, const std::function<QString()> &nameGen, int maxAttempts = 3);
// Same, with unique names "rws-preflight-<uuid>.tmp".
bool folderWritable(const QString &dir);

qint64 freeBytesIn(const QString &dir); // -1 if unreadable

struct DiskResult {
	bool valid = false; // false until the worker has answered for this path
	QString path;
	bool folderExists = false;
	bool folderWritable = false;
	qint64 freeBytes = -1;
};

// UI thread asks with request(); it returns instantly with the cached answer (valid=false until the
// first one arrives) and, when the answer is missing, for another path, invalidated, or older than
// 10 s, wakes the worker. Lifetime: shared_ptr State, so an abandoned worker never touches this object.
class DiskProbe {
public:
	explicit DiskProbe(int refreshMs = 10000);
	~DiskProbe();
	DiskProbe(const DiskProbe &) = delete;
	DiskProbe &operator=(const DiskProbe &) = delete;

	DiskResult request(const QString &path);
	void invalidate(); // profile change: refresh now, show Unknown until the new answer arrives
	// Idempotent. Waits at most `waitMs` for the worker (disk calls are short), then detaches it.
	void stop(int waitMs = 2000);

	struct State;

private:
	std::shared_ptr<State> st_;
	std::thread th_;
	bool started_ = false;
	bool stopped_ = false;
};

} // namespace preflight
