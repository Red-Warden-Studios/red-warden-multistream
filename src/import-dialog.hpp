/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "importers.hpp"

#include <QDialog>
#include <set>
#include <vector>

class QListWidget;
class QPushButton;

class ImportDialog : public QDialog {
	Q_OBJECT
public:
	// existing: "server\nkey" of destinations already set up, so duplicates
	// are shown but cannot be imported twice.
	ImportDialog(std::vector<ImportCandidate> candidates, const std::set<QString> &existing,
		     QWidget *parent = nullptr);
	std::vector<ImportCandidate> selected() const;

private:
	void updateButton();
	std::vector<ImportCandidate> items;
	QListWidget *list;
	QPushButton *importButton;
};
