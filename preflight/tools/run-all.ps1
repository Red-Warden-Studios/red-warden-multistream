param([string[]]$Steps = @("build", "unit", "load", "state", "ui", "integration"))
# Configures, builds and runs every test; results land in .testbed\run-*.txt. It takes several
# minutes, so run it in the background (add -WindowStyle Hidden to keep it off screen):
#   Start-Process powershell -ArgumentList '-ExecutionPolicy','Bypass','-File','tools\run-all.ps1','-Steps','configure,build,unit,load,state,ui,integration' -WorkingDirectory <preflight dir>
# Default steps (no -Steps): build, unit, load, state, ui, integration ("configure" only when CMake files changed).
# run-done.txt: first line "OK" only if every step exited 0, else "FAILED at <step> exit=<n>"
# (configure/build stop the run) or "FAILED: <steps>" (unit/load). All result files are ASCII.
# A -Steps subset runs only those steps, so its "OK" means OK for that subset only, not for the whole suite.
# The release packager reads each test's own result file (run-unit.txt, run-<harness>.txt), not run-done.txt.
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$bed = Join-Path $root ".testbed"
New-Item -ItemType Directory -Force $bed | Out-Null
Remove-Item (Join-Path $bed "run-*.txt") -ErrorAction SilentlyContinue
$doneFile = Join-Path $bed "run-done.txt"
$q = (Get-ChildItem .deps -Directory -Filter 'obs-deps-qt6-*x64' | Select-Object -First 1).FullName
$env:PATH = "$q\bin;$env:PATH"
$failed = @()
foreach ($s in ($Steps -join ',' -split ',')) {
  switch ($s) {
    "configure" {
      cmake --preset windows-x64 2>&1 | Out-File -Encoding ASCII (Join-Path $bed "run-configure.txt")
      $code = $LASTEXITCODE
      if ($code -ne 0) { "FAILED at configure exit=$code" | Set-Content -Encoding ASCII $doneFile; exit 1 }
    }
    "build" {
      cmake --build build_x64 --config RelWithDebInfo 2>&1 | Out-File -Encoding ASCII (Join-Path $bed "run-build.txt")
      $code = $LASTEXITCODE
      if ($code -ne 0) { "FAILED at build exit=$code" | Set-Content -Encoding ASCII $doneFile; exit 1 }
    }
    "unit" {
      $out = Join-Path $bed "run-unit.txt"
      foreach ($t in "rws-preflight-update-test", "rws-preflight-eval-test", "rws-preflight-disk-test", "rws-preflight-rungen-test") {
        $exe = ".\build_x64\RelWithDebInfo\$t.exe"
        "== $t" | Add-Content -Encoding ASCII $out
        if (Test-Path $exe) {
          $o = & $exe 2>&1
          $code = $LASTEXITCODE
          $o | ForEach-Object { "$_" } | Add-Content -Encoding ASCII $out
        } else {
          "MISSING $exe" | Add-Content -Encoding ASCII $out
          $code = -1
        }
        "exit=$code" | Add-Content -Encoding ASCII $out
        if ($code -ne 0) { $failed += "unit:$t" }
      }
    }
    "load" {
      powershell -ExecutionPolicy Bypass -File tools\test-load.ps1 2>&1 | Out-File -Encoding ASCII (Join-Path $bed "run-load-console.txt")
      $code = $LASTEXITCODE
      if ($code -ne 0) { $failed += "load" }
    }
    "ui" {
      powershell -ExecutionPolicy Bypass -File tools\test-ui.ps1 2>&1 | Out-File -Encoding ASCII (Join-Path $bed "run-ui-console.txt")
      $code = $LASTEXITCODE
      if ($code -ne 0) { $failed += "ui" }
    }
    "integration" {
      powershell -ExecutionPolicy Bypass -File tools\test-integration.ps1 2>&1 | Out-File -Encoding ASCII (Join-Path $bed "run-integration-console.txt")
      $code = $LASTEXITCODE
      if ($code -ne 0) { $failed += "integration" }
    }
    "state" {
      powershell -ExecutionPolicy Bypass -File tools\test-state.ps1 2>&1 | Out-File -Encoding ASCII (Join-Path $bed "run-state-console.txt")
      $code = $LASTEXITCODE
      if ($code -ne 0) { $failed += "state" }
    }
  }
}
$head = if ($failed.Count -eq 0) { "OK" } else { "FAILED: " + ($failed -join ",") }
@($head, "done $(Get-Date -Format HH:mm:ss)") | Set-Content -Encoding ASCII $doneFile
if ($failed.Count -ne 0) { exit 1 }
