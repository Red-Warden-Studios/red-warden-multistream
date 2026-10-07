<#
  Red Warden Multistream showcase run: website screenshots, not a test.

  Same isolated portable OBS + local ffmpeg receivers as test-resilience.ps1, but with
  realistic destination names and Qt rendering at 2x so the dock snapshots are crisp.
    -Scenario live     4 platforms live, Kick drops mid-stream and comes back by itself
    -Scenario setup    no stream; real-looking ingest URLs for the edit-dialog shots (never connects)
    -Scenario trouble  small upload (warning), an unreachable server (protected retries),
                       a broken URL (never retried), Facebook audio advice
  Snapshots land in .testbed\showcase\<scenario>\.

  powershell -ExecutionPolicy Bypass -File tools\showcase.ps1 -Scenario live
#>
param([ValidateSet("live", "trouble", "setup")][string]$Scenario = "live", [string]$Config = "RelWithDebInfo")
$ErrorActionPreference = "Stop"
$root   = Split-Path -Parent $PSScriptRoot
$bed    = Join-Path $root ".testbed"
$obs    = Join-Path $bed "obs"
$cfg    = Join-Path $obs "config\obs-studio"
$prof   = Join-Path $cfg "basic\profiles\MultistreamTest"
$dll    = Join-Path $root "build_x64\$Config\rws-multistream.dll"
$ids    = "00000000-beac-0000-0000-000000000001", "00000000-beac-0000-0000-000000000002",
          "00000000-beac-0000-0000-000000000003", "00000000-beac-0000-0000-000000000004",
          "00000000-beac-0000-0000-000000000005"
if ($Scenario -eq "live") { $ports = 19350, 19351, 19352, 19353, 19354 } elseif ($Scenario -eq "setup") { $ports = @(19350) } else { $ports = 19350, 19351, 19352 }
if (!(Test-Path $dll)) { throw "Build first: $dll not found" }

# 1. portable OBS
if (!(Test-Path (Join-Path $obs "bin\64bit\obs64.exe"))) {
  Write-Host "Copying OBS into the testbed (one time)..."
  robocopy "C:\Program Files\obs-studio" $obs /E /NFL /NDL /NJH /NJS /NP | Out-Null
  New-Item -ItemType File -Force (Join-Path $obs "portable_mode.txt") | Out-Null
}

# 2. install plugin
$pbin = Join-Path $obs "obs-plugins\64bit"
$pdat = Join-Path $obs "data\obs-plugins\rws-multistream\locale"
New-Item -ItemType Directory -Force $pdat | Out-Null
Copy-Item $dll $pbin -Force
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
Profile=MultistreamTest
ProfileDir=MultistreamTest
SceneCollection=MultistreamTest
SceneCollectionFile=MultistreamTest
[BasicWindow]
ConfirmOnExit=false
WarnBeforeStartingStream=false
WarnBeforeStoppingStream=false
"@
Set-Content (Join-Path $cfg "global.ini") $ini -Encoding UTF8
Set-Content (Join-Path $cfg "user.ini") $ini -Encoding UTF8

$wsDir = Join-Path $cfg "plugin_config\obs-websocket"
New-Item -ItemType Directory -Force $wsDir | Out-Null
Set-Content (Join-Path $wsDir "config.json") '{"alerts_enabled":false,"auth_required":false,"first_load":false,"server_enabled":true,"server_password":"","server_port":4466}' -Encoding UTF8

Set-Content (Join-Path $prof "basic.ini") @"
[General]
Name=MultistreamTest
[Output]
Mode=Advanced
[AdvOut]
Encoder=obs_x264
TrackIndex=1
Track1Bitrate=256
ApplyServiceSettings=true
[Video]
BaseCX=1920
BaseCY=1080
OutputCX=1920
OutputCY=1080
FPSType=0
FPSCommon=30
"@ -Encoding UTF8

# Real-world streaming settings: CBR with a 2 s keyframe interval.
Set-Content (Join-Path $prof "streamEncoder.json") '{"rate_control":"CBR","bitrate":2500,"keyint_sec":2,"preset":"veryfast"}' -Encoding UTF8

Set-Content (Join-Path $prof "service.json") ('{"type":"rtmp_custom","settings":{"server":"rtmp://127.0.0.1:' + $ports[0] + '/live","key":"k","use_auth":false}}') -Encoding UTF8

# A color source so the stream carries a real picture, not just black.
Set-Content (Join-Path $cfg "basic\scenes\MultistreamTest.json") @'
{"name":"MultistreamTest","current_scene":"Scene","current_program_scene":"Scene",
 "scene_order":[{"name":"Scene"}],
 "sources":[
  {"id":"color_source_v3","versioned_id":"color_source_v3","name":"Ember","uuid":"b0000000-0000-0000-0000-000000000002","settings":{"color":4282038770,"width":1920,"height":1080}},
  {"id":"scene","versioned_id":"scene","name":"Scene","uuid":"b0000000-0000-0000-0000-000000000001","settings":{"id_counter":1,"items":[{"name":"Ember","source_uuid":"b0000000-0000-0000-0000-000000000002","id":1,"visible":true}]}}
 ]}
