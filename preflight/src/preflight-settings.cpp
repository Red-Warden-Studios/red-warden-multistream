/*
Red Warden Pre-Flight
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "preflight-settings.hpp"

#include <obs-module.h>
#include <util/bmem.h>
#include <util/platform.h>

namespace preflight {
namespace {
QString getStr(obs_data_t *d, const char *key)
{
	const char *v = obs_data_get_string(d, key);
	return v ? QString::fromUtf8(v) : QString();
}

void setStr(obs_data_t *d, const char *key, const QString &v)
{
	obs_data_set_string(d, key, v.toUtf8().constData());
}
} // namespace

Settings Settings::load()
{
	Settings s;
	char *path = obs_module_config_path("settings.json");
	if (!path)
		return s;
	if (obs_data_t *d = obs_data_create_from_json_file_safe(path, "bak")) {
		obs_data_set_default_bool(d, "setup_done", s.setupDone);
		obs_data_set_default_bool(d, "use_main_stream", s.useMainStream);
		obs_data_set_default_bool(d, "use_multistream", s.useMultistream);
		obs_data_set_default_bool(d, "record_intent", s.recordIntent);
		obs_data_set_default_bool(d, "check_mic", true);
		obs_data_set_default_bool(d, "check_desktop", true);
		obs_data_set_default_bool(d, "check_scene", true);
		obs_data_set_default_bool(d, "check_main_stream", true);
		obs_data_set_default_bool(d, "check_multistream", true);
		obs_data_set_default_bool(d, "check_recording", true);
		obs_data_set_default_bool(d, "check_performance", true);
		obs_data_set_default_bool(d, "check_custom", true);
		obs_data_set_default_bool(d, "check_updates", true);

		s.setupDone = obs_data_get_bool(d, "setup_done");
		s.useMainStream = obs_data_get_bool(d, "use_main_stream");
		s.useMultistream = obs_data_get_bool(d, "use_multistream");
		s.micUuid = getStr(d, "mic_uuid");
		s.desktopUuid = getStr(d, "desktop_uuid");
		s.startSceneUuid = getStr(d, "start_scene_uuid");
		s.recordIntent = obs_data_get_bool(d, "record_intent");
		s.checkMic = obs_data_get_bool(d, "check_mic");
		s.checkDesktop = obs_data_get_bool(d, "check_desktop");
		s.checkScene = obs_data_get_bool(d, "check_scene");
		s.checkMainStream = obs_data_get_bool(d, "check_main_stream");
		s.checkMultistream = obs_data_get_bool(d, "check_multistream");
		s.checkRecording = obs_data_get_bool(d, "check_recording");
		s.checkPerformance = obs_data_get_bool(d, "check_performance");
		s.checkCustom = obs_data_get_bool(d, "check_custom");
		s.updateChecks = obs_data_get_bool(d, "check_updates");

		if (obs_data_array_t *arr = obs_data_get_array(d, "custom_items")) {
			const size_t n = obs_data_array_count(arr);
			for (size_t i = 0; i < n; ++i) {
				if (obs_data_t *item = obs_data_array_item(arr, i)) {
					const QString t = getStr(item, "text");
					if (!t.trimmed().isEmpty())
						s.customItems << t;
					obs_data_release(item);
				}
			}
			obs_data_array_release(arr);
		}
		obs_data_release(d);
	}
	bfree(path);
	return s;
}

bool Settings::save() const
{
	char *path = obs_module_config_path("settings.json");
	if (!path)
		return false;
	// Start from what is on disk so unknown keys are kept.
	obs_data_t *d = obs_data_create_from_json_file_safe(path, "bak");
	if (!d)
		d = obs_data_create();

	obs_data_set_bool(d, "setup_done", setupDone);
	obs_data_set_bool(d, "use_main_stream", useMainStream);
	obs_data_set_bool(d, "use_multistream", useMultistream);
	setStr(d, "mic_uuid", micUuid);
	setStr(d, "desktop_uuid", desktopUuid);
	setStr(d, "start_scene_uuid", startSceneUuid);
	obs_data_set_bool(d, "record_intent", recordIntent);
	obs_data_set_bool(d, "check_mic", checkMic);
	obs_data_set_bool(d, "check_desktop", checkDesktop);
	obs_data_set_bool(d, "check_scene", checkScene);
	obs_data_set_bool(d, "check_main_stream", checkMainStream);
	obs_data_set_bool(d, "check_multistream", checkMultistream);
	obs_data_set_bool(d, "check_recording", checkRecording);
	obs_data_set_bool(d, "check_performance", checkPerformance);
	obs_data_set_bool(d, "check_custom", checkCustom);
	obs_data_set_bool(d, "check_updates", updateChecks);

	obs_data_array_t *arr = obs_data_array_create();
	for (const QString &t : customItems) {
		obs_data_t *item = obs_data_create();
		setStr(item, "text", t);
		obs_data_array_push_back(arr, item);
		obs_data_release(item);
	}
	obs_data_set_array(d, "custom_items", arr);
	obs_data_array_release(arr);

	if (char *dir = obs_module_config_path("")) {
		os_mkdirs(dir);
		bfree(dir);
	}
	const bool ok = obs_data_save_json_safe(d, path, "tmp", "bak");
	obs_data_release(d);
	bfree(path);
	return ok;
}

} // namespace preflight
