/*
Red Warden Pre-Flight
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "multistream-probe.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#include <cassert>
#include <cstring>

#include "obs-websocket-api.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QtGlobal>

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace preflight {
namespace {
constexpr unsigned kStatusSuccess = 100;  // obs-websocket RequestStatus::Success
constexpr unsigned kStatusNotFound = 600; // obs-websocket RequestStatus::ResourceNotFound (no such vendor)
constexpr auto kPollEvery = std::chrono::seconds(5);
constexpr auto kStopWait = std::chrono::seconds(4);

const char *statusName(MultistreamStatus s)
{
	switch (s) {
	case MultistreamStatus::Pending:
		return "pending";
	case MultistreamStatus::NotFound:
		return "not found";
	case MultistreamStatus::Ok:
		return "ok";
	case MultistreamStatus::Error:
		return "error";
	}
	return "?";
}

// One blocking round trip. Never call on the UI thread.
MultistreamResult callOnce()
{
	MultistreamResult r;
	r.status = MultistreamStatus::Error;

	obs_data_t *req = obs_data_create();
	obs_data_set_string(req, "vendorName", "RedWardenMultistream");
	obs_data_set_string(req, "requestType", "GetDestinations");
	obs_data_t *inner = obs_data_create(); // requestData: {}
	obs_data_set_obj(req, "requestData", inner);
	obs_data_release(inner);

	obs_websocket_request_response *resp = obs_websocket_call_request("CallVendorRequest", req);
	obs_data_release(req);

	if (!resp) { // obs-websocket is not installed / not up yet
		r.status = MultistreamStatus::NotFound;
		return r;
	}
	const unsigned code = resp->status_code;
	QByteArray body;
	if (resp->response_data)
		body = QByteArray(resp->response_data);
	obs_websocket_request_response_free(resp);

	if (code == kStatusNotFound) {
		r.status = MultistreamStatus::NotFound;
	} else if (code == kStatusSuccess) {
		r.status = parseGetDestinations(body, &r.names);
		if (r.status != MultistreamStatus::Ok)
			r.names.clear();
	}
	return r;
}
} // namespace

MultistreamStatus parseGetDestinations(const QByteArray &json, QStringList *enabledNames)
{
	if (enabledNames)
		enabledNames->clear();
	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
		return MultistreamStatus::Error;

	// Unwrap obs-websocket's CallVendorRequest envelope (at most a couple of levels).
	QJsonObject obj = doc.object();
	for (int depth = 0; depth < 3; ++depth) {
		if (obj.contains(QLatin1String("destinations")) || obj.contains(QLatin1String("ok")))
			break;
		const QJsonValue inner = obj.value(QLatin1String("responseData"));
		if (!inner.isObject())
			break;
		obj = inner.toObject();
	}

	const QJsonValue ok = obj.value(QLatin1String("ok"));
	if (ok.isBool() && !ok.toBool())
		return MultistreamStatus::Error; // Multistream could not answer (UI thread busy / exiting)
	const QJsonValue dests = obj.value(QLatin1String("destinations"));
	if (!dests.isArray())
		return MultistreamStatus::Error;

	QStringList names;
	for (const QJsonValue &v : dests.toArray()) {
		const QJsonObject d = v.toObject();
		if (!d.value(QLatin1String("enabled")).toBool(false))
			continue;
		QString n = d.value(QLatin1String("name")).toString();
		if (n.isEmpty())
			n = d.value(QLatin1String("id")).toString();
		names << n;
	}
	if (enabledNames)
		*enabledNames = names;
	return MultistreamStatus::Ok;
}

// Shared with the worker thread by shared_ptr: if the worker has to be abandoned (see stop()) it
// still owns a live State, and nothing on the worker touches the MultistreamProbe object itself.
struct MultistreamProbe::State {
	std::mutex m;
	std::condition_variable cv;
	bool stop = false;
	bool poke = false;
	bool finished = false;
	MultistreamResult result;
};

namespace {
void workerMain(std::shared_ptr<MultistreamProbe::State> st, std::thread::id uiId, bool testHooks);
}

MultistreamProbe::MultistreamProbe() : st_(std::make_shared<State>()) {}

MultistreamProbe::~MultistreamProbe()
{
	stop();
}

void MultistreamProbe::start()
{
	if (started_)
		return;
	started_ = true;
	const bool hooks = qEnvironmentVariableIsSet("RWS_PREFLIGHT_TEST");
	const std::thread::id uiId = std::this_thread::get_id();
	auto st = st_;
	th_ = std::thread([st, uiId, hooks] { workerMain(st, uiId, hooks); });
}

void MultistreamProbe::poke()
{
	std::lock_guard<std::mutex> lk(st_->m);
	st_->poke = true;
	st_->cv.notify_all();
}

MultistreamResult MultistreamProbe::result() const
{
	std::lock_guard<std::mutex> lk(st_->m);
	return st_->result;
}

// Why a bounded wait and then detach: at OBS exit the worker may be inside a vendor call that is
// waiting for Multistream, which in turn is waiting (up to 3 s) for a UI thread that is busy running
// this very shutdown. Joining unconditionally could deadlock OBS's exit. So we wait up to 4 s for a
// clean finish and otherwise let the thread go. That is safe because (a) the worker owns its State by
// shared_ptr and never touches this object, and (b) the plugin DLL is pinned in memory
// (GET_MODULE_HANDLE_EX_FLAG_PIN in plugin-main.cpp), so the thread's code stays valid until the
// process ends.
//
// Why we PUMP THE UI EVENT LOOP while waiting (QCoreApplication::processEvents): Multistream's
// GetDestinations handler does not answer from its own thread. It posts a queued call onto THIS UI
// thread and waits for it (up to 3 s). If stop() simply blocked here, a vendor call that is in flight
// at exit could never be answered, so we would sit out Multistream's whole 3 s timeout (an OBS exit
// that freezes ~4 s) and, worse, the call could still be running while modules unload. Pumping
// events (ExcludeUserInputEvents: no clicks or keys get handled mid-shutdown) lets Multistream's
// queued call run, the handler return, and the worker see the stop flag and finish in milliseconds.
// Only if the worker is still running after the full 4 s do we detach it.
void MultistreamProbe::stop()
{
	if (!started_)
		return;
	started_ = false;
	const auto t0 = std::chrono::steady_clock::now();
	{
		std::lock_guard<std::mutex> lk(st_->m);
		st_->stop = true;
		st_->cv.notify_all();
	}
	bool finished = false;
	for (;;) {
		{
			std::unique_lock<std::mutex> lk(st_->m);
			finished = st_->finished;
			if (!finished && std::chrono::steady_clock::now() - t0 < kStopWait)
				finished = st_->cv.wait_for(lk, std::chrono::milliseconds(15), [this] { return st_->finished; });
		}
		if (finished || std::chrono::steady_clock::now() - t0 >= kStopWait)
			break;
		QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	}
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
	if (!th_.joinable())
		return;
	if (finished) {
		th_.join();
		obs_log(LOG_INFO, "multistream probe stopped in %d ms", static_cast<int>(ms));
	} else {
		obs_log(LOG_WARNING, "multistream probe detached at exit");
		th_.detach();
	}
}

namespace {
void workerMain(std::shared_ptr<MultistreamProbe::State> st, std::thread::id uiId, bool testHooks)
{
	bool firstRound = true;
	// Test hook only (needs RWS_PREFLIGHT_TEST too): poll back to back, so a vendor call is almost
	// always in flight when OBS exits. The integration test uses it to prove exit does not stall.
	const bool hammer = testHooks && qEnvironmentVariableIsSet("RWS_PREFLIGHT_TEST_HAMMER");
	bool loggedThread = false;
	bool haveLast = false;
	MultistreamResult last;

	std::unique_lock<std::mutex> lk(st->m);
	while (!st->stop) {
		if (!firstRound && !hammer) {
			st->cv.wait_for(lk, kPollEvery, [&] { return st->stop || st->poke; });
		}
		// Check the stop flag IMMEDIATELY before every vendor call (we hold the lock that stop() takes
		// to set it): once OBS is exiting we must never start a call into another module.
		if (st->stop)
			break;
		firstRound = false;
		st->poke = false;

		lk.unlock();
		const MultistreamResult r = callOnce();
		const bool offUi = std::this_thread::get_id() != uiId;
		lk.lock();

		st->result = r;
		// State changes only, never per poll.
		if (!haveLast || last.status != r.status || last.names != r.names) {
			obs_log(LOG_INFO, "multistream probe: %s (%d enabled)", statusName(r.status),
				(int)r.names.size());
			last = r;
			haveLast = true;
		}
		if (testHooks && !loggedThread) {
			loggedThread = true;
			obs_log(LOG_INFO, offUi ? "multistream-probe ui-thread-free"
						: "multistream-probe RAN-ON-UI-THREAD");
		}
	}
	st->finished = true;
	st->cv.notify_all();
}
} // namespace

} // namespace preflight
