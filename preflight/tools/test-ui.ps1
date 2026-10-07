<#
  Red Warden Pre-Flight UI test (dock UI + settings dialog).

  Same portable OBS copy and throwaway scene collection / settings as test-state.ps1 (.testbed\obs),
  plus two custom checklist items. Sets RWS_PREFLIGHT_TEST=1,
  RWS_PREFLIGHT_TEST_SHOT=.testbed\ui-dock.png and RWS_PREFLIGHT_TEST_SETTINGS_SHOT=.testbed\ui-settings.png,
  launches OBS with --portable, waits ~50 s, then asserts the log lines
  "[rws-preflight] ui-shot saved" and "[rws-preflight] settings-shot saved" and that both PNGs
  exist and are larger than 2 KB. Stops ONLY the OBS process it started (captured PID).
  Result: .testbed\run-ui.txt (PASS / FAIL + details), exit code 0 / 1.
#>
param(
  [string]$Config = "RelWithDebInfo",
  [int]$Seconds = 50
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$bed  = Join-Path $root ".testbed"
$obs  = Join-Path $bed "obs"
$cfg  = Join-Path $obs "config\obs-studio"
$prof = Join-Path $cfg "basic\profiles\PreflightState"
$dll  = Join-Path $root "build_x64\$Config\rws-preflight.dll"
$pdb  = Join-Path $root "build_x64\$Config\rws-preflight.pdb"
$src  = Join-Path (Split-Path -Parent $root) "red-warden-multistream\.testbed\obs"
$result = Join-Path $bed "run-ui.txt"
$shotDock = Join-Path $bed "ui-dock.png"
$shotSet  = Join-Path $bed "ui-settings.png"
New-Item -ItemType Directory -Force $bed | Out-Null
Remove-Item $result, $shotDock, $shotSet -ErrorAction SilentlyContinue

$uStart = "c0000000-0000-0000-0000-000000000001"
$uOther = "c0000000-0000-0000-0000-000000000002"
$uMic   = "c0000000-0000-0000-0000-000000000003"
$uColor = "c0000000-0000-0000-0000-000000000004"

function Finish([bool]$ok, [string[]]$lines) {
  $head = if ($ok) { "PASS" } else { "FAIL" }
  (@($head) + $lines) | Set-Content $result
  exit $(if ($ok) { 0 } else { 1 })
}

if (!(Test-Path $dll)) { Finish $false @("rws-preflight.dll not found: $dll") }

if (!(Test-Path (Join-Path $obs "bin\64bit\obs64.exe"))) {
  if (!(Test-Path (Join-Path $src "bin\64bit\obs64.exe"))) { Finish $false @("no source portable OBS at $src") }
  robocopy $src $obs /E /MT:16 /NFL /NDL /NJH /NJS /NP | Out-Null
  if ($LASTEXITCODE -ge 8) { Finish $false @("robocopy of portable OBS failed, exit $LASTEXITCODE") }
  New-Item -ItemType File -Force (Join-Path $obs "portable_mode.txt") | Out-Null
}

$pbin = Join-Path $obs "obs-plugins\64bit"
$pdat = Join-Path $obs "data\obs-plugins\rws-preflight\locale"
Remove-Item (Join-Path $pbin "rws-multistream.*") -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $pdat | Out-Null
Copy-Item $dll $pbin -Force
if (Test-Path $pdb) { Copy-Item $pdb $pbin -Force }
Copy-Item (Join-Path $root "data\locale\en-US.ini") $pdat -Force

if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
New-Item -ItemType Directory -Force $prof | Out-Null
New-Item -ItemType Directory -Force (Join-Path $cfg "basic\scenes") | Out-Null

$ini = @"
[General]
FirstRun=false
LastVersion=999999999
EnableAutoUpdates=false
[Basic]
Profile=PreflightState
ProfileDir=PreflightState
SceneCollection=PreflightState
SceneCollectionFile=PreflightState
[BasicWindow]
ConfirmOnExit=false
WarnBeforeStartingStream=false
WarnBeforeStoppingStream=false
"@
Set-Content (Join-Path $cfg "global.ini") $ini -Encoding UTF8
Set-Content (Join-Path $cfg "user.ini") $ini -Encoding UTF8

$wsDir = Join-Path $cfg "plugin_config\obs-websocket"
New-Item -ItemType Directory -Force $wsDir | Out-Null
Set-Content (Join-Path $wsDir "config.json") '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":false,"server_password":"","server_port":4466}' -Encoding UTF8

Set-Content (Join-Path $prof "basic.ini") @"
[General]
Name=PreflightState
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

# Same fixture as test-state.ps1 (see the note there): the mic must be a scene item or OBS drops it.
. (Join-Path $PSScriptRoot "testbed-common.ps1")
$wav = Join-Path $bed "silent.wav"
New-SilentWav $wav 1.0
$wavJson = $wav.Replace('\', '/')
$scenes = @"
{"name":"PreflightState","current_scene":"Start","current_program_scene":"Start",
 "scene_order":[{"name":"Start"},{"name":"Other"}],
 "sources":[
  {"id":"color_source_v3","versioned_id":"color_source_v3","name":"Ember","uuid":"$uColor","settings":{"color":4282038770,"width":1280,"height":720}},
  {"id":"ffmpeg_source","versioned_id":"ffmpeg_source","name":"Test Mic","uuid":"$uMic","muted":true,"volume":1.0,"mixers":255,"enabled":true,"settings":{"is_local_file":true,"local_file":"$wavJson","looping":true,"close_when_inactive":false}},
  {"id":"scene","versioned_id":"scene","name":"Start","uuid":"$uStart","settings":{"id_counter":2,"items":[{"name":"Ember","source_uuid":"$uColor","id":1,"visible":true},{"name":"Test Mic","source_uuid":"$uMic","id":2,"visible":true}]}},
  {"id":"scene","versioned_id":"scene","name":"Other","uuid":"$uOther","settings":{"id_counter":0,"items":[]}}
 ]}
"@
Set-Content (Join-Path $cfg "basic\scenes\PreflightState.json") $scenes -Encoding UTF8

$plugCfg = Join-Path $cfg "plugin_config\rws-preflight"
New-Item -ItemType Directory -Force $plugCfg | Out-Null
$settings = '{"setup_done":true,"use_main_stream":true,"use_multistream":true,"mic_uuid":"' + $uMic + '","start_scene_uuid":"' + $uStart + '","record_intent":false,"check_updates":false,"custom_items":[{"text":"Close the door"},{"text":"Phone on silent"}]}'
Set-Content (Join-Path $plugCfg "settings.json") $settings -Encoding UTF8

$env:RWS_PREFLIGHT_TEST = "1"
$env:RWS_PREFLIGHT_TEST_SHOT = $shotDock
$env:RWS_PREFLIGHT_TEST_SETTINGS_SHOT = $shotSet
$exeDir = Join-Path $obs "bin\64bit"
$proc = Start-Process (Join-Path $exeDir "obs64.exe") -WorkingDirectory $exeDir -PassThru -ArgumentList @(
  "--portable", "--multi", "--profile", "PreflightState", "--collection", "PreflightState",
  "--disable-updater", "--disable-missing-files-check")
$obsPid = $proc.Id
$text = ""
$log = $null
$alive = $false
$failure = $null
try {
  Start-Sleep -Seconds $Seconds
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
  try {
    $p = Get-Process -Id $obsPid -ErrorAction SilentlyContinue
    if ($p -and -not $p.HasExited) {
      [void]$p.CloseMainWindow()
      if (!$p.WaitForExit(15000)) { Stop-Process -Id $obsPid -Force -ErrorAction SilentlyContinue }
    }
  } catch { }
  Remove-Item Env:\RWS_PREFLIGHT_TEST -ErrorAction SilentlyContinue
  Remove-Item Env:\RWS_PREFLIGHT_TEST_SHOT -ErrorAction SilentlyContinue
  Remove-Item Env:\RWS_PREFLIGHT_TEST_SETTINGS_SHOT -ErrorAction SilentlyContinue
}

$all = @($text -split "`r?`n")
$plug = @($all | Where-Object { $_ -match '\[rws-preflight\]' })
$dockLine = ($plug | Where-Object { $_ -match 'ui-shot saved' }).Count -gt 0
$setLine  = ($plug | Where-Object { $_ -match 'settings-shot saved' }).Count -gt 0
$dockSize = if (Test-Path $shotDock) { (Get-Item $shotDock).Length } else { 0 }
$setSize  = if (Test-Path $shotSet) { (Get-Item $shotSet).Length } else { 0 }

# PNG width/height live big-endian at byte 16 and 20 of the file.
function Png-Size([string]$p) {
  if (!(Test-Path $p)) { return @(0, 0) }
  $b = [IO.File]::ReadAllBytes($p)
  if ($b.Length -lt 24) { return @(0, 0) }
  $w = ([int]$b[16] * 16777216) + ([int]$b[17] * 65536) + ([int]$b[18] * 256) + [int]$b[19]
  $h = ([int]$b[20] * 16777216) + ([int]$b[21] * 65536) + ([int]$b[22] * 256) + [int]$b[23]
  return @($w, $h)
}
$dockWH = Png-Size $shotDock
# The test hook floats the dock at about 360x720 logical px, so the whole dock is in the picture.
# Display scaling multiplies the grabbed pixels (150% gives 540x1080), so check the logical size
# at any common scale factor rather than raw pixels.
$sizeOk = $false
foreach ($s in 1.0, 1.25, 1.5, 1.75, 2.0) {
  $w = $dockWH[0] / $s; $h = $dockWH[1] / $s
  if (($w -ge 300) -and ($w -le 460) -and ($h -ge 600) -and ($h -le 800)) { $sizeOk = $true }
}

$checks = @(
  "ui-shot saved line=$dockLine",
  "settings-shot saved line=$setLine",
  "ui-dock.png bytes=$dockSize (want > 2048)",
  "ui-dock.png size=$($dockWH[0])x$($dockWH[1]) (want about 360x720 logical at 100-200% scaling)=$sizeOk",
  "ui-settings.png bytes=$setSize (want > 2048)"
)
$ok = $dockLine -and $setLine -and ($dockSize -gt 2048) -and $sizeOk -and ($setSize -gt 2048) -and $alive -and -not $failure

$lines = @("pid=$obsPid still_running_at_check=$alive", "log=$(if ($log) { $log.FullName } else { 'NONE' })")
if ($failure) { $lines += "error during check: $failure" }
$lines += $checks
$lines += "--- rws-preflight log lines ---"
$lines += $plug
Finish $ok $lines
