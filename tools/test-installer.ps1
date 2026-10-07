<#
  Installer test for the Red Warden Stream Kit. package.ps1 runs it as a required release gate (signed and
  -NoSign) after staging the DLLs; it can also be run by hand once staging\ exists:

    powershell -ExecutionPolicy Bypass -File tools\test-installer.ps1

  Compiles a TestRoot build (ISCC /DTestRoot=<kit>\.testbed\root: installs under that folder instead of
  C:\ProgramData, no admin, its own AppId, a TEST-ONLY legacy AppId, never signs) and checks, silently:
    - static: the release .iss uses the same legacy AppId as Multistream's own installer
    - legacy removal fails closed: a stale legacy entry that cannot be uninstalled aborts, installs nothing
    - OBS-running block: a PORTABLE OBS (red-warden-preflight\.testbed\obs, captured PID, stopped in finally)
      makes the installer refuse with a non-zero exit and install nothing
    - component sets: preflight only; both; multistream only removes Pre-Flight's files but not a user's
      own file in its folder; folders are removed when empty
    - uninstall leaves no files
    - legacy removal: a fake "legacy Multistream" installer (test-only AppId) is uninstalled by the Kit
      installer, its registry entry gone, and the Kit owns the files afterwards
  Writes PASS/FAIL lines and a final RESULT line to .testbed\run-installer.txt.
  It NEVER runs a non-TestRoot install and never touches a real OBS (it refuses to start if one is running).
#>
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$bed = Join-Path $root ".testbed"
New-Item -ItemType Directory -Force $bed | Out-Null
$out = Join-Path $bed "run-installer.txt"
Remove-Item $out -ErrorAction SilentlyContinue
$script:fails = 0
function Say($s) { Write-Host $s; Add-Content -Path $out -Value $s -Encoding ASCII }
function Check($cond, $msg) {
  if ($cond) { Say "PASS: $msg" } else { Say "FAIL: $msg"; $script:fails++ }
}

$projects = Split-Path -Parent $root
$testRoot = Join-Path $bed "root"
$stage = Join-Path $root "staging"
$iscc = Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"
$version = "1.0.0"
$testSetup = Join-Path $bed "RedWardenStreamKit-$version-TestRoot-Setup.exe"
$tmp = Join-Path $env:TEMP ("rws-kit-test-" + [guid]::NewGuid().ToString("N"))   # unique per run; only this folder is deleted
$fakeSetup = Join-Path $tmp "fake-legacy-setup.exe"
$uninstallBase = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall"
$legacyKey = "$uninstallBase\RWS-KIT-TESTLEGACY-DA4CA68C_is1"
$kitKey = "$uninstallBase\RWS-KIT-TESTROOT-035D8EC0_is1"
$obsExeDir = Join-Path $projects "red-warden-preflight\.testbed\obs\bin\64bit"

$msDll = Join-Path $testRoot "rws-multistream\bin\64bit\rws-multistream.dll"
$pfDll = Join-Path $testRoot "rws-preflight\bin\64bit\rws-preflight.dll"
$msLoc = Join-Path $testRoot "rws-multistream\data\locale\en-US.ini"
$pfLoc = Join-Path $testRoot "rws-preflight\data\locale\en-US.ini"
$uninst = Join-Path $testRoot "_kit\unins000.exe"

