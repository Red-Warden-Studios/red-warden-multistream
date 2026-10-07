/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "obs-audio.hpp"

#include <obs-frontend-api.h>
#include <obs.h>
#include <util/config-file.h>

#include <cstring>
#include <QFile>
#include <QString>
#include <QStringList>

namespace {
bool isAac(const char *id)
{
	const char *codec = id && *id ? obs_get_encoder_codec(id) : nullptr;
	return codec && std::strcmp(codec, "aac") == 0;
}
} // namespace

QString resolveAacEncoder(const QString &preferred)
{
	const QByteArray p = preferred.toUtf8();
	if (isAac(p.constData()))
		return preferred;
	if (isAac("CoreAudio_AAC"))
		return QStringLiteral("CoreAudio_AAC");
	return QStringLiteral("ffmpeg_aac");
}

QString encoderLabel(const QString &id)
{
	const char *name = obs_encoder_get_display_name(id.toUtf8().constData());
	return name ? QString::fromUtf8(name) : id;
}

ObsStreamAudio obsStreamAudio()
{
	ObsStreamAudio a;
	config_t *c = obs_frontend_get_profile_config();
	if (!c) {
		a.encoderId = resolveAacEncoder(QString());
		return a;
	}

	const char *mode = config_get_string(c, "Output", "Mode");
	if (mode && std::strcmp(mode, "Advanced") == 0) {
		int idx = (int)config_get_int(c, "AdvOut", "TrackIndex"); // 1-based
		if (idx < 1 || idx > 6)
			idx = 1;
		a.track = idx - 1;
		const QByteArray key = QStringLiteral("Track%1Bitrate").arg(idx).toUtf8();
		a.bitrate = (int)config_get_uint(c, "AdvOut", key.constData());
		a.encoderId = resolveAacEncoder(QString::fromUtf8(config_get_string(c, "AdvOut", "AudioEncoder")));
	} else {
		a.track = 0;
		a.bitrate = (int)config_get_uint(c, "SimpleOutput", "ABitrate");
		// Simple mode stores a family ("aac"/"opus"), not an encoder id.
		a.encoderId = resolveAacEncoder(QString());
	}
	if (a.bitrate <= 0)
		a.bitrate = 160; // OBS's own default
	return a;
}

int obsStreamVideoBitrate()
{
	int kbps = 0;
	config_t *c = obs_frontend_get_profile_config();
	const char *mode = c ? config_get_string(c, "Output", "Mode") : nullptr;
	if (mode && std::strcmp(mode, "Advanced") == 0) {
		if (char *profile = obs_frontend_get_current_profile_path()) {
			const QByteArray path = (QString::fromUtf8(profile) + "/streamEncoder.json").toUtf8();
			bfree(profile);
			if (obs_data_t *d = obs_data_create_from_json_file(path.constData())) {
				kbps = (int)obs_data_get_int(d, "bitrate");
				obs_data_release(d);
			}
		}
		if (kbps <= 0)
			kbps = 2500; // OBS's default for an untouched advanced encoder
	} else if (c) {
		kbps = (int)config_get_uint(c, "SimpleOutput", "VBitrate");
	}
	return kbps > 0 ? kbps : 2500;
}

int obsStreamKeyintSec()
{
	config_t *c = obs_frontend_get_profile_config();
	const char *mode = c ? config_get_string(c, "Output", "Mode") : nullptr;
	if (!mode || std::strcmp(mode, "Advanced") != 0)
		return 2;
	int k = 0;
	if (char *profile = obs_frontend_get_current_profile_path()) {
		const QByteArray path = (QString::fromUtf8(profile) + "/streamEncoder.json").toUtf8();
		bfree(profile);
		if (obs_data_t *d = obs_data_create_from_json_file(path.constData())) {
			k = (int)obs_data_get_int(d, "keyint_sec");
			obs_data_release(d);
		}
	}
	return k;
}

QByteArray obsServicesJson()
{
	obs_module_t *m = obs_get_module("rtmp-services");
	if (!m)
		return {};
	QStringList candidates;
	if (char *cfg = obs_module_get_config_path(m, "services.json")) {
		candidates << QString::fromUtf8(cfg);
		bfree(cfg);
	}
	if (const char *data = obs_get_module_data_path(m))
		candidates << QString::fromUtf8(data) + QStringLiteral("/services.json");
	for (const QString &p : candidates) {
		QFile f(p);
		if (f.open(QIODevice::ReadOnly)) {
			const QByteArray json = f.readAll();
			if (!json.isEmpty())
				return json;
		}
	}
	return {};
}
