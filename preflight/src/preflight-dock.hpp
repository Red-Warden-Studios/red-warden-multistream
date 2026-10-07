/*
Red Warden Pre-Flight
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "preflight-controller.hpp"
#include "update-check.hpp"

#include <obs-frontend-api.h>

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;
class QVBoxLayout;
class PreflightCheckRow;

// The Pre-Flight dock: setup card, one row per check, the Go Live bar, the "you went live with
// problems" banner and the once-per-start update notice. All UI-thread only.
class PreflightDock : public QWidget {
	Q_OBJECT
public:
	explicit PreflightDock(QWidget *parent = nullptr);
	~PreflightDock() override;

	// Routed from the frontend event callback (UI thread).
	void handleFrontendEvent(obs_frontend_event event);
	// Called at OBS_FRONTEND_EVENT_EXIT: stop the update worker, the controller and its timers.
	void shutdown();

private:
	void showUpdate(const updatecheck::Manifest &m);
	void scheduleUpdateCheck();
	bool updateCheckScheduled = false;
	void applyResults(const QVector<preflight::CheckResult> &results);
	void syncCustomItems();
	void updateMicButton();
	void updateLive();
	void openSettings();
	void goLive();
	void installTestHooks();
	QWidget *mainWindow() const;

	PreflightController *controller = nullptr;
	QLabel *updateNote = nullptr;
	QFrame *banner = nullptr;
	QLabel *bannerLabel = nullptr;
	QFrame *setupCard = nullptr;
	QWidget *rowsHost = nullptr;
	QVBoxLayout *rowsLayout = nullptr;
	QPushButton *goBtn = nullptr;
	QPushButton *gearBtn = nullptr;
	QMap<int, PreflightCheckRow *> rows;
	QVector<int> order;
	QVector<bool> ticks;
	QStringList customCache;
	UpdateChecker updates;
	updatecheck::Manifest pendingUpdate;
	bool checkUpdates = true;
	bool closing = false;
	bool micRunning = false;
	bool live = false;
	bool startedFromDock = false;
};
