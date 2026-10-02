/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include <QColor>
#include <QString>
#include <vector>

// A streaming platform the user can pick when adding a destination.
// The badge text is always shown next to the color so a platform is
// never identified by color alone.
struct PlatformPreset {
	QString id;            // stable key written to the profile config
	QString label;         // human name
	QString badge;         // 2-3 letter tag shown on the card
	QColor color;          // badge background
	QString defaultServer; // empty: the platform issues a per-account URL
	QString hint;          // where to find the URL and key
};

const std::vector<PlatformPreset> &platformPresets();
const PlatformPreset &platformById(const QString &id);
QColor badgeTextColor(const QColor &background);
