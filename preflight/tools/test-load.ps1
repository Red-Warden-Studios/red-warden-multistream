<#
  Red Warden Pre-Flight load test.

  Touches none of your real OBS. It:
    1. makes a portable OBS copy in .testbed\obs (first run only) by copying the
       Multistream testbed's portable OBS,
    2. installs the freshly built rws-preflight.dll (+ .pdb) and locale into it,
    3. launches that portable OBS with a throwaway profile/collection "PreflightTest",
    4. waits, reads the newest log, checks for the "[rws-preflight] loaded" line and the dock id,
    5. stops ONLY the OBS process it started (captured PID).
  Result: .testbed\run-load.txt (PASS / FAIL), exit code 0 / 1.
#>
param(
  [string]$Config = "RelWithDebInfo",
  [int]$Seconds = 20
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$bed  = Join-Path $root ".testbed"
$obs  = Join-Path $bed "obs"
$cfg  = Join-Path $obs "config\obs-studio"
$prof = Join-Path $cfg "basic\profiles\PreflightTest"
$dll  = Join-Path $root "build_x64\$Config\rws-preflight.dll"
$pdb  = Join-Path $root "build_x64\$Config\rws-preflight.pdb"
$src  = Join-Path (Split-Path -Parent $root) "red-warden-multistream\.testbed\obs"
$result = Join-Path $bed "run-load.txt"
New-Item -ItemType Directory -Force $bed | Out-Null
Remove-Item $result -ErrorAction SilentlyContinue

function Finish([bool]$ok, [string[]]$lines) {
  $head = if ($ok) { "PASS" } else { "FAIL" }
  (@($head) + $lines) | Set-Content $result
  exit $(if ($ok) { 0 } else { 1 })
}

if (!(Test-Path $dll)) { Finish $false @("rws-preflight.dll not found: $dll") }

# 1. portable OBS (first run only)
if (!(Test-Path (Join-Path $obs "bin\64bit\obs64.exe"))) {
  if (!(Test-Path (Join-Path $src "bin\64bit\obs64.exe"))) { Finish $false @("no source portable OBS at $src") }
  robocopy $src $obs /E /MT:16 /NFL /NDL /NJH /NJS /NP | Out-Null
  if ($LASTEXITCODE -ge 8) { Finish $false @("robocopy of portable OBS failed, exit $LASTEXITCODE") }
  New-Item -ItemType File -Force (Join-Path $obs "portable_mode.txt") | Out-Null
}

# 2. install plugin (and make sure Multistream is not in this copy)
$pbin = Join-Path $obs "obs-plugins\64bit"
$pdat = Join-Path $obs "data\obs-plugins\rws-preflight\locale"
Remove-Item (Join-Path $pbin "rws-multistream.*") -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $pdat | Out-Null
Copy-Item $dll $pbin -Force
if (Test-Path $pdb) { Copy-Item $pdb $pbin -Force }
Copy-Item (Join-Path $root "data\locale\en-US.ini") $pdat -Force

# fresh throwaway config every run
if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
New-Item -ItemType Directory -Force $prof | Out-Null
New-Item -ItemType Directory -Force (Join-Path $cfg "basic\scenes") | Out-Null

$ini = @"
[General]
FirstRun=false
LastVersion=999999999
EnableAutoUpdates=false
[Basic]
Profile=PreflightTest
ProfileDir=PreflightTest
SceneCollection=PreflightTest
SceneCollectionFile=PreflightTest
[BasicWindow]
ConfirmOnExit=false
WarnBeforeStartingStream=false
WarnBeforeStoppingStream=false
"@
Set-Content (Join-Path $cfg "global.ini") $ini -Encoding UTF8
Set-Content (Join-Path $cfg "user.ini") $ini -Encoding UTF8

# obs-websocket off in the test copy (never clash with a real OBS on the usual port)
$wsDir = Join-Path $cfg "plugin_config\obs-websocket"
New-Item -ItemType Directory -Force $wsDir | Out-Null
Set-Content (Join-Path $wsDir "config.json") '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":false,"server_password":"","server_port":4466}' -Encoding UTF8

Set-Content (Join-Path $prof "basic.ini") @"
[General]
Name=PreflightTest
[Output]
Mode=Advanced
[AdvOut]
Encoder=obs_x264
TrackIndex=1
Track1Bitrate=256
[Video]
BaseCX=1280
BaseCY=720
OutputCX=1280
OutputCY=720
FPSType=0
FPSCommon=30
"@ -Encoding UTF8

Set-Content (Join-Path $cfg "basic\scenes\PreflightTest.json") @'
{"name":"PreflightTest","current_scene":"Scene","current_program_scene":"Scene",
 "scene_order":[{"name":"Scene"}],
 "sources":[
  {"id":"color_source_v3","versioned_id":"color_source_v3","name":"Ember","uuid":"b0000000-0000-0000-0000-000000000002","settings":{"color":4282038770,"width":1280,"height":720}},
  {"id":"scene","versioned_id":"scene","name":"Scene","uuid":"b0000000-0000-0000-0000-000000000001","settings":{"id_counter":1,"items":[{"name":"Ember","source_uuid":"b0000000-0000-0000-0000-000000000002","id":1,"visible":true}]}}
 ]}
'@ -Encoding UTF8

# 3. run OBS (test env var keeps the update check off the network)
$env:RWS_PREFLIGHT_TEST = "1"
$exeDir = Join-Path $obs "bin\64bit"
$proc = Start-Process (Join-Path $exeDir "obs64.exe") -WorkingDirectory $exeDir -PassThru -ArgumentList @(
  "--portable", "--multi", "--profile", "PreflightTest", "--collection", "PreflightTest",
  "--disable-updater", "--disable-missing-files-check")
$obsPid = $proc.Id
$text = ""
$log = $null
$alive = $false
$failure = $null
try {
  Start-Sleep -Seconds $Seconds

  # 4. read the newest log (OBS holds it open, so open with sharing)
  $logDir = Join-Path $cfg "logs"
  $log = Get-ChildItem $logDir -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
  if ($log) {
    $fs = [IO.File]::Open($log.FullName, 'Open', 'Read', 'ReadWrite')
    try {
      $sr = New-Object IO.StreamReader($fs)
      $text = $sr.ReadToEnd()
    } finally {
      $fs.Close()
    }
  }
  $alive = -not $proc.HasExited
} catch {
  $failure = "$_"
} finally {
  # 5. stop ONLY the captured PID, even if reading the log threw (never by image name)
  try {
    $p = Get-Process -Id $obsPid -ErrorAction SilentlyContinue
    if ($p -and -not $p.HasExited) {
      [void]$p.CloseMainWindow()
      if (!$p.WaitForExit(15000)) { Stop-Process -Id $obsPid -Force -ErrorAction SilentlyContinue }
    }
  } catch { }
  Remove-Item Env:\RWS_PREFLIGHT_TEST -ErrorAction SilentlyContinue
}

$lines = @("pid=$obsPid still_running_at_check=$alive", "log=$(if ($log) { $log.FullName } else { 'NONE' })")
if ($failure) { $lines += "error during check: $failure" }
$loaded = $text -match '\[rws-preflight\] loaded version'
$dock = $text -match 'rws-preflight-dock'
$lines += "loaded_line=$loaded dock_id=$dock"
$lines += "--- plugin lines ---"
$lines += @($text -split "`r?`n" | Where-Object { $_ -match 'rws-preflight|Red Warden Pre' })
Finish ($loaded -and $dock -and $alive -and -not $failure) $lines