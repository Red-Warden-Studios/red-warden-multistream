// Tests for importers.cpp with sample files in the formats obs-multi-rtmp and
// Aitum Multistream write. No framework: exits non-zero on failure.
#include "importers.hpp"

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

static const char *kMultiRtmp = R"json({
  "targets": [
    { "id": "t1", "name": "YouTube main", "protocol": "RTMP", "sync-start": true,
      "service-param": { "server": "rtmps://a.rtmps.youtube.com:443/live2", "key": "yt-key-123" },
      "output-param": {} },
    { "id": "t2", "name": "Kick", "protocol": "RTMP",
      "service-param": { "server": "rtmps://fa723fc1b171.global-contribute.live-video.net/app", "key": "sk_kick" },
      "video-config": "v1", "audio-config": "a1" },
    { "id": "t3", "name": "SRT relay", "protocol": "SRT",
      "service-param": { "server": "srt://10.0.0.5:9000", "key": "" } },
    { "id": "t4", "name": "", "protocol": "RTMP",
      "service-param": { "server": "rtmp://live.twitch.tv/app", "key": "live_abc" } }
  ],
  "video_configs": [
    { "id": "v1", "encoder": "obs_nvenc_h264_tex", "param": { "bitrate": 8000, "rate_control": "CBR" },
      "resolution": "1280x720", "fps-denumerator": 1 }
  ],
  "audio_configs": [
    { "id": "a1", "encoder": "ffmpeg_aac", "param": { "bitrate": 320 }, "mixerId": 5 }
  ]
})json";

static const char *kAitum = R"json({
  "partner_block": 0,
  "profiles": [
    { "name": "Other profile", "outputs": [
      { "name": "Should not appear", "stream_server": "rtmp://live.twitch.tv/app", "stream_key": "x" } ] },
    { "name": "RedWardenCDR", "outputs": [
      { "name": "TikTok", "stream_server": "rtmp://push-rtmp-l11-va01.tiktokcdn.com/game/", "stream_key": "tt-key",
        "advanced": true, "video_encoder": "jim_nvenc",
        "video_encoder_settings": { "bitrate": 2500 }, "audio_encoder_settings": { "bitrate": 128 } },
      { "name": "Facebook", "server": "rtmps://rtmp-api.facebook.com:443/rtmp/", "key": "fb-key", "advanced": false }
    ] }
  ]
})json";

int main()
{
	const auto m = parseMultiRtmp(QByteArray(kMultiRtmp));
	CHECK(m.size() == 4, "obs-multi-rtmp: all four targets read");
	if (m.size() == 4) {
		CHECK(m[0].name == "YouTube main" && m[0].key == "yt-key-123", "target name and key");
		CHECK(m[0].shared && m[0].platform == "youtube", "no video-config means shared encoder; YouTube detected");
		CHECK(!m[1].shared && m[1].encoderId == "obs_nvenc_h264_tex" && m[1].videoBitrate == 8000,
		      "video-config becomes a separate encoder with its bitrate");
		CHECK(m[1].width == 1280 && m[1].height == 720, "resolution 1280x720 parsed");
		CHECK(m[1].audioBitrate == 320 && m[1].audioTrack == 5, "audio-config bitrate and mixer (track 6)");
		CHECK(m[1].platform == "kick", "Kick (IVS host) detected");
		CHECK(!m[2].skipReason.isEmpty(), "SRT target flagged as not importable");
		CHECK(m[3].name == "live.twitch.tv" && m[3].platform == "twitch", "unnamed target named after its host");
	}
	CHECK(parseMultiRtmp(QByteArray("{\"targets\":[],\"video_configs\":[],\"audio_configs\":[]}")).empty(),
	      "empty obs-multi-rtmp file (no targets) yields nothing");
	CHECK(parseMultiRtmp(QByteArray("not json")).empty(), "garbage file yields nothing, no crash");

	const auto a = parseAitum(QByteArray(kAitum), QStringLiteral("RedWardenCDR"));
	CHECK(a.size() == 2, "Aitum: only the current profile's outputs");
	if (a.size() == 2) {
		CHECK(a[0].name == "TikTok" && !a[0].shared && a[0].videoBitrate == 2500 && a[0].audioBitrate == 128,
		      "advanced output becomes a separate encoder");
		CHECK(a[0].platform == "tiktok", "TikTok detected");
		CHECK(a[1].shared && a[1].server.startsWith("rtmps://rtmp-api.facebook.com") && a[1].key == "fb-key",
		      "older server/key field names still read");
		CHECK(a[1].platform == "facebook", "Facebook detected");
	}
	CHECK(parseAitum(QByteArray(kAitum), QStringLiteral("No such profile")).empty(), "unknown profile yields nothing");

	std::printf(failures ? "\n%d FAILED\n" : "\nALL PASSED\n", failures);
	return failures ? 1 : 0;
}
