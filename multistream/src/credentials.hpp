/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

// Stream keys live in the OS credential store, never in OBS profile files.
// Windows: Credential Manager, generic credential
//   "RedWardenStudios/Multistream/<destination id>".
namespace creds {
bool storeKey(const QString &destinationId, const QString &key);
QString loadKey(const QString &destinationId);
bool hasKey(const QString &destinationId);
void deleteKey(const QString &destinationId);
} // namespace creds
