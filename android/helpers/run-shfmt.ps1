#!/usr/bin/env pwsh
# run-shfmt.ps1 -- Run shfmt on bash scripts in android/.
# Usage:
#   .\run-shfmt.ps1          # format in-place (default)
#   .\run-shfmt.ps1 --check  # dry-run, exit 1 if changes needed
#   .\run-shfmt.ps1 -Paths path\to\file path\to\dir

param(
    [switch]$Check,
    [string[]]$Paths
)

$ErrorActionPreference = "Stop"
$androidRoot = Split-Path $PSScriptRoot
$repoRoot = Split-Path $androidRoot
$platformHelper = Join-Path $androidRoot "get_deps/helpers/Get-DepPlatform.ps1"
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

# --- Locate shfmt ---
$DEP_BASE = Get-DependencyBase -RepoRoot $repoRoot
if (-not $DEP_BASE) {
    $depBaseFile = Join-Path $repoRoot "dependency_base.txt"
    Write-Error "dependency_base.txt not found at $depBaseFile"
    exit 1
}

$confFile = Join-Path $androidRoot "get_deps\tool_versions.conf"
$sfVersion = $null
foreach ($line in Get-Content $confFile) {
    if ($line -match '^SHFMT_VERSION=(.+)$') {
        $sfVersion = $Matches[1]
    }
}

$shfmt = $null
$installDir = Join-Path $DEP_BASE "shfmt-$sfVersion"
$shfmt = Get-PlatformToolPath -BaseDir $installDir -ToolName "shfmt"
if (-not $shfmt) {
    $inPath = Get-Command "shfmt" -ErrorAction SilentlyContinue
    if ($inPath) { $shfmt = $inPath.Source }
}
if (-not $shfmt) {
    Write-Error "shfmt not found. Run android/get_deps/helpers/get_shfmt.sh to install"
    exit 1
}
Write-Host "Using: $shfmt"
& $shfmt --version

# --- Gather .sh files ---
$files = Get-ScopedFiles -RootPath $androidRoot -InputPaths $Paths -ValidExtensions @('.sh')

if ($files.Count -eq 0) {
    Write-Host "No shell scripts found"
    exit 0
}

Write-Host "Found $($files.Count) shell scripts"

# --- Run ---
# shfmt flags: -i 4 (indent=4, matches .editorconfig), -bn (binary ops start of line)
$shfmtArgs = @("-i", "4", "-bn")

if ($Check) {
    $dirty = @()
    $savedPref = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    foreach ($f in $files) {
        & $shfmt @shfmtArgs -d "$($f.FullName)" 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            $dirty += $f.FullName
        }
    }
    $ErrorActionPreference = $savedPref
    if ($dirty.Count -gt 0) {
        Write-Host ""
        Write-Host "Files that need formatting ($($dirty.Count)):"
        foreach ($d in $dirty) {
            $rel = $d.Substring($repoRoot.Length + 1)
            Write-Host "  $rel"
        }
        exit 1
    }
    Write-Host "All shell scripts are correctly formatted"
} else {
    foreach ($f in $files) {
        & $shfmt @shfmtArgs -w "$($f.FullName)"
    }
    Write-Host "shfmt format pass complete"
}
