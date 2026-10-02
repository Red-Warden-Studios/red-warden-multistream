/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "retry-governor.hpp"

#include <QString>

#include <map>

// One connection budget per destination, shared by everything that opens a
// connection for it: the first connect, quick reconnects after a drop, the
// persistent retries, stall restarts, the Retry button AND the Twitch bandwidth
// test. Keyed by destination id; lives for the whole OBS session, so stopping
// and restarting can't reset it. UI thread only.
inline RetryGovernor &connectionBudget(const QString &destId)
{
	static std::map<QString, RetryGovernor> budgets;
	return budgets[destId];
}
