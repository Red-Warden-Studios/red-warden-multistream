/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// The update manifest, as pure logic (Qt Core only, no OBS, no network), so it
// can be unit-tested: tests/update-check-test.cpp.
//
// https://redwardenstudios.com/updates/stream-kit.json :
//   { "latest": "1.0.1",
//     "url": "https://redwardenstudios.com/division/bastion/",
//     "notes": "Fixes a reconnect bug on Kick.",      (optional, one line)
//     "min_obs": "31.0.0" }                              (optional)
// "min_obs" lets a release say "update OBS first" instead of breaking an older OBS.

#include <QByteArray>
#include <QString>

namespace updatecheck {

struct Manifest {
	QString latest;
	QString url;
	QString notes;
	QString minObs;
};

// Numeric dotted compare: "1.0.10" > "1.0.9"; missing parts count as 0;
// anything after the numbers ("-beta") is ignored. Returns -1, 0 or 1.
int compareVersions(const QString &a, const QString &b);

// False on malformed JSON or a missing/invalid "latest". A "url" that is not on
// https://redwardenstudios.com/ is replaced by the default project page, so a
// tampered manifest can't send users somewhere else. Notes are cut to one line.
bool parseManifest(const QByteArray &json, Manifest *out);

// Red Warden Stream Kit: Multistream and Pre-Flight ship as ONE product with one
// version and one update notice. Both plugins compare the manifest's "latest"
// against the KIT version, not their own. BOTH PLUGINS MUST AGREE on this value
// (red-warden-multistream/src and red-warden-preflight/src carry the same line).
constexpr const char *kKitVersion = "1.0.1";
constexpr const char *kKitHost = "redwardenstudios.com";
constexpr const char *kKitManifestPath = "/updates/stream-kit.json";

QString kitManifestUrl(); // https://<kKitHost><kKitPath>

// Exactly one notice per OBS start: Multistream owns it whenever it is loaded;
// Pre-Flight shows it only when Multistream is not loaded.
bool shouldShowKitNotice(bool iAmMultistream, bool multistreamLoaded);

// The shared "Tell me when an update is available" preference. Both Stream Kit plugins read and write
// <OBS config>/plugin_config/rws-stream-kit/settings.json (key "check_updates"). On first run each
// plugin's old per-plugin value is migrated: if either was explicitly false, the shared value is false.
// The first migrating plugin writes the file; the other then just reads it.
QString kitSettingsPath(const QString &pluginConfigRoot);
// -1: key absent or file malformed; 0: false; 1: true. For one plugin's settings.json.
int readCheckUpdatesKey(const QByteArray &json);
// The migration rule over the two legacy states (-1/0/1 as above): false if either is explicitly false.
bool mergeLegacyCheckUpdates(int multistreamState, int preflightState);
// Reads the shared value, migrating (and writing the shared file) when it does not exist yet.
// pluginConfigRoot is the folder holding every plugin's config folder (".../plugin_config").
bool loadSharedCheckUpdates(const QString &pluginConfigRoot);
bool saveSharedCheckUpdates(const QString &pluginConfigRoot, bool on);

QString defaultPage(); // the Stream Kit page

} // namespace updatecheck
