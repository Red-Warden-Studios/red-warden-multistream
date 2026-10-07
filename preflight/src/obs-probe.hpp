/*
Red Warden Pre-Flight
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "disk-probe.hpp"
#include "preflight-eval.hpp"
#include "preflight-settings.hpp"

#include <QString>
#include <QStringList>
#include <QVector>

namespace preflight {

// Reads OBS (public libobs / frontend API only) and fills the evaluator's Inputs.
// UI thread only. Call gather() about once a second: it also feeds the rolling frame windows.
class ObsProbe {
public:
	struct Runtime {
		qint64 nowMs = 0;
		MultistreamStatus multistreamStatus = MultistreamStatus::Pending;
		QStringList multistreamNames;
		MicTestStatus micTest = MicTestStatus::NotRun;
		QVector<bool> ticks; // runtime state of the custom items, same order as Settings::customItems
	};

	Inputs gather(const Settings &settings, const Runtime &rt);

	void resetFrameWindows();   // profile change
	void resetNetworkWindow();  // stream started / stopped
	void resetRecordingCache(); // profile change: recording state is Unknown until the disk worker answers
	void shutdown();            // OBS exit: stop the disk worker (short bounded wait)

private:
	void fillRecording(Inputs::Recording &out, const Settings &settings);

	FrameWindow render_;
	FrameWindow encoder_;
	FrameWindow network_;
	DiskProbe disk_; // recording-folder checks run on its worker thread, never on the UI thread
};

} // namespace preflight
