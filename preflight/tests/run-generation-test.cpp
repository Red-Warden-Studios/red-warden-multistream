// Tests for run-generation.hpp: the token that stops a cancelled mic test from publishing late.
#include "run-generation.hpp"

#include <cstdio>

using preflight::RunGeneration;

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

int main()
{
	{
		RunGeneration g;
		CHECK(!g.running(), "idle at start");
		const auto t = g.begin();
		CHECK(g.running(), "running after begin");
		CHECK(g.complete(t), "live run may publish");
		CHECK(!g.running(), "not running after complete");
		CHECK(!g.complete(t), "a run publishes at most once");
	}
	{
		// expire()/cancel while running: the timer that fires afterwards must not publish a result.
		RunGeneration g;
		const auto t = g.begin();
		CHECK(g.cancel(), "cancel reports a run was running");
		CHECK(!g.running(), "not running after cancel");
		CHECK(!g.complete(t), "finisher of a cancelled run cannot publish (no later Passed)");
	}
	{
		// cancel then restart: the OLD timer fires while the NEW test is running.
		RunGeneration g;
		const auto oldTok = g.begin();
		g.cancel();
		const auto newTok = g.begin();
		CHECK(oldTok != newTok, "a new run gets a new token");
		CHECK(!g.complete(oldTok), "stale finisher cannot end the new run");
		CHECK(g.running(), "new run still running after the stale finisher was refused");
		CHECK(g.complete(newTok), "the new run's own finisher publishes");
	}
	{
		RunGeneration g;
		CHECK(!g.cancel(), "cancel with nothing running reports false");
		const auto t = g.begin();
		const auto t2 = g.begin(); // superseded without an explicit cancel
		CHECK(!g.complete(t), "superseded token is refused");
		CHECK(g.complete(t2), "latest token accepted");
	}
	std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
