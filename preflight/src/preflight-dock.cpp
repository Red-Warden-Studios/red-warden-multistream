/*
Red Warden Pre-Flight
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "preflight-dock.hpp"
#include "settings-dialog.hpp"

#include <obs-module.h>
#include <plugin-support.h>
#include <util/bmem.h>
#include <util/platform.h>

#include <QCheckBox>
#include <QDir>
#include <QDesktopServices>
#include <QDockWidget>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

using preflight::CheckId;
using preflight::CheckResult;
using preflight::State;

namespace {
// The harness sets this so the portable test copy never touches the network.
bool testHooksEnabled()
{
	return qEnvironmentVariableIsSet("RWS_PREFLIGHT_TEST");
}

QString T(const char *key, const char *fallback)
{
	const char *t = obs_module_text(key);
	return (t && qstrcmp(t, key) != 0) ? QString::fromUtf8(t) : QString::fromUtf8(fallback);
}

// A small colored circle holding a symbol, so state never depends on color alone.
class StatusMark : public QWidget {
public:
	explicit StatusMark(QWidget *parent) : QWidget(parent) { setFixedSize(20, 20); }
	void setState(State s)
	{
		if (init_ && s == state_)
			return;
		init_ = true;
		state_ = s;
		update();
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QColor fill;
		QColor ink(Qt::white);
		QString sym;
		switch (state_) {
		case State::Green:
			fill = QColor(0x2E, 0x9E, 0x5B);
			sym = QString(QChar(0x2713));
			break;
		case State::Amber:
			fill = QColor(0xF2, 0xA6, 0x3B);
			ink = QColor(0x20, 0x20, 0x20);
			sym = QStringLiteral("!");
			break;
		case State::Red:
			fill = QColor(0xC0, 0x39, 0x2B);
			sym = QString(QChar(0x00D7));
			break;
		default:
			fill = palette().color(QPalette::Mid);
			sym = QStringLiteral("?");
			break;
		}
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		p.setPen(Qt::NoPen);
		p.setBrush(fill);
		p.drawEllipse(rect().adjusted(1, 1, -1, -1));
		QFont f = font();
		f.setBold(true);
		f.setPixelSize(13);
		p.setFont(f);
		p.setPen(ink);
		p.drawText(rect(), Qt::AlignCenter, sym);
	}

private:
	State state_ = State::Unknown;
	bool init_ = false;
};

QFrame *makeCard(QWidget *parent, const char *name)
{
	auto *f = new QFrame(parent);
	f->setObjectName(QString::fromLatin1(name));
	f->setStyleSheet(QStringLiteral("#%1 { background: palette(alternate-base); border: 1px solid palette(mid);"
					" border-radius: 4px; }")
				 .arg(QString::fromLatin1(name)));
	return f;
}
} // namespace

// One check row. Updates in place; only touches widgets whose content changed.
class PreflightCheckRow : public QWidget {
public:
	PreflightCheckRow(CheckId rowId, QWidget *parent) : QWidget(parent), id(rowId)
	{
		auto *h = new QHBoxLayout(this);
		h->setContentsMargins(0, 2, 0, 2);
		h->setSpacing(8);
		mark = new StatusMark(this);
		h->addWidget(mark, 0, Qt::AlignTop);

		auto *v = new QVBoxLayout();
		v->setSpacing(2);
		auto *head = new QHBoxLayout();
		title = new QLabel(this);
		QFont f = title->font();
		f.setBold(true);
		title->setFont(f);
		title->setWordWrap(true);
		head->addWidget(title, 1);
		if (id == CheckId::MicTest) {
			button = new QPushButton(T("PreFlight.TestMic", "Test mic"), this);
			head->addWidget(button, 0, Qt::AlignTop);
		}
		v->addLayout(head);
		msg = new QLabel(this);
		msg->setWordWrap(true);
		v->addWidget(msg);
		if (id == CheckId::Custom) {
			itemsLayout = new QVBoxLayout();
			itemsLayout->setSpacing(0);
			v->addLayout(itemsLayout);
		}
		h->addLayout(v, 1);
	}

	void apply(const CheckResult &r)
	{
		if (!filled || r.state != shownState) {
			mark->setState(r.state);
			shownState = r.state;
		}
		if (!filled || r.title != shownTitle) {
			title->setText(r.title);
			shownTitle = r.title;
		}
		if (!filled || r.message != shownMessage) {
			msg->setText(r.message);
			msg->setVisible(!r.message.isEmpty());
			setToolTip(r.message);
			shownMessage = r.message;
		}
		filled = true;
	}

	bool itemsBuiltFor(const QStringList &items) const { return itemsBuilt && builtItems == items; }

	void setItems(const QStringList &items, const QVector<bool> &ticks, std::function<void(int, bool)> cb)
	{
		if (!itemsLayout)
			return;
		for (QCheckBox *b : boxes) {
			itemsLayout->removeWidget(b);
			b->deleteLater();
		}
		boxes.clear();
		for (int i = 0; i < items.size(); ++i) {
			QString label = items[i];
			label.replace(QLatin1Char('&'), QStringLiteral("&&"));
			auto *b = new QCheckBox(label, this);
			b->setChecked(i < ticks.size() && ticks[i]);
			QObject::connect(b, &QCheckBox::toggled, b, [cb, i](bool on) { cb(i, on); });
			itemsLayout->addWidget(b);
			boxes.push_back(b);
		}
		builtItems = items;
		itemsBuilt = true;
	}

	void setTicks(const QVector<bool> &ticks)
	{
		for (int i = 0; i < boxes.size(); ++i) {
			const bool on = i < ticks.size() && ticks[i];
			if (boxes[i]->isChecked() != on) {
				const QSignalBlocker block(boxes[i]);
				boxes[i]->setChecked(on);
			}
		}
	}

	CheckId id;
	QPushButton *button = nullptr;

private:
	StatusMark *mark = nullptr;
	QLabel *title = nullptr;
	QLabel *msg = nullptr;
	QVBoxLayout *itemsLayout = nullptr;
	QVector<QCheckBox *> boxes;
	QStringList builtItems;
	bool itemsBuilt = false;
	bool filled = false;
	State shownState = State::Unknown;
	QString shownTitle;
	QString shownMessage;
};

// The folder holding every plugin's config folder (.../plugin_config), for the shared Stream Kit preference.
static QString kitConfigRoot()
{
	QString root;
	if (char *ours = obs_module_config_path("")) {
		QDir dir(QString::fromUtf8(ours)); // .../plugin_config/rws-preflight/
		bfree(ours);
		if (dir.cdUp())
			root = dir.absolutePath();
	}
	return root;
}

PreflightDock::PreflightDock(QWidget *parent) : QWidget(parent)
{
	controller = new PreflightController(this);
	// Shared with Multistream (first run migrates each plugin's old value; either explicit false wins).
	checkUpdates = updatecheck::loadSharedCheckUpdates(kitConfigRoot());
	live = obs_frontend_streaming_active();

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(6, 6, 6, 6);
	root->setSpacing(6);

	// Non-modal "you went live with problems" banner.
	banner = makeCard(this, "pfBanner");
	{
		auto *bl = new QHBoxLayout(banner);
		bl->setContentsMargins(8, 6, 4, 6);
		bannerLabel = new QLabel(banner);
		bannerLabel->setWordWrap(true);
		bl->addWidget(bannerLabel, 1);
		auto *close = new QToolButton(banner);
		close->setText(QString(QChar(0x00D7)));
		close->setAutoRaise(true);
		close->setToolTip(T("PreFlight.Dismiss", "Dismiss"));
		connect(close, &QToolButton::clicked, banner, &QWidget::hide);
		bl->addWidget(close, 0, Qt::AlignTop);
	}
	banner->setVisible(false);
	root->addWidget(banner);

	updateNote = new QLabel(this);
	updateNote->setWordWrap(true);
	updateNote->setTextFormat(Qt::RichText);
	updateNote->setVisible(false);
	connect(updateNote, &QLabel::linkActivated, this, [this](const QString &link) {
		if (link == QLatin1String("download"))
			QDesktopServices::openUrl(QUrl(pendingUpdate.url));
		updateNote->setVisible(false); // Download or Not now: gone until next OBS start
	});
	root->addWidget(updateNote);

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	auto *content = new QWidget(scroll);
	auto *cl = new QVBoxLayout(content);
	cl->setContentsMargins(0, 0, 0, 0);
	cl->setSpacing(4);

	setupCard = makeCard(content, "pfSetup");
	{
		auto *sl = new QVBoxLayout(setupCard);
		sl->setContentsMargins(10, 10, 10, 10);
		auto *text = new QLabel(T("PreFlight.SetupIntro",
					  "Pre-Flight checks your setup before you go live. Tell it how you stream to "
					  "get started."),
					setupCard);
		text->setWordWrap(true);
		sl->addWidget(text);
		auto *go = new QPushButton(T("PreFlight.SetupButton", "Set up Pre-Flight"), setupCard);
		connect(go, &QPushButton::clicked, this, [this] { openSettings(); });
		sl->addWidget(go, 0, Qt::AlignLeft);
	}
	cl->addWidget(setupCard);

	rowsHost = new QWidget(content);
	rowsLayout = new QVBoxLayout(rowsHost);
	rowsLayout->setContentsMargins(0, 0, 0, 0);
	rowsLayout->setSpacing(4);
	cl->addWidget(rowsHost);
	cl->addStretch(1);
	scroll->setWidget(content);
	root->addWidget(scroll, 1);

	auto *bar = new QHBoxLayout();
	goBtn = new QPushButton(this);
	{
		QFont f = goBtn->font();
		f.setBold(true);
		goBtn->setFont(f);
	}
	goBtn->setMinimumHeight(30);
	connect(goBtn, &QPushButton::clicked, this, [this] { goLive(); });
	bar->addWidget(goBtn, 1);
	// Text, not a gear glyph: U+2699 has no glyph in OBS's default font and rendered blank.
	gearBtn = new QPushButton(T("PreFlight.SettingsButton", "Settings"), this);
	gearBtn->setMinimumHeight(30);
	gearBtn->setToolTip(T("PreFlight.SettingsTooltip", "Pre-Flight settings"));
	connect(gearBtn, &QPushButton::clicked, this, [this] { openSettings(); });
	bar->addWidget(gearBtn);
	root->addLayout(bar);

	connect(controller, &PreflightController::resultsChanged, this, &PreflightDock::applyResults);
	connect(controller, &PreflightController::micTestRunningChanged, this, [this](bool r) {
		micRunning = r;
		updateMicButton();
	});
	connect(&updates, &UpdateChecker::updateAvailable, this, &PreflightDock::showUpdate);

	applyResults(controller->results());
	updateLive();

	// The Stream Kit update look is scheduled from OBS_FRONTEND_EVENT_FINISHED_LOADING
	// (handleFrontendEvent), once every module is loaded, so "is Multistream loaded?" has a final answer.
	installTestHooks();
}

PreflightDock::~PreflightDock()
{
	shutdown();
}

void PreflightDock::shutdown()
{
	closing = true;
	updates.stop();
	if (controller)
		controller->shutdown(); // stops the timer, the mic test and the Multistream worker
}

QWidget *PreflightDock::mainWindow() const
{
	return static_cast<QWidget *>(obs_frontend_get_main_window());
}

void PreflightDock::handleFrontendEvent(obs_frontend_event event)
{
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		shutdown();
		return;
	}
	if (!controller || closing)
		return;
	controller->handleFrontendEvent(event);
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
		scheduleUpdateCheck();
	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		live = true;
		if (controller->startedWithReds() && !startedFromDock) {
			bannerLabel->setText(T("PreFlight.WentLive", "You went live with problems: %1.")
						     .arg(controller->startedWithRedsList().join(QStringLiteral(", "))));
			banner->setVisible(true);
		}
		startedFromDock = false;
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		live = true;
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		live = false;
		startedFromDock = false;
		banner->setVisible(false);
		ticks.fill(false); // the controller resets its ticks at the same moment
		if (PreflightCheckRow *r = rows.value(static_cast<int>(CheckId::Custom), nullptr))
			r->setTicks(ticks);
		break;
	default:
		break;
	}
	updateLive();
}

void PreflightDock::applyResults(const QVector<CheckResult> &results)
{
	if (closing)
		return;
	const bool setup = controller->settings().setupDone;
	setupCard->setVisible(!setup);
	rowsHost->setVisible(setup);

	QVector<CheckResult> shown;
	if (setup) {
		for (const CheckResult &r : results)
			if (r.state != State::Hidden)
				shown.push_back(r);
	}
	QVector<int> newOrder;
	for (const CheckResult &r : shown)
		newOrder.push_back(static_cast<int>(r.id));

	auto rowFor = [this](CheckId id) {
		PreflightCheckRow *&row = rows[static_cast<int>(id)];
		if (!row) {
			row = new PreflightCheckRow(id, rowsHost);
			if (row->button)
				connect(row->button, &QPushButton::clicked, this, [this] { controller->startMicTest(); });
		}
		return row;
	};

	if (newOrder != order) {
		while (rowsLayout->count() > 0) {
			QLayoutItem *it = rowsLayout->takeAt(0);
			if (QWidget *w = it->widget())
				w->hide();
			delete it;
		}
		order = newOrder;
		for (int id : order) {
			PreflightCheckRow *row = rowFor(static_cast<CheckId>(id));
			rowsLayout->addWidget(row);
			row->show();
		}
	}
	for (const CheckResult &r : shown)
		rowFor(r.id)->apply(r);

	syncCustomItems();
	updateMicButton();
}

void PreflightDock::syncCustomItems()
{
	PreflightCheckRow *row = rows.value(static_cast<int>(CheckId::Custom), nullptr);
	if (!row)
		return;
	const QStringList items = controller->settings().customItems;
	if (row->itemsBuiltFor(items))
		return;
	ticks = QVector<bool>(items.size(), false); // a changed list starts its ticks over, like the controller
	row->setItems(items, ticks, [this](int index, bool on) {
		if (index >= 0 && index < ticks.size())
			ticks[index] = on;
		controller->setItemTicked(index, on);
	});
}

void PreflightDock::updateMicButton()
{
	PreflightCheckRow *row = rows.value(static_cast<int>(CheckId::MicTest), nullptr);
	if (!row || !row->button)
		return;
	row->button->setText(micRunning ? T("PreFlight.MicListening", "Listening... say something")
					: T("PreFlight.TestMic", "Test mic"));
	row->button->setEnabled(!micRunning);
}

void PreflightDock::updateLive()
{
	const bool isLive = live || obs_frontend_streaming_active();
	goBtn->setEnabled(!isLive);
	QString label = isLive ? T("PreFlight.Live", "Live") : T("PreFlight.GoLive", "Check & Go Live");
	label.replace(QLatin1Char('&'), QStringLiteral("&&")); // not a mnemonic
	goBtn->setText(label);
}

void PreflightDock::openSettings()
{
	if (closing)
		return;
	preflight::Settings current = controller->settings();
	current.updateChecks = updatecheck::loadSharedCheckUpdates(kitConfigRoot()); // Multistream may have changed it
	SettingsDialog dlg(current, mainWindow());
	if (dlg.exec() != QDialog::Accepted || closing)
		return;
	const preflight::Settings s = dlg.result(); // setupDone = true
	controller->setSettings(s);
	checkUpdates = s.updateChecks;
	updatecheck::saveSharedCheckUpdates(kitConfigRoot(), checkUpdates);
	if (!checkUpdates)
		updateNote->setVisible(false);
	applyResults(controller->results());
}

void PreflightDock::goLive()
{
	if (closing || live || obs_frontend_streaming_active())
		return;
	const preflight::GoLiveDecision d = controller->goLiveDecision();
	if (d.allowedWithoutAsking) {
		startedFromDock = true;
		QTimer::singleShot(5000, this, [this] { startedFromDock = false; });
		controller->startStreaming();
		return;
	}
	const QString dot = QString(QChar(0x2022)) + QLatin1Char(' ');
	QString body = T("PreFlight.GoLiveFailedIntro", "These checks failed:");
	for (const QString &t : d.reds)
		body += QStringLiteral("\n") + dot + t;
	if (!d.cautions.isEmpty()) {
		body += QStringLiteral("\n\n") + T("PreFlight.GoLiveAlso", "Also worth a look:");
		for (const QString &t : d.cautions)
			body += QStringLiteral("\n") + dot + t;
	}
	QMessageBox box(QMessageBox::Warning, T("PreFlight.GoLiveTitle", "Some checks failed"), body,
			QMessageBox::NoButton, mainWindow());
	box.setTextFormat(Qt::PlainText);
	QPushButton *anyway = box.addButton(T("PreFlight.GoAnyway", "Go live anyway"), QMessageBox::AcceptRole);
	QPushButton *cancel = box.addButton(T("PreFlight.Cancel", "Cancel"), QMessageBox::RejectRole);
	box.setDefaultButton(cancel);
	box.setEscapeButton(cancel);
	box.exec();
	if (closing || box.clickedButton() != anyway)
		return;
	startedFromDock = true;
	QTimer::singleShot(5000, this, [this] { startedFromDock = false; });
	controller->startStreaming();
}

void PreflightDock::installTestHooks()
{
	const QString shotPath = qEnvironmentVariable("RWS_PREFLIGHT_TEST_SHOT");
	if (!shotPath.isEmpty()) {
		QTimer::singleShot(40000, this, [this, shotPath] {
			if (closing)
				return;
			for (QWidget *w = parentWidget(); w; w = w->parentWidget()) {
				if (auto *d = qobject_cast<QDockWidget *>(w)) {
					if (!d->isVisible())
						d->show();
					// A docked widget cannot be resized (the main window's layout owns its size), so
					// float it first; then the whole dock fits at a phone-ish 360x720 for the screenshot.
					d->setFloating(true);
					d->resize(360, 720);
					d->raise();
					break;
				}
			}
			QTimer::singleShot(1000, this, [this, shotPath] {
				if (closing)
					return;
				if (width() < 100 || height() < 100)
					resize(sizeHint().expandedTo(QSize(320, 480)));
				const QPixmap px = grab();
				if (!px.isNull() && px.save(shotPath, "PNG"))
					obs_log(LOG_INFO, "ui-shot saved (%dx%d)", px.width(), px.height());
				else
					obs_log(LOG_WARNING, "ui-shot failed");
			});
		});
	}
	const QString setPath = qEnvironmentVariable("RWS_PREFLIGHT_TEST_SETTINGS_SHOT");
	if (!setPath.isEmpty()) {
		QTimer::singleShot(5000, this, [this, setPath] {
			if (closing)
				return;
			auto *dlg = new SettingsDialog(controller->settings(), mainWindow());
			dlg->show(); // non-blocking
			QTimer::singleShot(1500, this, [this, dlg, setPath] {
				const QPixmap px = dlg->grab();
				if (!px.isNull() && px.save(setPath, "PNG"))
					obs_log(LOG_INFO, "settings-shot saved");
				else
					obs_log(LOG_WARNING, "settings-shot failed");
				dlg->close();
				dlg->deleteLater();
			});
		});
	}
}

void PreflightDock::scheduleUpdateCheck()
{
	// Exactly one Stream Kit notice: Multistream owns it whenever it is loaded.
	// Called once OBS has finished loading, so every module is known. Never in the test copy.
	if (!checkUpdates || testHooksEnabled() || updateCheckScheduled)
		return;
	updateCheckScheduled = true;
	// Scheduled only if updates are on now (turning them on mid-session may wait until the next OBS
	// start); when the timer fires the shared file is re-read, so turning them off elsewhere wins.
	QTimer::singleShot(15000, this, [this] {
		if (closing)
			return;
		checkUpdates = updatecheck::loadSharedCheckUpdates(kitConfigRoot());
		if (!checkUpdates)
			return;
		const bool msLoaded = obs_get_module("rws-multistream") != nullptr;
		if (!updatecheck::shouldShowKitNotice(false, msLoaded))
			return;
		updates.start(QString::fromLatin1(updatecheck::kKitVersion));
	});
}

void PreflightDock::showUpdate(const updatecheck::Manifest &m)
{
	if (closing || !updateNote || !checkUpdates)
		return;
	pendingUpdate = m;
	obs_log(LOG_INFO, "Stream Kit update available: %s (installed kit %s, plugin %s)", m.latest.toUtf8().constData(), updatecheck::kKitVersion, PLUGIN_VERSION);
	const QString obsVersion = QString::fromUtf8(obs_get_version_string());
	QString note;
	if (!m.minObs.isEmpty() && updatecheck::compareVersions(obsVersion, m.minObs) < 0) {
		// Don't invite an upgrade that would break this OBS.
		note = QStringLiteral("Red Warden Stream Kit %1 is out. It needs OBS %2 or newer (you have %3), so "
				      "update OBS first. <a href=\"download\">Details</a> &middot; "
				      "<a href=\"dismiss\">Not now</a>")
			       .arg(m.latest.toHtmlEscaped(), m.minObs.toHtmlEscaped(), obsVersion.toHtmlEscaped());
	} else {
		note = QStringLiteral("Red Warden Stream Kit %1 is available. "
				      "<a href=\"download\">Download</a> &middot; <a href=\"dismiss\">Not now</a>")
			       .arg(m.latest.toHtmlEscaped());
		if (!m.notes.isEmpty())
			note += QStringLiteral("<br>") + m.notes.toHtmlEscaped();
	}
	updateNote->setText(note);
	updateNote->setVisible(true);
}
