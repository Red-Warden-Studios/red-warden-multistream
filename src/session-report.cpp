/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "session-report.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr size_t kMaxEvents = 60; // per destination; a flapping link should not make a novel
// Destinations that start with the stream are not "switched on" by anyone.
constexpr int64_t kStartGraceMs = 3000;

QString times(int n)
{
	return n == 1 ? QStringLiteral("once") : n == 2 ? QStringLiteral("twice") : QStringLiteral("%1 times").arg(n);
}
}

void SessionReport::begin(int64_t nowMs, const QString &startedAt)
{
	dests.clear();
	order.clear();
	running = true;
	finished = false;
	startMs = endMs = nowMs;
	started = startedAt;
}

SessionReport::Dest &SessionReport::dest(const QString &id)
{
	auto it = dests.find(id);
	if (it == dests.end()) {
		order.push_back(id);
		it = dests.emplace(id, Dest{}).first;
		it->second.id = id;
		it->second.phaseSinceMs = startMs;
	}
	return it->second;
}

void SessionReport::declare(const QString &id, const QString &name, const QString &platform)
{
	if (!running)
		return;
	Dest &d = dest(id);
	d.name = name;
	d.platform = platform;
}

void SessionReport::addEvent(Dest &d, int64_t nowMs, const QString &text)
{
	if (d.events.size() < kMaxEvents)
		d.events.push_back({nowMs - startMs, text});
	else if (d.events.size() == kMaxEvents)
		d.events.push_back({nowMs - startMs, QStringLiteral("(more events not listed)")});
}

void SessionReport::account(Dest &d, int64_t nowMs)
{
	const int64_t dt = std::max<int64_t>(0, nowMs - d.phaseSinceMs);
	d.phaseSinceMs = nowMs;
	if (d.phase == Phase::Off)
		return;
	d.onMs += dt;
	if (d.phase == Phase::Live)
		d.liveMs += dt;
	else if (d.inOutage)
		d.outageMs += dt;
	else
		d.connectMs += dt; // connecting after being switched on: not an outage
}

void SessionReport::update(const QString &id, const QString &name, const QString &platform, Phase phase,
			   int64_t nowMs, const QString &error)
{
	if (!running)
		return;
	Dest &d = dest(id);
	d.name = name;
	d.platform = platform;
	account(d, nowMs);
	if (phase == d.phase)
		return;

	const Phase was = d.phase;
	d.phase = phase;
	if (phase == Phase::Live || phase == Phase::Off)
		d.inOutage = false;
	else if (was == Phase::Live)
		d.inOutage = true;
	if (phase != Phase::Off && !d.everOn)
		d.everOn = true;

	if (phase == Phase::Live) {
		if (!d.everLive) {
			d.everLive = true;
			addEvent(d, nowMs, QStringLiteral("Live"));
		} else if (was == Phase::Off) {
			addEvent(d, nowMs, QStringLiteral("Live again"));
		} else {
			d.recoveries++;
			addEvent(d, nowMs, QStringLiteral("Back live"));
		}
	} else if (phase == Phase::Off) {
		addEvent(d, nowMs, QStringLiteral("Switched off"));
	} else if (was == Phase::Off && nowMs - startMs > kStartGraceMs) {
		addEvent(d, nowMs, QStringLiteral("Switched on"));
	} else if (was == Phase::Live) {
		d.drops++;
		addEvent(d, nowMs,
			 phase != Phase::Down
				 ? QStringLiteral("Connection dropped; reconnecting")
				 : (error.isEmpty() ? QStringLiteral("Connection dropped") : QStringLiteral("Dropped: ") + error));
	} else if (phase == Phase::Down && !error.isEmpty() && error != d.lastError) {
		addEvent(d, nowMs, error);
	}
	if (!error.isEmpty())
		d.lastError = error;
}

void SessionReport::traffic(const QString &id, double kbps, int64_t dtMs, int droppedFrames, int totalFrames)
{
	if (!running)
		return;
	Dest &d = dest(id);
	if (kbps > 0 && dtMs > 0)
		d.bits += kbps * (double)dtMs; // kbps == bits per ms
	// A new connection starts its counters at zero: bank the old ones.
	if (totalFrames < d.lastFrames) {
		d.dropped += d.lastDropped;
		d.frames += d.lastFrames;
	}
	d.lastDropped = droppedFrames;
	d.lastFrames = totalFrames;
}

void SessionReport::stall(const QString &id, int64_t nowMs)
{
	if (!running)
		return;
	Dest &d = dest(id);
	d.stalls++;
	addEvent(d, nowMs, QStringLiteral("Stopped sending data; restarted the connection"));
}

void SessionReport::note(const QString &id, int64_t nowMs, const QString &text)
{
	if (!running)
		return;
	addEvent(dest(id), nowMs, text);
}

