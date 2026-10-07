param([string[]]$Steps = @("build", "unit", "import", "local", "resilience", "bandwidth"))
# Builds and runs every test; results land in .testbed\run-*.txt. It takes several minutes, so run it
# in the background:  Start-Process powershell -WindowStyle Hidden -ArgumentList '-File','tools\run-all.ps1','-Steps','build,unit,local'
# Default steps: build, unit, import, local, resilience, bandwidth ("configure" only when CMake files changed).
# A -Steps subset runs only those steps, so run-done.txt then speaks for that subset only.
# run-done.txt: first line "OK" only if every selected step passed, else "FAILED at <step> exit=<n>"
# (configure/build stop the run) or "FAILED: <steps>" (unit/harness steps, or "unknown:<name>" for a
# step name this script does not know); second line is "done HH:mm:ss".
# A unit test passes on exit 0 (a missing exe fails); a harness passes on exit 0 AND "RESULT: PASS" in its file.
# The release packager reads each test's own result file (run-unit.txt, run-<harness>.txt), not run-done.txt.
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$bed = Join-Path $root ".testbed"
Remove-Item (Join-Path $bed "run-*.txt") -ErrorAction SilentlyContinue
$q = (Get-ChildItem .deps -Directory -Filter 'obs-deps-qt6-*x64' | Select-Object -First 1).FullName
$env:PATH = "$q\bin;$env:PATH"
$doneFile = Join-Path $bed "run-done.txt"
$failed = @()

# Runs one harness script (extra arguments in $more), then fails the step on a nonzero exit or on a
# result file without "RESULT: PASS".
function Run-Harness($name, $script, $more) {
  $resultFile = Join-Path $bed "run-$name.txt"
  powershell -ExecutionPolicy Bypass -File $script @more *> $resultFile
  $code = $LASTEXITCODE
  $passed = (Test-Path $resultFile) -and (Select-String -Path $resultFile -Pattern 'RESULT: PASS' -Quiet)
  if ($code -ne 0 -or !$passed) { $script:failed += $name }
}

foreach ($s in ($Steps -join ',' -split ',')) {
  switch ($s.Trim()) {
    "configure" {
      cmake --preset windows-x64 *> (Join-Path $bed "run-configure.txt")
      $code = $LASTEXITCODE
      if ($code -ne 0) { "FAILED at configure exit=$code" | Set-Content $doneFile; exit 1 }
    }
    "build" {
      cmake --build build_x64 --config RelWithDebInfo *> (Join-Path $bed "run-build.txt")
      $code = $LASTEXITCODE
      if ($code -ne 0) { "FAILED at build exit=$code" | Set-Content $doneFile; exit 1 }
    }
    "unit" {
      $out = Join-Path $bed "run-unit.txt"
      foreach ($t in "rws-multistream-tests", "rws-multistream-importers-test", "rws-multistream-session-test", "rws-multistream-limits-test", "rws-multistream-update-test", "rws-multistream-serverurl-test") {
        $exe = ".\build_x64\RelWithDebInfo\$t.exe"
        "== $t" | Add-Content $out
        if (Test-Path $exe) {
          & $exe *>> $out
          $code = $LASTEXITCODE
        } else {
          "MISSING $exe" | Add-Content $out
          $code = -1
        }
        "exit=$code" | Add-Content $out
        if ($code -ne 0) { $failed += "unit:$t" }
      }
    }
    "import" { Run-Harness "import" "tools\test-import.ps1" @() }
    "local" { Run-Harness "local" "tools\test-local.ps1" @() }
    "bandwidth" { Run-Harness "bandwidth" "tools\test-bandwidth.ps1" @("-Snapshots") }
    "resilience" { Run-Harness "resilience" "tools\test-resilience.ps1" @("-Snapshots") }
    default { if ($s.Trim() -ne "") { $failed += "unknown:$($s.Trim())" } }
  }
}
# The release gate does not trust this line: it reads the per-test result files.
$head = if ($failed.Count -eq 0) { "OK" } else { "FAILED: " + ($failed -join ",") }
@($head, "done $(Get-Date -Format HH:mm:ss)") | Set-Content $doneFile
if ($failed.Count -ne 0) { exit 1 }

