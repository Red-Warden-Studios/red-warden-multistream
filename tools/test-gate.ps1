<#
  Self-contained test of tools\check-test-results.ps1, the release test gate.

    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test-gate.ps1

  Needs no OBS and no build. Each case builds a fake plugin folder (a dummy DLL plus .testbed result
  files with controlled content and timestamps) under a unique folder in $env:TEMP, runs the checker
  on it and compares the exit code with the expected one. The temp folder is always deleted.
  Prints "ALL PASSED" and exits 0, or "<n> FAILED" and exits 1.
#>
$ErrorActionPreference = "Stop"
$checker = Join-Path $PSScriptRoot "check-test-results.ps1"
$root = Join-Path $env:TEMP ("rws-gate-test-" + [guid]::NewGuid().ToString("N"))
$failures = 0
$seq = 0

# The checker's lists, repeated here on purpose: if someone edits the checker's lists, this test
# must be edited too, so a test cannot be dropped from the gate silently.
$units = @{
  multistream = "rws-multistream-tests", "rws-multistream-importers-test", "rws-multistream-session-test",
                "rws-multistream-limits-test", "rws-multistream-update-test", "rws-multistream-serverurl-test"
  preflight   = "rws-preflight-update-test", "rws-preflight-eval-test", "rws-preflight-disk-test", "rws-preflight-rungen-test"
}
$harnesses = @{
  multistream = "import", "local", "resilience", "bandwidth"
  preflight   = "load", "state", "ui", "integration"
}

$base = (Get-Date).AddHours(-2)         # the DLL's timestamp
$fresh = $base.AddMinutes(30)           # result files newer than the DLL
$stale = $base.AddMinutes(-30)          # result files older than the DLL

function Put($path, $text, $time) {
  [IO.File]::WriteAllText($path, $text, [Text.Encoding]::ASCII)
  (Get-Item $path).LastWriteTime = $time
}

# run-unit.txt text. Multistream's real file mixes in NULs (UTF-16 output from the test exes).
function Unit-Text($plugin, $skip, $badExit, $noExit) {
  $nul = if ($plugin -eq "multistream") { [string][char]0 } else { "" }
  $out = @()
  foreach ($t in $units[$plugin]) {
    if ($t -eq $skip) { continue }
    $code = if ($t -eq $badExit) { 1 } else { 0 }
    $out += "== $t$nul"
    $out += "[  PASSED  ]$nul 3 tests."
    $out += "o${nul}k${nul}"
    if ($t -ne $noExit) { $out += "exit=$code$nul" }
  }
  return (($out -join "`r`n") + "`r`n")
}

function Harness-Text($plugin) {
  if ($plugin -eq "multistream") { return "step one`r`nstep two`r`nRESULT: PASS`r`n" }
  return "PASS`r`nmore detail`r`n"
}

# A plugin folder that passes everything: DLL, fresh unit file, fresh passing harness files.
function New-Fixture($plugin) {
  $script:seq++
  $dir = Join-Path $root "case$($script:seq)"
  $bed = Join-Path $dir ".testbed"
  New-Item -ItemType Directory -Force $bed | Out-Null
  $dll = Join-Path $dir "x.dll"
  Put $dll "dummy" $base
  Put (Join-Path $bed "run-unit.txt") (Unit-Text $plugin $null $null) $fresh
  foreach ($h in $harnesses[$plugin]) { Put (Join-Path $bed "run-$h.txt") (Harness-Text $plugin) $fresh }
  # The unit-test executables sit next to the DLL, older than the unit results.
  foreach ($t in $units[$plugin]) { Put (Join-Path $dir "$t.exe") "dummy" $base.AddMinutes(10) }
  return @{ Dir = $dir; Bed = $bed; Dll = $dll; Plugin = $plugin }
}

function Run-Case($name, $plugin, $expected, $mutate) {
  $fx = New-Fixture $plugin
  if ($mutate) { & $mutate $fx }
  & powershell -NoProfile -ExecutionPolicy Bypass -File $checker -Plugin $plugin -PluginDir $fx.Dir -Dll $fx.Dll *> $null
  $got = $LASTEXITCODE
  if ($got -eq $expected) { Write-Host "ok:   $name" }
  else { Write-Host "FAIL: $name (expected $expected, got $got)"; $script:failures++ }
}

function Harness-Path($fx, $h) { Join-Path $fx.Bed "run-$h.txt" }

