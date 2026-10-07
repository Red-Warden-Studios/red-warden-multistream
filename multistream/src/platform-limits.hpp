/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// What each platform accepts, read from OBS's own rtmp-services list
// (services.json, which OBS keeps up to date), and the advice we give when a
// destination sends more than that. Pure Qt Core: unit-tested in
// tests/platform-limits-test.cpp.

#include <QByteArray>
#include <QString>
#include <map>
#include <vector>

struct PlatformLimits {
	QString service;      // the entry it came from, e.g. "Facebook Live"
	int maxVideoKbps = 0; // 0 = no limit listed
	int maxAudioKbps = 0;
	int keyintSec = 0;    // keyframe interval the platform expects
	int maxFps = 0;
	int maxHeight = 0;    // tallest supported resolution, if a list is given
};

// Keyed by our platform id (twitch, youtube, facebook, x, kick, ...). Only
// platforms found in the file are present.
std::map<QString, PlatformLimits> parseServiceLimits(const QByteArray &servicesJson);

// What a destination will actually send.
struct StreamFacts {
	bool shared = true;   // OBS's own encoder
	int videoKbps = 0;
	int audioKbps = 0;
	int keyintSec = -1;   // -1 unknown, 0 = OBS "auto"
	int height = 0;
	double fps = 0.0;
};

struct LimitIssue {
	bool serious = false; // likely to cause trouble (shown in amber), or just good to know
	QString text;
};

std::vector<LimitIssue> checkLimits(const QString &platformId, const QString &platformLabel, const PlatformLimits &limits,
				    const StreamFacts &facts);
