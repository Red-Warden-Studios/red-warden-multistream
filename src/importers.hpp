/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// Reads destinations saved by other multistream plugins so switching to Red
// Warden Multistream does not mean retyping every server and stream key.
// Pure Qt Core (JSON in, candidates out): unit-tested without OBS in
// tests/importers-test.cpp.
//
//  obs-multi-rtmp (SoraYuki): <profile dir>/obs-multi-rtmp.json
//    { "targets": [ { "id", "name", "protocol", "service-param": {server, key},
//                     "video-config": id?, "audio-config": id? } ],
//      "video_configs": [ { "id", "encoder", "param": {bitrate}, "resolution": "WxH" } ],
//      "audio_configs": [ { "id", "encoder", "param": {bitrate}, "mixerId" } ] }
//  Aitum Multistream: <plugin_config>/aitum-multistream/config.json
//    { "profiles": [ { "name": <OBS profile>, "outputs": [ { "name",
//        "stream_server"|"server", "stream_key"|"key", "advanced",
//        "video_encoder", "video_encoder_settings": {bitrate}, ... } ] } ] }

#include <QByteArray>
#include <QString>
#include <vector>

struct ImportCandidate {
	QString source;   // "obs-multi-rtmp" or "Aitum Multistream"
	QString name;
	QString server;
	QString key;
	QString platform; // PlatformPreset id guessed from the server host
	bool shared = true;
	QString encoderId;
	int videoBitrate = 0;
	int audioBitrate = 0; // 0 = match OBS
	int audioTrack = -1;  // -1 = match OBS
	int width = 0, height = 0;
	QString skipReason; // non-empty: cannot be imported (e.g. SRT/WHIP target)
};

std::vector<ImportCandidate> parseMultiRtmp(const QByteArray &json);
std::vector<ImportCandidate> parseAitum(const QByteArray &json, const QString &profileName);
QString guessPlatform(const QString &server);