'@ -Encoding UTF8


if ($Scenario -eq "live") {
  $dests = @"
{"version":1,"destinations":[
 {"id":"$($ids[0])","name":"YouTube","platform":"youtube","server":"rtmp://127.0.0.1:19351/live","enabled":true,"encoder":{"shared":true}},
 {"id":"$($ids[1])","name":"Twitch","platform":"twitch","server":"rtmp://127.0.0.1:19352/live","enabled":true,"encoder":{"shared":true}},
 {"id":"$($ids[2])","name":"Kick","platform":"kick","server":"rtmp://127.0.0.1:19353/live","enabled":true,"encoder":{"shared":true}},
 {"id":"$($ids[3])","name":"TikTok","platform":"tiktok","server":"rtmp://127.0.0.1:19354/live","enabled":true,
  "encoder":{"shared":false,"encoder_id":"obs_x264","video_bitrate":2500,"width":1280,"height":720}},
 {"id":"$($ids[4])","name":"Facebook","platform":"facebook","server":"rtmps://rtmp-api.facebook.com:443/rtmp/","enabled":false,"encoder":{"shared":true}}
]}
"@
  $upload = 40.0
} elseif ($Scenario -eq "setup") {
  # Never streams, so these real-looking ingest URLs are never contacted.
  $dests = @"
{"version":1,"destinations":[
 {"id":"$($ids[0])","name":"YouTube","platform":"youtube","server":"rtmps://a.rtmps.youtube.com:443/live2","enabled":true,"encoder":{"shared":true}},
 {"id":"$($ids[1])","name":"Twitch","platform":"twitch","server":"rtmp://live.twitch.tv/app","enabled":true,"encoder":{"shared":true}},
 {"id":"$($ids[2])","name":"Kick","platform":"kick","server":"rtmps://fa723fc1b171.global-contribute.live-video.net:443/app/","enabled":true,"encoder":{"shared":true}},
 {"id":"$($ids[3])","name":"TikTok","platform":"tiktok","server":"rtmp://push-rtmp-l11-va01.tiktokcdn.com/game/","enabled":true,
  "encoder":{"shared":false,"encoder_id":"obs_x264","video_bitrate":2500,"width":1280,"height":720}},
 {"id":"$($ids[4])","name":"Facebook","platform":"facebook","server":"rtmps://rtmp-api.facebook.com:443/rtmp/","enabled":false,"encoder":{"shared":true}}
]}
"@
  $upload = 40.0
} else { $dests = @"
{"version":1,"destinations":[
 {"id":"$($ids[0])","name":"YouTube","platform":"youtube","server":"rtmp://127.0.0.1:19351/live","enabled":true,"encoder":{"shared":true}},
 {"id":"$($ids[1])","name":"Twitch","platform":"twitch","server":"rtmp://127.0.0.1:19352/live","enabled":true,
  "encoder":{"shared":false,"encoder_id":"obs_x264","video_bitrate":1500,"width":1280,"height":720}},
 {"id":"$($ids[2])","name":"Kick","platform":"kick","server":"rtmp://127.0.0.1:19359/live","enabled":true,
  "encoder":{"shared":false,"encoder_id":"obs_x264","video_bitrate":800,"width":854,"height":480}},
 {"id":"$($ids[3])","name":"TikTok","platform":"tiktok","server":"rtmp:///live","enabled":true,
  "encoder":{"shared":false,"encoder_id":"obs_x264","video_bitrate":800,"width":854,"height":480}},
 {"id":"$($ids[4])","name":"Facebook","platform":"facebook","server":"rtmps://rtmp-api.facebook.com:443/rtmp/","enabled":false,"encoder":{"shared":true}}
]}
"@
  $upload = 7.0
}
Set-Content (Join-Path $prof "rws-multistream.json") $dests -Encoding UTF8

