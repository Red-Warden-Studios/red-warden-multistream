/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "report-dialog.hpp"
#include "dock-ui.hpp"
#include "platforms.hpp"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>

namespace {
QString esc(const QString &s)
{
	return s.toHtmlEscaped();
}

QString pct(double share)
{
	// Floor, so 99.96% never reads as 100% when something did go wrong.
	return QString::number(std::floor(share * 1000.0) / 10.0, 'f', 1) + QLatin1Char('%');
}
} // namespace

ReportDialog::ReportDialog(const SessionReport &r, const QString &reportsDir, QWidget *parent) : QDialog(parent)
{
	setWindowTitle(QStringLiteral("Stream report"));
	setWindowIcon(QIcon(QStringLiteral(":/rws-multistream/logo-64.png")));
	resize(520, 560);
	const ui::Theme t = ui::Theme::from(palette());
	const QString secondary = ui::Theme::css(t.secondary);

	auto *root = new QVBoxLayout(this);
	auto *title = new QLabel(QStringLiteral("%1 stream, started %2").arg(SessionReport::formatDuration(r.durationMs()), r.startedAt()),
				 this);
	title->setWordWrap(true);
	title->setStyleSheet(QStringLiteral("font-size:15px; font-weight:600;"));
	root->addWidget(title);

	QString html;
	const auto list = r.destinations();
	if (list.empty())
		html += QStringLiteral("<p>No destinations were switched on during this stream.</p>");
	for (const auto *d : list) {
		QString verdict;
		QColor color = t.secondary;
		if (!d->everLive) {
			verdict = QStringLiteral("Never went live");
			color = t.error;
		} else if (d->drops == 0) {
			verdict = QStringLiteral("Stayed live");
			color = t.text;
		} else {
			verdict = d->drops == 1 ? QStringLiteral("Dropped once") : QStringLiteral("Dropped %1 times").arg(d->drops);
			color = t.ember;
		}
		html += QStringLiteral("<p style=\"margin-top:10px; margin-bottom:2px;\"><b>%1</b> <span style=\"color:%2;\">%3</span>"
				       "<br><span style=\"color:%4;\">%5</span></p>")
				.arg(esc(d->name), ui::Theme::css(color), esc(verdict), secondary, esc(platformById(d->platform).label));

		QStringList lines;
		if (!d->everLive) {
			if (!d->lastError.isEmpty())
				lines << esc(d->lastError);
		} else {
			lines << QStringLiteral("Live for %1, %2 of the time it was on")
					 .arg(SessionReport::formatDuration(d->liveMs), pct(d->uptime()));
			if (d->drops > 0)
				lines << QStringLiteral("Offline %1 in total; %2")
						 .arg(SessionReport::formatDuration(d->outageMs),
						      d->recoveries >= d->drops ? (d->drops == 1 ? QStringLiteral("came back")
												 : QStringLiteral("came back each time"))
						      : d->recoveries == 0 ? QStringLiteral("did not come back")
									   : QStringLiteral("came back %1 of %2 times")
										     .arg(d->recoveries)
										     .arg(d->drops));
			if (d->stalls > 0)
				lines << (d->stalls == 1 ? QStringLiteral("Stopped sending data once and was restarted")
							 : QStringLiteral("Stopped sending data %1 times and was restarted").arg(d->stalls));
			lines << QStringLiteral("Sent %1 at an average of %2 kbps")
					 .arg(SessionReport::formatBytes(d->sentBytes()))
					 .arg(qRound(d->avgKbps()));
			if (d->totalFrames() > 0 && d->droppedFrames() > 0)
				lines << QStringLiteral("%1 of %2 frames dropped by the network (%3)")
						 .arg(d->droppedFrames())
						 .arg(d->totalFrames())
						 .arg(pct((double)d->droppedFrames() / d->totalFrames()));
		}
		html += QStringLiteral("<p style=\"margin-top:0; margin-bottom:4px;\">%1</p>").arg(lines.join(QStringLiteral("<br>")));

		QStringList ev;
		for (const auto &e : d->events) {
			if (!d->everLive && e.text == d->lastError)
				continue; // already the headline for this destination
			ev << QStringLiteral("%1&nbsp;&nbsp;%2").arg(SessionReport::formatDuration(e.atMs), esc(e.text));
		}
		if (!ev.isEmpty()) {
			html += QStringLiteral("<p style=\"margin-top:0; color:%1; font-size:11px;\">%2</p>")
					.arg(secondary, ev.join(QStringLiteral("<br>")));
		}
	}

	auto *body = new QTextBrowser(this);
	body->setOpenLinks(false);
	body->setFrameShape(QFrame::NoFrame);
	body->setHtml(html);
	root->addWidget(body, 1);

	auto *hint = new QLabel(QStringLiteral("Times in the timeline are from when you pressed Start Streaming."), this);
	hint->setWordWrap(true);
	hint->setStyleSheet(QStringLiteral("color:%1; font-size:11px;").arg(secondary));
	root->addWidget(hint);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	QPushButton *copy = buttons->addButton(QStringLiteral("Copy report"), QDialogButtonBox::ActionRole);
	const QString text = r.text();
	connect(copy, &QPushButton::clicked, this, [copy, text] {
		QApplication::clipboard()->setText(text);
		copy->setText(QStringLiteral("Copied"));
		QTimer::singleShot(1500, copy, [copy] { copy->setText(QStringLiteral("Copy report")); });
	});
	if (!reportsDir.isEmpty()) {
		QPushButton *folder = buttons->addButton(QStringLiteral("Open past reports"), QDialogButtonBox::ActionRole);
		connect(folder, &QPushButton::clicked, this,
			[reportsDir] { QDesktopServices::openUrl(QUrl::fromLocalFile(reportsDir)); });
	}
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	root->addWidget(buttons);
}
