// Tests for update-manifest.cpp: version comparison and manifest parsing.
// No framework: exits non-zero on failure.
#include "update-manifest.hpp"

#include <cstdio>

using namespace updatecheck;

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
	// --- compareVersions ---
	CHECK(compareVersions("1.0.1", "1.0.0") == 1, "patch bump is newer");
	CHECK(compareVersions("1.0.0", "1.0.0") == 0, "equal versions");
	CHECK(compareVersions("1.0.10", "1.0.9") == 1, "numeric, not text: 1.0.10 > 1.0.9");
	CHECK(compareVersions("1.1", "1.0.9") == 1, "missing parts count as 0");
	CHECK(compareVersions("1.0", "1.0.0") == 0, "1.0 == 1.0.0");
	CHECK(compareVersions("0.9.9", "1.0.0") == -1, "older is older");
	CHECK(compareVersions("v1.2.0", "1.1.9") == 1, "leading v ignored");
	CHECK(compareVersions("32.0.1-beta1", "32.0.1") == 0, "suffix after the numbers ignored");
	CHECK(compareVersions("31.1.1", "32.0.0") == -1, "OBS version compare");
	CHECK(compareVersions("garbage", "1.0.0") == -1, "unparseable counts as 0.0.0");

	// --- parseManifest ---
	Manifest m;
	CHECK(parseManifest(R"({"latest":"1.0.1","url":"https://redwardenstudios.com/division/bastion/",
		"notes":"Fixes a reconnect bug.\nSecond line","min_obs":"31.0.0"})",
			    &m),
	      "full manifest parses");
	CHECK(m.latest == "1.0.1", "latest read");
	CHECK(m.url == "https://redwardenstudios.com/division/bastion/", "our https url kept");
	CHECK(m.notes == "Fixes a reconnect bug.", "notes cut to one line");
	CHECK(m.minObs == "31.0.0", "min_obs read");

	CHECK(parseManifest(R"({"latest":"1.0.1","url":"https://evil.example.com/redwardenstudios.com/"})", &m),
	      "foreign url still parses");
	CHECK(m.url == defaultPage(), "foreign url replaced by our page");
	CHECK(parseManifest(R"({"latest":"1.0.1","url":"http://redwardenstudios.com/x"})", &m) &&
		      m.url == defaultPage(),
	      "plain http replaced");
	CHECK(parseManifest(R"({"latest":"1.0.1","url":"https://redwardenstudios.com.evil.com/"})", &m) &&
		      m.url == defaultPage(),
	      "look-alike host replaced");
	CHECK(parseManifest(R"({"latest":"1.0.1","url":"https://user@redwardenstudios.com/"})", &m) &&
		      m.url == defaultPage(),
	      "userinfo url replaced");
	CHECK(parseManifest(R"({"latest":"1.0.1"})", &m) && m.url == defaultPage() && m.minObs.isEmpty(),
	      "url and min_obs optional");

	CHECK(!parseManifest("not json", &m), "malformed JSON rejected");
	CHECK(!parseManifest("[1,2]", &m), "non-object rejected");
	CHECK(!parseManifest(R"({"url":"https://redwardenstudios.com/"})", &m), "missing latest rejected");
	CHECK(!parseManifest(R"({"latest":"soon"})", &m), "non-version latest rejected");
	CHECK(parseManifest(R"({"latest":"1.0.1","min_obs":"whenever"})", &m) && m.minObs.isEmpty(),
	      "bad min_obs ignored, not trusted");

	std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