# 3. stream keys, written exactly the way the plugin writes them (UTF-8 blob)
Add-Type -Namespace RWS -Name Cred -MemberDefinition @'
[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
public struct CREDENTIAL { public int Flags; public int Type; public string TargetName; public string Comment;
  public long LastWritten; public int CredentialBlobSize; public System.IntPtr CredentialBlob; public int Persist;
  public int AttributeCount; public System.IntPtr Attributes; public string TargetAlias; public string UserName; }
[DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
public static extern bool CredWriteW(ref CREDENTIAL c, int flags);
[DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
public static extern bool CredDeleteW(string target, int type, int flags);
'@ -ErrorAction SilentlyContinue
function Set-Key($id, $key) {
  $bytes = [Text.Encoding]::UTF8.GetBytes($key)
  $ptr = [Runtime.InteropServices.Marshal]::AllocHGlobal($bytes.Length)
  [Runtime.InteropServices.Marshal]::Copy($bytes, 0, $ptr, $bytes.Length)
  $c = New-Object RWS.Cred+CREDENTIAL
  $c.Type = 1; $c.TargetName = "RedWardenStudios/Multistream/$id"; $c.UserName = "stream-key"
  $c.CredentialBlobSize = $bytes.Length; $c.CredentialBlob = $ptr; $c.Persist = 2
  if (![RWS.Cred]::CredWriteW([ref]$c, 0)) { throw "CredWrite failed" }
  [Runtime.InteropServices.Marshal]::FreeHGlobal($ptr)
}
foreach ($id in $ids) { Set-Key $id "k" }

# 4. receivers
$outDir = Join-Path $bed "received"
New-Item -ItemType Directory -Force $outDir | Out-Null
Get-ChildItem $outDir | Remove-Item -Force
function Start-Sink($p, $name) {
  Start-Process ffmpeg -PassThru -WindowStyle Hidden -RedirectStandardError (Join-Path $outDir "$name.log") -ArgumentList @(
    "-hide_banner", "-loglevel", "warning", "-y", "-listen", "1",
    "-i", "rtmp://127.0.0.1:$p/live/k", "-c", "copy", ('"' + (Join-Path $outDir "$name.mkv") + '"'))
}
$sinks = @{}
foreach ($p in $ports) { $sinks["port$p"] = Start-Sink $p "port$p" }
Start-Sleep -Seconds 2

# obs-websocket v5 client: Hello -> Identify -> requests
function Invoke-ObsWs([string[]]$Requests, [int]$Port = 4466) {
  $ws = New-Object System.Net.WebSockets.ClientWebSocket
  $ws.Options.AddSubProtocol("obswebsocket.json")
  $ct = [Threading.CancellationToken]::None
  $ws.ConnectAsync([Uri]"ws://127.0.0.1:$Port", $ct).Wait()
  $buf = New-Object byte[] 65536
  $recv = { $r = $ws.ReceiveAsync((New-Object ArraySegment[byte] -ArgumentList (,$buf)), $ct).Result; [Text.Encoding]::UTF8.GetString($buf, 0, $r.Count) }
  $send = { param($t) $b = [Text.Encoding]::UTF8.GetBytes($t); $ws.SendAsync((New-Object ArraySegment[byte] -ArgumentList (,$b)), "Text", $true, $ct).Wait() }
  [void](& $recv)
  & $send '{"op":1,"d":{"rpcVersion":1,"eventSubscriptions":0}}'
  [void](& $recv)
  $i = 0
  foreach ($rq in $Requests) { $i++; & $send ('{"op":6,"d":{"requestType":"' + $rq + '","requestId":"' + $i + '"}}'); & $recv }
  $ws.Dispose()
}


$pcfg = Join-Path $cfg "plugin_config\rws-multistream"
New-Item -ItemType Directory -Force $pcfg | Out-Null
Set-Content (Join-Path $pcfg "settings.json") ('{"upload_mbps":' + $upload + ',"upload_source":"measured","upload_when":"2026-10-02T07:00:00","check_updates":false}') -Encoding UTF8

$shots = Join-Path $bed "showcase\$Scenario"
if (Test-Path $shots) { Remove-Item -Recurse -Force $shots }
$env:RWS_MULTISTREAM_SNAPSHOT = $shots
$env:QT_SCALE_FACTOR = "2"

$exeDir = Join-Path $obs "bin\64bit"
$proc = Start-Process (Join-Path $exeDir "obs64.exe") -WorkingDirectory $exeDir -PassThru -ArgumentList @(
  "--portable", "--multi", "--profile", "MultistreamTest", "--collection", "MultistreamTest",
  "--disable-updater", "--disable-missing-files-check")
$t0 = Get-Date
function Wait-Until($sec) { $left = $sec - ((Get-Date) - $t0).TotalSeconds; if ($left -gt 0) { Start-Sleep -Milliseconds ([int]($left * 1000)) } }
Wait-Until 9
if ($Scenario -ne "setup") { try { Invoke-ObsWs @("StartStream") | Out-Null } catch { Write-Warning "websocket: $_" } }
if ($Scenario -eq "live") {
  Wait-Until 20
  $sinks["port19353"].Kill()          # Kick drops...
  Wait-Until 30
  $sinks["revived"] = Start-Sink 19353 "port19353-revived"   # ...and comes back
}
if ($Scenario -eq "setup") { Wait-Until 16 } else {
  Wait-Until 40
  try { Invoke-ObsWs @("StopStream") | Out-Null } catch { Write-Warning "websocket: $_" }
}
Start-Sleep -Seconds 7
[void]$proc.CloseMainWindow()
if (!$proc.WaitForExit(20000)) { $proc.Kill() }
Remove-Item Env:RWS_MULTISTREAM_SNAPSHOT, Env:QT_SCALE_FACTOR -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
$sinks.Values | ForEach-Object { if (!$_.HasExited) { $_.Kill() } }
foreach ($id in $ids) { [void][RWS.Cred]::CredDeleteW("RedWardenStudios/Multistream/$id", 1, 0) }
Get-ChildItem $shots | Select-Object Name, Length | Format-Table -AutoSize
Write-Host "SHOWCASE DONE ($Scenario)"


