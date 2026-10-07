<#
  Install (or remove) Red Warden Pre-Flight into the local OBS Studio.

  Installs to C:\ProgramData\obs-studio\plugins\rws-preflight\ - the per-machine
  plugin folder OBS 28+ scans (same place as Red Warden Multistream).
  Nothing inside C:\Program Files is touched.

    powershell -ExecutionPolicy Bypass -File tools\install.ps1             # install
    powershell -ExecutionPolicy Bypass -File tools\install.ps1 -Uninstall  # remove

  OBS must be closed. Uninstalling leaves Pre-Flight's settings.json (in OBS's
  plugin_config\rws-preflight folder) so a reinstall picks it back up.
#>
param(
  [switch]$Uninstall,
  [string]$Config = "RelWithDebInfo"
)
$ErrorActionPreference = "Stop"
$root   = Split-Path -Parent $PSScriptRoot
$target = Join-Path $env:ProgramData "obs-studio\plugins\rws-preflight"

if (Get-Process obs64 -ErrorAction SilentlyContinue | Where-Object { $_.Path -notmatch '\\.testbed\\' }) {
  throw "OBS is running. Close it first - plugins load only when OBS starts."
}

if ($Uninstall) {
  if (Test-Path $target) { Remove-Item -Recurse -Force $target; Write-Host "Removed $target" }
  else { Write-Host "Red Warden Pre-Flight is not installed." }
  exit 0
}

$dll = Join-Path $root "build_x64\$Config\rws-preflight.dll"
$pdb = Join-Path $root "build_x64\$Config\rws-preflight.pdb"
if (!(Test-Path $dll)) { throw "Build first: $dll not found" }

$bin = Join-Path $target "bin\64bit"
$loc = Join-Path $target "data\locale"
New-Item -ItemType Directory -Force $bin, $loc | Out-Null
Copy-Item $dll $bin -Force
if (Test-Path $pdb) { Copy-Item $pdb $bin -Force }
Copy-Item (Join-Path $root "data\locale\*.ini") $loc -Force

$v = (Get-Item (Join-Path $bin "rws-preflight.dll")).LastWriteTime
Write-Host "Installed Red Warden Pre-Flight to $target (build $v)"
Write-Host "Start OBS, then open Docks > Pre-Flight."
