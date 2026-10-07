<#
  Red Warden Pre-Flight integration test: Pre-Flight talking to the REAL Red Warden Multistream plugin.

  Touches none of your real OBS and never streams. It:
    1. uses the same portable OBS copy as the other tests (.testbed\obs),
    2. installs rws-preflight.dll AND (for this run only, removed again at the end) a COPY of
       rws-multistream.dll + its locale from ..\red-warden-multistream (read-only source),
    3. writes a throwaway profile + scene collection "PreflightInteg", obs-websocket server DISABLED
       (the in-process vendor API still works), Multistream's own config with two enabled
       destinations that point at an unreachable local URL (rtmp://127.0.0.1:1/x; nothing is ever
       started, no key is stored), Pre-Flight settings with Multistream + "I record" on, and a
       recording folder = .testbed\rec (Simple output, existing folder),
    4. launches OBS with --portable (RWS_PREFLIGHT_TEST=1, RWS_PREFLIGHT_TEST_HAMMER=1 so a vendor
       call is nearly always in flight when OBS exits), waits until Multistream is Green and
       Recording is Green/Amber,
    5. closes OBS with CloseMainWindow and requires the process to exit within 10 s (else Stop-Process
       on the captured PID and FAIL), then asserts the log: both destination names listed, no
       "detached at exit", probe stopped quickly, no crash.
  Stops ONLY the OBS process it started (captured PID). Result: .testbed\run-integration.txt.
