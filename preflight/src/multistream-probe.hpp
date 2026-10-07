/*
Red Warden Pre-Flight
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "preflight-eval.hpp"

#include <QByteArray>
#include <QStringList>

#include <memory>
#include <thread>

namespace preflight {

struct MultistreamResult {
	MultistreamStatus status = MultistreamStatus::Pending;
	QStringList names; // enabled destination names (Ok only)
};

// Pure: turns the JSON text obs-websocket hands back for a CallVendorRequest of
// RedWardenMultistream/GetDestinations into a status. Accepts the vendor reply either wrapped
// (obs-websocket: {"vendorName":..,"requestType":..,"responseData":{...}}) or bare.
// Multistream's reply is {"destinations":[{"id","name","platform","enabled","state","kbps"}], "ok":bool}.
// Ok + the names of the enabled destinations; Error if the shape is unrecognised or ok is false.
MultistreamStatus parseGetDestinations(const QByteArray &json, QStringList *enabledNames);

// Asks Multistream (through obs-websocket's in-process vendor API) which destinations are on.
// Runs on its own worker thread: Multistream's handler marshals to the UI thread and waits up to
// 3 s, so calling it on the UI thread would stall OBS. result() is safe from any thread.
class MultistreamProbe {
public:
	MultistreamProbe();
	~MultistreamProbe();
	MultistreamProbe(const MultistreamProbe &) = delete;
	MultistreamProbe &operator=(const MultistreamProbe &) = delete;

	// Construct and start() on the UI thread (the worker compares its thread id to the starter's).
	void start();
	void poke();     // poll again now instead of waiting for the 5 s timer
	void stop();     // idempotent; bounded wait, see the .cpp
	MultistreamResult result() const;

	struct State; // opaque; shared with the worker thread

private:
	std::shared_ptr<State> st_;
	std::thread th_;
	bool started_ = false;
};

} // namespace preflight
