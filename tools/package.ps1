<#
  Package the Red Warden Stream Kit (Multistream + Pre-Flight in one installer).

    powershell -ExecutionPolicy Bypass -File tools\package.ps1            # signed release
    powershell -ExecutionPolicy Bypass -File tools\package.ps1 -NoSign    # dry run, unsigned

  Builds NOTHING. It takes each plugin's existing RelWithDebInfo DLL, locale and PDB, and refuses
  unless each plugin's tests passed against that exact DLL, as judged by tools\check-test-results.ps1:
    Multistream: all 6 unit tests exit 0 in run-unit.txt, and the import/local/resilience/bandwidth
                 harness files say RESULT: PASS.
    Pre-Flight : all 4 unit tests exit 0 in run-unit.txt, and the load/state/ui/integration harness
                 files have PASS as their first line.
    Every one of those files must be newer than the DLL. run-done.txt is not trusted.
  Output in dist\: RedWardenStreamKit-<ver>-Setup.exe, RedWardenStreamKit-<ver>.zip, SHA256SUMS.txt,
  symbols\ (the PDBs, for our own crash symbolication; NOT shipped). Staging lives in staging\.

  Signing (skipped with -NoSign): same as Multistream - SSL.com OV certificate on the YubiKey, one PIN
  prompt per signature (two DLLs, uninstaller, setup.exe). Timestamp MUST be RFC 3161 (/tr).
