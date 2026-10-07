/*
Red Warden Pre-Flight
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "settings-dialog.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {
QString T(const char *key, const char *fallback)
{
	const char *t = obs_module_text(key);
	return (t && qstrcmp(t, key) != 0) ? QString::fromUtf8(t) : QString::fromUtf8(fallback);
}

struct Named {
	QString name;
	QString uuid;
};

bool enumAudio(void *param, obs_source_t *src)
{
	auto *out = static_cast<QVector<Named> *>(param);
	if (!(obs_source_get_output_flags(src) & OBS_SOURCE_AUDIO))
		return true;
	const char *n = obs_source_get_name(src);
	const char *u = obs_source_get_uuid(src);
	if (n && u)
		out->push_back({QString::fromUtf8(n), QString::fromUtf8(u)});
	return true;
}

QVector<Named> audioSources()
{
	QVector<Named> v;
	obs_enum_sources(enumAudio, &v);
	std::sort(v.begin(), v.end(), [](const Named &a, const Named &b) {
		return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
	});
	return v;
}

QVector<Named> sceneList()
{
	QVector<Named> v;
	struct obs_frontend_source_list list = {};
	obs_frontend_get_scenes(&list);
	for (size_t i = 0; i < list.sources.num; ++i) {
		obs_source_t *s = list.sources.array[i];
		const char *n = obs_source_get_name(s);
		const char *u = obs_source_get_uuid(s);
		if (n && u)
			v.push_back({QString::fromUtf8(n), QString::fromUtf8(u)});
	}
	obs_frontend_source_list_free(&list);
	return v;
}

// Item 0 is the "nothing chosen" entry (empty UUID). If a saved UUID is no longer in the list,
// nothing is preselected and item 0 reads "(choose again)".
void fillCombo(QComboBox *combo, const QVector<Named> &items, const QString &saved, const QString &firstLabel)
{
	combo->clear();
	combo->addItem(firstLabel, QString());
	int select = 0;
	for (const Named &n : items) {
		combo->addItem(n.name, n.uuid);
		if (!saved.isEmpty() && n.uuid == saved)
			select = combo->count() - 1;
	}
	if (!saved.isEmpty() && select == 0)
		combo->setItemText(0, T("PreFlight.Settings.ChooseAgain", "(choose again)"));
	combo->setCurrentIndex(select);
}
} // namespace

SettingsDialog::SettingsDialog(const preflight::Settings &current, QWidget *parent) : QDialog(parent), base_(current)
{
	setWindowTitle(T("PreFlight.Settings.Title", "Pre-Flight settings"));
	setModal(true);

	auto *root = new QVBoxLayout(this);

	auto *howBox = new QGroupBox(T("PreFlight.Settings.HowStream", "How do you stream?"), this);
	auto *howLay = new QVBoxLayout(howBox);
	mainOnly_ = new QRadioButton(T("PreFlight.Settings.StreamMain", "OBS's normal Stream button"), howBox);
	multiOnly_ = new QRadioButton(T("PreFlight.Settings.StreamMulti", "Red Warden Multistream"), howBox);
	both_ = new QRadioButton(T("PreFlight.Settings.StreamBoth", "Both"), howBox);
	howLay->addWidget(mainOnly_);
	howLay->addWidget(multiOnly_);
	howLay->addWidget(both_);
	if (current.useMainStream && current.useMultistream)
		both_->setChecked(true);
	else if (current.useMultistream)
		multiOnly_->setChecked(true);
	else
		mainOnly_->setChecked(true);
	root->addWidget(howBox);

	auto *form = new QFormLayout();
	mic_ = new QComboBox(this);
	desktop_ = new QComboBox(this);
	scene_ = new QComboBox(this);
	for (QComboBox *c : {mic_, desktop_, scene_}) {
		c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		c->setMinimumContentsLength(20);
	}
	const QVector<Named> audio = audioSources();
	fillCombo(mic_, audio, current.micUuid, T("PreFlight.Settings.Choose", "(choose)"));
	fillCombo(desktop_, audio, current.desktopUuid, T("PreFlight.Settings.None", "None"));
	fillCombo(scene_, sceneList(), current.startSceneUuid, T("PreFlight.Settings.Choose", "(choose)"));
	form->addRow(T("PreFlight.Settings.Mic", "Microphone"), mic_);
	form->addRow(T("PreFlight.Settings.Desktop", "Desktop / game audio"), desktop_);
	form->addRow(T("PreFlight.Settings.Scene", "Starting scene"), scene_);
	root->addLayout(form);

	record_ = new QCheckBox(T("PreFlight.Settings.Record", "I record my streams"), this);
	record_->setChecked(current.recordIntent);
	root->addWidget(record_);

	auto *checksBox = new QGroupBox(T("PreFlight.Settings.Checks", "Checks to run"), this);
	auto *checksLay = new QVBoxLayout(checksBox);
	struct CheckDef {
		const char *key;
		const char *fallback;
		bool value;
	};
	const CheckDef defs[8] = {
		{"PreFlight.Settings.CheckMic", "Microphone (muted and mic test)", current.checkMic},
		{"PreFlight.Settings.CheckDesktop", "Desktop / game audio", current.checkDesktop},
		{"PreFlight.Settings.CheckScene", "Starting scene is live", current.checkScene},
		{"PreFlight.Settings.CheckMain", "OBS stream is set up", current.checkMainStream},
		{"PreFlight.Settings.CheckMulti", "Multistream destinations", current.checkMultistream},
		{"PreFlight.Settings.CheckRecording", "Recording", current.checkRecording},
		{"PreFlight.Settings.CheckPerf", "OBS keeping up", current.checkPerformance},
		{"PreFlight.Settings.CheckCustom", "Your checklist", current.checkCustom},
	};
	for (int i = 0; i < 8; ++i) {
		checks_[i] = new QCheckBox(T(defs[i].key, defs[i].fallback), checksBox);
		checks_[i]->setChecked(defs[i].value);
		checksLay->addWidget(checks_[i]);
	}
	root->addWidget(checksBox);

	auto *listBox = new QGroupBox(T("PreFlight.Settings.Checklist", "Your checklist"), this);
	auto *listLay = new QVBoxLayout(listBox);
	items_ = new QListWidget(listBox);
	items_->setMinimumHeight(80);
	for (const QString &s : current.customItems) {
		auto *it = new QListWidgetItem(s, items_);
		it->setFlags(it->flags() | Qt::ItemIsEditable);
	}
	listLay->addWidget(items_);
	auto *btnRow = new QHBoxLayout();
	auto *add = new QPushButton(T("PreFlight.Settings.Add", "Add"), listBox);
	auto *remove = new QPushButton(T("PreFlight.Settings.Remove", "Remove"), listBox);
	btnRow->addWidget(add);
	btnRow->addWidget(remove);
	btnRow->addStretch(1);
	listLay->addLayout(btnRow);
	connect(add, &QPushButton::clicked, this, [this] {
		auto *it = new QListWidgetItem(T("PreFlight.Settings.NewItem", "New item"), items_);
		it->setFlags(it->flags() | Qt::ItemIsEditable);
		items_->setCurrentItem(it);
		items_->editItem(it);
	});
	connect(remove, &QPushButton::clicked, this, [this] {
		const int row = items_->currentRow();
		if (row >= 0)
			delete items_->takeItem(row);
	});
	root->addWidget(listBox);

	updates_ = new QCheckBox(T("PreFlight.UpdatesCheckbox", "Tell me when an update is available"), this);
	updates_->setChecked(current.updateChecks);
	root->addWidget(updates_);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	root->addWidget(buttons);
}

preflight::Settings SettingsDialog::result() const
{
	preflight::Settings s = base_;
	s.setupDone = true;
	s.useMainStream = mainOnly_->isChecked() || both_->isChecked();
	s.useMultistream = multiOnly_->isChecked() || both_->isChecked();
	s.micUuid = mic_->currentData().toString();
	s.desktopUuid = desktop_->currentData().toString();
	s.startSceneUuid = scene_->currentData().toString();
	s.recordIntent = record_->isChecked();
	s.checkMic = checks_[0]->isChecked();
	s.checkDesktop = checks_[1]->isChecked();
	s.checkScene = checks_[2]->isChecked();
	s.checkMainStream = checks_[3]->isChecked();
	s.checkMultistream = checks_[4]->isChecked();
	s.checkRecording = checks_[5]->isChecked();
	s.checkPerformance = checks_[6]->isChecked();
	s.checkCustom = checks_[7]->isChecked();
	s.customItems.clear();
	for (int i = 0; i < items_->count(); ++i) {
		const QString t = items_->item(i)->text().trimmed();
		if (!t.isEmpty())
			s.customItems << t;
	}
	s.updateChecks = updates_->isChecked();
	return s;
}
