# Releasing Red Warden Multistream

1. Bump `version` in `buildspec.json`.
2. `tools\run-all.ps1 -Steps configure,build,unit,import,local,resilience,bandwidth` (detached; read
   `.testbed\run-*.txt`). Every harness must say `RESULT: PASS`.
3. YubiKey in. `powershell -ExecutionPolicy Bypass -File tools\package.ps1`
   (one PIN prompt per signature: DLL, uninstaller, setup.exe). It refuses to package unless every
   unit test and harness passed against this exact DLL. Output lands in `dist\`.
4. Upload `dist\*` to the release page, with the matching source (GPL: publish the source for every
   binary you ship).
5. LAST, and only once the download is live: copy `installer\multistream.json` to the website as
   `/updates/multistream.json` with the new `latest`, a one-line `notes`, and `min_obs` if this
   release needs a newer OBS. That file is what makes every installed copy show "Update available".
   Publishing it before the download works sends users to a dead link.

Upgrades: the installer has a fixed AppId, so a newer installer replaces the plugin in place.
Destinations (per OBS profile) and stream keys (Windows Credential Manager) are never touched.
