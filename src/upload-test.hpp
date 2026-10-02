/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// Measures upload capacity the way speed-test sites do: several parallel
// HTTPS uploads to Cloudflare's public speed-test endpoint
// (https://speed.cloudflare.com/__up), ignoring the first seconds while TCP
// ramps up. Plain C++ + WinHTTP (built into Windows, does its own TLS), no
// Qt or OBS, so it can be tested on its own (tools: rws-multistream-uploadtest).

#include <atomic>
#include <functional>
#include <string>

namespace uploadtest {

struct Result {
	bool ok = false;
	double mbps = 0.0;       // megabits per second, measured window only
	double seconds = 0.0;    // length of the measured window
	unsigned long long bytes = 0;
	std::string error;       // set when !ok
};

struct Options {
	int connections = 4;
	double warmupSeconds = 2.0;  // excluded from the measurement
	double measureSeconds = 8.0;
	const wchar_t *host = L"speed.cloudflare.com";
	const wchar_t *path = L"/__up";
};

// Blocking. Call from a worker thread. `cancel` may be null. `progress`
// (optional) is called roughly 4x a second with elapsed seconds and the
// running estimate.
Result run(const Options &opt, std::atomic<bool> *cancel,
	   const std::function<void(double elapsed, double mbpsSoFar)> &progress = {});

} // namespace uploadtest
