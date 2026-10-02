<#
  Package a signed public release of Red Warden Multistream.

    powershell -ExecutionPolicy Bypass -File tools\package.ps1            # signed release
    powershell -ExecutionPolicy Bypass -File tools\package.ps1 -NoSign    # dry run, unsigned

  Ships the exact RelWithDebInfo binary the test harnesses ran against (optimized; the .pdb
  stays in build_x64 for our own crash symbolication and is NOT shipped).
  Output in dist\: <name>-<ver>-windows-x64-setup.exe, <name>-<ver>-windows-x64.zip, SHA256SUMS.txt

  Signing: SSL.com OV certificate on the YubiKey (ECC P-256, needs the YubiKey minidriver).
  Expect one YubiKey PIN prompt per signature (DLL, uninstaller, setup.exe).
  Timestamp MUST be RFC 3161 (/tr). SSL.com refuses legacy /t - LLypses SOLUTIONS 2026-07-31.
#>
param(
  [switch]$NoSign,
  [string]$Thumbprint = "FE5CCAC231978A6489E4DBAE2A333E2F633053E4",
  [string]$Config = "RelWithDebInfo"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$spec    = Get-Content buildspec.json -Raw | ConvertFrom-Json
$version = $spec.version
$name    = "red-warden-multistream"
$dll     = Join-Path $root "build_x64\$Config\rws-multistream.dll"
$dist    = Join-Path $root "dist"
$stage   = Join-Path $dist "stage\rws-multistream"
$iscc    = Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"
$signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter signtool.exe |
  Where-Object FullName -like '*\x64\*' | Sort-Object FullName | Select-Object -Last 1 -ExpandProperty FullName

function Fail($msg) { Write-Host "[FAIL] $msg" -ForegroundColor Red; exit 1 }

# --- Gates: refuse to package anything we can't stand behind -------------------------------
if (!(Test-Path $dll)) { Fail "Build first: $dll not found" }
$fv = (Get-Item $dll).VersionInfo.ProductVersion
if ($fv -ne $version) { Fail "DLL says version '$fv' but buildspec.json says '$version'. Re-run configure + build." }
if (!(Test-Path $iscc)) { Fail "Inno Setup not found at $iscc (winget install JRSoftware.InnoSetup --scope user)" }
foreach ($f in "LICENSE", "NOTICE.txt", "brand\logo.ico") { if (!(Test-Path $f)) { Fail "Missing $f" } }
$unit = Join-Path $root ".testbed\run-unit.txt"
if (!(Test-Path $unit) -or (Get-Item $unit).LastWriteTime -lt (Get-Item $dll).LastWriteTime) {
  Fail "No unit-test run newer than the DLL. Run tools\run-all.ps1 -Steps build,unit,... first."
}
# Every expected test must have run AND exited 0 (a missing exe or a negative exit code is a fail).
$expected = "rws-multistream-tests", "rws-multistream-importers-test", "rws-multistream-session-test",
            "rws-multistream-limits-test", "rws-multistream-update-test"
$exits = @{}; $cur = $null
# The test exes write UTF-16, so the appended ASCII lines can carry a stray NUL: strip it first.
foreach ($raw in Get-Content $unit) {
  $l = $raw -replace '\x00', ''
  if ($l -match '^== (\S+)') { $cur = $Matches[1] }
  elseif ($l -match '^exit=(-?\d+)' -and $cur) { $exits[$cur] = [int]$Matches[1]; $cur = $null }
}
foreach ($t in $expected) {
  if (!$exits.ContainsKey($t)) { Fail "Unit test $t did not run ($unit)" }
  if ($exits[$t] -ne 0) { Fail "Unit test $t exited $($exits[$t]) ($unit)" }
}
# The OBS harnesses must have passed against this DLL too.
foreach ($h in "import", "local", "resilience", "bandwidth") {
  $f = Join-Path $root ".testbed\run-$h.txt"
  if (!(Test-Path $f) -or (Get-Item $f).LastWriteTime -lt (Get-Item $dll).LastWriteTime) { Fail "No $h test run newer than the DLL" }
  if (!(Select-String -Path $f -Pattern 'RESULT: PASS' -Quiet)) { Fail "$h test did not pass ($f)" }
}
if (!$NoSign) {
  if (!$signtool) { Fail "signtool.exe not found (Windows SDK)" }
  if (!(Get-ChildItem Cert:\CurrentUser\My | Where-Object Thumbprint -eq $Thumbprint)) {
    Fail "Code-signing cert $Thumbprint not in the store. Plug in the YubiKey (minidriver installed?)."
  }
}

function Sign($file) {
  if ($NoSign) { return }
  & $signtool sign /sha1 $Thumbprint /fd sha256 /tr http://ts.ssl.com /td sha256 /d "Red Warden Multistream" $file
  if ($LASTEXITCODE -ne 0) { Fail "signtool sign failed on $file" }
  & $signtool verify /pa /q $file
  if ($LASTEXITCODE -ne 0) { Fail "signature does not verify on $file" }
  Write-Host "[OK] signed $(Split-Path -Leaf $file)"
}

# --- Stage -----------------------------------------------------------------------------------
if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
New-Item -ItemType Directory -Force (Join-Path $stage "bin\64bit"), (Join-Path $stage "data\locale") | Out-Null
Copy-Item $dll (Join-Path $stage "bin\64bit")
Copy-Item "data\locale\*.ini" (Join-Path $stage "data\locale")
Sign (Join-Path $stage "bin\64bit\rws-multistream.dll")

# --- Zip (manual / portable OBS installs) ----------------------------------------------------
Copy-Item LICENSE (Join-Path $stage "LICENSE.txt")
Copy-Item NOTICE.txt $stage
@"
Red Warden Multistream $version - manual install

1. Close OBS.
2. Copy the rws-multistream folder into C:\ProgramData\obs-studio\plugins\
   (create the plugins folder if it does not exist).
3. Start OBS, then open Docks > Red Warden Multistream.

The installer (.exe) does all of this for you and is the easier route.
"@ | Set-Content (Join-Path $stage "INSTALL.txt") -Encoding ascii
$zip = Join-Path $dist "$name-$version-windows-x64.zip"
Compress-Archive -Path $stage -DestinationPath $zip

# --- Installer ---------------------------------------------------------------------------------
$isccArgs = @("/DAppVersion=$version", "/DStage=$stage", "/Q")
# Every quote inside the sign command is Inno's $q token, never a literal ": Windows PowerShell 5.1
# mangles embedded quotes in native arguments, and the signtool path has spaces. ($f arrives quoted.)
if ($NoSign) { $isccArgs += "/DNoSign" }
else { $isccArgs += "/Srws=`$q$signtool`$q sign /sha1 $Thumbprint /fd sha256 /tr http://ts.ssl.com /td sha256 /d `$qRed Warden Multistream`$q `$f" }
& $iscc @isccArgs "installer\rws-multistream.iss"
if ($LASTEXITCODE -ne 0) { Fail "ISCC failed" }
$setup = Join-Path $dist "$name-$version-windows-x64-setup.exe"
if (!(Test-Path $setup)) { Fail "ISCC reported success but $setup is missing" }
if (!$NoSign) {
  & $signtool verify /pa /q $setup
  if ($LASTEXITCODE -ne 0) { Fail "setup.exe signature does not verify" }
}

# --- Checksums ---------------------------------------------------------------------------------
Get-ChildItem $dist -File | ForEach-Object {
  "{0}  {1}" -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(), $_.Name
} | Set-Content (Join-Path $dist "SHA256SUMS.txt") -Encoding ascii
Remove-Item -Recurse -Force (Join-Path $dist "stage")
Write-Host "[OK] Red Warden Multistream $version packaged$(if ($NoSign) {' (UNSIGNED dry run)'}) -> $dist"
Get-ChildItem $dist | Format-Table Name, Length -AutoSize
