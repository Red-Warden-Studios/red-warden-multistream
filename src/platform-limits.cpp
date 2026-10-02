/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platform-limits.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <algorithm>

namespace {
// services.json entry names for each of our platforms, best match first.
const std::vector<std::pair<QString, QStringList>> &serviceNames()
{
	static const std::vector<std::pair<QString, QStringList>> names = {
		{QStringLiteral("twitch"), {QStringLiteral("Twitch")}},
		{QStringLiteral("youtube"), {QStringLiteral("YouTube - RTMPS"), QStringLiteral("YouTube / YouTube Gaming")}},
		{QStringLiteral("facebook"), {QStringLiteral("Facebook Live")}},
		{QStringLiteral("x"), {QStringLiteral("X"), QStringLiteral("Twitter")}},
		{QStringLiteral("kick"), {QStringLiteral("Kick")}},
		{QStringLiteral("tiktok"), {QStringLiteral("TikTok"), QStringLiteral("TikTok Live")}},
		{QStringLiteral("rumble"), {QStringLiteral("Rumble")}},
	};
	return names;
}

int toInt(const QJsonValue &v)
{
	return v.isDouble() ? v.toInt() : v.toString().toInt();
}
} // namespace

std::map<QString, PlatformLimits> parseServiceLimits(const QByteArray &json)
{
	std::map<QString, QJsonObject> byName;
	for (const auto &v : QJsonDocument::fromJson(json).object().value(QStringLiteral("services")).toArray()) {
		const QJsonObject s = v.toObject();
		const QString name = s.value(QStringLiteral("name")).toString();
		if (!name.isEmpty() && !byName.count(name))
			byName.emplace(name, s);
	}

	std::map<QString, PlatformLimits> out;
	for (const auto &[id, names] : serviceNames()) {
		for (const QString &n : names) {
			auto it = byName.find(n);
			if (it == byName.end())
				continue;
			const QJsonObject r = it->second.value(QStringLiteral("recommended")).toObject();
			PlatformLimits l;
			l.service = n;
			l.maxVideoKbps = toInt(r.value(QStringLiteral("max video bitrate")));
			l.maxAudioKbps = toInt(r.value(QStringLiteral("max audio bitrate")));
			l.keyintSec = toInt(r.value(QStringLiteral("keyint")));
			l.maxFps = toInt(r.value(QStringLiteral("max fps")));
			for (const auto &res : r.value(QStringLiteral("supported resolutions")).toArray()) {
				const QStringList wh = res.toString().split(QLatin1Char('x'));
				if (wh.size() == 2)
					l.maxHeight = std::max(l.maxHeight, wh[1].toInt());
			}
			out.emplace(id, l);
			break;
		}
	}
	return out;
}

std::vector<LimitIssue> checkLimits(const QString &platformId, const QString &label, const PlatformLimits &l,
				    const StreamFacts &f)
{
	std::vector<LimitIssue> out;
	const QString sender = f.shared ? QStringLiteral("OBS sends") : QStringLiteral("this sends");

	if (l.maxVideoKbps > 0 && f.videoKbps > l.maxVideoKbps)
		out.push_back({true, (f.shared ? QStringLiteral("%1 takes video up to %2 kbps (%3 %4). Give it its own encoder "
								"at %2 kbps or less, or viewers may buffer.")
					       : QStringLiteral("%1 takes video up to %2 kbps (%3 %4). Lower it in Edit."))
					     .arg(label)
					     .arg(l.maxVideoKbps)
					     .arg(sender)
					     .arg(f.videoKbps)});

	if (l.keyintSec > 0 && f.keyintSec >= 0 && (f.keyintSec == 0 || f.keyintSec > l.keyintSec))
		out.push_back({true, QStringLiteral("%1 expects a keyframe every %2 s. Set Keyframe Interval to %2 s in OBS "
						    "Settings > Output > Streaming.")
					     .arg(label)
					     .arg(l.keyintSec)});

	if (l.maxHeight > 0 && f.height > l.maxHeight)
		out.push_back({true, QStringLiteral("%1 takes up to %2p (%3 %4p).").arg(label).arg(l.maxHeight).arg(sender).arg(f.height)});

	if (l.maxFps > 0 && f.fps > l.maxFps + 0.5)
		out.push_back({true, QStringLiteral("%1 takes up to %2 fps (OBS runs at %3).")
					     .arg(label)
					     .arg(l.maxFps)
					     .arg(QString::number(f.fps, 'f', f.fps == (int)f.fps ? 0 : 2))});

	// Audio: OBS lists 160 kbps for YouTube, a conservative figure; YouTube
	// ingests higher AAC bitrates and re-encodes for viewers anyway. Audio
	// quality matters more to streamers than that cap, so no nagging there.
	// Elsewhere it is a gentle note, not a warning.
	if (platformId != QLatin1String("youtube") && l.maxAudioKbps > 0 && f.audioKbps > l.maxAudioKbps)
		out.push_back({false, (f.shared ? QStringLiteral("%1 lists audio up to %2 kbps (%3 %4). If %1 objects, give it "
								 "its own encoder with %2 kbps audio.")
						: QStringLiteral("%1 lists audio up to %2 kbps (%3 %4). If %1 objects, set its "
								 "audio to %2 kbps in Edit."))
					      .arg(label)
					      .arg(l.maxAudioKbps)
					      .arg(sender)
					      .arg(f.audioKbps)});
	return out;
}
