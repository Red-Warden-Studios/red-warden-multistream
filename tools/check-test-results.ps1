<#
  Release test gate for one plugin: succeeds only if every named unit test AND every integration
  harness passed against the exact DLL being shipped.

    powershell -NoProfile -ExecutionPolicy Bypass -File tools\check-test-results.ps1 `
      -Plugin multistream|preflight -PluginDir <plugin folder> -Dll <path to the plugin DLL>

  Reads the plugin's .testbed\run-unit.txt and .testbed\run-<harness>.txt files. Every file must be
  newer than the DLL. run-done.txt is never read: a partial test run writes a clean-looking summary
  there, so the per-test files are the only evidence accepted.
  Prints "[OK] ..." and exits 0, or prints "[FAIL] <plugin>: <first problem>" and exits 1.
#>
param(
  [Parameter(Mandatory = $true)][ValidateSet("multistream", "preflight")][string]$Plugin,
  [Parameter(Mandatory = $true)][string]$PluginDir,
  [Parameter(Mandatory = $true)][string]$Dll
)
$ErrorActionPreference = "Stop"

# --- What must have passed. Add a new test here, in the plugin's own list. -------------------------
$units = @{
  # Every unit-test executable run-all.ps1 writes into run-unit.txt as "== <name>" ... "exit=<n>".
  multistream = "rws-multistream-tests", "rws-multistream-importers-test", "rws-multistream-session-test",
                "rws-multistream-limits-test", "rws-multistream-update-test", "rws-multistream-serverurl-test"
  preflight   = "rws-preflight-update-test", "rws-preflight-eval-test", "rws-preflight-disk-test", "rws-preflight-rungen-test"
}
$harnesses = @{
  # Every harness whose .testbed\run-<name>.txt must exist, be fresh and say it passed.
  multistream = "import", "local", "resilience", "bandwidth"
  preflight   = "load", "state", "ui", "integration"
}

function Fail($msg) { Write-Host "[FAIL] ${Plugin}: $msg" -ForegroundColor Red; exit 1 }

# Some test exes write UTF-16, so appended ASCII lines can carry stray NULs: strip them before matching.
function Read-Lines($path) { @(Get-Content $path) | ForEach-Object { "$_" -replace '\x00', '' } }

if (!(Test-Path -LiteralPath $Dll)) { Fail "DLL not found: $Dll" }
$dllItem = Get-Item -LiteralPath $Dll
$bed = Join-Path $PluginDir ".testbed"

# --- Unit tests: each named test must appear with exit=0 -------------------------------------------
$unitFile = Join-Path $bed "run-unit.txt"
if (!(Test-Path -LiteralPath $unitFile)) { Fail "no run-unit.txt ($unitFile). Run the plugin's tools\run-all.ps1 first." }
$unitItem = Get-Item -LiteralPath $unitFile
# Freshness is strict: a result file with the same timestamp as the DLL proves nothing about it.
if ($unitItem.LastWriteTime -le $dllItem.LastWriteTime) { Fail "run-unit.txt is not newer than $($dllItem.Name). Re-run the unit tests." }
# The test executables sit next to the DLL. Results older than an exe describe a previous build of it.
$dllDir = Split-Path -Parent $dllItem.FullName
foreach ($t in $units[$Plugin]) {
  $exe = Join-Path $dllDir "$t.exe"
  if (!(Test-Path -LiteralPath $exe)) { Fail "$t.exe not found in $dllDir" }
  if ($unitItem.LastWriteTime -le (Get-Item -LiteralPath $exe).LastWriteTime) { Fail "$t.exe is newer than run-unit.txt: re-run the unit tests" }
}
# Every "== name" entry must be closed by exactly one exit line, and appear once. A name that appears
# twice could hide a failing run behind a later passing one.
$exits = @{}; $seen = @{}; $dangling = @(); $cur = $null
foreach ($l in Read-Lines $unitFile) {
  if ($l -match '^== (\S+)') {
    if ($cur) { $dangling += $cur }
    $cur = $Matches[1]
    $seen[$cur] = 1 + [int]$seen[$cur]
  }
  elseif ($l -match '^exit=(-?\d+)' -and $cur) { $exits[$cur] = [int]$Matches[1]; $cur = $null }
}
if ($cur) { $dangling += $cur }
foreach ($n in ($seen.Keys | Sort-Object)) {
  if ($seen[$n] -gt 1) { Fail "$n appears $($seen[$n]) times in run-unit.txt ($unitFile)" }
}
if ($dangling.Count -gt 0) { Fail "$($dangling[0]) has no exit line ($unitFile)" }
foreach ($t in $units[$Plugin]) {
  if (!$exits.ContainsKey($t)) { Fail "unit test $t did not run ($unitFile)" }
  if ($exits[$t] -ne 0) { Fail "unit test $t exited $($exits[$t]) ($unitFile)" }
}

# --- Harnesses: a fresh result file that says PASS --------------------------------------------------
foreach ($h in $harnesses[$Plugin]) {
  $f = Join-Path $bed "run-$h.txt"
  if (!(Test-Path -LiteralPath $f)) { Fail "$h harness did not run (no $f)" }
  if ((Get-Item -LiteralPath $f).LastWriteTime -le $dllItem.LastWriteTime) { Fail "$h harness result is not newer than $($dllItem.Name) ($f)" }
  $lines = @(Read-Lines $f)
  if ($Plugin -eq "multistream") {
    # Multistream harnesses print a "RESULT: PASS" line on their own and never a "RESULT: FAIL" line.
    # The file may be UTF-16 (PowerShell's *> redirect), so NULs were stripped when it was read.
    $ok = @($lines | Where-Object { $_ -match '^\s*RESULT: PASS\s*$' }).Count -gt 0 -and
          @($lines | Where-Object { $_ -match 'RESULT: FAIL' }).Count -eq 0
  } else {
    # Pre-Flight harnesses write exactly PASS or FAIL as the first line.
    $ok = $lines.Count -gt 0 -and $lines[0].Trim() -ceq "PASS"
  }
  if (!$ok) { Fail "$h harness did not pass ($f)" }
}

Write-Host "[OK] ${Plugin}: $(@($units[$Plugin]).Count) unit tests and $(@($harnesses[$Plugin]).Count) harnesses passed against $($dllItem.Name)"
exit 0
