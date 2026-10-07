// Tests for session-report.cpp: a scripted stream with one clean destination,
// one that drops twice, one that never connects and one switched on late.
// No framework: exits non-zero on failure.
#include "session-report.hpp"

#include <cstdio>

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

using P = SessionReport::Phase;

int main()
{
	SessionReport r;
	const int64_t t0 = 1000000;
	auto at = [&](int s) { return t0 + (int64_t)s * 1000; };
	r.begin(t0, QStringLiteral("Thu Oct 1 2026, 7:00 PM"));

	// Stream starts; three destinations on.
	r.update("yt", "YouTube", "youtube", P::Connecting, at(0));
	r.update("kk", "Kick", "kick", P::Connecting, at(0));
	r.update("tt", "TikTok", "tiktok", P::Connecting, at(0));
	r.update("fb", "Facebook", "facebook", P::Off, at(0)); // off all stream
	r.update("yt", "YouTube", "youtube", P::Live, at(2));
	r.update("kk", "Kick", "kick", P::Live, at(3));
	r.update("tt", "TikTok", "tiktok", P::Down, at(4), "Could not reach the server");

	// One second of traffic per tick for YouTube, 6000 kbps.
	for (int s = 2; s < 600; s++)
		r.traffic("yt", 6000.0, 1000, 0, (s - 2) * 30);

	// Kick drops at 100 s, back at 118 s; drops again at 300 s, gives up at
	// 340 s, a persistent retry gets it back at 400 s.
	r.update("kk", "Kick", "kick", P::Reconnecting, at(100));
	r.update("kk", "Kick", "kick", P::Live, at(118));
	r.traffic("kk", 6000.0, 1000, 12, 3500);
	r.traffic("kk", 6000.0, 1000, 3, 100); // new connection: counters restarted
	r.update("kk", "Kick", "kick", P::Reconnecting, at(300));
	r.update("kk", "Kick", "kick", P::Down, at(340), "Disconnected and could not reconnect");
	r.update("kk", "Kick", "kick", P::Connecting, at(399));
	r.update("kk", "Kick", "kick", P::Live, at(400));
	r.stall("kk", at(450));

	// Facebook switched on late.
	r.update("fb", "Facebook", "facebook", P::Connecting, at(200));
	r.update("fb", "Facebook", "facebook", P::Live, at(202));
	r.update("fb", "Facebook", "facebook", P::Off, at(500));

	r.end(at(600));
	r.update("yt", "YouTube", "youtube", P::Off, at(601)); // after the end: ignored

	CHECK(!r.active() && r.hasResult(), "report finished");
	CHECK(r.durationMs() == 600000, "duration 10 min");
	const auto list = r.destinations();
	CHECK(list.size() == 4, "all four were on at some point");
	const SessionReport::Dest *yt = nullptr, *kk = nullptr, *tt = nullptr, *fb = nullptr;
	for (auto *d : list) {
		if (d->id == "yt") yt = d;
		if (d->id == "kk") kk = d;
		if (d->id == "tt") tt = d;
		if (d->id == "fb") fb = d;
	}
	CHECK(yt && kk && tt && fb, "found each destination");
	if (!(yt && kk && tt && fb))
		return 1;

	CHECK(yt->liveMs == 598000 && yt->drops == 0, "YouTube live 598 s, no drops");
	CHECK(yt->uptime() > 0.999, "YouTube uptime leaves out the first connect");
	CHECK(yt->sentBytes() == 598LL * 6000 * 1000 / 8, "YouTube bytes from kbps x time");
	CHECK(qRound(yt->avgKbps()) == 6000, "YouTube average 6000 kbps");

	CHECK(kk->drops == 2 && kk->recoveries == 2, "Kick dropped twice, recovered twice");
	CHECK(kk->outageMs == 18000 + 100000, "Kick offline 18 s + 100 s (persistent retry counts as outage)");
	CHECK(kk->liveMs == (100 - 3) * 1000 + (300 - 118) * 1000 + (600 - 400) * 1000, "Kick live time");
	CHECK(kk->stalls == 1, "Kick stall counted");
	CHECK(kk->droppedFrames() == 15 && kk->totalFrames() == 3600, "Kick frame counters banked across connections");

	CHECK(!tt->everLive && tt->lastError == "Could not reach the server", "TikTok never live, error kept");
	CHECK(tt->outageMs == 0, "never-live time is not an outage");

	CHECK(fb->liveMs == 298000 && fb->drops == 0, "Facebook live 298 s, switching off is not a drop");
	bool switchedOn = false, switchedOff = false;
	for (const auto &e : fb->events) {
		switchedOn |= e.text == "Switched on";
		switchedOff |= e.text == "Switched off";
	}
	CHECK(switchedOn && switchedOff, "Facebook switch on/off in its timeline");
	bool ytSwitched = false;
	for (const auto &e : yt->events)
		ytSwitched |= e.text.startsWith("Switched");
	CHECK(!ytSwitched, "starting with the stream is not a switch-on, ending is not a switch-off");

	const QString h = r.headline();
	std::printf("headline: %s\n", h.toUtf8().constData());
	CHECK(h.startsWith("Last stream: 10 min."), "headline duration");
	CHECK(h.contains("Kick dropped 2 times (offline 1 min 58 s)"), "headline names Kick's drops");
	CHECK(h.contains("TikTok never went live"), "headline names TikTok");
	CHECK(!h.contains("YouTube"), "headline leaves clean destinations out");

	const QString t = r.text();
	std::printf("\n%s\n", t.toUtf8().constData());
	CHECK(t.contains("Never went live: Could not reach the server"), "text gives TikTok's reason");
	CHECK(t.contains("Sent 449 MB, average 6000 kbps"), "text: YouTube data sent");
	CHECK(t.contains("+1 min 40 s  Connection dropped; reconnecting"), "text: Kick timeline");

	SessionReport clean;
	clean.begin(0, "x");
	clean.update("a", "YouTube", "youtube", P::Connecting, 0);
	clean.update("a", "YouTube", "youtube", P::Live, 1500);
	clean.end(3600000 + 120000);
	CHECK(clean.headline() == "Last stream: 1 h 2 min. YouTube stayed live the whole time.", "clean single headline");

	CHECK(SessionReport::formatBytes(1500000000) == "1.5 GB", "GB formatting");
	CHECK(SessionReport::formatDuration(45000) == "45 s", "seconds formatting");

	std::printf(failures ? "\n%d FAILED\n" : "\nALL PASSED\n", failures);
	return failures ? 1 : 0;
}
