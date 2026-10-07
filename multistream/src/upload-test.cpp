/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "upload-test.hpp"

#include <chrono>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

namespace uploadtest {

namespace {
using Clock = std::chrono::steady_clock;

constexpr DWORD kChunk = 64 * 1024;           // bytes per WinHttpWriteData
constexpr DWORD kBodyBytes = 8 * 1024 * 1024; // bytes per POST

std::string lastError(const char *what)
{
	return std::string(what) + " failed (Windows error " + std::to_string(GetLastError()) + ")";
}

struct Shared {
	std::atomic<unsigned long long> bytes{0};
	std::atomic<bool> stop{false};
	std::mutex m;
	std::string firstError;
	std::atomic<int> failedWorkers{0};
	void fail(const std::string &e)
	{
		std::lock_guard<std::mutex> lock(m);
		if (firstError.empty())
			firstError = e;
		failedWorkers++;
	}
};

void worker(const Options &opt, Shared &sh, const std::vector<char> &chunk)
{
	HINTERNET session = WinHttpOpen(L"RedWardenMultistream-UploadTest/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
					 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session) {
		sh.fail(lastError("WinHttpOpen"));
		return;
	}
	WinHttpSetTimeouts(session, 10000, 10000, 15000, 15000);
	HINTERNET conn = WinHttpConnect(session, opt.host, INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (!conn) {
		sh.fail(lastError("Connecting to the speed-test server"));
		WinHttpCloseHandle(session);
		return;
	}

	bool sentAny = false;
	while (!sh.stop) {
		HINTERNET req = WinHttpOpenRequest(conn, L"POST", opt.path, nullptr, WINHTTP_NO_REFERER,
						   WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
		if (!req) {
			sh.fail(lastError("WinHttpOpenRequest"));
			break;
		}
		if (!WinHttpSendRequest(req, L"Content-Type: application/octet-stream\r\n", (DWORD)-1L,
					WINHTTP_NO_REQUEST_DATA, 0, kBodyBytes, 0)) {
			if (!sh.stop && !sentAny)
				sh.fail(lastError("Sending to the speed-test server"));
			WinHttpCloseHandle(req);
			break;
		}
		DWORD sent = 0;
		bool ok = true;
		while (sent < kBodyBytes && !sh.stop) {
			const DWORD n = (kBodyBytes - sent) < kChunk ? (kBodyBytes - sent) : kChunk;
			DWORD written = 0;
			if (!WinHttpWriteData(req, chunk.data(), n, &written)) {
				ok = false;
				break;
			}
			sent += written;
			sh.bytes += written;
			sentAny = true;
		}
		if (ok && !sh.stop)
			WinHttpReceiveResponse(req, nullptr); // finish the request cleanly
		WinHttpCloseHandle(req);
		if (!ok && !sh.stop) {
			if (!sentAny)
				sh.fail(lastError("Uploading test data"));
			break;
		}
	}
	WinHttpCloseHandle(conn);
	WinHttpCloseHandle(session);
}
} // namespace

Result run(const Options &opt, std::atomic<bool> *cancel,
	   const std::function<void(double elapsed, double mbpsSoFar)> &progress)
{
	Result res;
	Shared sh;

	// Incompressible test data.
	std::vector<char> chunk(kChunk);
	std::mt19937 rng(0x5EED);
	for (auto &c : chunk)
		c = (char)(rng() & 0xFF);

	std::vector<std::thread> threads;
	for (int i = 0; i < opt.connections; i++)
		threads.emplace_back(worker, std::cref(opt), std::ref(sh), std::cref(chunk));

	const auto start = Clock::now();
	auto seconds = [&] { return std::chrono::duration<double>(Clock::now() - start).count(); };
	unsigned long long warmBytes = 0;
	double warmAt = -1.0;
	const double total = opt.warmupSeconds + opt.measureSeconds;

	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
		const double t = seconds();
		const unsigned long long b = sh.bytes;
		if (warmAt < 0 && t >= opt.warmupSeconds) {
			warmAt = t;
			warmBytes = b;
		}
		if (progress) {
			const double est = warmAt >= 0 && t > warmAt ? double(b - warmBytes) * 8.0 / (t - warmAt) / 1e6
								     : (t > 0 ? double(b) * 8.0 / t / 1e6 : 0.0);
			progress(t, est);
		}
		if (cancel && *cancel) {
			res.error = "Cancelled";
			break;
		}
		if (sh.failedWorkers >= opt.connections) {
			res.error = sh.firstError.empty() ? "The speed-test server could not be reached" : sh.firstError;
			break;
		}
		if (t >= total) {
			res.seconds = t - warmAt;
			res.bytes = b - warmBytes;
			break;
		}
	}
	sh.stop = true;
	for (auto &th : threads)
		th.join();

	if (res.error.empty()) {
		if (res.bytes == 0 || res.seconds <= 0) {
			res.error = sh.firstError.empty() ? "No data could be uploaded" : sh.firstError;
		} else {
			res.ok = true;
			res.mbps = double(res.bytes) * 8.0 / res.seconds / 1e6;
		}
	}
	return res;
}

} // namespace uploadtest

#else

namespace uploadtest {
Result run(const Options &, std::atomic<bool> *, const std::function<void(double, double)> &)
{
	Result r;
	r.error = "Upload testing is only available on Windows in this version; enter your upload speed manually.";
	return r;
}
} // namespace uploadtest

#endif