void SessionReport::end(int64_t nowMs)
{
	if (!running)
		return;
	for (auto &[id, d] : dests) {
		account(d, nowMs);
		d.phase = Phase::Off;
		d.dropped += d.lastDropped;
		d.frames += d.lastFrames;
		d.lastDropped = d.lastFrames = 0;
	}
	endMs = nowMs;
	running = false;
	finished = true;
}

std::vector<const SessionReport::Dest *> SessionReport::destinations() const
{
	std::vector<const Dest *> out;
	for (const auto &id : order) {
		const auto it = dests.find(id);
		if (it != dests.end() && it->second.everOn)
			out.push_back(&it->second);
	}
	return out;
}

QString SessionReport::formatDuration(int64_t ms)
{
	const int64_t s = (ms + 500) / 1000;
	if (s < 60)
		return QStringLiteral("%1 s").arg(s);
	const int64_t m = s / 60;
	if (m < 60)
		return s % 60 ? QStringLiteral("%1 min %2 s").arg(m).arg(s % 60) : QStringLiteral("%1 min").arg(m);
	return QStringLiteral("%1 h %2 min").arg(m / 60).arg(m % 60);
}

QString SessionReport::formatBytes(int64_t bytes)
{
	const double mb = (double)bytes / 1e6;
	if (mb < 1000.0)
		return QStringLiteral("%1 MB").arg(mb < 10.0 ? QString::number(mb, 'f', 1) : QString::number(std::round(mb)));
	return QStringLiteral("%1 GB").arg(QString::number(mb / 1000.0, 'f', 1));
}

bool SessionReport::hadTrouble() const
{
	for (const Dest *d : destinations())
		if (!d->everLive || d->drops > 0)
			return true;
	return false;
}

QString SessionReport::headline() const
{
	const auto list = destinations();
	const QString len = formatDuration(durationMs());
	if (list.empty())
		return QStringLiteral("Last stream: %1. No destinations were on.").arg(len);

	QStringList trouble;
	for (const Dest *d : list) {
		if (!d->everLive)
			trouble << QStringLiteral("%1 never went live").arg(d->name);
		else if (d->drops == 1)
			trouble << QStringLiteral("%1 dropped once (offline %2)").arg(d->name, formatDuration(d->outageMs));
		else if (d->drops > 1)
			trouble << QStringLiteral("%1 dropped %2 times (offline %3)")
					   .arg(d->name)
					   .arg(d->drops)
					   .arg(formatDuration(d->outageMs));
	}
	if (trouble.isEmpty())
		return list.size() == 1 ? QStringLiteral("Last stream: %1. %2 stayed live the whole time.").arg(len, list.front()->name)
					: QStringLiteral("Last stream: %1. All %2 destinations stayed live the whole time.")
						  .arg(len)
						  .arg(list.size());
	return QStringLiteral("Last stream: %1. %2.").arg(len, trouble.join(QStringLiteral("; ")));
}

QString SessionReport::text() const
{
	QStringList out;
	out << QStringLiteral("Red Warden Multistream: stream report");
	out << QStringLiteral("Started %1, lasted %2").arg(started, formatDuration(durationMs()));
	out << QString();
	const auto list = destinations();
	if (list.empty())
		out << QStringLiteral("No destinations were switched on during this stream.");
	for (const Dest *d : list) {
		out << d->name;
		if (!d->everLive) {
			out << QStringLiteral("  Never went live%1")
				       .arg(d->lastError.isEmpty() ? QString() : QStringLiteral(": ") + d->lastError);
		} else {
			out << QStringLiteral("  Live %1 (%2% of the time it was on)")
				       .arg(formatDuration(d->liveMs))
				       .arg(QString::number(std::floor(d->uptime() * 1000.0) / 10.0, 'f', 1));
			if (d->drops > 0)
				out << QStringLiteral("  Dropped %1, offline %2 in total, came back %3")
					       .arg(times(d->drops), formatDuration(d->outageMs),
						    d->recoveries > 0 ? times(d->recoveries) : QStringLiteral("never"));
			else
				out << QStringLiteral("  No drops");
			if (d->stalls > 0)
				out << QStringLiteral("  Stopped sending data %1 and was restarted").arg(times(d->stalls));
			out << QStringLiteral("  Sent %1, average %2 kbps")
				       .arg(formatBytes(d->sentBytes()))
				       .arg(qRound(d->avgKbps()));
			if (d->totalFrames() > 0 && d->droppedFrames() > 0)
				out << QStringLiteral("  Frames dropped by the network: %1 of %2 (%3%)")
					       .arg(d->droppedFrames())
					       .arg(d->totalFrames())
					       .arg(QString::number(100.0 * d->droppedFrames() / d->totalFrames(), 'f', 2));
		}
		for (const auto &e : d->events)
			out << QStringLiteral("    +%1  %2").arg(formatDuration(e.atMs), e.text);
		out << QString();
	}
	return out.join(QLatin1Char('\n'));
}
