
// Tests for serverUrlContainsKey: a stream key must be refused anywhere after
// the host of a server URL, however short, and never because of the host itself.
// No framework: exits non-zero on failure.
#include "server-url.hpp"

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

static bool has(const char *server, const char *key)
{
	return serverUrlContainsKey(QString::fromUtf8(server), QString::fromUtf8(key));
}

int main()
{
	CHECK(has("rtmp://live.example.com/app/sk_abcdef123456", "sk_abcdef123456"), "long key in the path is found");
	CHECK(has("rtmp://h.example.com/app/abc", "abc"), "short key in the path is found");
	CHECK(has("rtmp://h.example.com/app/a%20b%2Bc", "a b+c"), "percent-encoded key is found");
	CHECK(has("rtmps://h.example.com/app?key=xyz9", "xyz9"), "key in the query is found");
	CHECK(has("rtmps://h.example.com/app#xyz9", "xyz9"), "key in the fragment is found");
	CHECK(!has("rtmp://live.twitch.tv/app", "live"), "short key matching only the host is not a hit");
	CHECK(has("rtmp://sk_abcdef123456.example.com/app", "sk_abcdef123456"), "long key in the host is found");
	CHECK(has("rtmp://h.example.com:1935/app", "h.example.com:1935"), "8+ character key spanning host and port is found");
	CHECK(!has("rtmp://h.example.com:1935/app", "1935"), "key matching only the port is not a hit");
	CHECK(!has("rtmp://h.example.com:1935/app", "rtmp"), "key matching only the scheme is not a hit");
	CHECK(!has("rtmp://h.example.com/app/abc", ""), "empty key is never a hit");
	CHECK(!has("rtmp://h.example.com/app/abc", "   "), "whitespace-only key is never a hit");
	CHECK(!has("rtmps://a.rtmps.youtube.com:443/live2", "abcd-efgh-ijkl"), "key that is not in the URL is not a hit");
	CHECK(has("rtmp://h.example.com/app/abc", "  abc "), "key with surrounding whitespace is trimmed before matching");
	CHECK(!has("rtmp://h.example.com", "abc"), "URL with no path has nothing to match");
	CHECK(has("h.example.com/app/abc", "abc"), "text without a scheme is checked as a whole");
	CHECK(has("rtmp://h.example.com/app", "app"), "a key equal to the app name is refused");

	std::printf(failures ? "\n%d FAILED\n" : "\nALL PASSED\n", failures);
	return failures ? 1 : 0;
}
