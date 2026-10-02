/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "bandwidth-test.hpp"

#include <QDialog>

class QLabel;
class QProgressBar;
class QPushButton;

// Runs TwitchBandwidthTest as soon as it opens and explains the result.
class BandwidthDialog : public QDialog {
	Q_OBJECT
public:
	BandwidthDialog(const Destination &d, const QString &streamKey, QWidget *parent = nullptr);

	static QString verdict(const TwitchBandwidthTest::Result &r); // plain text, for the dialog and the log

signals:
	void done(const QString &verdict);

private:
	void run();
	void onProgress(int seconds, double kbps);
	void onFinished(const TwitchBandwidthTest::Result &r);

	Destination dest;
	QString key;
	TwitchBandwidthTest test;
	QLabel *status;
	QProgressBar *bar;
	QLabel *result;
	QPushButton *again;
	QPushButton *cancel;
};
