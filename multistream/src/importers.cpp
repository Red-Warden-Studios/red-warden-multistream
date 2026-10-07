/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "importers.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace {
QString str(const QJsonObject &o, const char *k)
{
	return o.value(QLatin1String(k)).toString();
}

QJsonObject findById(const QJsonArray &arr, const QString &id)
{
	for (const auto &v : arr) {
		const QJsonObject o = v.toObject();
		if (o.value(QStringLiteral("id")).toString() == id)
			return o;
	}
	return {};
}

void parseResolution(const QString &res, int *w, int *h)
{
	const QStringList parts = res.split(QLatin1Char('x'), Qt::SkipEmptyParts);
	if (parts.size() == 2) {
		bool ok1 = false, ok2 = false;
		const int a = parts[0].trimmed().toInt(&ok1), b = parts[1].trimmed().toInt(&ok2);
		if (ok1 && ok2 && a > 0 && b > 0) {
			*w = a;
			*h = b;
		}
	}
}
} // namespace

QString guessPlatform(const QString &server)
{
	const QString host = QUrl(server.trimmed()).host().toLower();
	if (host.endsWith(QLatin1String("twitch.tv")))
		return QStringLiteral("twitch");
	if (host.contains(QLatin1String("youtube.com")))
		return QStringLiteral("youtube");
	if (host.endsWith(QLatin1String("live-video.net")) || host.contains(QLatin1String("kick")))
		return QStringLiteral("kick");
	if (host.contains(QLatin1String("facebook.com")))
		return QStringLiteral("facebook");
	if (host.contains(QLatin1String("tiktok")))
		return QStringLiteral("tiktok");
	if (host.contains(QLatin1String("rumble")))
		return QStringLiteral("rumble");
	if (host.endsWith(QLatin1String("pscp.tv")) || host == QLatin1String("x.com"))
		return QStringLiteral("x");
	return QStringLiteral("custom");
}

std::vector<ImportCandidate> parseMultiRtmp(const QByteArray &json)
{
	std::vector<ImportCandidate> out;
	const QJsonObject root = QJsonDocument::fromJson(json).object();
	const QJsonArray videos = root.value(QStringLiteral("video_configs")).toArray();
	const QJsonArray audios = root.value(QStringLiteral("audio_configs")).toArray();
	for (const auto &v : root.value(QStringLiteral("targets")).toArray()) {
		const QJsonObject t = v.toObject();
		ImportCandidate c;
		c.source = QStringLiteral("obs-multi-rtmp");
		c.name = str(t, "name");
		const QJsonObject sp = t.value(QStringLiteral("service-param")).toObject();
		c.server = str(sp, "server");
		c.key = str(sp, "key");
		const QString protocol = str(t, "protocol");
		if (!protocol.isEmpty() && protocol.compare(QLatin1String("RTMP"), Qt::CaseInsensitive) != 0)
			c.skipReason = QStringLiteral("%1 targets are not supported yet (RTMP only)").arg(protocol);

		const QString vid = str(t, "video-config");
		if (!vid.isEmpty()) {
			const QJsonObject vc = findById(videos, vid);
			if (!vc.isEmpty()) {
				c.shared = false;
				c.encoderId = str(vc, "encoder");
				c.videoBitrate = vc.value(QStringLiteral("param")).toObject().value(QStringLiteral("bitrate")).toInt();
				parseResolution(str(vc, "resolution"), &c.width, &c.height);
			}
		}
		const QString aid = str(t, "audio-config");
		if (!aid.isEmpty() && !c.shared) {
			const QJsonObject ac = findById(audios, aid);
			if (!ac.isEmpty()) {
				c.audioBitrate = ac.value(QStringLiteral("param")).toObject().value(QStringLiteral("bitrate")).toInt();
				if (ac.contains(QStringLiteral("mixerId")))
					c.audioTrack = ac.value(QStringLiteral("mixerId")).toInt();
			}
		}
		if (c.name.isEmpty())
			c.name = QUrl(c.server).host();
		c.platform = guessPlatform(c.server);
		if (!c.server.isEmpty() || !c.skipReason.isEmpty())
			out.push_back(c);
	}
	return out;
}

std::vector<ImportCandidate> parseAitum(const QByteArray &json, const QString &profileName)
{
	std::vector<ImportCandidate> out;
	const QJsonObject root = QJsonDocument::fromJson(json).object();
	for (const auto &pv : root.value(QStringLiteral("profiles")).toArray()) {
		const QJsonObject profile = pv.toObject();
		if (str(profile, "name") != profileName)
			continue;
		for (const auto &ov : profile.value(QStringLiteral("outputs")).toArray()) {
			const QJsonObject o = ov.toObject();
			ImportCandidate c;
			c.source = QStringLiteral("Aitum Multistream");
			c.name = str(o, "name");
			c.server = str(o, "stream_server");
			if (c.server.isEmpty())
				c.server = str(o, "server");
			c.key = str(o, "stream_key");
			if (c.key.isEmpty())
				c.key = str(o, "key");
			if (o.value(QStringLiteral("advanced")).toBool()) {
				c.shared = false;
				c.encoderId = str(o, "video_encoder");
				c.videoBitrate = o.value(QStringLiteral("video_encoder_settings"))
							 .toObject()
							 .value(QStringLiteral("bitrate"))
							 .toInt();
				c.audioBitrate = o.value(QStringLiteral("audio_encoder_settings"))
							 .toObject()
							 .value(QStringLiteral("bitrate"))
							 .toInt();
			}
			if (c.name.isEmpty())
				c.name = QUrl(c.server).host();
			c.platform = guessPlatform(c.server);
			if (!c.server.isEmpty())
				out.push_back(c);
		}
	}
	return out;
}
