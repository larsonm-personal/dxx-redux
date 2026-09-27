#!/usr/bin/env pwsh
# get_7zip.ps1 -- Install the pinned standalone 7-Zip console for this host.
# Returns the verified executable path. Uses dependency_base.txt.
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
$hostPlatform = Get-HostPlatform
$executableName = '7za.exe'
$archiveExtension = '7z'
if ($hostPlatform -eq 'Linux') {
    if ([Runtime.InteropServices.RuntimeInformation]::OSArchitecture -ne [Runtime.InteropServices.Architecture]::X64) {
        throw 'The pinned Linux 7-Zip package requires an x64 host'
    }
    $url = $conf['SEVENZIP_LINUX_URL']
    $archiveSha256 = $conf['SEVENZIP_LINUX_ARCHIVE_SHA256']
    $exeSha256 = $conf['SEVENZIP_LINUX_EXE_SHA256']
    $executableName = '7zz'
    $archiveExtension = 'tar.xz'
} elseif ($hostPlatform -ne 'Windows') {
    throw "The repository-pinned 7-Zip package is not available for $hostPlatform"
}
$sevenZa = Join-Path $installDir $executableName
. (Join-Path $repoRoot 'android/helpers/output_disk_space.ps1')

if ((Test-Path $sevenZa) -and -not $Force) {
    $verified = Assert-DxxFileSha256 -Path $sevenZa -ExpectedSha256 $exeSha256 -Label "cached $executableName"
    Write-Host "Using verified ${executableName}: $verified"
    return $verified
}

Write-Host "Downloading 7-Zip $version from $url"

$operationId = [Guid]::NewGuid().ToString('N')
$archivePath = Join-Path $depBase "7z-$operationId.$archiveExtension"
$bootstrapPath = Join-Path $depBase "7zr-$operationId.exe"
$stagingDir = Join-Path $depBase ".${dirName}-$operationId"
$backupDir = Join-Path $depBase ".${dirName}-backup-$operationId"
$lockPath = Join-Path $depBase ".${dirName}.install.lock"
$lock = $null
try {
    $lock = [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    if ((Test-Path -LiteralPath $sevenZa -PathType Leaf) -and -not $Force) {
        return Assert-DxxFileSha256 -Path $sevenZa -ExpectedSha256 $exeSha256 -Label "cached $executableName"
    }

    Assert-OutputDiskSpace -Paths @($archivePath, $stagingDir) -MinimumFreeGB 4
    if ($hostPlatform -eq 'Windows') {
        Invoke-WebRequest -Uri $bootstrapUrl -OutFile $bootstrapPath -UseBasicParsing
        $extractor = Assert-DxxFileSha256 -Path $bootstrapPath -ExpectedSha256 $bootstrapSha256 -Label "7-Zip bootstrap"
    }
    Invoke-WebRequest -Uri $url -OutFile $archivePath -UseBasicParsing
    Assert-DxxFileSha256 -Path $archivePath -ExpectedSha256 $archiveSha256 -Label "7-Zip package" | Out-Null

    New-Item -ItemType Directory -Path $stagingDir | Out-Null
    if ($hostPlatform -eq 'Windows') {
        & $extractor x "-o$stagingDir" -y $archivePath 2>&1 | Out-Null
    } else {
        & tar -xJf $archivePath -C $stagingDir
    }
    if ($LASTEXITCODE -ne 0) {
        throw "7-Zip package extraction failed with exit code $LASTEXITCODE"
    }
    $stagedSevenZa = Join-Path $stagingDir $executableName
    Assert-DxxFileSha256 -Path $stagedSevenZa -ExpectedSha256 $exeSha256 -Label "staged $executableName" | Out-Null

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
    Remove-Item -LiteralPath $archivePath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $bootstrapPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $stagingDir -Recurse -Force -ErrorAction SilentlyContinue
    # Keep the lock inode stable so concurrent installers cannot bypass it
    if ($lock) { $lock.Dispose() }
}

$verifiedSevenZa = Assert-DxxFileSha256 -Path $sevenZa -ExpectedSha256 $exeSha256 -Label "installed $executableName"
Write-Host "$executableName installed and verified: $verifiedSevenZa"
return $verifiedSevenZa
