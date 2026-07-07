#!/usr/bin/env pwsh
param(
    [string]$CorpusDir,
    [string]$OutputDir,
    [string]$ToolDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$AndroidRoot = Split-Path $PSScriptRoot -Parent
$RepoRoot = Split-Path $AndroidRoot -Parent
$ManifestSchemaVersion = 2
$UnarPackageUrl = 'https://cdn.theunarchiver.com/downloads/unarWindows.zip'
$UnarPackageSha256 = '61a6b299606282f72f51c278801eac11d3dccfac83e2d68bccce33539912e0dd'
$SelectedArchives = @(
    'testfile.stuffit45_dlx.mac9.sit',
    'testfile.stuffit7.win.sit',
    'testfile.stuffit651_dlx.mac9.sit',
    'testfile.stuffit651_dlx.macx1.sit',
    'testfile.stuffit7_dlx.mac9.sit',
    'testfile.stuffit7_dlx.macx1.sit'
)

function Write-Status {
    param(
        [string]$Message,
        [ConsoleColor]$Color = [ConsoleColor]::Gray
    )

    Write-Host $Message -ForegroundColor $Color
}

function To-JsonString {
    param([string]$Value)

    return ($Value | ConvertTo-Json -Compress)
}

function Write-Utf8NoBomFile {
    param(
        [string]$Path,
        [string]$Text
    )

    $normalized = $Text -replace "`r`n", "`n"
    [System.IO.File]::WriteAllText($Path, $normalized, [System.Text.UTF8Encoding]::new($false))
}

function Resolve-CorpusBuildDir {
    param([string]$Candidate)

    if ([string]::IsNullOrWhiteSpace($Candidate)) {
        return $null
    }

    $resolved = (Resolve-Path $Candidate).Path
    if (Test-Path (Join-Path $resolved 'build')) {
        return (Join-Path $resolved 'build')
    }
    if (Test-Path (Join-Path $resolved 'testfile.stuffit7.win.sit')) {
        return $resolved
    }

    throw "Corpus path '$Candidate' does not contain a build directory or StuffIt samples"
}

function Find-DefaultCorpusBuildDir {
    $candidates = @(
        (Join-Path $RepoRoot 'temp\stuffit-test-files'),
        (Join-Path $AndroidRoot 'tests\build\_deps\stuffit_test_files-src'),
        (Join-Path $RepoRoot 'android\tests\build\_deps\stuffit_test_files-src')
    )

    foreach ($candidate in $candidates) {
        if (-not (Test-Path $candidate)) {
            continue
        }

        try {
            return Resolve-CorpusBuildDir -Candidate $candidate
        } catch {
            continue
        }
    }

    throw 'Could not find the stuffit-test-files corpus checkout'
}

function Resolve-ToolPaths {
    param([string]$CandidateToolDir)

    if (-not [string]::IsNullOrWhiteSpace($CandidateToolDir)) {
        $resolved = (Resolve-Path $CandidateToolDir).Path
        $unarExe = Join-Path $resolved 'unar.exe'
        $lsarExe = Join-Path $resolved 'lsar.exe'
        if ((Test-Path $unarExe) -and (Test-Path $lsarExe)) {
            return [pscustomobject]@{
                UnarExe = $unarExe
                LsarExe = $lsarExe
                PackageUrl = $UnarPackageUrl
                PackageSha256 = $UnarPackageSha256
            }
        }

        throw "ToolDir '$CandidateToolDir' does not contain unar.exe and lsar.exe"
    }

    $toolRoot = Join-Path $RepoRoot 'temp\stuffit_manifest_tools'
    $zipPath = Join-Path $toolRoot 'unarWindows.zip'
    $extractDir = Join-Path $toolRoot 'unarWindows'
    $unarExe = Join-Path $extractDir 'unar.exe'
    $lsarExe = Join-Path $extractDir 'lsar.exe'

    New-Item -ItemType Directory -Path $toolRoot -Force | Out-Null
    if (-not (Test-Path $zipPath)) {
        Write-Status 'Downloading unar Windows package' Cyan
        Invoke-WebRequest -Uri $UnarPackageUrl -OutFile $zipPath
    }

    $actualHash = (Get-FileHash $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -ne $UnarPackageSha256) {
        throw "unar Windows package hash mismatch expected=$UnarPackageSha256 actual=$actualHash"
    }

    if (-not ((Test-Path $unarExe) -and (Test-Path $lsarExe))) {
        if (Test-Path $extractDir) {
            Remove-Item $extractDir -Recurse -Force
        }
        Expand-Archive -Path $zipPath -DestinationPath $extractDir
    }

    return [pscustomobject]@{
        UnarExe = $unarExe
        LsarExe = $lsarExe
        PackageUrl = $UnarPackageUrl
        PackageSha256 = $UnarPackageSha256
    }
}

function Get-ToolVersion {
    param([string]$UnarExe)

    $line = (& $UnarExe -v 2>$null | Select-Object -First 1)
    if (-not $line) {
        return 'unknown'
    }

    return $line.Trim()
}

function Get-ExtractedRoot {
    param(
        [string]$WorkingDir,
        [string]$ArchiveBaseName
    )

    $candidate = Join-Path $WorkingDir $ArchiveBaseName
    if (Test-Path $candidate) {
        return $candidate
    }

    return $WorkingDir
}

function Get-LsarContents {
    param(
        [string]$LsarExe,
        [string]$ArchivePath
    )

    $json = (& $LsarExe -j $ArchivePath 2>$null | Out-String)
    $root = $json | ConvertFrom-Json
    return @($root.lsarContents)
}

function Get-ManifestEntries {
    param(
        [object[]]$LsarContents,
        [string]$RootDir
    )

    foreach ($item in ($LsarContents | Sort-Object XADFileName, XADIsResourceFork, XADIsDirectory)) {
        $logicalPath = $item.XADFileName.Replace('\', '/')
        $kind = if ($item.PSObject.Properties.Name -contains 'XADIsDirectory' -and $item.XADIsDirectory) {
            'directory'
        } elseif ($item.PSObject.Properties.Name -contains 'XADIsResourceFork' -and $item.XADIsResourceFork) {
            'resource'
        } else {
            'file'
        }
        $actualPath = $null
        $sha256 = $null
        $size = if ($kind -eq 'directory') { 0 } else { [int64]$item.XADFileSize }
        $method = $null
        $archiveDataOffset = $null
        $archiveDataLength = $null

        if ($kind -ne 'directory') {
            $actualRelativePath = if ($kind -eq 'resource') {
                $logicalPath + '.rsrc'
            } else {
                $logicalPath
            }
            $actualPath = Join-Path $RootDir ($actualRelativePath -replace '/', [System.IO.Path]::DirectorySeparatorChar)
            if (-not (Test-Path $actualPath)) {
                throw "Expected extracted file not found: $actualPath"
            }
            $sha256 = (Get-FileHash $actualPath -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($item.PSObject.Properties.Name -contains 'StuffItCompressionMethod') {
                $method = [int64]$item.StuffItCompressionMethod
            }
            if ($item.PSObject.Properties.Name -contains 'XADDataOffset') {
                $archiveDataOffset = [int64]$item.XADDataOffset
            }
            if ($item.PSObject.Properties.Name -contains 'XADDataLength') {
                $archiveDataLength = [int64]$item.XADDataLength
            } elseif ($item.PSObject.Properties.Name -contains 'XADCompressedSize') {
                $archiveDataLength = [int64]$item.XADCompressedSize
            }
        }

        [pscustomobject]@{
            Path = $logicalPath
            Kind = $kind
            Size = $size
            Sha256 = $sha256
            Method = $method
            ArchiveDataOffset = $archiveDataOffset
            ArchiveDataLength = $archiveDataLength
        }
    }
}

function Write-ManifestFile {
    param(
        [string]$ArchiveName,
        [string]$DestinationPath,
        [string]$ToolVersion,
        [string]$PackageUrl,
        [string]$PackageSha256,
        [object[]]$Entries
    )

    $lines = @(
        '{',
        ('  "schema_version": {0},' -f $ManifestSchemaVersion),
        ('  "archive_name": {0},' -f (To-JsonString $ArchiveName)),
        '  "tool_name": "unar",',
        ('  "tool_version": {0},' -f (To-JsonString $ToolVersion)),
        ('  "tool_package_url": {0},' -f (To-JsonString $PackageUrl)),
        ('  "tool_package_sha256": {0},' -f (To-JsonString $PackageSha256)),
        '  "entries": ['
    )

    for ($i = 0; $i -lt $Entries.Count; $i++) {
        $entry = $Entries[$i]
        $shaJson = if ($null -eq $entry.Sha256) { 'null' } else { To-JsonString $entry.Sha256 }
        $methodJson = if ($null -eq $entry.Method) { 'null' } else { $entry.Method }
        $offsetJson = if ($null -eq $entry.ArchiveDataOffset) { 'null' } else { $entry.ArchiveDataOffset }
        $lengthJson = if ($null -eq $entry.ArchiveDataLength) { 'null' } else { $entry.ArchiveDataLength }
        $comma = if ($i -lt $Entries.Count - 1) { ',' } else { '' }
        $lines += ('    {{"path":{0},"kind":{1},"size":{2},"sha256":{3},"method":{4},"archive_data_offset":{5},"archive_data_length":{6}}}{7}' -f
            (To-JsonString $entry.Path),
            (To-JsonString $entry.Kind),
            $entry.Size,
            $shaJson,
            $methodJson,
            $offsetJson,
            $lengthJson,
            $comma)
    }

    $lines += '  ]'
    $lines += '}'

    Write-Utf8NoBomFile -Path $DestinationPath -Text (($lines -join "`n") + "`n")
}

if (-not $IsWindows) {
    throw 'This script currently supports Windows only because it pins the Windows unar package'
}

$corpusBuildDir = if ($CorpusDir) {
    Resolve-CorpusBuildDir -Candidate $CorpusDir
} else {
    Find-DefaultCorpusBuildDir
}

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $AndroidRoot 'app\src\main\cpp\extract\test\data\stuffit_manifests'
}

$tools = Resolve-ToolPaths -CandidateToolDir $ToolDir
$toolVersion = Get-ToolVersion -UnarExe $tools.UnarExe
$workRoot = Join-Path $RepoRoot 'temp\stuffit_manifest_work'

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
if (Test-Path $workRoot) {
    Remove-Item $workRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $workRoot -Force | Out-Null

Write-Status "Using corpus build dir $corpusBuildDir" Cyan
Write-Status "Writing manifests to $OutputDir" Cyan

foreach ($archiveName in $SelectedArchives) {
    $archivePath = Join-Path $corpusBuildDir $archiveName
    $archiveWorkDir = Join-Path $workRoot ([System.IO.Path]::GetFileNameWithoutExtension($archiveName))
    $archiveBaseName = [System.IO.Path]::GetFileNameWithoutExtension($archiveName)
    $rootDir = $null
    $lsarContents = @()
    $entries = @()
    $manifestPath = Join-Path $OutputDir ($archiveName + '.json')

    if (-not (Test-Path $archivePath)) {
        throw "Archive not found: $archivePath"
    }

    if (Test-Path $archiveWorkDir) {
        Remove-Item $archiveWorkDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $archiveWorkDir -Force | Out-Null

    Write-Status "Extracting $archiveName" Yellow
    & $tools.UnarExe -f -q -o $archiveWorkDir $archivePath 2>$null | Out-Null

    $rootDir = Get-ExtractedRoot -WorkingDir $archiveWorkDir -ArchiveBaseName $archiveBaseName
    $lsarContents = @(Get-LsarContents -LsarExe $tools.LsarExe -ArchivePath $archivePath)
    $entries = @(Get-ManifestEntries -LsarContents $lsarContents -RootDir $rootDir)
    Write-ManifestFile -ArchiveName $archiveName `
        -DestinationPath $manifestPath `
        -ToolVersion $toolVersion `
        -PackageUrl $tools.PackageUrl `
        -PackageSha256 $tools.PackageSha256 `
        -Entries $entries
    Write-Status "Wrote $manifestPath" Green
}

Write-Status 'StuffIt corpus manifests generated successfully' Green
