/*
Red Warden Pre-Flight
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mic-test.hpp"
#include "multistream-probe.hpp"
#include "obs-probe.hpp"
#include "preflight-eval.hpp"
#include "preflight-settings.hpp"

#include <obs-frontend-api.h>

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVector>

// Everything between OBS and the evaluator. UI thread only. Owns the settings, the OBS probe, the
// mic test, the Multistream probe (its own worker thread) and the 1 s timer.
class PreflightController : public QObject {
	Q_OBJECT
public:
	explicit PreflightController(QObject *parent = nullptr);
	~PreflightController() override;

	const preflight::Settings &settings() const { return settings_; }
	// Saves, expires the mic pass if the mic changed, pokes the Multistream probe, re-evaluates.
	void setSettings(const preflight::Settings &s);

	QVector<preflight::CheckResult> results() const { return results_; }

	void startMicTest();
	bool micTestRunning() const { return mic_.running(); }
	void setItemTicked(int index, bool ticked);

	preflight::GoLiveDecision goLiveDecision() const;
	void startStreaming(); // obs_frontend_streaming_start(); the caller has already asked the user if needed

	// OBS's own Start Streaming went ahead while checks were red (for the dock's banner). Cleared
	// when the stream stops.
	bool startedWithReds() const { return startedWithReds_; }
	QStringList startedWithRedsList() const { return startedReds_; }

	void handleFrontendEvent(obs_frontend_event event);
	void shutdown(); // idempotent; call at OBS_FRONTEND_EVENT_EXIT before Qt objects die

	static QString stateName(preflight::State s);
	static QString checkName(preflight::CheckId id);

signals:
	void resultsChanged(QVector<preflight::CheckResult> results);
	void micTestRunningChanged(bool running);

private:
	void tick();
	void logStates(const QVector<preflight::CheckResult> &res);
	void syncTicks();

	preflight::Settings settings_;
	preflight::ObsProbe obs_;
	preflight::MicTest mic_;
	preflight::MultistreamProbe probe_;
	QTimer timer_;
	QVector<bool> ticks_;
	QVector<preflight::CheckResult> results_;
	QVector<int> lastStates_;
	bool startedWithReds_ = false;
	QStringList startedReds_;
	bool testHooks_ = false;
	bool shutdown_ = false;
};
