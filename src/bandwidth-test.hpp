/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// Twitch's bandwidth test: Twitch accepts a stream whose key ends in
// "?bandwidthtest=true" and never puts it on air. Sending a real stream that
// way for a few seconds shows whether this connection, to this Twitch server,
// carries the bitrate, without anyone seeing it.
//
// Other platforms have no such mode: connecting to them means going live, so
// this test is Twitch-only.

#include "config.hpp"
#include "output-runner.hpp"

#include <QObject>
#include <QTimer>

class TwitchBandwidthTest : public QObject {
	Q_OBJECT
public:
	struct Result {
		bool ok = false;        // connected and measured
		QString error;          // why not, when !ok
		double avgKbps = 0.0;   // measured, video + audio
		int targetKbps = 0;     // what it should send, video + audio
		int droppedFrames = 0;
		int totalFrames = 0;
		int seconds = 0;
		bool steady() const;
	};

	static constexpr int kWarmupSeconds = 3;
	static constexpr int kMeasureSeconds = 15;
	static constexpr int kConnectTimeoutSeconds = 20;

	explicit TwitchBandwidthTest(QObject *parent = nullptr);
	~TwitchBandwidthTest() override;

	// d: the destination as configured. A destination that shares OBS's
	// encoder is tested with a temporary encoder at OBS's stream settings, so
	// the test works without streaming. Returns false if it could not start.
	bool start(const Destination &d, const QString &streamKey, QString *error);
	void cancel();
	bool running() const { return active; }

	static QString withBandwidthTest(const QString &key);

signals:
	void progress(int measuredSeconds, double kbps); // measuredSeconds < 0 while connecting
	void finished(const TwitchBandwidthTest::Result &result);

private:
	void onTick();
	void onRunnerChanged();
	void finish(bool ok, const QString &error);

	OutputRunner runner;
	QTimer tick;
	bool active = false;
	int waited = 0;    // seconds connecting
	int liveFor = 0;   // seconds live
	double kbpsSum = 0.0;
	int kbpsSamples = 0;
	Result res;
};
