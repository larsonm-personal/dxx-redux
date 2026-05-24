# run-shellcheck.ps1 -- Run shellcheck on bash scripts in android/.
# Usage:
#   .\run-shellcheck.ps1          # report issues (shellcheck has no auto-fix)
#   .\run-shellcheck.ps1 --check  # same -- check mode for consistency with other tools
#   .\run-shellcheck.ps1 -Paths path\to\file path\to\dir

param(
    [switch]$Check,
    [string[]]$Paths
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot
$platformHelper = Join-Path $PSScriptRoot "get_deps/Get-DepPlatform.ps1"
. $platformHelper

function Get-ScopedFiles {
    param(
        [string]$RootPath,
        [string[]]$InputPaths,
        [string[]]$ValidExtensions
    )

    $results = @()
    if ($InputPaths -and $InputPaths.Count -gt 0) {
        foreach ($inputPath in $InputPaths) {
            if ([string]::IsNullOrWhiteSpace($inputPath)) {
                continue
            }

            $candidate = $inputPath
            if (-not [System.IO.Path]::IsPathRooted($candidate)) {
                $candidate = Join-Path $repoRoot $candidate
            }

            $item = Get-Item -LiteralPath $candidate -ErrorAction SilentlyContinue
            if (-not $item) {
                continue
            }

            if ($item.PSIsContainer) {
                $results += Get-ChildItem -LiteralPath $item.FullName -Recurse -File
            } else {
                $results += $item
            }
        }
    } else {
        $results = Get-ChildItem -Path $RootPath -Recurse -File
    }

    return @($results | Where-Object {
            $_.FullName.StartsWith($RootPath, [System.StringComparison]::OrdinalIgnoreCase) -and
            ($ValidExtensions -contains $_.Extension.ToLowerInvariant()) -and
            $_.FullName -notmatch '[\\/](build|build-outputs|\.cxx)[\\/]'
        } | Sort-Object FullName -Unique)
}

# --- Locate shellcheck ---
$DEP_BASE = Get-DependencyBase -RepoRoot $repoRoot
if (-not $DEP_BASE) {
    $depBaseFile = Join-Path $repoRoot "dependency_base.txt"
    Write-Error "dependency_base.txt not found at $depBaseFile"
    exit 1
}

$confFile = Join-Path $PSScriptRoot "get_deps\tool_versions.conf"
$scVersion = $null
foreach ($line in Get-Content $confFile) {
    if ($line -match '^SHELLCHECK_VERSION=(.+)$') {
        $scVersion = $Matches[1]
    }
}

$shellcheck = $null
$installDir = Join-Path $DEP_BASE "shellcheck-$scVersion"
$shellcheck = Get-PlatformToolPath -BaseDir $installDir -ToolName "shellcheck"
if (-not $shellcheck) {
    $inPath = Get-Command "shellcheck" -ErrorAction SilentlyContinue
    if ($inPath) { $shellcheck = $inPath.Source }
}
if (-not $shellcheck) {
    Write-Error "shellcheck not found. Run android/get_deps/get_shellcheck.sh to install"
    exit 1
}
Write-Host "Using: $shellcheck"
& $shellcheck --version | Select-Object -First 2

# --- Gather .sh files ---
$files = Get-ScopedFiles -RootPath $PSScriptRoot -InputPaths $Paths -ValidExtensions @('.sh')

if ($files.Count -eq 0) {
    Write-Host "No shell scripts found"
    exit 0
}

Write-Host "Found $($files.Count) shell scripts"

# --- Run ---
# shellcheck has no auto-fix mode; both modes report issues.
# Exclude SC1091: can't follow non-constant source (expected with variable paths).
# Exclude SC2317: "unreachable" functions defined in sourced files.
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
    Write-Host "shellcheck found issues in one or more scripts"
    exit 1
}
Write-Host "All shell scripts pass shellcheck"
