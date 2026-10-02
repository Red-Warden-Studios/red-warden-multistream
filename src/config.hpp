/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <vector>

struct EncoderConfig {
	bool shared = true;    // ride OBS's own stream encoder: zero extra load
	QString encoderId;     // video encoder type id when not shared
	int videoBitrate = 6000;
	int audioBitrate = 0;  // kbps; 0 = match OBS's stream audio bitrate
	int audioTrack = -1;   // 0-based mixer index; -1 = match OBS's stream track
	int width = 0;         // 0 = same as OBS output resolution
	int height = 0;
};

struct Destination {
	QString id;            // uuid; also names the stream-key credential
	QString name;
	QString platform;      // PlatformPreset::id
	QString server;
	bool enabled = false;  // the on/off switch: on = live whenever OBS is live
	EncoderConfig enc;
	QString hotkeyJson;    // OBS hotkey bindings for "toggle this destination"
};

struct MultistreamConfig {
	std::vector<Destination> destinations;
};

// Per OBS profile: <profile dir>/rws-multistream.json. Stream keys are NOT in
// this file; see credentials.hpp.
QString configPath();
MultistreamConfig loadConfig();
bool saveConfig(const MultistreamConfig &cfg);
QString newDestinationId();

// Machine-wide settings (not per OBS profile): the user's upload capacity.
struct GlobalSettings {
	double uploadMbps = 0.0; // 0 = unknown
	QString uploadSource;    // "measured" or "manual"
	QString uploadWhen;      // ISO date-time of the test / entry
	bool checkUpdates = true; // look for a newer version once per OBS start
};
GlobalSettings loadGlobalSettings();
void saveGlobalSettings(const GlobalSettings &g);
