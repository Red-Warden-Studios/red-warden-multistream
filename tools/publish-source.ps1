<#
  Publish the Red Warden Stream Kit source to the public GitHub repo (GPL: the source for every
  binary we ship).

    powershell -ExecutionPolicy Bypass -File tools\publish-source.ps1            # commit + tag + push
    powershell -ExecutionPolicy Bypass -File tools\publish-source.ps1 -DryRun    # stage only, show the diff
    powershell -ExecutionPolicy Bypass -File tools\publish-source.ps1 -DocsOnly "README: ..."   # no tag

  The private Streaming Tools repo stays the place we develop. This exports exactly what is
  COMMITTED (uncommitted edits under the three project folders are refused) into a clone of the
  public repo, then commits "Red Warden Stream Kit <ver> (Multistream <ver>, Pre-Flight <ver>)"
  and tags kit-v<ver>.

  Public repo layout:
    /                README.md, LICENSE, NOTICE.txt, .gitignore, installer/, tools/   (red-warden-stream-kit)
    multistream/     red-warden-multistream
    preflight/       red-warden-preflight

  Never published:  .github/         template CI (would build macOS/Linux and fail: Windows-only plugins)
                    brand/concepts/  Bastion division emblem work, not the product logo
                    preflight SPEC.md  internal design notes
                    untracked files (.testbed, dist, staging, build output): git archive only
                    exports committed files
#>
param(
  [string]$Repo = "https://github.com/Red-Warden-Studios/red-warden-stream-kit.git",
  [switch]$DryRun,
  [string]$DocsOnly   # commit message for a docs-only push (no tag), e.g. -DocsOnly "README: clarify X"
)
$ErrorActionPreference = "Stop"
$kit = Split-Path -Parent $PSScriptRoot
$mono = Split-Path -Parent $kit
function Fail($m) { Write-Host "[FAIL] $m" -ForegroundColor Red; exit 1 }

$msDir = "red-warden-multistream"
$pfDir = "red-warden-preflight"
$kitDir = "red-warden-stream-kit"

$kitVersion = (Get-Content (Join-Path $kit "installer\stream-kit.json") -Raw | ConvertFrom-Json).latest
$msVersion = (Get-Content (Join-Path $mono "$msDir\buildspec.json") -Raw | ConvertFrom-Json).version
$pfVersion = (Get-Content (Join-Path $mono "$pfDir\buildspec.json") -Raw | ConvertFrom-Json).version
if (-not $kitVersion -or -not $msVersion -or -not $pfVersion) { Fail "Could not read a version (kit '$kitVersion', multistream '$msVersion', preflight '$pfVersion')." }

$dirty = git -C $mono status --porcelain -- $msDir $pfDir $kitDir
if ($dirty) { Fail "Uncommitted changes under $msDir/, $pfDir/ or $kitDir/. Commit first:`n$($dirty -join "`n")" }

$work = Join-Path $env:TEMP "rws-kit-publish"
if (Test-Path $work) { Remove-Item -Recurse -Force $work }
New-Item -ItemType Directory -Force $work | Out-Null
$export = Join-Path $work "export"
$clone = Join-Path $work "repo"
New-Item -ItemType Directory -Force $export | Out-Null

# Export the committed files of one folder into $export\<dest> ("" = export root).
function Export-Committed($srcDir, $dest) {
  $zip = Join-Path $work ("$srcDir.zip")
  git -C $mono archive --format=zip -o $zip "HEAD:$srcDir"
  if ($LASTEXITCODE -ne 0) { Fail "git archive failed for $srcDir (is it committed?)" }
  $target = if ($dest) { Join-Path $export $dest } else { $export }
  Expand-Archive $zip $target -Force
  Remove-Item -Force $zip
  return $target
}
function Remove-Paths($root, $paths) {
  foreach ($p in $paths) { $x = Join-Path $root $p; if (Test-Path $x) { Remove-Item -Recurse -Force $x } }
}

# Kit root: README, LICENSE, NOTICE, installer\, tools\. The private .gitignore ("exclude everything
# except...") is replaced by a plain one for the public tree.
$null = Export-Committed $kitDir ""
Remove-Paths $export @(".testbed", "dist", "staging", ".gitignore")
$publicIgnore = "build_x64/`n.deps/`n.testbed/`ndist/`nstaging/`n"
[System.IO.File]::WriteAllText((Join-Path $export ".gitignore"), $publicIgnore, (New-Object System.Text.UTF8Encoding($false)))

$msOut = Export-Committed $msDir "multistream"
Remove-Paths $msOut @(".github", "brand\concepts")

$pfOut = Export-Committed $pfDir "preflight"
Remove-Paths $pfOut @(".github", "brand\concepts", "SPEC.md")

git clone -q $Repo $clone
if ($LASTEXITCODE -ne 0) { Fail "clone failed: $Repo" }
Get-ChildItem $clone -Force | Where-Object Name -ne ".git" | Remove-Item -Recurse -Force
Get-ChildItem $export -Force | Copy-Item -Destination $clone -Recurse -Force   # includes dotfiles

$msg = "Red Warden Stream Kit $kitVersion (Multistream $msVersion, Pre-Flight $pfVersion)"
$tag = "kit-v$kitVersion"

git -C $clone add -A
git -C $clone -c core.autocrlf=false status --short | Select-Object -First 200
if ($DryRun) {
  Write-Host "[DRY RUN] would commit: $msg"
  Write-Host "[DRY RUN] would tag: $tag (annotated)"
  if (git -C $clone tag --list $tag) { Write-Host "[DRY RUN] WARNING: tag $tag already exists in the public repo" -ForegroundColor Yellow }
  Write-Host "[DRY RUN] staged in $clone - nothing pushed"
  exit 0
}

$who = @("-c", "user.name=Red Warden Studios", "-c", "user.email=support@redwardenstudios.com")
if ($DocsOnly) {
  # README/docs fix between releases: commit to main, no tag (the release tag keeps pointing at the
  # source that was actually built).
  git -C $clone @who commit -q -m $DocsOnly
  if ($LASTEXITCODE -ne 0) { Fail "commit failed (nothing changed?)" }
  git -C $clone push -q origin main
  if ($LASTEXITCODE -ne 0) { Fail "push failed" }
  Write-Host "[OK] Pushed docs update to $Repo (no tag)"
  exit 0
}
if (git -C $clone tag --list $tag) { Fail "Tag $tag already exists in the public repo. Bump the version, or use -DocsOnly." }
git -C $clone @who commit -q -m $msg
if ($LASTEXITCODE -ne 0) { Fail "commit failed (nothing changed?)" }
git -C $clone branch -M main
git -C $clone @who tag -a $tag -m $msg
if ($LASTEXITCODE -ne 0) { Fail "tag failed" }
git -C $clone push -q -u origin main --follow-tags
if ($LASTEXITCODE -ne 0) { Fail "push failed" }
Write-Host "[OK] Published source for Stream Kit $kitVersion to $Repo (tag $tag)"
