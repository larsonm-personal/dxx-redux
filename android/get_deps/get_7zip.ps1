# get_7zip.ps1 -- Download 7-Zip standalone console (7za.exe) if not present.
# Returns the path to 7za.exe. Uses $DEP_BASE from dependency_base.txt.
#
# Bootstrap: downloads 7zr.exe (single-file extractor from 7-zip.org) first,
# then uses it to extract the full 7z-extra package containing 7za.exe.

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

$version = $conf["SEVENZIP_VERSION"]
$url = $conf["SEVENZIP_URL"]
$dirName = $conf["SEVENZIP_DIR_NAME"]
$installDir = Join-Path $depBase $dirName
$sevenZa = Join-Path $installDir "7za.exe"
$hostPlatform = Get-HostPlatform

if ($hostPlatform -ne "Windows") {
    foreach ($commandName in @("7zz", "7z", "7za")) {
        $existing = Get-Command $commandName -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($existing) {
            Write-Host "Using host $commandName: $($existing.Source)"
            return $existing.Source
        }
    }

    Write-Error "No host 7z binary found on PATH. Install p7zip-full or 7zip and re-run"
}

if ((Test-Path $sevenZa) -and -not $Force) {
    Write-Host "7za.exe already present: $sevenZa"
    return $sevenZa
}

Write-Host "Downloading 7-Zip $version from $url"

if (-not (Test-Path $installDir)) {
    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
}

# Find an existing extractor: 7z or 7za on PATH, or download 7zr.exe bootstrap
$extractor = $null
$existing = Get-Command 7z -ErrorAction SilentlyContinue
if ($existing) { $extractor = $existing.Source }
if (-not $extractor) {
    $existing = Get-Command 7za -ErrorAction SilentlyContinue
    if ($existing) { $extractor = $existing.Source }
}
if (-not $extractor) {
    # Download 7zr.exe -- a standalone single-file 7z extractor from 7-zip.org
    $sevenZrUrl = $conf["SEVENZIP_BOOTSTRAP_URL"]
    $sevenZr = Join-Path $installDir "7zr.exe"
    if (-not (Test-Path $sevenZr)) {
        Write-Host "Downloading bootstrap 7zr.exe..."
        Invoke-WebRequest -Uri $sevenZrUrl -OutFile $sevenZr -UseBasicParsing
    }
    $extractor = $sevenZr
}

$tmpFile = Join-Path $installDir "7z-download.7z"
try {
    Invoke-WebRequest -Uri $url -OutFile $tmpFile -UseBasicParsing
    & $extractor x "-o$installDir" -y $tmpFile 2>&1 | Out-Null
} finally {
    Remove-Item $tmpFile -ErrorAction SilentlyContinue
}

if (-not (Test-Path $sevenZa)) {
    Write-Error "7za.exe not found after extraction at: $sevenZa"
}

Write-Host "7za.exe installed: $sevenZa"
return $sevenZa
