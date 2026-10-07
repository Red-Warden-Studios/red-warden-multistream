/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "session-report.hpp"

#include <QDialog>

// The end-of-stream report: one block per destination, with its timeline.
class ReportDialog : public QDialog {
	Q_OBJECT
public:
	// reportsDir: where the text copies are kept (offered as a button); may be empty.
	ReportDialog(const SessionReport &report, const QString &reportsDir, QWidget *parent = nullptr);
};
