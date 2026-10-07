/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// What happened on each destination during one stream: how long it was live,
// how often it dropped and for how long, what it sent. Shown in the dock when
// the stream ends and kept as a text file per stream.
//
// Pure Qt Core: fed with phases, traffic and events by the dock, unit-tested
// in tests/session-report-test.cpp without OBS. Times are milliseconds on any
// monotonic clock the caller picks.

#include <QString>
#include <QStringList>
#include <cstdint>
#include <map>
#include <vector>

class SessionReport {
public:
	enum class Phase { Off, Connecting, Live, Reconnecting, Down };

	struct Event {
		int64_t atMs = 0; // since the stream started
		QString text;
	};

	struct Dest {
		QString id, name, platform;
		bool everOn = false;       // switched on at some point during the stream
		bool everLive = false;
		bool inOutage = false;     // dropped and not back yet
		Phase phase = Phase::Off;
		int64_t phaseSinceMs = 0;
		int64_t onMs = 0;          // switched on (any phase but Off)
		int64_t liveMs = 0;
		int64_t outageMs = 0;      // from a drop until live again (or switched off)
		int64_t connectMs = 0;     // connecting after being switched on or the stream starting
		int drops = 0;             // live -> anything but off
		int recoveries = 0;        // back to live after a drop
		int stalls = 0;
		double bits = 0.0;
		int dropped = 0, frames = 0;               // banked from earlier connections
		int lastDropped = 0, lastFrames = 0;       // current connection
		QString lastError;
		std::vector<Event> events;

		int64_t sentBytes() const { return (int64_t)(bits / 8.0); }
		double avgKbps() const { return liveMs > 0 ? bits / (double)liveMs : 0.0; } // bits per ms == kbps
		int droppedFrames() const { return dropped + lastDropped; }
		int totalFrames() const { return frames + lastFrames; }
		// Share of the time it was switched on that it was actually live,
		// leaving out connecting after being switched on (nobody watching yet).
		double uptime() const
		{
			const int64_t base = onMs - connectMs;
			return base > 0 ? (double)liveMs / (double)base : (everLive ? 1.0 : 0.0);
		}
	};

	void begin(int64_t nowMs, const QString &startedAt);
	// Lists a destination (switched off for now) so the report keeps the
	// dock's order, whatever connects first.
	void declare(const QString &id, const QString &name, const QString &platform);
	bool active() const { return running; }
	bool hasResult() const { return finished; }

	// Call on every state change and once a second. Idempotent when nothing
	// changed; accumulates the time spent in the previous phase.
	void update(const QString &id, const QString &name, const QString &platform, Phase phase, int64_t nowMs,
		    const QString &error = QString());
	// Called once a second for a destination with a connection: kbps over
	// dtMs, and the output's own frame counters (they restart at 0 for every
	// new connection).
	void traffic(const QString &id, double kbps, int64_t dtMs, int droppedFrames, int totalFrames);
	void stall(const QString &id, int64_t nowMs);
	void note(const QString &id, int64_t nowMs, const QString &text);
	void end(int64_t nowMs);

	int64_t durationMs() const { return endMs - startMs; }
	QString startedAt() const { return started; }
	std::vector<const Dest *> destinations() const; // in the order first seen, only ones switched on
	QString headline() const;                         // one line for the dock
	bool hadTrouble() const;                          // a drop, or a destination that never went live
	QString text() const;                             // the whole report, plain text

	static QString formatDuration(int64_t ms);
	static QString formatBytes(int64_t bytes);

private:
	void account(Dest &d, int64_t nowMs);
	Dest &dest(const QString &id);
	void addEvent(Dest &d, int64_t nowMs, const QString &text);

	bool running = false, finished = false;
	int64_t startMs = 0, endMs = 0;
	QString started;
	std::map<QString, Dest> dests;
	std::vector<QString> order;
};
