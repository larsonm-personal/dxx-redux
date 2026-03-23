# run-clang-format.ps1 -- Run clang-format on android/ C/C++ code only.
# Usage:
#   .\run-clang-format.ps1          # format in-place
#   .\run-clang-format.ps1 --check  # dry-run, exit 1 if changes needed

param(
    [switch]$Check
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot

# --- Locate clang-format ---
$depBaseFile = Join-Path $repoRoot "dependency_base.txt"
if (-not (Test-Path $depBaseFile)) {
    Write-Error "dependency_base.txt not found at $depBaseFile"
    exit 1
}
$DEP_BASE = (Get-Content $depBaseFile -First 1).Trim()

# Load version from tool_versions.conf
$confFile = Join-Path $PSScriptRoot "get_deps\tool_versions.conf"
$cfVersion = $null
foreach ($line in Get-Content $confFile) {
    if ($line -match '^CLANG_FORMAT_VERSION=(.+)$') {
        $cfVersion = $Matches[1]
    }
}

$clangFormat = $null
# Try dep base install
$candidate = Join-Path $DEP_BASE "clang-format-$cfVersion\clang-format.exe"
if (Test-Path $candidate) {
    $clangFormat = $candidate
}
# Fallback: PATH
if (-not $clangFormat) {
    $inPath = Get-Command "clang-format" -ErrorAction SilentlyContinue
    if ($inPath) {
        $clangFormat = $inPath.Source
    }
}
if (-not $clangFormat) {
    Write-Error "clang-format not found. Run android/get_deps/get_clang_format.sh to install."
    exit 1
}
Write-Host "Using: $clangFormat"
& $clangFormat --version

# --- Gather files ---
$cppDir = Join-Path $PSScriptRoot "app\src\main\cpp"

# SDL patch files to exclude (these track upstream SDL)
$excludes = @(
    "SDL_androidaudio.c",
    "SDL_androidaudio.h",
    "SDL_config_android.h"
)

$files = Get-ChildItem -Path $cppDir -Recurse -Include "*.c", "*.cpp", "*.h" |
    Where-Object { $excludes -notcontains $_.Name }

if ($files.Count -eq 0) {
    Write-Host "No files found to format."
    exit 0
}

Write-Host "Found $($files.Count) files to check."

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
    Write-Host "All files are correctly formatted."
} else {
    foreach ($f in $files) {
        & $clangFormat -i --style=file "$($f.FullName)"
    }
    Write-Host "Formatted $($files.Count) files."
}
