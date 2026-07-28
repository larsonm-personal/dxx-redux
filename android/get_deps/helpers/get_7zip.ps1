#!/usr/bin/env pwsh
# get_7zip.ps1 -- Download 7-Zip standalone console (7za.exe) if not present.
# Returns the path to 7za.exe. Uses $DEP_BASE from dependency_base.txt.
#
# Bootstrap: downloads 7zr.exe (single-file extractor from 7-zip.org) first,
# then uses it to extract the full 7z-extra package containing 7za.exe.

param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path (Split-Path (Split-Path $PSScriptRoot))
. (Join-Path $PSScriptRoot "Get-DepPlatform.ps1")
. (Join-Path $repoRoot "android\helpers\verified_dependencies.ps1")
$depBase = Get-DependencyBase -RepoRoot $repoRoot -CreateIfMissing

$conf = Read-DxxDependencyConfig -RepoRoot $repoRoot

$version = $conf["SEVENZIP_VERSION"]
$url = $conf["SEVENZIP_URL"]
$archiveSha256 = $conf["SEVENZIP_ARCHIVE_SHA256"]
$bootstrapUrl = $conf["SEVENZIP_BOOTSTRAP_URL"]
$bootstrapSha256 = $conf["SEVENZIP_BOOTSTRAP_SHA256"]
$exeSha256 = $conf["SEVENZIP_EXE_SHA256"]
$dirName = $conf["SEVENZIP_DIR_NAME"]
$installDir = Join-Path $depBase $dirName
$sevenZa = Join-Path $installDir "7za.exe"
$hostPlatform = Get-HostPlatform

if ($hostPlatform -ne "Windows") {
    Write-Error "The repository-pinned 7-Zip package is currently available only for Windows"
}

if ((Test-Path $sevenZa) -and -not $Force) {
    $verified = Assert-DxxFileSha256 -Path $sevenZa -ExpectedSha256 $exeSha256 -Label "cached 7za.exe"
    Write-Host "Using verified 7za.exe: $verified"
    return $verified
}

Write-Host "Downloading 7-Zip $version from $url"

$operationId = [Guid]::NewGuid().ToString('N')
$archivePath = Join-Path $depBase "7z-$operationId.7z"
$bootstrapPath = Join-Path $depBase "7zr-$operationId.exe"
$stagingDir = Join-Path $depBase ".${dirName}-$operationId"
$backupDir = Join-Path $depBase ".${dirName}-backup-$operationId"
$lockPath = Join-Path $depBase ".${dirName}.install.lock"
$lock = $null
try {
    $lock = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    if ((Test-Path -LiteralPath $sevenZa -PathType Leaf) -and -not $Force) {
        return Assert-DxxFileSha256 -Path $sevenZa -ExpectedSha256 $exeSha256 -Label "cached 7za.exe"
    }

    Invoke-WebRequest -Uri $bootstrapUrl -OutFile $bootstrapPath -UseBasicParsing
    $extractor = Assert-DxxFileSha256 -Path $bootstrapPath -ExpectedSha256 $bootstrapSha256 -Label "7-Zip bootstrap"
    Invoke-WebRequest -Uri $url -OutFile $archivePath -UseBasicParsing
    Assert-DxxFileSha256 -Path $archivePath -ExpectedSha256 $archiveSha256 -Label "7-Zip package" | Out-Null

    New-Item -ItemType Directory -Path $stagingDir | Out-Null
    & $extractor x "-o$stagingDir" -y $archivePath 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "7-Zip bootstrap extraction failed with exit code $LASTEXITCODE"
    }
    $stagedSevenZa = Join-Path $stagingDir "7za.exe"
    Assert-DxxFileSha256 -Path $stagedSevenZa -ExpectedSha256 $exeSha256 -Label "staged 7za.exe" | Out-Null

    if (Test-Path -LiteralPath $installDir) {
        Move-Item -LiteralPath $installDir -Destination $backupDir
    }
    try {
        Move-Item -LiteralPath $stagingDir -Destination $installDir
    } catch {
        if (Test-Path -LiteralPath $backupDir) {
            Move-Item -LiteralPath $backupDir -Destination $installDir
        }
        throw
    }
    if (Test-Path -LiteralPath $backupDir) {
        Remove-Item -LiteralPath $backupDir -Recurse -Force
    }
} finally {
    if ($lock) { $lock.Dispose() }
    Remove-Item -LiteralPath $archivePath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $bootstrapPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $stagingDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $lockPath -Force -ErrorAction SilentlyContinue
}

$verifiedSevenZa = Assert-DxxFileSha256 -Path $sevenZa -ExpectedSha256 $exeSha256 -Label "installed 7za.exe"
Write-Host "7za.exe installed and verified: $verifiedSevenZa"
return $verifiedSevenZa
