<#
  Publish the plugin's source to the public GitHub repo (GPL: the source for every binary we ship).

    powershell -ExecutionPolicy Bypass -File tools\publish-source.ps1            # commit + tag + push
    powershell -ExecutionPolicy Bypass -File tools\publish-source.ps1 -DryRun    # stage only, show the diff
    powershell -ExecutionPolicy Bypass -File tools\publish-source.ps1 -DocsOnly "README: ..."   # no tag

  The private Streaming Tools repo stays the place we develop. This exports exactly what is
  COMMITTED under red-warden-multistream/ (uncommitted edits are refused), minus what must not go
  public, into a clone of the public repo, then commits "Red Warden Multistream <version>" and
  tags v<version>.

  Never published:  .github/         template CI (would build macOS/Linux and fail: Windows-only plugin)
                    brand/concepts/  Bastion division emblem work, not the product logo
#>
param(
  [string]$Repo = "https://github.com/Red-Warden-Studios/red-warden-multistream.git",
  [switch]$DryRun,
  [string]$DocsOnly   # commit message for a docs-only push (no tag), e.g. -DocsOnly "README: clarify X"
)
$ErrorActionPreference = "Stop"
$plugin = Split-Path -Parent $PSScriptRoot
$mono = Split-Path -Parent $plugin
$version = (Get-Content (Join-Path $plugin "buildspec.json") -Raw | ConvertFrom-Json).version
function Fail($m) { Write-Host "[FAIL] $m" -ForegroundColor Red; exit 1 }

$dirty = git -C $mono status --porcelain -- red-warden-multistream
if ($dirty) { Fail "Uncommitted changes under red-warden-multistream/. Commit first:`n$($dirty -join "`n")" }

$work = Join-Path $env:TEMP "rwsms-publish"
if (Test-Path $work) { Remove-Item -Recurse -Force $work }
New-Item -ItemType Directory -Force $work | Out-Null
$zip = Join-Path $work "export.zip"
$export = Join-Path $work "export"
$clone = Join-Path $work "repo"

git -C $mono archive --format=zip -o $zip "HEAD:red-warden-multistream"
if ($LASTEXITCODE -ne 0) { Fail "git archive failed" }
Expand-Archive $zip $export
foreach ($p in ".github", "brand\concepts") { $x = Join-Path $export $p; if (Test-Path $x) { Remove-Item -Recurse -Force $x } }

git clone -q $Repo $clone
if ($LASTEXITCODE -ne 0) { Fail "clone failed: $Repo" }
Get-ChildItem $clone -Force | Where-Object Name -ne ".git" | Remove-Item -Recurse -Force
Copy-Item (Join-Path $export "*") $clone -Recurse -Force
Get-ChildItem $export -Force -Filter ".*" | Copy-Item -Destination $clone -Recurse -Force   # dotfiles

git -C $clone add -A
git -C $clone -c core.autocrlf=false status --short | Select-Object -First 200
if ($DryRun) { Write-Host "[DRY RUN] staged in $clone - nothing pushed"; exit 0 }

if ($DocsOnly) {
  # README/docs fix between releases: commit to main, no tag (the release tag keeps pointing at the
  # source that was actually built).
  git -C $clone -c user.name="Red Warden Studios" -c user.email="support@redwardenstudios.com" commit -q -m $DocsOnly
  if ($LASTEXITCODE -ne 0) { Fail "commit failed (nothing changed?)" }
  git -C $clone push -q origin main
  if ($LASTEXITCODE -ne 0) { Fail "push failed" }
  Write-Host "[OK] Pushed docs update to $Repo (no tag)"
  exit 0
}
$tag = "v$version"
if (git -C $clone tag --list $tag) { Fail "Tag $tag already exists in the public repo. Bump the version, or use -DocsOnly." }
git -C $clone -c user.name="Red Warden Studios" -c user.email="support@redwardenstudios.com" commit -q -m "Red Warden Multistream $version"
if ($LASTEXITCODE -ne 0) { Fail "commit failed (nothing changed?)" }
git -C $clone branch -M main
git -C $clone tag -a $tag -m "Red Warden Multistream $version"
git -C $clone push -q -u origin main --follow-tags
if ($LASTEXITCODE -ne 0) { Fail "push failed" }
Write-Host "[OK] Published source for $version to $Repo (tag $tag)"
