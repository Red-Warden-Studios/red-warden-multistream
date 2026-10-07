/*
Red Warden Pre-Flight
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "obs-probe.hpp"

#include <obs-frontend-api.h>
#include <obs.h>
#include <util/config-file.h>

#include <algorithm>
#include <cstring>

namespace preflight {
namespace {
QString fromC(const char *s)
{
	return s ? QString::fromUtf8(s) : QString();
}

Inputs::Audio probeAudio(const QString &uuid)
{
	Inputs::Audio a;
	a.selected = !uuid.isEmpty();
	if (!a.selected)
		return a;
	obs_source_t *src = obs_get_source_by_uuid(uuid.toUtf8().constData());
	if (src) {
		a.exists = true;
		a.isAudioSource = (obs_source_get_output_flags(src) & OBS_SOURCE_AUDIO) != 0;
		a.muted = obs_source_muted(src);
		obs_source_release(src);
	}
	return a;
}

} // namespace

void ObsProbe::resetFrameWindows()
{
	render_.reset();
	encoder_.reset();
	network_.reset();
}

void ObsProbe::resetNetworkWindow()
{
	network_.reset();
}

void ObsProbe::resetRecordingCache()
{
	disk_.invalidate();
}

void ObsProbe::shutdown()
{
	disk_.stop(); // 2 s bounded wait, then the worker is detached
}

void ObsProbe::fillRecording(Inputs::Recording &rec, const Settings &settings)
{
	rec.intent = settings.recordIntent;
	if (!settings.recordIntent)
		return;

	// Auto-record: user config. OBS keeps a default for this key, so a missing value reads as off.
	config_t *user = obs_frontend_get_user_config();
	rec.autoRecordOn = user && config_get_bool(user, "BasicWindow", "RecordWhenStreaming");

	// Where would OBS write? Profile config.
	config_t *prof = obs_frontend_get_profile_config();
	QString path;
	rec.pathKind = PathKind::File;
	if (!prof) {
		rec.pathKind = PathKind::Unknown;
		return;
	}
	const char *mode = config_get_string(prof, "Output", "Mode");
	if (mode && std::strcmp(mode, "Advanced") == 0) {
		const char *type = config_get_string(prof, "AdvOut", "RecType");
		if (type && std::strcmp(type, "FFmpeg") == 0) {
			if (!config_get_bool(prof, "AdvOut", "FFOutputToFile"))
				rec.pathKind = PathKind::Url; // custom FFmpeg output to a URL: no folder to check
			else
				path = fromC(config_get_string(prof, "AdvOut", "FFFilePath"));
		} else {
			path = fromC(config_get_string(prof, "AdvOut", "RecFilePath"));
		}
	} else {
		path = fromC(config_get_string(prof, "SimpleOutput", "FilePath"));
	}
	if (rec.pathKind != PathKind::File)
		return;

	// Disk work (stat, temp file, free space) happens on the DiskProbe worker; this only reads its
	// cached answer (and asks for a refresh when it is missing, for another path, or 10 s old).
	// No answer yet = Unknown, never a guess.
	const DiskResult d = disk_.request(path);
	rec.diskKnown = d.valid;
	if (!d.valid)
		return;
	rec.folderExists = d.folderExists;
	rec.folderWritable = d.folderWritable;
	if (d.freeBytes >= 0)
		rec.freeBytes = d.freeBytes;
}

Inputs ObsProbe::gather(const Settings &s, const Runtime &rt)
{
	Inputs in;
	in.setupDone = s.setupDone;
	in.workflow.useMainStream = s.useMainStream;
	in.workflow.useMultistream = s.useMultistream;
	in.enabled.mic = s.checkMic;
	in.enabled.desktop = s.checkDesktop;
	in.enabled.scene = s.checkScene;
	in.enabled.mainStream = s.checkMainStream;
	in.enabled.multistream = s.checkMultistream;
	in.enabled.recording = s.checkRecording;
	in.enabled.performance = s.checkPerformance;
	in.enabled.custom = s.checkCustom;

	// Frame windows are fed every tick, even before setup, so they are full when needed.
	render_.add(rt.nowMs, obs_get_lagged_frames(), obs_get_total_frames());
	if (video_t *video = obs_get_video())
		encoder_.add(rt.nowMs, video_output_get_skipped_frames(video), video_output_get_total_frames(video));
	const bool live = obs_frontend_streaming_active();
	if (live) {
		if (obs_output_t *out = obs_frontend_get_streaming_output()) {
			network_.add(rt.nowMs, static_cast<quint64>(std::max(0, obs_output_get_frames_dropped(out))),
				     static_cast<quint64>(std::max(0, obs_output_get_total_frames(out))));
			obs_output_release(out);
		}
	} else {
		network_.reset();
	}
	in.performance.live = live;
	in.performance.renderWindow = render_.stats(rt.nowMs);
	in.performance.encoderWindow = encoder_.stats(rt.nowMs);
	in.performance.networkWindow = network_.stats(rt.nowMs);

	if (!s.setupDone)
		return in; // the evaluator shows the setup card; skip the rest

	// Audio
	in.mic = probeAudio(s.micUuid);
	in.micTest = rt.micTest;
	in.desktop = probeAudio(s.desktopUuid);

	// Scene
	in.scene.chosen = !s.startSceneUuid.isEmpty();
	if (in.scene.chosen) {
		if (obs_source_t *sc = obs_get_source_by_uuid(s.startSceneUuid.toUtf8().constData())) {
			in.scene.chosenExists = obs_source_is_scene(sc);
			obs_source_release(sc);
		}
		if (in.scene.chosenExists) {
			// Program scene (correct in Studio Mode too). Returns a ref.
			if (obs_source_t *cur = obs_frontend_get_current_scene()) {
				const char *u = obs_source_get_uuid(cur);
				in.scene.chosenIsLive = u && s.startSceneUuid == QString::fromUtf8(u);
				obs_source_release(cur);
			}
		}
	}

	// Main stream. Only emptiness of server / key is ever recorded: the strings are never stored,
	// copied or logged.
	if (s.useMainStream) {
		// obs_frontend_get_streaming_service() does not add a reference (OBSStudioAPI returns the
		// main window's own pointer), so there is nothing to release.
		if (obs_service_t *svc = obs_frontend_get_streaming_service()) {
			in.mainStream.serviceType = fromC(obs_service_get_type(svc));
			in.mainStream.canTryToConnect = obs_service_can_try_to_connect(svc);
			const char *server = obs_service_get_connect_info(svc, OBS_SERVICE_CONNECT_INFO_SERVER_URL);
			const char *key = obs_service_get_connect_info(svc, OBS_SERVICE_CONNECT_INFO_STREAM_KEY);
			in.mainStream.hasServer = server && *server;
			in.mainStream.hasKey = key && *key;
		}
	}

	// Multistream (cached by the worker thread)
	in.multistream.status = rt.multistreamStatus;
	in.multistream.enabledNames = rt.multistreamNames;

	// Recording
	if (s.recordIntent && s.checkRecording)
		fillRecording(in.recording, s);
	else
		in.recording.intent = s.recordIntent;

	// Own items
	for (int i = 0; i < s.customItems.size(); ++i) {
		Inputs::CustomItem c;
		c.text = s.customItems[i];
		c.ticked = i < rt.ticks.size() && rt.ticks[i];
		in.custom.push_back(c);
	}
	return in;
}

} // namespace preflight
