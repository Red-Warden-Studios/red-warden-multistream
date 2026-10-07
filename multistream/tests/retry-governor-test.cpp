// Tests for the connection budget. No framework: exits non-zero on failure.
// Build: part of the CMake project (target rws-beacon-tests), or
//   cl /std:c++17 /EHsc /I..\src retry-governor-test.cpp
#include "retry-governor.hpp"

#include <cstdio>
#include <random>

static int failures = 0;
#define CHECK(cond, msg)                                                       \
	do {                                                                   \
		if (!(cond)) {                                                 \
			std::printf("FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); \
			failures++;                                            \
		} else {                                                       \
			std::printf("ok:   %s\n", msg);                         \
		}                                                              \
	} while (0)

constexpr int64_t S = 1000;
constexpr int64_t M = 60 * S;

int main()
{
	{
		RetryGovernor g;
		CHECK(g.budgetWaitMs(0) == 0, "fresh destination may connect");
	}
	{
		RetryGovernor g;
		for (int i = 0; i < 12; i++)
			g.recordAttempt(i * 10 * S); // 12 attempts in 110 s
		const int64_t now = 120 * S;
		const int64_t w = g.budgetWaitMs(now);
		CHECK(w > 0, "13th attempt inside 10 min is held back");
		// oldest attempt (t=0) leaves the 10-min window just after t=600 s
		CHECK(w >= 600 * S - now && w <= 600 * S - now + 2, "hold lasts until the oldest attempt ages out");
		CHECK(g.budgetWaitMs(now + w) == 0, "allowed again once it has aged out");
	}
	{
		RetryGovernor g;
		for (int i = 0; i < 40; i++)
			g.recordAttempt(i * 90 * S); // one per 90 s: under the 10-min cap
		const int64_t now = 39 * 90 * S + 30 * S; // 59 min after the first: all 40 still count
		const int64_t w = g.budgetWaitMs(now);
		CHECK(w > 0, "41st attempt inside an hour is held back");
		CHECK(g.attemptsInLast(now, 10 * M) <= 12, "never more than 12 in 10 min while hourly cap applies");
	}
	{
		RetryGovernor g;
		CHECK(!g.unstable(0), "not unstable with no stalls");
		g.recordStall(0);
		g.recordStall(2 * M);
		CHECK(!g.unstable(2 * M), "two stalls is not unstable");
		g.recordStall(4 * M);
		CHECK(g.unstable(4 * M), "three stalls in 10 min marks unstable");
		CHECK(g.autoRetryDelayMs(0, 0.0, 4 * M) >= 4 * M, "unstable destination waits ~5 min (minus jitter)");
		CHECK(!g.unstable(15 * M), "unstable clears once stalls age out");
	}
	{
		RetryGovernor g;
		const auto d0lo = g.autoRetryDelayMs(0, 0.0, 0), d0hi = g.autoRetryDelayMs(0, 0.999, 0);
		const auto d1lo = g.autoRetryDelayMs(1, 0.0, 0), d1hi = g.autoRetryDelayMs(1, 0.999, 0);
		const auto d9lo = g.autoRetryDelayMs(9, 0.0, 0), d9hi = g.autoRetryDelayMs(9, 0.999, 0);
		CHECK(d0lo >= 24 * S && d0hi <= 36 * S, "first retry after ~30 s (+/-20%)");
		CHECK(d1lo >= 48 * S && d1hi <= 72 * S, "second retry after ~60 s (+/-20%)");
		CHECK(d9lo >= 96 * S && d9hi <= 144 * S, "later retries every ~2 min (+/-20%)");
	}
	{
		// Worst case: someone clicks Retry every second for two hours. Every
		// click goes through the budget, exactly as the dock does.
		RetryGovernor g;
		int made = 0, worst10 = 0, worst60 = 0;
		for (int64_t t = 0; t < 120 * M; t += S) {
			if (g.budgetWaitMs(t) == 0) {
				g.recordAttempt(t);
				made++;
			}
			worst10 = std::max(worst10, g.attemptsInLast(t, 10 * M));
			worst60 = std::max(worst60, g.attemptsInLast(t, 60 * M));
		}
		std::printf("      (retry mashing: %d connections in 2 h; worst 10 min = %d, worst hour = %d)\n", made,
			    worst10, worst60);
		CHECK(worst10 <= 12, "Retry mashing never exceeds 12 attempts in any 10 min");
		CHECK(worst60 <= 40, "Retry mashing never exceeds 40 attempts in any hour");
		CHECK(made <= 80, "at most 80 connections across 2 hours of mashing");
	}
	{
		// Realistic outage: OBS's own reconnect (10 tries, 5 s base, ~1.5x
		// backoff) gives up, then Beacon's persistent retries take over for an
		// hour-long platform outage. Count everything.
		RetryGovernor g;
		std::mt19937 rng(7);
		std::uniform_real_distribution<double> jit(0.0, 1.0);
		int64_t t = 0;
		g.recordAttempt(t); // the original connect
		double delay = 5.0;
		for (int i = 0; i < 10; i++) { // libobs reconnects
			t += (int64_t)(delay * S);
			g.recordAttempt(t);
			delay *= 1.5;
		}
		const int obsPhase10 = g.attemptsInLast(t, 10 * M);
		int worst10 = obsPhase10, worst60 = g.attemptsInLast(t, 60 * M);
		for (int fail = 0; t < 70 * M; fail++) { // Beacon's retries
			t += g.autoRetryDelayMs(fail, jit(rng), t);
			const int64_t w = g.budgetWaitMs(t);
			t += w;
			g.recordAttempt(t);
			worst10 = std::max(worst10, g.attemptsInLast(t, 10 * M));
			worst60 = std::max(worst60, g.attemptsInLast(t, 60 * M));
		}
		std::printf("      (hour-long outage: worst 10 min = %d, worst hour = %d)\n", worst10, worst60);
		CHECK(obsPhase10 <= 12, "OBS's own reconnect phase fits inside the budget");
		CHECK(worst10 <= 12, "a long outage never exceeds 12 attempts in 10 min");
		CHECK(worst60 <= 40, "a long outage never exceeds 40 attempts in an hour");
	}

	std::printf(failures ? "\n%d FAILED\n" : "\nALL PASSED\n", failures);
	return failures ? 1 : 0;
}