#>
param(
  [string]$Config = "RelWithDebInfo",
  [int]$WaitStatesSeconds = 75
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$bed  = Join-Path $root ".testbed"
$obs  = Join-Path $bed "obs"
$cfg  = Join-Path $obs "config\obs-studio"
$profName = "PreflightInteg"
$prof = Join-Path $cfg "basic\profiles\$profName"
$dll  = Join-Path $root "build_x64\$Config\rws-preflight.dll"
$pdb  = Join-Path $root "build_x64\$Config\rws-preflight.pdb"
$msRoot = Join-Path (Split-Path -Parent $root) "red-warden-multistream"
$msDll  = Join-Path $msRoot "build_x64\$Config\rws-multistream.dll"
$msLoc  = Join-Path $msRoot "data\locale"
$src  = Join-Path $msRoot ".testbed\obs"
$result = Join-Path $bed "run-integration.txt"
$recDir = Join-Path $bed "rec"
New-Item -ItemType Directory -Force $bed | Out-Null
Remove-Item $result -ErrorAction SilentlyContinue

$uStart = "c0000000-0000-0000-0000-000000000001"
$uOther = "c0000000-0000-0000-0000-000000000002"
$uMic   = "c0000000-0000-0000-0000-000000000003"
$uColor = "c0000000-0000-0000-0000-000000000004"

$pbin  = Join-Path $obs "obs-plugins\64bit"
$msData = Join-Path $obs "data\obs-plugins\rws-multistream"

function Cleanup-Multistream {
  Remove-Item (Join-Path $pbin "rws-multistream.*") -Force -ErrorAction SilentlyContinue
  Remove-Item $msData -Recurse -Force -ErrorAction SilentlyContinue
}
function Finish([bool]$ok, [string[]]$lines) {
  Cleanup-Multistream
  $head = if ($ok) { "PASS" } else { "FAIL" }
  (@($head) + $lines) | Set-Content $result
  exit $(if ($ok) { 0 } else { 1 })
}

if (!(Test-Path $dll)) { Finish $false @("rws-preflight.dll not found: $dll") }
if (!(Test-Path $msDll)) { Finish $false @("rws-multistream.dll not found (build Multistream first): $msDll") }

# 1. portable OBS
if (!(Test-Path (Join-Path $obs "bin\64bit\obs64.exe"))) {
  if (!(Test-Path (Join-Path $src "bin\64bit\obs64.exe"))) { Finish $false @("no source portable OBS at $src") }
  robocopy $src $obs /E /MT:16 /NFL /NDL /NJH /NJS /NP | Out-Null
  if ($LASTEXITCODE -ge 8) { Finish $false @("robocopy of portable OBS failed, exit $LASTEXITCODE") }
  New-Item -ItemType File -Force (Join-Path $obs "portable_mode.txt") | Out-Null
}

# 2. install Pre-Flight, and a COPY of Multistream for this run only (removed in Finish / at the end)
$pdat = Join-Path $obs "data\obs-plugins\rws-preflight\locale"
New-Item -ItemType Directory -Force $pdat | Out-Null
Copy-Item $dll $pbin -Force
if (Test-Path $pdb) { Copy-Item $pdb $pbin -Force }
Copy-Item (Join-Path $root "data\locale\en-US.ini") $pdat -Force
Copy-Item $msDll $pbin -Force
New-Item -ItemType Directory -Force (Join-Path $msData "locale") | Out-Null
Copy-Item (Join-Path $msLoc "*") (Join-Path $msData "locale") -Force

# fresh throwaway config
if (Test-Path $cfg) { Remove-Item -Recurse -Force $cfg }
New-Item -ItemType Directory -Force $prof | Out-Null
New-Item -ItemType Directory -Force (Join-Path $cfg "basic\scenes") | Out-Null
New-Item -ItemType Directory -Force $recDir | Out-Null
$recIni = $recDir.Replace('\', '/')

$ini = @"
[General]
FirstRun=false
LastVersion=999999999
EnableAutoUpdates=false
[Basic]
Profile=$profName
ProfileDir=$profName
SceneCollection=$profName
SceneCollectionFile=$profName
[BasicWindow]
ConfirmOnExit=false
WarnBeforeStartingStream=false
WarnBeforeStoppingStream=false
RecordWhenStreaming=true
"@
Set-Content (Join-Path $cfg "global.ini") $ini -Encoding UTF8
Set-Content (Join-Path $cfg "user.ini") $ini -Encoding UTF8

# obs-websocket server DISABLED (the in-process vendor API is what Pre-Flight uses)
$wsDir = Join-Path $cfg "plugin_config\obs-websocket"
New-Item -ItemType Directory -Force $wsDir | Out-Null
Set-Content (Join-Path $wsDir "config.json") '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":false,"server_password":"","server_port":4466}' -Encoding UTF8

Set-Content (Join-Path $prof "basic.ini") @"
[General]
Name=$profName
[Output]
Mode=Simple
[SimpleOutput]
FilePath=$recIni
[Video]
BaseCX=1280
BaseCY=720
OutputCX=1280
OutputCY=720
FPSType=0
FPSCommon=30
"@ -Encoding UTF8

# Multistream's own config: two enabled destinations at an unreachable local URL. Never started.
Set-Content (Join-Path $prof "rws-multistream.json") @"
{"version":2,"destinations":[
 {"id":"00000000-beac-0000-0000-0000000000c1","name":"Alpha test","platform":"custom","server":"rtmp://127.0.0.1:1/x","enabled":true,"encoder":{"shared":true,"encoder_id":"","video_bitrate":2500,"audio_bitrate":0,"audio_track":-1,"width":0,"height":0},"hotkey":[]},
 {"id":"00000000-beac-0000-0000-0000000000c2","name":"Bravo test","platform":"custom","server":"rtmp://127.0.0.1:1/x","enabled":true,"encoder":{"shared":true,"encoder_id":"","video_bitrate":2500,"audio_bitrate":0,"audio_track":-1,"width":0,"height":0},"hotkey":[]}
]}
"@ -Encoding UTF8
$msCfg = Join-Path $cfg "plugin_config\rws-multistream"
New-Item -ItemType Directory -Force $msCfg | Out-Null
Set-Content (Join-Path $msCfg "settings.json") '{"upload_mbps":0.0,"upload_source":"","upload_when":"","check_updates":false}' -Encoding UTF8

# Same fixture as test-state.ps1 (the mic must be a scene item, see the note there)
. (Join-Path $PSScriptRoot "testbed-common.ps1")
$wav = Join-Path $bed "silent.wav"
New-SilentWav $wav 1.0
$wavJson = $wav.Replace('\', '/')
$scenes = @"
{"name":"$profName","current_scene":"Start","current_program_scene":"Start",
 "scene_order":[{"name":"Start"},{"name":"Other"}],
 "sources":[
  {"id":"color_source_v3","versioned_id":"color_source_v3","name":"Ember","uuid":"$uColor","settings":{"color":4282038770,"width":1280,"height":720}},
  {"id":"ffmpeg_source","versioned_id":"ffmpeg_source","name":"Test Mic","uuid":"$uMic","muted":true,"volume":1.0,"mixers":255,"enabled":true,"settings":{"is_local_file":true,"local_file":"$wavJson","looping":true,"close_when_inactive":false}},
  {"id":"scene","versioned_id":"scene","name":"Start","uuid":"$uStart","settings":{"id_counter":2,"items":[{"name":"Ember","source_uuid":"$uColor","id":1,"visible":true},{"name":"Test Mic","source_uuid":"$uMic","id":2,"visible":true}]}},
  {"id":"scene","versioned_id":"scene","name":"Other","uuid":"$uOther","settings":{"id_counter":0,"items":[]}}
 ]}
"@
Set-Content (Join-Path $cfg "basic\scenes\$profName.json") $scenes -Encoding UTF8

# Pre-Flight settings: Multistream only, "I record" on (folder comes from the profile: .testbed\rec)
$plugCfg = Join-Path $cfg "plugin_config\rws-preflight"
New-Item -ItemType Directory -Force $plugCfg | Out-Null
$settings = '{"setup_done":true,"use_main_stream":false,"use_multistream":true,"mic_uuid":"' + $uMic + '","start_scene_uuid":"' + $uStart + '","record_intent":true,"check_updates":false}'
Set-Content (Join-Path $plugCfg "settings.json") $settings -Encoding UTF8

function Read-Log([string]$logDir) {
  $l = Get-ChildItem $logDir -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
  if (!$l) { return @("", $null) }
  $fs = [IO.File]::Open($l.FullName, 'Open', 'Read', 'ReadWrite')
  try { $sr = New-Object IO.StreamReader($fs); $t = $sr.ReadToEnd() } finally { $fs.Close() }
  return @($t, $l)
}

# 4. run OBS
$env:RWS_PREFLIGHT_TEST = "1"
$env:RWS_PREFLIGHT_TEST_HAMMER = "1"
$exeDir = Join-Path $obs "bin\64bit"
$proc = Start-Process (Join-Path $exeDir "obs64.exe") -WorkingDirectory $exeDir -PassThru -ArgumentList @(
  "--portable", "--multi", "--profile", $profName, "--collection", $profName,
  "--disable-updater", "--disable-missing-files-check")
$obsPid = $proc.Id
$logDir = Join-Path $cfg "logs"
$failure = $null
$statesOk = $false
$exitedInTime = $false
$exitSeconds = -1.0
$exitCode = $null
$killed = $false
$text = ""
$log = $null
try {
  $deadline = (Get-Date).AddSeconds($WaitStatesSeconds)
  while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 3
    if ($proc.HasExited) { $failure = "OBS exited on its own before the checks (exit code $($proc.ExitCode))"; break }
    $r = Read-Log $logDir
    $t = [string]$r[0]
    if (($t -match 'state Multistream=Green') -and ($t -match 'state Recording=(Green|Amber)')) { $statesOk = $true; break }
  }
  Start-Sleep -Seconds 2  # let the hammering vendor calls run a moment longer
} catch {
  $failure = "$_"
} finally {
  # 5. close the normal way and time it. Only the captured PID is ever stopped.
  try {
    # $proc is the object Start-Process returned (it keeps the exit code); it is the captured PID.
    $proc.Refresh()
    if (-not $proc.HasExited) {
      $sw = [Diagnostics.Stopwatch]::StartNew()
      [void]$proc.CloseMainWindow()
      $exitedInTime = $proc.WaitForExit(10000)
      $exitSeconds = [math]::Round($sw.Elapsed.TotalSeconds, 1)
      if (!$exitedInTime) {
        $killed = $true
        Stop-Process -Id $obsPid -Force -ErrorAction SilentlyContinue
      } else {
        $exitCode = $proc.ExitCode
      }
    }
  } catch { if (!$failure) { $failure = "closing OBS: $_" } }
  Remove-Item Env:\RWS_PREFLIGHT_TEST -ErrorAction SilentlyContinue
  Remove-Item Env:\RWS_PREFLIGHT_TEST_HAMMER -ErrorAction SilentlyContinue
}
Start-Sleep -Seconds 1
$r = Read-Log $logDir
$text = [string]$r[0]
$log = $r[1]

$all = @($text -split "`r?`n")
$plug = @($all | Where-Object { $_ -match '\[rws-preflight\]' })
function Last-State([string]$name) {
  $m = @($plug | Where-Object { $_ -match ("state " + $name + "=") })
  if ($m.Count -eq 0) { return $null }
  if ($m[-1] -match ("state " + $name + "=(\w+)")) { return $Matches[1] }
  return $null
}
$multi = Last-State "Multistream"
$rec   = Last-State "Recording"
$msgLine = @($plug | Where-Object { $_ -match 'message Multistream: ' })
$msgText = if ($msgLine.Count -gt 0) { $msgLine[-1] } else { "" }
$namesOk = ($msgText -match 'Alpha test') -and ($msgText -match 'Bravo test')
$detached = @($plug | Where-Object { $_ -match 'detached at exit' }).Count
$stopLine = @($plug | Where-Object { $_ -match 'multistream probe stopped in (\d+) ms' })
$stopMs = -1
if ($stopLine.Count -gt 0 -and $stopLine[-1] -match 'stopped in (\d+) ms') { $stopMs = [int]$Matches[1] }
$crashDir = Join-Path $cfg "crashes"
$crashFiles = @(Get-ChildItem $crashDir -ErrorAction SilentlyContinue)
$crashLog = @($all | Where-Object { $_ -match '(?i)unhandled exception|crash(ed)? ' -and $_ -notmatch '(?i)crash handler|crash_handler' })
$msLoaded = @($all | Where-Object { $_ -match 'rws-multistream' }).Count -gt 0
$leaks = @($plug | Where-Object { $_ -match '(?i)key' -or $_ -match 'rtmp://' })

$checks = @()
$checks += "Multistream plugin loaded (log mentions rws-multistream)=$msLoaded"
$checks += "Multistream=$multi (want Green)"
$checks += "Multistream message lists 'Alpha test' and 'Bravo test'=$namesOk : $msgText"
$checks += "Recording=$rec (want Green or Amber, not Unknown)"
$checks += "states reached within ${WaitStatesSeconds}s=$statesOk"
$checks += "OBS exited within 10 s of CloseMainWindow=$exitedInTime (took $exitSeconds s; killed=$killed)"
$checks += "OBS exit code=$exitCode (want 0)"
$checks += "'detached at exit' log lines=$detached (want 0)"
$checks += "multistream probe stop time=$stopMs ms (want 0..1500)"
$checks += "crash files=$($crashFiles.Count), crash log lines=$($crashLog.Count) (want 0 / 0)"
$checks += "key/rtmp lines from rws-preflight=$($leaks.Count) (want 0)"

$ok = $msLoaded -and ($multi -eq "Green") -and $namesOk -and (($rec -eq "Green") -or ($rec -eq "Amber")) -and $statesOk -and
      $exitedInTime -and (-not $killed) -and ($exitCode -eq 0) -and ($detached -eq 0) -and ($stopMs -ge 0) -and ($stopMs -le 1500) -and
      ($crashFiles.Count -eq 0) -and ($crashLog.Count -eq 0) -and ($leaks.Count -eq 0) -and (-not $failure)

$lines = @("pid=$obsPid", "log=$(if ($log) { $log.FullName } else { 'NONE' })")
if ($failure) { $lines += "error during check: $failure" }
$lines += $checks
$lines += "--- rws-preflight log lines ---"
$lines += $plug
Finish $ok $lines
