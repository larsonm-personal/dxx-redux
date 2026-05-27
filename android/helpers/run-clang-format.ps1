#!/usr/bin/env pwsh
# run-clang-format.ps1 -- Run clang-format on android/ C/C++ code only.
# Usage:
#   .\run-clang-format.ps1          # format in-place
#   .\run-clang-format.ps1 --check  # dry-run, exit 1 if changes needed
#   .\run-clang-format.ps1 -Paths path\to\file path\to\dir

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
            ($ValidExtensions -contains $_.Extension.ToLowerInvariant())
        } | Sort-Object FullName -Unique)
}

# --- Locate clang-format ---
$DEP_BASE = Get-DependencyBase -RepoRoot $repoRoot
if (-not $DEP_BASE) {
    $depBaseFile = Join-Path $repoRoot "dependency_base.txt"
    Write-Error "dependency_base.txt not found at $depBaseFile"
    exit 1
}

# Load version from tool_versions.conf
$confFile = Join-Path $androidRoot "get_deps\tool_versions.conf"
$cfVersion = $null
foreach ($line in Get-Content $confFile) {
    if ($line -match '^CLANG_FORMAT_VERSION=(.+)$') {
        $cfVersion = $Matches[1]
    }
}

$clangFormat = $null
$installDir = Join-Path $DEP_BASE "clang-format-$cfVersion"
$clangFormat = Get-PlatformToolPath -BaseDir $installDir -ToolName "clang-format"
# Fallback: PATH
if (-not $clangFormat) {
    $inPath = Get-Command "clang-format" -ErrorAction SilentlyContinue
    if ($inPath) {
        $clangFormat = $inPath.Source
    }
}
if (-not $clangFormat) {
    Write-Error "clang-format not found. Run android/get_deps/helpers/get_clang_format.sh to install"
    exit 1
}
Write-Host "Using: $clangFormat"
& $clangFormat --version

# --- Gather files ---
$cppDir = Join-Path $androidRoot "app\src\main\cpp"

# SDL patch files to exclude (these track upstream SDL)
$excludes = @(
    "SDL_androidaudio.c",
    "SDL_androidaudio.h",
    "SDL_config_android.h"
)

$files = Get-ScopedFiles -RootPath $cppDir -InputPaths $Paths -ValidExtensions @('.c', '.cpp', '.h') |
    Where-Object { $excludes -notcontains $_.Name }

if ($files.Count -eq 0) {
    Write-Host "No files found to format"
    exit 0
}

Write-Host "Found $($files.Count) files to check"

# --- Run ---
if ($Check) {
    $dirty = @()
    $savedPref = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    foreach ($f in $files) {
        & $clangFormat --dry-run --Werror --style=file "$($f.FullName)" 2>$null
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
    Write-Host "All files are correctly formatted"
} else {
    foreach ($f in $files) {
        & $clangFormat -i --style=file "$($f.FullName)"
    }
    Write-Host "Formatted $($files.Count) files"
}
