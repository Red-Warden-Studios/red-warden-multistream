/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// The connection budget: what keeps the plugin from ever hammering a platform.
//
// Streaming platforms run DDoS protection on their ingest servers and do not
// publish how many connection attempts trip it. So every destination gets a
// hard, rolling budget that counts EVERY attempt, whoever made it: the first
// connect, the quick reconnects after a drop, the plugin's persistent retries,
// stall restarts, the Twitch bandwidth test, and the user mashing Retry. (libobs'
// own internal reconnect is switched off for our outputs, because it could not
// be gated.) The plugin will not open a connection the budget does not allow,
// and it says on the card when it is holding back. One budget per destination:
// connectionBudget() in connection-budget.hpp.
//
// For scale: OBS's own default stream reconnect is 25 tries starting 2 s
// apart. The plugin's budget allows at most 12 attempts in any 10 minutes and 40
// in any hour, per destination.
//
// Pure logic, no OBS or Qt: time is passed in, so tests can drive it.

#include <algorithm>
#include <cstdint>
#include <deque>

class RetryGovernor {
public:
	static constexpr int64_t kMinute = 60 * 1000;

	struct Limits {
		int maxPer10Min = 12;
		int maxPerHour = 40;
		int stallsForUnstable = 3; // this many stalls in 10 min ...
		int64_t unstableDelayMs = 5 * kMinute; // ... and retries wait this long
	};

	RetryGovernor() = default;
	explicit RetryGovernor(Limits l) : lim(l) {}

	// Any connection attempt, from any source.
	void recordAttempt(int64_t nowMs)
	{
		attempts.push_back(nowMs);
		prune(nowMs);
	}

	// 0 if an attempt is allowed right now, else how long until one is.
	int64_t budgetWaitMs(int64_t nowMs)
	{
		prune(nowMs);
		int64_t wait = 0;
		wait = std::max(wait, waitFor(nowMs, 10 * kMinute, lim.maxPer10Min));
		wait = std::max(wait, waitFor(nowMs, 60 * kMinute, lim.maxPerHour));
		return wait;
	}

	int attemptsInLast(int64_t nowMs, int64_t windowMs) const
	{
		return (int)std::count_if(attempts.begin(), attempts.end(),
					  [&](int64_t t) { return t > nowMs - windowMs; });
	}

	// A live output stopped sending data and was (or will be) restarted.
	void recordStall(int64_t nowMs)
	{
		stalls.push_back(nowMs);
		while (!stalls.empty() && stalls.front() <= nowMs - 10 * kMinute)
			stalls.pop_front();
	}

	bool unstable(int64_t nowMs) const
	{
		const int n = (int)std::count_if(stalls.begin(), stalls.end(),
						 [&](int64_t t) { return t > nowMs - 10 * kMinute; });
		return n >= lim.stallsForUnstable;
	}

	// Delay before the plugin's Nth automatic retry (failureIndex 0 = first retry
	// after OBS's own reconnects gave up). 30 s, 60 s, then every 2 min, with
	// +/-20 % jitter so many streamers dropped by one outage do not all
	// reconnect in the same second. jitter01 is a random value in [0, 1).
	int64_t autoRetryDelayMs(int failureIndex, double jitter01, int64_t nowMs) const
	{
		static const int64_t schedule[] = {30 * 1000, 60 * 1000, 2 * kMinute};
		const int last = (int)(sizeof(schedule) / sizeof(schedule[0])) - 1;
		int64_t base = schedule[std::clamp(failureIndex, 0, last)];
		if (unstable(nowMs))
			base = std::max(base, lim.unstableDelayMs);
		const double factor = 0.8 + 0.4 * std::clamp(jitter01, 0.0, 1.0);
		return (int64_t)(double(base) * factor);
	}

	const Limits &limits() const { return lim; }

private:
	void prune(int64_t nowMs)
	{
		while (!attempts.empty() && attempts.front() <= nowMs - 60 * kMinute)
			attempts.pop_front();
	}

	// If the window already holds `max` attempts, wait until the oldest one
	// that counts toward the limit slides out of it.
	int64_t waitFor(int64_t nowMs, int64_t windowMs, int max) const
	{
		std::deque<int64_t> in;
		for (int64_t t : attempts)
			if (t > nowMs - windowMs)
				in.push_back(t);
		if ((int)in.size() < max)
			return 0;
		const int64_t oldestCounting = in[in.size() - (size_t)max];
		return std::max<int64_t>(1, oldestCounting + windowMs - nowMs + 1);
	}

	Limits lim;
	std::deque<int64_t> attempts; // timestamps, oldest first
	std::deque<int64_t> stalls;
};