try {
  New-Item -ItemType Directory -Force $root | Out-Null

  # --- Pre-Flight ---
  Run-Case "preflight: full pass is accepted" preflight 0 $null
  Run-Case "preflight: run-done.txt FAILED but every per-test file passes is accepted" preflight 0 {
    param($fx) Put (Join-Path $fx.Bed "run-done.txt") "FAILED: ui`r`n" $fresh }
  Run-Case "preflight: units only, no harness files, run-done.txt OK is refused" preflight 1 {
    param($fx)
    foreach ($h in $harnesses.preflight) { Remove-Item (Harness-Path $fx $h) }
    Put (Join-Path $fx.Bed "run-done.txt") "OK`r`ndone 12:00:00`r`n" $fresh }
  Run-Case "preflight: one harness first line FAIL is refused" preflight 1 {
    param($fx) Put (Harness-Path $fx "state") "FAIL`r`n" $fresh }
  Run-Case "preflight: one harness older than the DLL is refused" preflight 1 {
    param($fx) (Get-Item (Harness-Path $fx "ui")).LastWriteTime = $stale }
  Run-Case "preflight: harness first line PASSED-ish is refused" preflight 1 {
    param($fx) Put (Harness-Path $fx "load") "PASSED-ish`r`n" $fresh }
  Run-Case "preflight: harness first line whitespace + FAIL is refused" preflight 1 {
    param($fx) Put (Harness-Path $fx "integration") "  FAIL`r`nPASS`r`n" $fresh }
  Run-Case "preflight: PASS only on a later line is refused" preflight 1 {
    param($fx) Put (Harness-Path $fx "ui") "FAIL`r`nPASS`r`n" $fresh }
  Run-Case "preflight: empty harness file is refused" preflight 1 {
    param($fx) Put (Harness-Path $fx "state") "" $fresh }
  Run-Case "preflight: one unit test missing is refused" preflight 1 {
    param($fx) Put (Join-Path $fx.Bed "run-unit.txt") (Unit-Text "preflight" "rws-preflight-disk-test" $null) $fresh }
  Run-Case "preflight: one unit test exit=1 is refused" preflight 1 {
    param($fx) Put (Join-Path $fx.Bed "run-unit.txt") (Unit-Text "preflight" $null "rws-preflight-eval-test") $fresh }
  Run-Case "preflight: run-unit.txt older than the DLL is refused" preflight 1 {
    param($fx) (Get-Item (Join-Path $fx.Bed "run-unit.txt")).LastWriteTime = $stale }
  Run-Case "preflight: DLL missing is refused" preflight 1 {
    param($fx) Remove-Item $fx.Dll }
  Run-Case "preflight: run-unit.txt timestamp equal to the DLL's is refused" preflight 1 {
    param($fx) (Get-Item (Join-Path $fx.Bed "run-unit.txt")).LastWriteTime = (Get-Item $fx.Dll).LastWriteTime }
  Run-Case "preflight: harness timestamp equal to the DLL's is refused" preflight 1 {
    param($fx) (Get-Item (Harness-Path $fx "load")).LastWriteTime = (Get-Item $fx.Dll).LastWriteTime }
  Run-Case "preflight: duplicate unit entry (exit=1 then exit=0) is refused" preflight 1 {
    param($fx) Put (Join-Path $fx.Bed "run-unit.txt") ("== rws-preflight-eval-test`r`nexit=1`r`n" + (Unit-Text "preflight" $null $null $null)) $fresh }
  Run-Case "preflight: named unit entry with no exit line is refused" preflight 1 {
    param($fx) Put (Join-Path $fx.Bed "run-unit.txt") (Unit-Text "preflight" $null $null "rws-preflight-disk-test") $fresh }
  Run-Case "preflight: unnamed trailing entry with no exit line is refused" preflight 1 {
    param($fx) Put (Join-Path $fx.Bed "run-unit.txt") ((Unit-Text "preflight" $null $null $null) + "== some-other-test`r`n") $fresh }
  Run-Case "preflight: unit exe newer than run-unit.txt is refused" preflight 1 {
    param($fx) (Get-Item (Join-Path $fx.Dir "rws-preflight-update-test.exe")).LastWriteTime = $fresh.AddMinutes(5) }
  Run-Case "preflight: unit exe missing is refused" preflight 1 {
    param($fx) Remove-Item (Join-Path $fx.Dir "rws-preflight-rungen-test.exe") }

  # --- Multistream ---
  Run-Case "multistream: full pass (NULs in run-unit.txt) is accepted" multistream 0 $null
  Run-Case "multistream: run-done.txt FAILED but every per-test file passes is accepted" multistream 0 {
    param($fx) Put (Join-Path $fx.Bed "run-done.txt") "FAILED: local`r`n" $fresh }
  Run-Case "multistream: build and unit only, run-done.txt OK is refused" multistream 1 {
    param($fx)
    foreach ($h in $harnesses.multistream) { Remove-Item (Harness-Path $fx $h) }
    Put (Join-Path $fx.Bed "run-done.txt") "OK`r`ndone 12:00:00`r`n" $fresh }
  Run-Case "multistream: rws-multistream-serverurl-test missing is refused" multistream 1 {
    param($fx) Put (Join-Path $fx.Bed "run-unit.txt") (Unit-Text "multistream" "rws-multistream-serverurl-test" $null) $fresh }
  Run-Case "multistream: one unit test exit=1 is refused" multistream 1 {
    param($fx) Put (Join-Path $fx.Bed "run-unit.txt") (Unit-Text "multistream" $null "rws-multistream-limits-test") $fresh }
  Run-Case "multistream: one harness missing is refused" multistream 1 {
    param($fx) Remove-Item (Harness-Path $fx "bandwidth") }
  Run-Case "multistream: harness without RESULT: PASS is refused" multistream 1 {
    param($fx) Put (Harness-Path $fx "local") "step one`r`nRESULT: FAIL`r`n" $fresh }
  Run-Case "multistream: harness older than the DLL is refused" multistream 1 {
    param($fx) (Get-Item (Harness-Path $fx "import")).LastWriteTime = $stale }
  Run-Case "multistream: run-unit.txt older than the DLL is refused" multistream 1 {
    param($fx) (Get-Item (Join-Path $fx.Bed "run-unit.txt")).LastWriteTime = $stale }
  Run-Case "multistream: DLL missing is refused" multistream 1 {
    param($fx) Remove-Item $fx.Dll }
  Run-Case "multistream: run-unit.txt timestamp equal to the DLL's is refused" multistream 1 {
    param($fx) (Get-Item (Join-Path $fx.Bed "run-unit.txt")).LastWriteTime = (Get-Item $fx.Dll).LastWriteTime }
  Run-Case "multistream: harness timestamp equal to the DLL's is refused" multistream 1 {
    param($fx) (Get-Item (Harness-Path $fx "resilience")).LastWriteTime = (Get-Item $fx.Dll).LastWriteTime }
  Run-Case "multistream: duplicate unit entry (exit=1 then exit=0) is refused" multistream 1 {
    param($fx) Put (Join-Path $fx.Bed "run-unit.txt") ("== rws-multistream-tests`r`nexit=1`r`n" + (Unit-Text "multistream" $null $null $null)) $fresh }
  Run-Case "multistream: named unit entry with no exit line is refused" multistream 1 {
    param($fx) Put (Join-Path $fx.Bed "run-unit.txt") (Unit-Text "multistream" $null $null "rws-multistream-session-test") $fresh }
  Run-Case "multistream: unit exe newer than run-unit.txt is refused" multistream 1 {
    param($fx) (Get-Item (Join-Path $fx.Dir "rws-multistream-update-test.exe")).LastWriteTime = $fresh.AddMinutes(5) }
  Run-Case "multistream: unit exe missing is refused" multistream 1 {
    param($fx) Remove-Item (Join-Path $fx.Dir "rws-multistream-serverurl-test.exe") }
  Run-Case "multistream: harness with both RESULT: FAIL and RESULT: PASS is refused" multistream 1 {
    param($fx) Put (Harness-Path $fx "import") "RESULT: FAIL`r`nretry`r`nRESULT: PASS`r`n" $fresh }
  Run-Case "multistream: harness with RESULT: PASSED (not exact) is refused" multistream 1 {
    param($fx) Put (Harness-Path $fx "local") "RESULT: PASSED`r`n" $fresh }
  Run-Case "multistream: UTF-16LE harness file with BOM ending RESULT: PASS is accepted" multistream 0 {
    param($fx)
    $f = Harness-Path $fx "bandwidth"
    "step one`r`nRESULT: PASS" | Out-File -Encoding Unicode $f
    (Get-Item $f).LastWriteTime = $fresh }
}
finally {
  if (Test-Path $root) { Remove-Item -Recurse -Force $root -ErrorAction SilentlyContinue }
}

if ($failures -eq 0) { Write-Host "ALL PASSED"; exit 0 }
Write-Host "$failures FAILED"
exit 1
