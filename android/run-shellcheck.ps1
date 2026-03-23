# run-shellcheck.ps1 -- Run shellcheck on bash scripts in android/.
# Usage:
#   .\run-shellcheck.ps1          # report issues (shellcheck has no auto-fix)
#   .\run-shellcheck.ps1 --check  # same -- check mode for consistency with other tools

param(
    [switch]$Check
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot

# --- Locate shellcheck ---
$depBaseFile = Join-Path $repoRoot "dependency_base.txt"
if (-not (Test-Path $depBaseFile)) {
    Write-Error "dependency_base.txt not found at $depBaseFile"
    exit 1
}
$DEP_BASE = (Get-Content $depBaseFile -First 1).Trim()

$confFile = Join-Path $PSScriptRoot "get_deps\tool_versions.conf"
$scVersion = $null
foreach ($line in Get-Content $confFile) {
    if ($line -match '^SHELLCHECK_VERSION=(.+)$') {
        $scVersion = $Matches[1]
    }
}

$shellcheck = $null
$candidate = Join-Path $DEP_BASE "shellcheck-$scVersion\shellcheck.exe"
if (Test-Path $candidate) {
    $shellcheck = $candidate
}
if (-not $shellcheck) {
    $inPath = Get-Command "shellcheck" -ErrorAction SilentlyContinue
    if ($inPath) { $shellcheck = $inPath.Source }
}
if (-not $shellcheck) {
    Write-Error "shellcheck not found. Run android/get_deps/get_shellcheck.sh to install."
    exit 1
}
Write-Host "Using: $shellcheck"
& $shellcheck --version | Select-Object -First 2

# --- Gather .sh files ---
$files = Get-ChildItem -Path $PSScriptRoot -Recurse -Include "*.sh" |
    Where-Object { $_.FullName -notmatch '[\\\\/](build|build-outputs|\.cxx)[\\\\/]' }

if ($files.Count -eq 0) {
    Write-Host "No shell scripts found."
    exit 0
}

Write-Host "Found $($files.Count) shell scripts."

# --- Run ---
# shellcheck has no auto-fix mode; both modes report issues.
# Exclude SC1091: can't follow non-constant source (expected with variable paths).
# Exclude SC2317: "unreachable" functions defined in source'd files.
$hasIssues = $false
$savedPref = $ErrorActionPreference
$ErrorActionPreference = "Continue"
foreach ($f in $files) {
    & $shellcheck -f gcc -x -e "SC1091,SC2317" "$($f.FullName)" 2>&1
    if ($LASTEXITCODE -ne 0) {
        $hasIssues = $true
    }
}
$ErrorActionPreference = $savedPref

if ($hasIssues) {
    Write-Host ""
    Write-Host "shellcheck found issues in one or more scripts."
    exit 1
}
Write-Host "All shell scripts pass shellcheck."
