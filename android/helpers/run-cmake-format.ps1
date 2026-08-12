#!/usr/bin/env pwsh
# run-cmake-format.ps1 -- Run cmake-format (cheshirekow/cmakelang) on cmake files
# added by this branch. Skips upstream d1/ and d2/ CMakeLists.txt files.
# Usage:
#   .\run-cmake-format.ps1          # format in-place (default)
#   .\run-cmake-format.ps1 --check  # dry-run, exit 1 if changes needed
#   .\run-cmake-format.ps1 -Paths path\to\file path\to\dir

param(
    [switch]$Check,
    [string[]]$Paths
)

$ErrorActionPreference = "Stop"
$androidRoot = Split-Path $PSScriptRoot
$repoRoot = Split-Path $androidRoot

# --- In-scope cmake files (added by the cmake branch) ---
# These globs are evaluated relative to $repoRoot. d1/, d2/ are excluded.
$inScopeGlobs = @(
    "android\app\src\main\cpp\CMakeLists.txt",
    "android\app\src\main\cpp\extract\CMakeLists.txt",
    "android\tests\CMakeLists.txt",
    "cmake\*.cmake",
    "android\tools\etc2tool\CMakeLists.txt"
)

function Resolve-InScopeFiles {
    $all = @()
    foreach ($g in $inScopeGlobs) {
        $matches = Get-ChildItem -Path (Join-Path $repoRoot $g) -ErrorAction SilentlyContinue
        if ($matches) { $all += $matches }
    }
    return @($all | Sort-Object FullName -Unique)
}

function Filter-ToInputPaths {
    param([System.IO.FileInfo[]]$AllFiles, [string[]]$InputPaths)
    if (-not $InputPaths -or $InputPaths.Count -eq 0) { return $AllFiles }
    $resolvedInputs = @()
    foreach ($p in $InputPaths) {
        if ([string]::IsNullOrWhiteSpace($p)) { continue }
        $candidate = $p
        if (-not [System.IO.Path]::IsPathRooted($candidate)) {
            $candidate = Join-Path $repoRoot $candidate
        }
        $item = Get-Item -LiteralPath $candidate -ErrorAction SilentlyContinue
        if ($item) { $resolvedInputs += $item }
    }
    if ($resolvedInputs.Count -eq 0) { return @() }
    return @($AllFiles | Where-Object {
            $f = $_
            foreach ($r in $resolvedInputs) {
                if ($r.PSIsContainer) {
                    if ($f.FullName.StartsWith($r.FullName, [System.StringComparison]::OrdinalIgnoreCase)) { return $true }
                } elseif ($f.FullName -ieq $r.FullName) { return $true }
            }
            return $false
        })
}

# --- Locate cmake-format ---
$depBaseFile = Join-Path $repoRoot "dependency_base.txt"
if (-not (Test-Path $depBaseFile)) {
    Write-Error "dependency_base.txt not found at $depBaseFile"
    exit 1
}
$DEP_BASE = (Get-Content $depBaseFile -First 1).Trim()

$confFile = Join-Path $androidRoot "get_deps\tool_versions.conf"
$cmVersion = $null
foreach ($line in Get-Content $confFile) {
    if ($line -match '^CMAKELANG_VERSION=(.+)$') { $cmVersion = $Matches[1] }
}

$cmakeFormat = $null
foreach ($candidate in @(
        (Join-Path $DEP_BASE "cmakelang-$cmVersion\python\Scripts\cmake-format.exe"),
        (Join-Path $DEP_BASE "cmakelang-$cmVersion\venv\bin\cmake-format")
    )) {
    if (Test-Path $candidate) { $cmakeFormat = $candidate; break }
}
if (-not $cmakeFormat) {
    $inPath = Get-Command "cmake-format" -ErrorAction SilentlyContinue
    if ($inPath) { $cmakeFormat = $inPath.Source }
}
if (-not $cmakeFormat) {
    Write-Error "cmake-format not found. Run android/get_deps/helpers/get_cmake_format.sh to install"
    exit 1
}
Write-Host "Using: $cmakeFormat"
& $cmakeFormat --version

# --- Gather files ---
$files = Filter-ToInputPaths -AllFiles (Resolve-InScopeFiles) -InputPaths $Paths
if ($files.Count -eq 0) {
    Write-Host "No cmake files in scope to format"
    exit 0
}
Write-Host "Found $($files.Count) cmake files"

# --- Run ---
if ($Check) {
    $dirty = @()
    $savedPref = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    foreach ($f in $files) {
        & $cmakeFormat --check "$($f.FullName)" 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) { $dirty += $f.FullName }
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
    Write-Host "All cmake files are correctly formatted"
} else {
    foreach ($f in $files) {
        & $cmakeFormat -i "$($f.FullName)"
    }
    Write-Host "Formatted $($files.Count) cmake files"
}
