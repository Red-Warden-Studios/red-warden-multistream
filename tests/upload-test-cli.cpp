// Standalone check of the upload test: prints the measured upload speed.
// Uploads test data to speed.cloudflare.com for about 10 seconds.
#include "upload-test.hpp"

#include <cstdio>

int main()
{
	uploadtest::Options opt;
	std::printf("Testing upload against speed.cloudflare.com (%d connections, %.0f s warm-up, %.0f s measured)...\n",
		    opt.connections, opt.warmupSeconds, opt.measureSeconds);
	const auto r = uploadtest::run(opt, nullptr, [](double t, double mbps) {
		std::printf("  %4.1f s  %7.1f Mbps\n", t, mbps);
	});
	if (!r.ok) {
		std::printf("FAILED: %s\n", r.error.c_str());
		return 1;
	}
	std::printf("RESULT: %.1f Mbps (%.1f MB in %.1f s)\n", r.mbps, r.bytes / 1e6, r.seconds);
	return 0;
}
