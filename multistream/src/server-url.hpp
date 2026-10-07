
/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// Pure Qt Core: unit-tested without OBS in tests/server-url-test.cpp.
#include <QString>

// True when the stream key appears in the part of an RTMP URL after the host
// (path, query or fragment), or, for a key of 8 or more characters, anywhere
// in it; either as typed or percent-decoded. OBS writes server URLs to its
// log, so a URL like this must be refused.
bool serverUrlContainsKey(const QString &server, const QString &key);
