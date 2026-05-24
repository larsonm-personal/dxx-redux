#!/usr/bin/env pwsh
# get_imagemagick.ps1 -- Download portable ImageMagick if not present.
# Returns the path to magick.exe. Uses $DEP_BASE from dependency_base.txt.
# Requires 7-Zip (7za.exe) for extraction -- will auto-fetch via get_7zip.ps1.

param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot "Get-DepPlatform.ps1")
$depBase = Get-DependencyBase -RepoRoot $repoRoot -CreateIfMissing

# Parse version info from tool_versions.conf
$conf = @{}
Get-Content "$PSScriptRoot/tool_versions.conf" | ForEach-Object {
    if ($_ -match '^([A-Z_]+)=(.+)$') {
        $conf[$Matches[1]] = $Matches[2]
    }
}

$version = $conf["IMAGEMAGICK_VERSION"]
$url = $conf["IMAGEMAGICK_URL"]
$dirName = $conf["IMAGEMAGICK_DIR_NAME"]
$installDir = Join-Path $depBase $dirName
$magick = Join-Path $installDir "magick.exe"
$hostPlatform = Get-HostPlatform

if ($hostPlatform -ne "Windows") {
    $hostMagick = Get-Command magick -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($hostMagick) {
        Write-Host "Using host magick: $($hostMagick.Source)"
        return $hostMagick.Source
    }

    Write-Error "ImageMagick is not installed on this host. Install a distro package that provides magick and re-run"
}

if ((Test-Path $magick) -and -not $Force) {
    Write-Host "magick.exe already present: $magick"
    return $magick
}

Write-Host "Downloading ImageMagick $version from $url"

if (-not (Test-Path $installDir)) {
    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
}

# Need 7z to extract .7z archive -- auto-fetch if needed
$sevenZa = & "$PSScriptRoot/get_7zip.ps1"
if (-not $sevenZa -or -not (Test-Path $sevenZa)) {
    Write-Error "Could not obtain 7za.exe for extraction"
}

$tmpFile = Join-Path $installDir "imagemagick-download.7z"
try {
    Invoke-WebRequest -Uri $url -OutFile $tmpFile -UseBasicParsing
    & $sevenZa x "-o$installDir" -y $tmpFile 2>&1 | Out-Null
} finally {
    Remove-Item $tmpFile -ErrorAction SilentlyContinue
}

if (-not (Test-Path $magick)) {
    Write-Error "magick.exe not found after extraction at: $magick"
}

# Verify it runs
$ver = & $magick --version 2>&1 | Select-Object -First 1
Write-Host "ImageMagick installed: $ver"
return $magick
