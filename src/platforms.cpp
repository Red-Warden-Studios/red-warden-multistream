/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platforms.hpp"

#include <cmath>

// Default servers come from OBS's own rtmp-services list where OBS ships one
// (Twitch, YouTube RTMPS, Facebook, X). Kick, TikTok and Rumble hand each
// account its own ingest URL, so those start blank and the hint says where
// to copy it from.
const std::vector<PlatformPreset> &platformPresets()
{
	static const std::vector<PlatformPreset> presets = {
		{"twitch", "Twitch", "TW", QColor("#9146FF"), "rtmp://live.twitch.tv/app",
		 "Stream key: Twitch Creator Dashboard > Settings > Stream."},
		{"youtube", "YouTube", "YT", QColor("#FF0033"), "rtmps://a.rtmps.youtube.com:443/live2",
		 "Stream key: YouTube Studio > Go Live > Stream settings."},
		{"kick", "Kick", "KK", QColor("#53FC18"), "",
		 "Kick gives every channel its own Stream URL. Copy both the Stream URL and the Stream Key "
		 "from your Kick creator dashboard's stream settings."},
		{"tiktok", "TikTok Live", "TT", QColor("#FE2C55"), "",
		 "TikTok issues a Server URL and Stream Key through LIVE Producer (your account needs LIVE "
		 "access). They can change between sessions, so paste fresh ones if TikTok refuses the connection."},
		{"facebook", "Facebook Live", "FB", QColor("#1877F2"), "rtmps://rtmp-api.facebook.com:443/rtmp/",
		 "Stream key: Facebook Live Producer > Streaming software."},
		{"rumble", "Rumble", "RU", QColor("#85C742"), "",
		 "Copy the Server URL and Stream Key from Rumble's live stream setup page."},
		{"x", "X", "X", QColor("#2A2A2A"), "rtmp://va.pscp.tv:80/x",
		 "X Media Studio > Producer shows the server and key for your broadcast. Your assigned "
		 "region server may differ from this default."},
		{"custom", "Custom RTMP", "RT", QColor("#C0392B"), "",
		 "Any rtmp:// or rtmps:// ingest URL, for example a Restream or self-hosted relay."},
	};
	return presets;
}

const PlatformPreset &platformById(const QString &id)
{
	for (const auto &p : platformPresets())
		if (p.id == id)
			return p;
	return platformPresets().back();
}

QColor badgeTextColor(const QColor &bg)
{
	// Relative luminance per WCAG; pick black or white for contrast.
	auto lin = [](double c) {
		c /= 255.0;
		return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
	};
	const double l = 0.2126 * lin(bg.red()) + 0.7152 * lin(bg.green()) + 0.0722 * lin(bg.blue());
	return l > 0.35 ? QColor("#0A0506") : QColor("#FFFFFF");
}
