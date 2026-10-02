/*
Red Warden Multistream
Copyright (C) 2026 Red Warden Studios LLC
SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "credentials.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincred.h>
#include <string>

namespace {
std::wstring target(const QString &id)
{
	return (QStringLiteral("RedWardenStudios/Multistream/") + id).toStdWString();
}
// 0.1.x stored keys under the working name "Beacon"; moved on first read.
std::wstring legacyTarget(const QString &id)
{
	return (QStringLiteral("RedWardenStudios/Beacon/") + id).toStdWString();
}
QString readBlob(const std::wstring &t, bool *found)
{
	PCREDENTIALW cred = nullptr;
	*found = CredReadW(t.c_str(), CRED_TYPE_GENERIC, 0, &cred) == TRUE;
	if (!*found)
		return {};
	QString key = QString::fromUtf8(reinterpret_cast<const char *>(cred->CredentialBlob),
					(qsizetype)cred->CredentialBlobSize);
	CredFree(cred);
	return key;
}
} // namespace

namespace creds {

bool storeKey(const QString &id, const QString &key)
{
	if (key.isEmpty()) {
		deleteKey(id);
		return true;
	}
	QByteArray blob = key.toUtf8();
	std::wstring t = target(id);
	std::wstring user = L"stream-key";

	CREDENTIALW c = {};
	c.Type = CRED_TYPE_GENERIC;
	c.TargetName = t.data();
	c.UserName = user.data();
	c.CredentialBlobSize = (DWORD)blob.size();
	c.CredentialBlob = reinterpret_cast<LPBYTE>(blob.data());
	c.Persist = CRED_PERSIST_LOCAL_MACHINE;

	const bool ok = CredWriteW(&c, 0) == TRUE;
	SecureZeroMemory(blob.data(), blob.size());
	if (!ok)
		obs_log(LOG_WARNING, "Credential Manager refused the stream key (error %lu)", GetLastError());
	return ok;
}

QString loadKey(const QString &id)
{
	bool found = false;
	QString key = readBlob(target(id), &found);
	if (found)
		return key;
	key = readBlob(legacyTarget(id), &found);
	if (found && storeKey(id, key)) // move it under the current name
		CredDeleteW(legacyTarget(id).c_str(), CRED_TYPE_GENERIC, 0);
	return key;
}

bool hasKey(const QString &id)
{
	bool found = false;
	readBlob(target(id), &found);
	if (!found)
		readBlob(legacyTarget(id), &found);
	return found;
}

void deleteKey(const QString &id)
{
	CredDeleteW(target(id).c_str(), CRED_TYPE_GENERIC, 0);
	CredDeleteW(legacyTarget(id).c_str(), CRED_TYPE_GENERIC, 0);
}

} // namespace creds

#else
// Non-Windows builds are not shipped yet. Until a Keychain / libsecret
// backend exists, keys are held in memory for the session only, so a key
// is never written to disk in plain text.
#include <QHash>

namespace creds {
static QHash<QString, QString> &mem()
{
	static QHash<QString, QString> m;
	return m;
}
bool storeKey(const QString &id, const QString &key)
{
	if (key.isEmpty())
		mem().remove(id);
	else
		mem()[id] = key;
	return true;
}
QString loadKey(const QString &id)
{
	return mem().value(id);
}
bool hasKey(const QString &id)
{
	return mem().contains(id);
}
void deleteKey(const QString &id)
{
	mem().remove(id);
}
} // namespace creds
#endif
