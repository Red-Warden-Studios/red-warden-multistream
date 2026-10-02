param([string[]]$Steps = @("build", "unit", "resilience"))
# Builds and runs every test; results land in .testbed\run-*.txt. Start it detached so it outlives
# the 60 s bridge limit:  Start-Process powershell -ArgumentList '-File','tools\run-all.ps1','-Steps','build,unit,local'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$bed = Join-Path $root ".testbed"
Remove-Item (Join-Path $bed "run-*.txt") -ErrorAction SilentlyContinue
$q = (Get-ChildItem .deps -Directory -Filter 'obs-deps-qt6-*x64' | Select-Object -First 1).FullName
$env:PATH = "$q\bin;$env:PATH"
foreach ($s in ($Steps -join ',' -split ',')) {
  switch ($s) {
    "configure" { cmake --preset windows-x64 *> (Join-Path $bed "run-configure.txt") }
    "build" { cmake --build build_x64 --config RelWithDebInfo *> (Join-Path $bed "run-build.txt") }
    "unit" {
      $out = Join-Path $bed "run-unit.txt"
      foreach ($t in "rws-multistream-tests", "rws-multistream-importers-test", "rws-multistream-session-test", "rws-multistream-limits-test", "rws-multistream-update-test") {
        $exe = ".\build_x64\RelWithDebInfo\$t.exe"
        if (Test-Path $exe) { "== $t" | Add-Content $out; & $exe *>> $out; "exit=$LASTEXITCODE" | Add-Content $out }
      }
    }
    "import" { powershell -ExecutionPolicy Bypass -File tools\test-import.ps1 *> (Join-Path $bed "run-import.txt") }
    "local" { powershell -ExecutionPolicy Bypass -File tools\test-local.ps1 *> (Join-Path $bed "run-local.txt") }
    "bandwidth" { powershell -ExecutionPolicy Bypass -File tools\test-bandwidth.ps1 -Snapshots *> (Join-Path $bed "run-bandwidth.txt") }
    "resilience" { powershell -ExecutionPolicy Bypass -File tools\test-resilience.ps1 -Snapshots *> (Join-Path $bed "run-resilience.txt") }
  }
}
"done $(Get-Date -Format HH:mm:ss)" | Set-Content (Join-Path $bed "run-done.txt")

