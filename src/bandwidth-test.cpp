/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "bandwidth-test.hpp"
#include "obs-audio.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>
#include <util/config-file.h>

#include "connection-budget.hpp"

#include <QDateTime>

#include <cstring>

namespace {
// The encoder OBS's own stream uses, so a shared destination is tested with
// the same kind of encoder (Advanced mode); x264 otherwise.
QString obsStreamEncoderId()
{
	config_t *c = obs_frontend_get_profile_config();
	const char *mode = c ? config_get_string(c, "Output", "Mode") : nullptr;
	if (mode && std::strcmp(mode, "Advanced") == 0) {
		const char *id = config_get_string(c, "AdvOut", "Encoder");
		if (id && *id && obs_get_encoder_codec(id))
			return QString::fromUtf8(id);
	}
	return QStringLiteral("obs_x264");
}
} // namespace

bool TwitchBandwidthTest::Result::steady() const
{
	if (!ok || targetKbps <= 0)
		return false;
	const double droppedShare = totalFrames > 0 ? (double)droppedFrames / totalFrames : 0.0;
	return avgKbps >= 0.85 * targetKbps && droppedShare < 0.01;
}

TwitchBandwidthTest::TwitchBandwidthTest(QObject *parent) : QObject(parent)
{
	tick.setInterval(1000);
	connect(&tick, &QTimer::timeout, this, &TwitchBandwidthTest::onTick);
	connect(&runner, &OutputRunner::changed, this, &TwitchBandwidthTest::onRunnerChanged);
	connect(&runner, &OutputRunner::stalled, this,
		[this] { finish(false, QStringLiteral("The connection stopped carrying data")); });
}

TwitchBandwidthTest::~TwitchBandwidthTest()
{
	tick.stop();
	runner.shutdown();
}

QString TwitchBandwidthTest::withBandwidthTest(const QString &key)
{
	const QString k = key.trimmed();
	if (k.contains(QLatin1String("bandwidthtest=")))
		return k;
	return k + (k.contains(QLatin1Char('?')) ? QStringLiteral("&bandwidthtest=true") : QStringLiteral("?bandwidthtest=true"));
}

bool TwitchBandwidthTest::start(const Destination &d, const QString &key, QString *error)
{
	if (active)
		return true;
	const int64_t now = QDateTime::currentMSecsSinceEpoch();
	// Test connections count against the destination's one connection budget,
	// the same one its live connections use, so "Test again" can't hammer
	// Twitch's ingest, alone or on top of live traffic.
	RetryGovernor &budget = connectionBudget(d.id);
	if (const int64_t wait = budget.budgetWaitMs(now); wait > 0) {
		if (error)
			*error = QStringLiteral("That's a lot of connections to Twitch in a short time. "
						"To protect your account, try again in %1 s.")
					 .arg((wait + 999) / 1000);
		return false;
	}
	budget.recordAttempt(now);
	Destination t = d;
	t.id = d.id + QStringLiteral("-bwtest");
	t.name = d.name + QStringLiteral(" (Twitch bandwidth test)");
	if (t.enc.shared) {
		t.enc.shared = false;
		t.enc.encoderId = obsStreamEncoderId();
		t.enc.videoBitrate = obsStreamVideoBitrate();
		t.enc.width = t.enc.height = 0;
		t.enc.audioBitrate = 0; // match OBS
		t.enc.audioTrack = -1;
	}
	const int audio = t.enc.audioBitrate > 0 ? t.enc.audioBitrate : obsStreamAudio().bitrate;
	res = Result();
	res.targetKbps = t.enc.videoBitrate + audio;
	waited = liveFor = kbpsSamples = 0;
	kbpsSum = 0.0;
	if (!runner.start(t, withBandwidthTest(key), error))
		return false;
	active = true; // after start: a failed start is reported by the return value only
	obs_log(LOG_INFO, "[%s] Twitch bandwidth test started (target %d kbps)", d.name.toUtf8().constData(),
		res.targetKbps);
	tick.start();
	emit progress(-1, 0.0);
	return true;
}

void TwitchBandwidthTest::cancel()
{
	if (!active)
		return;
	finish(false, QStringLiteral("Cancelled"));
}

void TwitchBandwidthTest::onRunnerChanged()
{
	if (active && runner.state() == OutputRunner::State::Error)
		finish(false, runner.detail());
}

void TwitchBandwidthTest::onTick()
{
	if (!active)
		return;
	runner.poll();
	if (runner.state() != OutputRunner::State::Live) {
		if (runner.state() == OutputRunner::State::Reconnecting) {
			finish(false, QStringLiteral("The connection dropped during the test"));
			return;
		}
		if (++waited >= kConnectTimeoutSeconds)
			finish(false, QStringLiteral("Could not connect to Twitch within %1 s").arg(kConnectTimeoutSeconds));
		else
			emit progress(-1, 0.0);
		return;
	}
	liveFor++;
	const double kbps = runner.kbps();
	if (liveFor > kWarmupSeconds) { // let the encoder and the connection settle
		kbpsSum += kbps;
		kbpsSamples++;
	}
	emit progress(qMax(0, liveFor - kWarmupSeconds), kbps);
	res.droppedFrames = runner.droppedFrames();
	res.totalFrames = runner.totalFrames();
	if (kbpsSamples >= kMeasureSeconds)
		finish(true, QString());
}

void TwitchBandwidthTest::finish(bool ok, const QString &error)
{
	if (!active)
		return;
	active = false;
	tick.stop();
	res.ok = ok;
	res.error = error;
	res.seconds = kbpsSamples;
	res.avgKbps = kbpsSamples > 0 ? kbpsSum / kbpsSamples : 0.0;
	runner.stop();
	if (ok)
		obs_log(LOG_INFO, "Twitch bandwidth test: %.0f of %d kbps, %d/%d frames dropped", res.avgKbps, res.targetKbps,
			res.droppedFrames, res.totalFrames);
	else
		obs_log(LOG_INFO, "Twitch bandwidth test failed: %s", error.toUtf8().constData());
	emit finished(res);
}
