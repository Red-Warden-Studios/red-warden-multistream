/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QByteArray>
#include <QString>

// What OBS's own stream output uses for audio, read from the current profile
// (Settings > Output). The plugin's "Match OBS" options resolve to these so a
// separate encoder never sends lower-fidelity audio than the main stream.
struct ObsStreamAudio {
	int bitrate = 160;  // kbps
	int track = 0;      // 0-based mixer index (OBS "Track 1" = 0)
	QString encoderId;  // an AAC encoder type id that exists in this OBS
};

ObsStreamAudio obsStreamAudio();

// OBS's own stream video bitrate (kbps) from the current profile, for the
// upload estimate. Advanced mode keeps it in <profile>/streamEncoder.json.
int obsStreamVideoBitrate();

// OBS's own stream keyframe interval in seconds: 0 = "auto" (encoder default,
// often 8 s or more). Simple mode always uses 2 s.
int obsStreamKeyintSec();

// Platform limits from OBS's rtmp-services list (services.json): the copy OBS
// keeps updated in its config, else the one it shipped with. Empty if neither
// can be read.
QByteArray obsServicesJson();

// The best AAC encoder available: the preferred id if it is a valid AAC
// encoder here, else Apple CoreAudio AAC (Windows), else FFmpeg AAC.
QString resolveAacEncoder(const QString &preferred);

// Human label for an encoder id, e.g. "CoreAudio AAC".
QString encoderLabel(const QString &id);
