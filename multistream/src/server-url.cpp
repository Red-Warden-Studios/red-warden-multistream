
/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "server-url.hpp"

#include <QUrl>

bool serverUrlContainsKey(const QString &server, const QString &key)
{
	const QString k = key.trimmed();
	if (k.isEmpty())
		return false;

	// Split the text exactly as typed (QUrl would normalize it). Everything
	// before the first '/', '?' or '#' after "://" is scheme and host, which a
	// short key such as "live" could match by accident, so it is left out.
	QString tail = server;
	const int schemeEnd = server.indexOf(QStringLiteral("://"));
	if (schemeEnd >= 0) {
		int i = schemeEnd + 3;
		while (i < server.size() && server.at(i) != QLatin1Char('/') && server.at(i) != QLatin1Char('?') &&
		       server.at(i) != QLatin1Char('#'))
			i++;
		tail = server.mid(i);
	}

	if (tail.contains(k) || QUrl::fromPercentEncoding(tail.toUtf8()).contains(k))
		return true;
	// A key long enough not to match a host name by accident is refused
	// anywhere in the URL, the host included.
	return k.size() >= 8 && (server.contains(k) || QUrl::fromPercentEncoding(server.toUtf8()).contains(k));
}