#>
param(
  [switch]$NoSign,
  [string]$Thumbprint = "FE5CCAC231978A6489E4DBAE2A333E2F633053E4",
  [string]$Config = "RelWithDebInfo"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$kitVersion = "1.0.1"
# Plugin folders: multistream\ + preflight\ inside this repo (public layout) when both exist, else the
# sibling red-warden-multistream + red-warden-preflight folders (development layout).
if ((Test-Path (Join-Path $root "multistream")) -and (Test-Path (Join-Path $root "preflight"))) {
  $ms = Join-Path $root "multistream"; $pf = Join-Path $root "preflight"
} else {
  $projects = Split-Path -Parent $root
  $ms = Join-Path $projects "red-warden-multistream"; $pf = Join-Path $projects "red-warden-preflight"
}
$msLogo = Join-Path $ms "brand\logo.ico"
$dist = Join-Path $root "dist"
$stage = Join-Path $root "staging"
$iscc = Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"
$signtool = $null
if (!$NoSign) {
  $signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
    Where-Object FullName -like '*\x64\*' | Sort-Object FullName | Select-Object -Last 1 -ExpandProperty FullName
}

function Fail($msg) { Write-Host "[FAIL] $msg" -ForegroundColor Red; exit 1 }
function Spec($dir) { Get-Content (Join-Path $dir "buildspec.json") -Raw | ConvertFrom-Json }

# --- Gates: refuse to package anything we can't stand behind --------------------------------------
if (!(Test-Path $iscc)) { Fail "Inno Setup not found at $iscc (winget install JRSoftware.InnoSetup --scope user)" }
foreach ($f in "LICENSE", "NOTICE.txt", "installer\stream-kit.iss", "installer\stream-kit.json") { if (!(Test-Path (Join-Path $root $f))) { Fail "Missing $f" } }
if (!(Test-Path $msLogo)) { Fail "Missing Multistream brand\logo.ico" }

$manifest = Get-Content (Join-Path $root "installer\stream-kit.json") -Raw | ConvertFrom-Json
if ($manifest.latest -ne $kitVersion) { Fail "installer\stream-kit.json says latest '$($manifest.latest)', this script packages $kitVersion" }
# Both plugins compare the update manifest against a compiled-in kit version: it must be this one.
foreach ($d in $ms, $pf) {
  $hpp = Join-Path $d "src\update-manifest.hpp"
  if (!(Select-String -Path $hpp -Pattern ('kKitVersion = "' + [regex]::Escape($kitVersion) + '"') -Quiet)) {
    Fail "$hpp does not carry kKitVersion = $kitVersion (both plugins must agree with the Kit)"
  }
}

$msSpec = Spec $ms
$pfSpec = Spec $pf
$msDll = Join-Path $ms "build_x64\$Config\rws-multistream.dll"
$pfDll = Join-Path $pf "build_x64\$Config\rws-preflight.dll"
foreach ($x in @(@($msDll, $msSpec), @($pfDll, $pfSpec))) {
  if (!(Test-Path $x[0])) { Fail "Build first: $($x[0]) not found" }
  $fv = (Get-Item $x[0]).VersionInfo.ProductVersion
  if ($fv -ne $x[1].version) { Fail "$($x[0]) says version '$fv' but its buildspec.json says '$($x[1].version)'. Re-run configure + build." }
}

# Stale-DLL hole: a DLL built before the last source edit proves nothing. Refuse if ANY file under a
# plugin's src\ or data\, its CMakeLists.txt or its buildspec.json is newer than that plugin's DLL.
foreach ($x in @(@($msDll, $ms), @($pfDll, $pf))) {
  $dllTime = (Get-Item $x[0]).LastWriteTime
  $srcFiles = @(Get-ChildItem (Join-Path $x[1] "src"), (Join-Path $x[1] "data") -Recurse -File -ErrorAction SilentlyContinue) +
              @(Get-Item (Join-Path $x[1] "CMakeLists.txt"), (Join-Path $x[1] "buildspec.json"))
  $newer = $srcFiles | Where-Object { $_.LastWriteTime -gt $dllTime } | Sort-Object LastWriteTime | Select-Object -First 1
  if ($newer) { Fail "$($newer.FullName) (modified $($newer.LastWriteTime)) is newer than $($x[0]) (built $dllTime). Rebuild and re-run that plugin's tests." }
}

# Test gate, both plugins: tools\check-test-results.ps1 requires every named unit test to exit 0 and
# every harness to pass, each result file newer than the DLL. run-done.txt is not consulted.
$checker = Join-Path $PSScriptRoot "check-test-results.ps1"
& powershell -NoProfile -ExecutionPolicy Bypass -File $checker -Plugin multistream -PluginDir $ms -Dll $msDll
if ($LASTEXITCODE -ne 0) { Fail "Multistream tests did not all pass against its DLL (see above)" }
& powershell -NoProfile -ExecutionPolicy Bypass -File $checker -Plugin preflight -PluginDir $pf -Dll $pfDll
if ($LASTEXITCODE -ne 0) { Fail "Pre-Flight tests did not all pass against its DLL (see above)" }

if (!$NoSign) {
  if (!$signtool) { Fail "signtool.exe not found (Windows SDK)" }
  if (!(Get-ChildItem Cert:\CurrentUser\My | Where-Object Thumbprint -eq $Thumbprint)) {
    Fail "Code-signing cert $Thumbprint not in the store. Plug in the YubiKey (minidriver installed?)."
  }
}

function Sign($file, $desc) {
  if ($NoSign) { return }
  & $signtool sign /sha1 $Thumbprint /fd sha256 /tr http://ts.ssl.com /td sha256 /d $desc $file
  if ($LASTEXITCODE -ne 0) { Fail "signtool sign failed on $file" }
  & $signtool verify /pa /q $file
  if ($LASTEXITCODE -ne 0) { Fail "signature does not verify on $file" }
  Write-Host "[OK] signed $(Split-Path -Leaf $file)"
}

# --- Stage ----------------------------------------------------------------------------------------
if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
$sym = Join-Path $dist "symbols"
New-Item -ItemType Directory -Force $dist, $sym | Out-Null
foreach ($p in @(@("rws-multistream", $ms), @("rws-preflight", $pf))) {
  $mod = $p[0]; $dir = $p[1]
  $bin = Join-Path $stage "$mod\bin\64bit"
  $loc = Join-Path $stage "$mod\data\locale"
  New-Item -ItemType Directory -Force $bin, $loc | Out-Null
  $built = Join-Path $dir "build_x64\$Config"
  Copy-Item (Join-Path $built "$mod.dll") $bin
  if (Test-Path (Join-Path $built "$mod.pdb")) { Copy-Item (Join-Path $built "$mod.pdb") $sym } else { Write-Host "[warn] no $mod.pdb in $built" }
  Copy-Item (Join-Path $dir "data\locale\*.ini") $loc
}

# --- Installer tests: a required gate, BEFORE anything is signed or the release Setup is built ------
# test-installer.ps1 compiles a TestRoot build from the staged (still unsigned) DLLs and runs the whole
# install / uninstall / legacy-removal / OBS-running matrix in a scratch folder.
$instResult = Join-Path $root ".testbed\run-installer.txt"
Remove-Item $instResult -ErrorAction SilentlyContinue
Write-Host "Running tools\test-installer.ps1 (release gate) ..."
& powershell -ExecutionPolicy Bypass -File (Join-Path $root "tools\test-installer.ps1") | Out-Null
if (!(Test-Path $instResult)) { Fail "test-installer.ps1 wrote no $instResult" }
$instLast = (Get-Content $instResult | Select-Object -Last 1)
if ($instLast -ne "RESULT: PASS") { Fail "installer tests did not pass ($instLast). See $instResult" }
$instTime = (Get-Item $instResult).LastWriteTime
foreach ($dep in @($msDll, $pfDll, (Join-Path $root "installer\stream-kit.iss"), (Join-Path $root "tools\test-fake-legacy.iss"), (Join-Path $root "tools\test-installer.ps1"))) {
  if ($instTime -lt (Get-Item $dep).LastWriteTime) { Fail "installer test result is older than $dep" }
}
Write-Host "[OK] installer tests passed"

# --- Sign the staged DLLs (skipped with -NoSign) ----------------------------------------------------
foreach ($mod in "rws-multistream", "rws-preflight") {
  Sign (Join-Path $stage "$mod\bin\64bit\$mod.dll") ("Red Warden " + $(if ($mod -eq "rws-multistream") { "Multistream" } else { "Pre-Flight" }))
}

# --- Zip (manual / portable OBS installs) ---------------------------------------------------------
$zipStage = Join-Path $dist "zipstage"
New-Item -ItemType Directory -Force $zipStage | Out-Null
Copy-Item (Join-Path $stage "rws-multistream"), (Join-Path $stage "rws-preflight") $zipStage -Recurse
Copy-Item LICENSE (Join-Path $zipStage "LICENSE.txt")
Copy-Item NOTICE.txt $zipStage
@"
Red Warden Stream Kit $kitVersion - manual install

1. Close OBS.
2. Copy the rws-multistream and rws-preflight folders (either or both) into
   C:\ProgramData\obs-studio\plugins\   (create the plugins folder if it does not exist).
3. Start OBS, then open the Docks menu.

The installer (.exe) does all of this for you and is the easier route.
"@ | Set-Content (Join-Path $zipStage "INSTALL.txt") -Encoding ascii
$zip = Join-Path $dist "RedWardenStreamKit-$kitVersion.zip"
Compress-Archive -Path (Join-Path $zipStage "*") -DestinationPath $zip
Remove-Item -Recurse -Force $zipStage

# --- Installer ------------------------------------------------------------------------------------
$isccArgs = @("/DAppVersion=$kitVersion", "/DStage=$stage", "/DMsLogo=$msLogo", "/Q")
# Every quote inside the sign command is Inno's $q token, never a literal ": Windows PowerShell 5.1
# mangles embedded quotes in native arguments, and the signtool path has spaces. ($f arrives quoted.)
if ($NoSign) { $isccArgs += "/DNoSign" }
else { $isccArgs += "/Srws=`$q$signtool`$q sign /sha1 $Thumbprint /fd sha256 /tr http://ts.ssl.com /td sha256 /d `$qRed Warden Stream Kit`$q `$f" }
& $iscc @isccArgs "installer\stream-kit.iss"
if ($LASTEXITCODE -ne 0) { Fail "ISCC failed" }
$setup = Join-Path $dist "RedWardenStreamKit-$kitVersion-Setup.exe"
if (!(Test-Path $setup)) { Fail "ISCC reported success but $setup is missing" }
if (!$NoSign) {
  & $signtool verify /pa /q $setup
  if ($LASTEXITCODE -ne 0) { Fail "setup.exe signature does not verify" }
}

# --- Checksums ------------------------------------------------------------------------------------
Get-ChildItem $dist -File | ForEach-Object {
  "{0}  {1}" -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(), $_.Name
} | Set-Content (Join-Path $dist "SHA256SUMS.txt") -Encoding ascii
Write-Host "[OK] Red Warden Stream Kit $kitVersion packaged$(if ($NoSign) {' (UNSIGNED dry run)'}) -> $dist"
Get-ChildItem $dist | Format-Table Name, Length -AutoSize
