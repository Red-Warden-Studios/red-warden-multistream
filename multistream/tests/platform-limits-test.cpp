// Tests for platform-limits.cpp with an excerpt of OBS's rtmp-services
// services.json (format_version 5, values as shipped with OBS 32).
// No framework: exits non-zero on failure.
#include "platform-limits.hpp"

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

static const char *kServices = R"json({
 "format_version": 5,
 "services": [
  {"name": "Twitch", "common": true, "recommended": {"keyint": 2, "max audio bitrate": 320, "max video bitrate": 6000, "x264opts": "scenecut=0"}},
  {"name": "YouTube - HLS", "recommended": {"keyint": 2, "max audio bitrate": 160, "max video bitrate": 51000, "output": "ffmpeg_hls_muxer"}},
  {"name": "YouTube - RTMPS", "common": true, "recommended": {"keyint": 2, "max audio bitrate": 160, "max video bitrate": 51000}},
  {"name": "Facebook Live", "common": true, "recommended": {"keyint": 2, "max audio bitrate": 128, "max video bitrate": 9000, "max fps": 60,
    "supported resolutions": ["1920x1080", "1280x720", "852x480", "640x360"]}},
  {"name": "Twitter", "common": true, "recommended": {"keyint": 3, "max audio bitrate": 128, "max video bitrate": 12000, "max fps": 60}}
 ]
})json";

static bool any(const std::vector<LimitIssue> &v, const char *needle, bool serious)
{
	for (const auto &i : v)
		if (i.text.contains(QString::fromUtf8(needle)) && i.serious == serious)
			return true;
	return false;
}

int main()
{
	const auto m = parseServiceLimits(QByteArray(kServices));
	CHECK(m.size() == 4, "Twitch, YouTube, Facebook and X found; Kick/TikTok/Rumble absent");
	CHECK(m.count("youtube") && m.at("youtube").service == "YouTube - RTMPS", "YouTube uses the RTMPS entry, not HLS");
	CHECK(m.count("x") && m.at("x").keyintSec == 3 && m.at("x").maxVideoKbps == 12000, "X read from the Twitter entry");
	const PlatformLimits fb = m.count("facebook") ? m.at("facebook") : PlatformLimits{};
	CHECK(fb.maxAudioKbps == 128 && fb.maxVideoKbps == 9000 && fb.maxFps == 60 && fb.maxHeight == 1080,
	      "Facebook limits including tallest resolution");
	CHECK(parseServiceLimits(QByteArray("nonsense")).empty(), "unreadable file yields nothing");

	// Andrew's setup: shared encoder, 6000 kbps, 256 kbps audio, 1080p60, auto keyframes.
	StreamFacts f;
	f.shared = true;
	f.videoKbps = 6000;
	f.audioKbps = 256;
	f.keyintSec = 2;
	f.height = 1080;
	f.fps = 60;

	CHECK(checkLimits("youtube", "YouTube", m.at("youtube"), f).empty(), "YouTube: 256 kbps audio is not nagged about");
	CHECK(checkLimits("twitch", "Twitch", m.at("twitch"), f).empty(), "Twitch: 6000 kbps and 256 audio are fine");
	const auto fbi = checkLimits("facebook", "Facebook", fb, f);
	CHECK(fbi.size() == 1 && any(fbi, "Facebook lists audio up to 128 kbps (OBS sends 256)", false),
	      "Facebook: audio note, gentle, mentions its own encoder");

	f.videoKbps = 12000;
	f.keyintSec = 0;
	f.height = 1440;
	const auto fb2 = checkLimits("facebook", "Facebook", fb, f);
	CHECK(any(fb2, "Facebook takes video up to 9000 kbps (OBS sends 12000)", true), "Facebook: video over limit is serious");
	CHECK(any(fb2, "keyframe every 2 s", true), "auto keyframes flagged");
	CHECK(any(fb2, "up to 1080p", true), "1440p flagged for Facebook");

	StreamFacts own;
	own.shared = false;
	own.videoKbps = 8000;
	own.audioKbps = 160;
	own.keyintSec = 2;
	own.height = 720;
	own.fps = 30;
	const auto tw = checkLimits("twitch", "Twitch", m.at("twitch"), own);
	CHECK(tw.size() == 1 && any(tw, "Lower it in Edit", true), "own encoder over Twitch's limit: lower it in Edit");
	CHECK(checkLimits("x", "X", m.at("x"), own).size() == 1, "X: only the audio note (2 s keyframes satisfy 3 s)");

	CHECK(checkLimits("kick", "Kick", PlatformLimits{}, f).empty(), "no listed limits: no advice");

	std::printf(failures ? "\n%d FAILED\n" : "\nALL PASSED\n", failures);
	return failures ? 1 : 0;
}
