/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// The update manifest, as pure logic (Qt Core only, no OBS, no network), so it
// can be unit-tested: tests/update-check-test.cpp.
//
// https://redwardenstudios.com/updates/multistream.json :
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

QString defaultPage();

} // namespace updatecheck
