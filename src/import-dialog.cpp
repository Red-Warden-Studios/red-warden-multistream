/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "import-dialog.hpp"
#include "dock-ui.hpp"
#include "platforms.hpp"

#include <QDialogButtonBox>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

ImportDialog::ImportDialog(std::vector<ImportCandidate> candidates, const std::set<QString> &existing, QWidget *parent)
	: QDialog(parent), items(std::move(candidates))
{
	setWindowTitle(QStringLiteral("Import destinations"));
	setWindowIcon(QIcon(QStringLiteral(":/rws-multistream/logo-64.png")));
	setMinimumWidth(480);
	const ui::Theme theme = ui::Theme::from(palette());
	const QString hintCss = QStringLiteral("color:%1; font-size:11px;").arg(ui::Theme::css(theme.secondary));

	auto *root = new QVBoxLayout(this);
	QStringList sources;
	for (const auto &c : items)
		if (!sources.contains(c.source))
			sources << c.source;
	auto *intro = new QLabel(QStringLiteral("Found these destinations in %1 for this OBS profile. Imported "
						"destinations start switched off, so nothing goes live until you "
						"turn them on.")
					 .arg(sources.join(QStringLiteral(" and "))),
				 this);
	intro->setWordWrap(true);
	root->addWidget(intro);

	list = new QListWidget(this);
	list->setSpacing(2);
	std::set<QString> seen = existing; // also catches the same target saved in both plugins
	for (const auto &c : items) {
		const auto &p = platformById(c.platform);
		const QString sk = c.server.trimmed() + QLatin1Char('\n') + c.key;
		const bool dup = seen.count(sk) > 0;
		QString second = QStringLiteral("%1, from %2").arg(p.label, c.source);
		if (!c.shared)
			second += QStringLiteral(", own encoder at %1 kbps").arg(c.videoBitrate);
		const QString host = QUrl(c.server).host();
		if (!host.isEmpty())
			second += QStringLiteral(", %1").arg(host);
		if (dup)
			second = QStringLiteral("Already added. ") + second;
		else if (!c.skipReason.isEmpty())
			second = c.skipReason + QStringLiteral(". ") + second;
		else if (c.key.isEmpty())
			second = QStringLiteral("No stream key saved; add one with Edit after importing. ") + second;

		auto *it = new QListWidgetItem(QStringLiteral("%1\n%2").arg(c.name, second), list);
		const bool importable = !dup && c.skipReason.isEmpty();
		if (importable)
			seen.insert(sk);
		it->setFlags(importable ? (Qt::ItemIsEnabled | Qt::ItemIsUserCheckable) : Qt::ItemFlags());
		it->setCheckState(importable ? Qt::Checked : Qt::Unchecked);
	}
	connect(list, &QListWidget::itemChanged, this, &ImportDialog::updateButton);
	root->addWidget(list, 1);

	auto *keys = new QLabel(QStringLiteral("Stream keys are copied into Windows Credential Manager. The other plugin "
					       "still keeps its own copy in plain text in its settings file, so remove "
					       "destinations there once you no longer use it."),
				this);
	keys->setWordWrap(true);
	keys->setStyleSheet(hintCss);
	root->addWidget(keys);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
	importButton = buttons->addButton(QStringLiteral("Import selected"), QDialogButtonBox::AcceptRole);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	root->addWidget(buttons);
	updateButton();
}

void ImportDialog::updateButton()
{
	int n = 0;
	for (int i = 0; i < list->count(); i++)
		if (list->item(i)->checkState() == Qt::Checked)
			n++;
	importButton->setEnabled(n > 0);
	importButton->setText(n == 1 ? QStringLiteral("Import 1 destination") : QStringLiteral("Import %1 destinations").arg(n));
}

std::vector<ImportCandidate> ImportDialog::selected() const
{
	std::vector<ImportCandidate> out;
	for (int i = 0; i < list->count() && i < (int)items.size(); i++)
		if (list->item(i)->checkState() == Qt::Checked)
			out.push_back(items[(size_t)i]);
	return out;
}
