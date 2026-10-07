/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "bandwidth-dialog.hpp"
#include "dock-ui.hpp"

#include <obs-frontend-api.h>

#include <QDialogButtonBox>
#include <QIcon>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QString mbps(double kbps)
{
	return QString::number(kbps / 1000.0, 'f', 1);
}
} // namespace

BandwidthDialog::BandwidthDialog(const Destination &d, const QString &streamKey, QWidget *parent)
	: QDialog(parent), dest(d), key(streamKey)
{
	setWindowTitle(QStringLiteral("Twitch bandwidth test"));
	setWindowIcon(QIcon(QStringLiteral(":/rws-multistream/logo-64.png")));
	setMinimumWidth(440);
	const ui::Theme t = ui::Theme::from(palette());

	auto *root = new QVBoxLayout(this);
	root->setSizeConstraint(QLayout::SetMinimumSize); // grow to fit the wrapped result text
	QString intro = QStringLiteral("Sends your stream to Twitch for about 20 seconds in bandwidth-test mode. Twitch "
				       "receives it but never shows it, so you are not live and nobody is notified.");
	if (obs_frontend_streaming_active())
		intro += QStringLiteral(" OBS is streaming right now, so the test shares your upload with the live stream.");
	auto *introLabel = new QLabel(intro, this);
	introLabel->setWordWrap(true);
	root->addWidget(introLabel);

	status = new QLabel(this);
	status->setStyleSheet(QStringLiteral("color:%1;").arg(ui::Theme::css(t.secondary)));
	root->addWidget(status);
	bar = new QProgressBar(this);
	bar->setRange(0, TwitchBandwidthTest::kMeasureSeconds);
	bar->setTextVisible(false);
	bar->setFixedHeight(6);
	root->addWidget(bar);
	result = new QLabel(this);
	result->setWordWrap(true);
	result->setVisible(false);
	root->addWidget(result);

	auto *buttons = new QDialogButtonBox(this);
	again = buttons->addButton(QStringLiteral("Test again"), QDialogButtonBox::ActionRole);
	cancel = buttons->addButton(QStringLiteral("Stop test"), QDialogButtonBox::RejectRole);
	auto *close = buttons->addButton(QDialogButtonBox::Close);
	connect(again, &QPushButton::clicked, this, &BandwidthDialog::run);
	connect(cancel, &QPushButton::clicked, &test, &TwitchBandwidthTest::cancel);
	connect(close, &QPushButton::clicked, this, &QDialog::reject);
	root->addWidget(buttons);

	connect(&test, &TwitchBandwidthTest::progress, this, &BandwidthDialog::onProgress);
	connect(&test, &TwitchBandwidthTest::finished, this, &BandwidthDialog::onFinished);
	connect(this, &QDialog::rejected, &test, &TwitchBandwidthTest::cancel);

	QTimer::singleShot(0, this, &BandwidthDialog::run);
}

void BandwidthDialog::run()
{
	result->setVisible(false);
	bar->setValue(0);
	again->setVisible(false);
	cancel->setVisible(true);
	QString error;
	if (!test.start(dest, key, &error)) {
		TwitchBandwidthTest::Result r;
		r.error = error;
		onFinished(r);
	}
}

void BandwidthDialog::onProgress(int seconds, double kbps)
{
	if (seconds < 0) {
		status->setText(QStringLiteral("Connecting to Twitch..."));
		return;
	}
	bar->setValue(seconds);
	status->setText(seconds == 0 ? QStringLiteral("Connected. Letting the stream settle...")
				     : QStringLiteral("Measuring: %1 Mbps").arg(mbps(kbps)));
}

void BandwidthDialog::onFinished(const TwitchBandwidthTest::Result &r)
{
	const ui::Theme t = ui::Theme::from(palette());
	status->setText(r.ok ? QStringLiteral("Done. Nothing was shown on Twitch.") : QStringLiteral("The test did not finish."));
	bar->setValue(r.ok ? TwitchBandwidthTest::kMeasureSeconds : bar->value());
	result->setText(verdict(r));
	result->setStyleSheet(QStringLiteral("color:%1;").arg(ui::Theme::css(r.steady() ? t.text : (r.ok ? t.ember : t.error))));
	result->setVisible(true);
	cancel->setVisible(false);
	again->setVisible(true);
	adjustSize();
	emit done(result->text());
}

QString BandwidthDialog::verdict(const TwitchBandwidthTest::Result &r)
{
	if (!r.ok) {
		if (r.error == QLatin1String("Cancelled"))
			return QStringLiteral("Stopped before the test finished.");
		QString s = r.error.isEmpty() ? QStringLiteral("The test could not start.") : r.error + QLatin1Char('.');
		if (r.error.contains(QLatin1String("stream key")))
			s += QStringLiteral(" Copy the stream key again from Twitch's dashboard (Settings > Stream).");
		else if (r.error.contains(QLatin1String("reach")) || r.error.contains(QLatin1String("connect")))
			s += QStringLiteral(" Check the server URL, or try again in a minute.");
		return s;
	}
	const double share = r.totalFrames > 0 ? 100.0 * r.droppedFrames / r.totalFrames : 0.0;
	const QString frames = r.droppedFrames == 0
				       ? QStringLiteral("no dropped frames")
				       : QStringLiteral("%1% of frames dropped").arg(QString::number(share, 'f', share < 1 ? 2 : 1));
	if (r.steady())
		return QStringLiteral("Twitch received a steady %1 Mbps (this stream needs %2 Mbps) with %3. Your connection "
				      "to this Twitch server handles this bitrate.")
			.arg(mbps(r.avgKbps), mbps(r.targetKbps), frames);
	if (r.avgKbps < 0.85 * r.targetKbps)
		return QStringLiteral("Twitch received only %1 of the %2 Mbps this stream needs, with %3. Lower the video "
				      "bitrate, or pick a Twitch server closer to you.")
			.arg(mbps(r.avgKbps), mbps(r.targetKbps), frames);
	return QStringLiteral("Twitch received %1 Mbps, but %2 on the way. Lower the video bitrate, or pick a Twitch "
			      "server closer to you.")
		.arg(mbps(r.avgKbps), frames);
}
