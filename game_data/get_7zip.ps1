# get_7zip.ps1 -- Download 7-Zip standalone console (7za.exe) if not present.
# Returns the path to 7za.exe. Uses $DEP_BASE from dependency_base.txt.

param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$depBase = (Get-Content "$repoRoot/dependency_base.txt" -Raw).Trim()

# Parse version info from tool_versions.conf
$conf = @{}
Get-Content "$repoRoot/android/get_deps/tool_versions.conf" | ForEach-Object {
    if ($_ -match '^([A-Z_]+)=(.+)$') {
        $conf[$Matches[1]] = $Matches[2]
    }
}

$version = $conf["SEVENZIP_VERSION"]
$url = $conf["SEVENZIP_URL"]
$dirName = $conf["SEVENZIP_DIR_NAME"]
$installDir = Join-Path $depBase $dirName
$sevenZa = Join-Path $installDir "7za.exe"

if ((Test-Path $sevenZa) -and -not $Force) {
    Write-Host "7za.exe already present: $sevenZa"
    return $sevenZa
}

Write-Host "Downloading 7-Zip $version from $url"

# The download is itself a .7z file. We need an existing 7z to extract it,
# OR we can try to find 7z on PATH first. If not available, download the
# .zip variant of the extra package instead.
# Strategy: check if 7z is already on PATH. If so, use it. Otherwise,
# download the zip-compatible installer.
$existing7z = Get-Command 7z -ErrorAction SilentlyContinue
$existing7za = Get-Command 7za -ErrorAction SilentlyContinue

if (-not (Test-Path $installDir)) {
    New-Item -ItemType Directory -Path $installDir -Force | Out-Null
}

$tmpFile = Join-Path $installDir "7z-download.7z"

try {
    Invoke-WebRequest -Uri $url -OutFile $tmpFile -UseBasicParsing

    if ($existing7z) {
        & $existing7z.Source x "-o$installDir" -y $tmpFile 2>&1 | Out-Null
    } elseif ($existing7za) {
        & $existing7za.Source x "-o$installDir" -y $tmpFile 2>&1 | Out-Null
    } else {
        Write-Error "Cannot extract .7z archive: no 7z or 7za found on PATH. Please install 7-Zip first, then re-run"
    }
} finally {
    Remove-Item $tmpFile -ErrorAction SilentlyContinue
}

if (-not (Test-Path $sevenZa)) {
    Write-Error "7za.exe not found after extraction at: $sevenZa"
}

Write-Host "7za.exe installed: $sevenZa"
return $sevenZa