function ResetRoot {
  # Scratch only: refuse to delete anything that is not inside this project's .testbed.
  if (!$testRoot.StartsWith($bed, [StringComparison]::OrdinalIgnoreCase)) { throw "test root outside .testbed" }
  if ($testRoot -match 'ProgramData|Program Files') { throw "test root looks like a real install location" }
  if (Test-Path $testRoot) { Remove-Item -Recurse -Force $testRoot }
  New-Item -ItemType Directory -Force $testRoot | Out-Null
  foreach ($k in $legacyKey, $kitKey) { if (Test-Path $k) { Remove-Item -Recurse -Force $k } }
}
function RunProc($exe, $argList, $label, [switch]$ExpectFailure) {
  $p = Start-Process -FilePath $exe -ArgumentList $argList -PassThru -WindowStyle Hidden
  if (!$p.WaitForExit(120000)) {
    Stop-Process -Id $p.Id -Force   # captured PID only
    Check $false "$label finished within 120 s"
    return -1
  }
  if ($ExpectFailure) { Check ($p.ExitCode -ne 0) "$label refused with a non-zero exit code (got $($p.ExitCode))" }
  else { Check ($p.ExitCode -eq 0) "$label exit code 0 (got $($p.ExitCode))" }
  return $p.ExitCode
}
function Install($components, $label, [switch]$ExpectFailure) {
  $log = Join-Path $bed "install-$label.log"
  $a = @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/COMPONENTS=`"$components`"", "/LOG=`"$log`"")
  if ($ExpectFailure) { return RunProc $testSetup $a "install ($components)" -ExpectFailure }
  return RunProc $testSetup $a "install ($components)"
}
function PluginFiles { @(Get-ChildItem $testRoot -Recurse -File -ErrorAction SilentlyContinue) }
function Sha($p) { (Get-FileHash $p -Algorithm SHA256).Hash }
function KitUninstall($label) {
  RunProc $uninst @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART") "uninstall ($label)" | Out-Null
  for ($i = 0; $i -lt 150 -and (Test-Path $uninst); $i++) { Start-Sleep -Milliseconds 200 }   # second phase deletes itself
  $left = @(PluginFiles)
  Check ($left.Count -eq 0) "uninstall ($label): TestRoot holds no files (left: $($left.Count) $(($left | Select-Object -First 5 | ForEach-Object { $_.FullName.Substring($testRoot.Length) }) -join ', '))"
}

$obsProc = $null
try {
  # --- Safety and preconditions ---
  $running = @(Get-Process -Name obs64 -ErrorAction SilentlyContinue)
  if ($running.Count -gt 0) { throw "an OBS (obs64.exe) is already running; this test will not touch it. Close it and re-run" }
  Check (Test-Path (Join-Path $stage "rws-multistream\bin\64bit\rws-multistream.dll")) "staging holds the Multistream DLL"
  Check (Test-Path (Join-Path $stage "rws-preflight\bin\64bit\rws-preflight.dll")) "staging holds the Pre-Flight DLL"
  Check (Test-Path (Join-Path $obsExeDir "obs64.exe")) "portable test OBS exists ($obsExeDir)"
  if ($script:fails -gt 0) { throw "preconditions failed (run tools\package.ps1 first)" }

  # --- Static: the release script uses Multistream's real legacy AppId ---
  $iss = Get-Content (Join-Path $root "installer\stream-kit.iss") -Raw
  $msIss = Get-Content (Join-Path $projects "red-warden-multistream\installer\rws-multistream.iss") -Raw
  $realId = if ($msIss -match 'AppId=\{\{([0-9A-Fa-f-]+)\}') { $Matches[1] } else { "" }
  Check ($realId.Length -gt 30) "found Multistream's own AppId ($realId)"
  $branches = $iss -split '(?m)^#else\s*$'
  Check ($branches[1] -match [regex]::Escape('#define LegacyAppId "{' + $realId + '}"')) "release build: LegacyAppId is Multistream's real AppId"
  Check ($branches[0] -match 'LegacyAppId "RWS-KIT-TESTLEGACY') "TestRoot build: LegacyAppId is the test-only id"

  # --- Compile the TestRoot installer and the fake legacy installer ---
  ResetRoot
  Remove-Item $testSetup -ErrorAction SilentlyContinue
  & $iscc "/DAppVersion=$version" "/DStage=$stage" "/DNoSign" "/DTestRoot=$testRoot" "/Q" "installer\stream-kit.iss"
  Check ($LASTEXITCODE -eq 0 -and (Test-Path $testSetup)) "ISCC compiled the TestRoot installer"
  if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
  New-Item -ItemType Directory -Force $tmp | Out-Null
  & $iscc "/DTestRoot=$testRoot" "/O$tmp" "/Q" "tools\test-fake-legacy.iss"
  Check ($LASTEXITCODE -eq 0 -and (Test-Path $fakeSetup)) "ISCC compiled the fake legacy installer (into %TEMP%)"
  if (!(Test-Path $testSetup) -or !(Test-Path $fakeSetup)) { throw "could not compile the installers" }

  # --- 1. Stale legacy entry that cannot be uninstalled: abort, install nothing ---
  ResetRoot
  New-Item -Path $legacyKey -Force | Out-Null
  New-ItemProperty -Path $legacyKey -Name UninstallString -Value ('"' + (Join-Path $tmp "no-such-unins000.exe") + '"') | Out-Null
  Install "multistream,preflight" "legacyfail" -ExpectFailure | Out-Null
  Check ((PluginFiles).Count -eq 0) "legacy removal failed: nothing was installed"
  Check (Test-Path $legacyKey) "legacy removal failed: the legacy entry is untouched"
  Remove-Item -Recurse -Force $legacyKey

  # --- 2. OBS running (portable copy, captured PID): installer refuses ---
  ResetRoot
  $env:RWS_PREFLIGHT_TEST = "1"
  $obsProc = Start-Process (Join-Path $obsExeDir "obs64.exe") -WorkingDirectory $obsExeDir -PassThru -ArgumentList @(
    "--portable", "--multi", "--profile", "PreflightTest", "--collection", "PreflightTest",
    "--disable-updater", "--disable-missing-files-check")
  Start-Sleep -Seconds 6
  Check (!$obsProc.HasExited) "portable test OBS is running (PID $($obsProc.Id))"
  Install "multistream,preflight" "obsrunning" -ExpectFailure | Out-Null
  Check ((PluginFiles).Count -eq 0) "OBS running: nothing was installed"
  Stop-Process -Id $obsProc.Id -Force -ErrorAction SilentlyContinue   # ONLY the captured PID
  Check ($obsProc.WaitForExit(20000)) "portable test OBS stopped"
  $obsProc = $null

  # --- 3. Components ---
  ResetRoot
  Install "preflight" "1" | Out-Null
  Check (Test-Path $pfDll) "preflight only: rws-preflight.dll installed"
  Check (Test-Path $pfLoc) "preflight only: rws-preflight locale installed"
  Check (!(Test-Path (Join-Path $testRoot "rws-multistream"))) "preflight only: no rws-multistream folder"
  Check ((Sha $pfDll) -eq (Sha (Join-Path $stage "rws-preflight\bin\64bit\rws-preflight.dll"))) "preflight only: installed DLL is the staged DLL"
  Check (Test-Path $uninst) "preflight only: uninstaller present"

  Install "multistream,preflight" "2" | Out-Null
  Check ((Test-Path $msDll) -and (Test-Path $msLoc)) "both: Multistream DLL + locale installed"
  Check ((Test-Path $pfDll) -and (Test-Path $pfLoc)) "both: Pre-Flight DLL + locale installed"
  Check ((Sha $msDll) -eq (Sha (Join-Path $stage "rws-multistream\bin\64bit\rws-multistream.dll"))) "both: Multistream DLL is the staged DLL"

  # A user's own file in a plugin folder must survive deselecting that plugin.
  $userFile = Join-Path $testRoot "rws-preflight\my-own-notes.txt"
  Set-Content -Path $userFile -Value "mine" -Encoding ASCII
  Install "multistream" "3" | Out-Null
  Check (Test-Path $msDll) "multistream only: Multistream DLL still installed"
  Check (!(Test-Path $pfDll) -and !(Test-Path $pfLoc)) "multistream only: Pre-Flight's DLL and locale removed"
  Check ((Test-Path $userFile) -and ((Get-Content $userFile -Raw).Trim() -eq "mine")) "multistream only: the user's own file in rws-preflight\ survived"
  Check (!(Test-Path (Join-Path $testRoot "rws-preflight\bin")) -and !(Test-Path (Join-Path $testRoot "rws-preflight\data"))) "multistream only: Pre-Flight's now-empty bin and data folders removed"
  Remove-Item $userFile -Force

  Install "preflight" "4" | Out-Null
  Check (Test-Path $pfDll) "preflight only (again): Pre-Flight installed"
  Check (!(Test-Path (Join-Path $testRoot "rws-multistream"))) "preflight only (again): Multistream's folder removed when nothing else was in it"

  KitUninstall "components"
  Check (!(Test-Path (Join-Path $testRoot "rws-preflight"))) "uninstall: rws-preflight folder gone"

  # --- 4. Legacy removal: a fake legacy Multistream installed first, then the Kit ---
  ResetRoot
  RunProc $fakeSetup @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART") "fake legacy install" | Out-Null
  Check (Test-Path $legacyKey) "legacy: its uninstall entry is registered"
  Check (Test-Path (Join-Path $testRoot "rws-multistream\unins000.exe")) "legacy: its uninstaller is in the plugin folder"
  Check (Test-Path $msDll) "legacy: its (dummy) DLL is installed"
  Install "multistream,preflight" "5" | Out-Null
  Check (!(Test-Path $legacyKey)) "legacy: its uninstall entry is gone after the Kit install"
  Check (!(Test-Path (Join-Path $testRoot "rws-multistream\unins000.exe"))) "legacy: its uninstaller is gone"
  Check ((Test-Path $msDll) -and ((Sha $msDll) -eq (Sha (Join-Path $stage "rws-multistream\bin\64bit\rws-multistream.dll")))) "legacy: the Kit now owns the real Multistream DLL"
  Check (Test-Path $pfDll) "legacy: Pre-Flight installed too"
  Check (Test-Path $uninst) "legacy: the Kit's own uninstaller is present"
  KitUninstall "after legacy"
} catch {
  Say "FAIL: $($_.Exception.Message)"
  $script:fails++
} finally {
  if ($obsProc -and !$obsProc.HasExited) { Stop-Process -Id $obsProc.Id -Force -ErrorAction SilentlyContinue }   # ONLY the captured PID
  Remove-Item Env:\RWS_PREFLIGHT_TEST -ErrorAction SilentlyContinue
  foreach ($k in $legacyKey, $kitKey) { if (Test-Path $k) { Remove-Item -Recurse -Force $k -ErrorAction SilentlyContinue } }
  if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue }
}
Say ($(if ($script:fails -eq 0) { "RESULT: PASS" } else { "RESULT: FAIL ($($script:fails) failure(s))" }))
if ($script:fails -ne 0) { exit 1 }
