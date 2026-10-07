<#
  Install (or remove) Red Warden Multistream into the local OBS Studio.

  Installs to C:\ProgramData\obs-studio\plugins\rws-multistream\ - the per-machine
  plugin folder OBS 28+ scans, the same place obs-multi-rtmp and the ATK
  plugin live. Nothing inside C:\Program Files is touched.

    powershell -ExecutionPolicy Bypass -File tools\install.ps1             # install
    powershell -ExecutionPolicy Bypass -File tools\install.ps1 -Uninstall  # remove

  OBS must be closed. Uninstalling leaves your destinations (rws-multistream.json in
  each OBS profile folder) and stream keys (Windows Credential Manager,
  "RedWardenStudios/Multistream/...") in place so a reinstall picks them back up.
#>
param(
  [switch]$Uninstall,
  [string]$Config = "RelWithDebInfo"
)
$ErrorActionPreference = "Stop"
$root   = Split-Path -Parent $PSScriptRoot
$target = Join-Path $env:ProgramData "obs-studio\plugins\rws-multistream"
# 0.1.x was installed under its working name, "Beacon".
$legacy = Join-Path $env:ProgramData "obs-studio\plugins\rws-beacon"

if (Get-Process obs64 -ErrorAction SilentlyContinue | Where-Object { $_.Path -notmatch '\\.testbed\\' }) {
  throw "OBS is running. Close it first - plugins load only when OBS starts."
}

if (Test-Path $legacy) { Remove-Item -Recurse -Force $legacy; Write-Host "Removed the old 'Beacon' install ($legacy)" }

if ($Uninstall) {
  if (Test-Path $target) { Remove-Item -Recurse -Force $target; Write-Host "Removed $target" }
  else { Write-Host "Red Warden Multistream is not installed." }
  exit 0
}

$dll = Join-Path $root "build_x64\$Config\rws-multistream.dll"
$pdb = Join-Path $root "build_x64\$Config\rws-multistream.pdb"
if (!(Test-Path $dll)) { throw "Build first: $dll not found" }

$bin = Join-Path $target "bin\64bit"
$loc = Join-Path $target "data\locale"
New-Item -ItemType Directory -Force $bin, $loc | Out-Null
Copy-Item $dll $bin -Force
if (Test-Path $pdb) { Copy-Item $pdb $bin -Force }   # crash reports name the line
Copy-Item (Join-Path $root "data\locale\*.ini") $loc -Force

$v = (Get-Item (Join-Path $bin "rws-multistream.dll")).LastWriteTime
Write-Host "Installed Red Warden Multistream to $target (build $v)"
Write-Host "Start OBS, then open Docks > Red Warden Multistream."
