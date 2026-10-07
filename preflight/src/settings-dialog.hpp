/*
Red Warden Pre-Flight
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "preflight-settings.hpp"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QListWidget;
class QRadioButton;

// The Pre-Flight setup / settings dialog. Edits a copy of the settings; the caller applies
// result() (with setupDone = true) after exec() returns Accepted. UI thread only.
class SettingsDialog : public QDialog {
public:
	SettingsDialog(const preflight::Settings &current, QWidget *parent);

	preflight::Settings result() const;

private:
	preflight::Settings base_;
	QRadioButton *mainOnly_ = nullptr;
	QRadioButton *multiOnly_ = nullptr;
	QRadioButton *both_ = nullptr;
	QComboBox *mic_ = nullptr;
	QComboBox *desktop_ = nullptr;
	QComboBox *scene_ = nullptr;
	QCheckBox *record_ = nullptr;
	QCheckBox *checks_[8] = {};
	QListWidget *items_ = nullptr;
	QCheckBox *updates_ = nullptr;
};
