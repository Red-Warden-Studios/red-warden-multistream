/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config.hpp"
#include "dock-ui.hpp"
#include "importers.hpp"
#include "obs-audio.hpp"
#include "output-runner.hpp"
#include "platform-limits.hpp"
#include "retry-governor.hpp"
#include "session-report.hpp"
#include "update-check.hpp"

#include <QAbstractButton>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <obs-hotkey.h>

#include <atomic>
#include <functional>
#include <thread>
#include <map>
#include <set>
#include <memory>

class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;

class MultistreamDock : public QWidget {
	Q_OBJECT

public:
	explicit MultistreamDock(QWidget *parent = nullptr);
	// (OBS themes can change at runtime)

	~MultistreamDock() override;

	// Called from the OBS frontend event callback (UI thread).
	void onStreamingStarted();
	void onStreamingStopping();
	void onProfileChanging();
	void onProfileChanged();
	void shutdown();

	// ---- external control: hotkeys, obs-websocket, Stream Deck ----------
	struct DestinationInfo {
		QString id, name, platform, state;
		bool enabled = false;
		double kbps = 0.0;
	};
	std::vector<DestinationInfo> snapshot();
	// mode: 0 = off, 1 = on, 2 = toggle. Matches an id, or a name (case-
	// insensitive). Returns false if nothing matched.
	bool setEnabledByKey(const QString &idOrName, int mode, DestinationInfo *out = nullptr);
	void setAll(bool on);
	// Told about every state change; obs-websocket turns these into events.
	std::function<void(const DestinationInfo &)> stateListener;

protected:
	void changeEvent(QEvent *e) override;

private:
	int snapshotCount = 0;
	QString snapshotDir;
	void reload();
	void save();
	void rebuild();
	void refreshCard(const QString &id);
	ui::RowView rowView(const Destination &d);
	void applyTheme();
	void takeSnapshot(); // RWS_MULTISTREAM_SNAPSHOT: render the dock to PNGs for review
	void refreshAll();
	void refreshSummary();

	// Update notice: one line at the top of the dock, only when a newer version
	// is published. See update-check.hpp.
	UpdateChecker updates;
	QLabel *updateNote = nullptr;
	updatecheck::Manifest pendingUpdate;
	void showUpdate(const updatecheck::Manifest &m);

	Destination *find(const QString &id);
	OutputRunner *runner(const QString &id);
	void startWhenMainReady();

	// Persistent reconnect, always through the connection budget.
	void scheduleRetry(const QString &id, int64_t delayMs, bool heldByBudget);
	void cancelRetry(const QString &id);
	void onRunnerChanged(const QString &id);

	void setOn(const QString &id, bool on);
	DestinationInfo info(const Destination &d);
	void notify(const QString &id);

	void syncHotkeys();      // (re)register one toggle hotkey per destination
	void captureHotkeys();   // copy current bindings into cfg before saving
	void unregisterHotkeys();
	static void onHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *, bool pressed);
	// skipMainWait: start a shared-encoder destination even though OBS's own
	// stream has not produced a frame yet (used once the wait has timed out).
	void startOne(const QString &id, bool skipMainWait = false);
	void stopOne(const QString &id);
	void stopAll(bool immediate);

	void addDestination();
	void editDestination(const QString &id);

	MultistreamConfig cfg;
	std::map<QString, std::unique_ptr<OutputRunner>> runners;
	std::map<QString, ui::DestinationRow *> rows;

	QLabel *headline = nullptr;
	QLabel *subline = nullptr;
	ui::BeaconStrip *strip = nullptr;
	QPushButton *addButton = nullptr;
	QVBoxLayout *list = nullptr;
	ui::Theme theme;
	QTimer statsTimer;
	// Upload capacity: measured (Cloudflare speed test) or entered by hand.
	GlobalSettings globals;
	QLabel *note = nullptr;           // upload estimate and warnings, under the strip
	QToolButton *menuButton = nullptr;
	std::unique_ptr<std::thread> uploadThread;
	std::atomic<bool> uploadCancel{false};
	bool uploadTesting = false;
	double uploadProgressSeconds = 0.0;
	double uploadProgressMbps = 0.0;
	void startUploadTest();
	void finishUploadTest(bool ok, double mbps, const QString &error);
	void enterUploadSpeed();

	// Import from obs-multi-rtmp / Aitum Multistream (current OBS profile).
	std::vector<ImportCandidate> findImportCandidates();
	void addImported(const std::vector<ImportCandidate> &chosen);
	std::set<QString> existingServerKeys();
	void importDestinations();
	int importableCount = 0; // shown as a nudge while the dock is empty

	// End-of-stream report: what happened on each destination.
	SessionReport report;
	QLabel *recap = nullptr;   // "Last stream: ..." line under the note
	QString reportsDir;        // plugin_config/rws-multistream/reports
	int64_t lastTrafficMs = 0;
	QString reportStamp;       // file name part: when the stream started
	SessionReport::Phase phaseFor(const Destination &d);
	void updateReport(const QString &id);
	void finishReport();

	// What each platform accepts (from OBS's services.json), and advice when a
	// destination sends more.
	std::map<QString, PlatformLimits> limits;
	std::vector<LimitIssue> limitIssues(const Destination &d);
	void showReport();
	double plannedUploadKbps(); // what the enabled platforms will need, with overhead

	// OBS's own stream, measured the same way as destinations, so the header
	// can show the TOTAL upload: the number people know from their internet plan.
	void pollMainStream();
	uint64_t mainLastBytes = 0;
	qint64 mainLastMs = 0;
	double mainKbps = 0.0;
	bool closing = false;
	ObsStreamAudio obsAudio;

	// The connection budget itself is connectionBudget(id) (connection-budget.hpp),
	// shared with the Twitch bandwidth test.
	struct RetryState {
		int autoFailures = 0;    // consecutive automatic retries without success
		QTimer *timer = nullptr; // pending retry, if any
		int64_t retryAtMs = 0;
		bool heldByBudget = false; // waiting because the budget is spent
		OutputRunner::State lastState = OutputRunner::State::Idle;
		// Quick reconnect after a live connection dropped (replaces libobs' own
		// reconnect, which bypassed the budget). Cleared on Live or a user stop.
		bool recovering = false;
		int dropAttempts = 0; // quick reconnects made since the drop
	};
	std::map<QString, RetryState> retries;

	std::map<obs_hotkey_id, QString> hotkeyDest; // toggle hotkey -> destination
	obs_hotkey_id allOnHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id allOffHotkey = OBS_INVALID_HOTKEY_ID;

	// Shared-encoder destinations wait here until OBS's own stream is
	// actually sending frames; attaching earlier can stall the output.
	std::set<QString> waitingForMain;
	QTimer mainReadyTimer;
	qint64 mainWaitStartedMs = 0;
};
