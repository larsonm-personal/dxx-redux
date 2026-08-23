#!/usr/bin/env pwsh
# extract_mac_demos.ps1 -- Extract game files from Mac StuffIt demo installers.
#
# Requires The Unarchiver CLI installed via android\get_deps\get_unar.sh.
# Usage: .\extract_mac_demos.ps1 [-Force] [-WriteOracle]
param(
    [switch]$Force,
    [switch]$WriteOracle
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$RepoRoot = Split-Path $ScriptDir
$DemoDir = Join-Path $ScriptDir "demo installers"
$OracleFile = Join-Path $DemoDir "mac_stuffit_oracles.json"
$OracleArchives = @()
$OracleFailures = @()
$failures = 0
. (Join-Path $RepoRoot 'android\helpers\bounded_extraction.ps1')
. (Join-Path $RepoRoot 'android\helpers\verified_dependencies.ps1')

$UnarExe = Resolve-DxxVerifiedDependencyExecutable -RepoRoot $RepoRoot `
    -DirectoryKey 'UNAR_DIR_NAME' -RelativePath 'unar.exe' `
    -Sha256Key 'UNAR_EXE_SHA256' -Label 'unar'

$GameExtensions = @("*.hog", "*.pig", "*.ham", "*.s11", "*.s22", "*.dem", "*.mvl", "*.mn2", "*.msn")

$Installers = @(
    @{
        Archive = "Descent Shareware.sit"
        Sha256 = "f45c338df4bc4ceda38e6541f14b8dc93b543fd07d90a2c5d5118d2001c12ad2"
        NestedInstaller = $null
        ExpectFiles = @("descent.hog", "descent.pig")
        Oracle = $true
    },
    @{
        Archive = "Descent_demo.HQX"
        Sha256 = "e485a1570cb6079d3ec55a52ed9150792f5ef450b653e5db9748a305fed2dfe4"
        NestedInstaller = $null
        ExpectFiles = @("descent.hog", "descent.pig")
        Oracle = $false
    },
    @{
        Archive = "descent_demo.sit_.hqx"
        Sha256 = "87375e89e71f5d43e342ec5666f71347fe2797f2a80838c00dac71f1ae181ebe"
        NestedInstaller = $null
        ExpectFiles = @("descent.hog", "descent.pig")
        Oracle = $false
    },
    @{
        Archive = "Descent II Preview.sit"
        Sha256 = "4b5b7739b9da59472bcdca92f23957f90247bedd84ef8bded57d37d5d229f6d6"
        NestedInstaller = "Install Descent II Preview"
        ExpectFiles = @("d2demo.hog", "d2demo.ham", "d2demo.pig", "descent2.s11", "exit.ham")
        Oracle = $true
    },
    @{
        Archive = "descent2preview.sit"
        Sha256 = "5b9c359e47e4e458f655ef5a28e6110ea1deee60d79a08f7ebdb2144ec9263fd"
        NestedInstaller = $null
        ExpectFiles = @("d2demo.hog", "d2demo.ham", "d2demo.pig", "descent2.s11", "exit.ham")
        Oracle = $true
    },
    @{
        Archive = "descent2preview.sit_.hqx"
        Sha256 = "b7c55f60f11a1d0d72658f8a30fecdebef9251e0e86eeff747888fc4f56fcd19"
        NestedInstaller = $null
        ExpectFiles = @("d2demo.hog", "d2demo.ham", "d2demo.pig", "descent2.s11", "exit.ham")
        Oracle = $false
    }
)

function Invoke-UnarExtract {
    param(
        [Parameter(Mandatory = $true)][string]$ArchivePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath
    )

    New-Item -ItemType Directory -Force -Path $DestinationPath | Out-Null
    & $UnarExe -quiet -force-overwrite -output-directory $DestinationPath $ArchivePath | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "unar failed for $ArchivePath" }
}

function Get-GameFiles {
    param([Parameter(Mandatory = $true)][string]$RootPath)

    $found = @()
    foreach ($ext in $GameExtensions) {
        $found += Get-ChildItem -LiteralPath $RootPath -Recurse -Filter $ext -File -ErrorAction SilentlyContinue
    }
    return $found | Sort-Object Name, FullName
}

foreach ($inst in $Installers) {
    $archiveName = $inst.Archive
    $archivePath = Join-Path $DemoDir $archiveName
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($archiveName)
    $outputDir = Join-Path $DemoDir "${baseName}_extracted"

    if (-not (Test-Path -LiteralPath $archivePath)) {
        Write-Warning "Archive not found, skipping: $archivePath"
        if ($WriteOracle -and $inst.Oracle) { $OracleFailures += $archiveName }
        continue
    }
    Assert-DxxFileSha256 -Path $archivePath -ExpectedSha256 $inst.Sha256 `
        -Label "$archiveName demo package" | Out-Null
    $provenance = New-ExtractionProvenance -Policy 'extract-mac-demos-v1' -Sources @(
        (Get-ExtractionPathIdentity -Path $archivePath -Name $archiveName)
    ) -Tools @(
        (Get-ExtractionPathIdentity -Path $UnarExe -Name 'unar'),
        (Get-ExtractionPathIdentity -Path $PSCommandPath -Name 'extract_mac_demos.ps1')
    )

    if ((Test-ExtractionCompletionManifest -Directory $outputDir -ExpectedProvenance $provenance) -and
        -not $Force -and -not $WriteOracle) {
        Write-Host "$baseName already extracted. Use -Force to redo"
        continue
    }

    Write-Host "`n=== $baseName ===" -ForegroundColor Cyan
    $safeName = $baseName -replace '[^a-zA-Z0-9_-]', '_'
    $tempBase = Join-Path $env:TEMP "dxx_mac_demo_$safeName"
    $sourceDir = Join-Path $tempBase "src"
    $nestedDir = Join-Path $tempBase "nested"

    if (Test-Path -LiteralPath $tempBase) { Remove-Item -LiteralPath $tempBase -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $sourceDir | Out-Null

    Write-Host "  Extracting $archiveName"
    Invoke-UnarExtract -ArchivePath $archivePath -DestinationPath $sourceDir

    $scanDir = $sourceDir
    if ($inst.NestedInstaller) {
        $nestedInstaller =
        Get-ChildItem -LiteralPath $sourceDir -Recurse -File |
            Where-Object { $_.Name -eq $inst.NestedInstaller } |
            Select-Object -First 1
        if (-not $nestedInstaller) {
            Write-Warning "  Nested installer not found: $($inst.NestedInstaller)"
            $failures++
            if ($WriteOracle -and $inst.Oracle) { $OracleFailures += $archiveName }
            Remove-Item -LiteralPath $tempBase -Recurse -Force -ErrorAction SilentlyContinue
            continue
        }
        Write-Host "  Extracting nested installer $($inst.NestedInstaller)"
        Invoke-UnarExtract -ArchivePath $nestedInstaller.FullName -DestinationPath $nestedDir
        $scanDir = $nestedDir
    }

    $found = Get-GameFiles -RootPath $scanDir
    if ($found.Count -eq 0) {
        Write-Warning "  No game files found"
        $failures++
        if ($WriteOracle -and $inst.Oracle) { $OracleFailures += $archiveName }
    } else {
        $foundNames = @{}
        $collision = $false
        foreach ($f in $found) {
            $key = $f.Name.ToLowerInvariant()
            if ($foundNames.ContainsKey($key)) { $collision = $true }
            $foundNames[$key] = $f
        }
        $missing = @($inst.ExpectFiles | Where-Object { -not $foundNames.ContainsKey($_.ToLowerInvariant()) })
        if ($missing.Count -gt 0 -or $collision) {
            Write-Warning "  Missing expected files or colliding basenames: $($missing -join ', ')"
            $failures++
            if ($WriteOracle -and $inst.Oracle) { $OracleFailures += $archiveName }
            Remove-Item -LiteralPath $tempBase -Recurse -Force -ErrorAction SilentlyContinue
            continue
        }
        $stagingDir = Join-Path $DemoDir ".${baseName}_extracted-$([Guid]::NewGuid().ToString('N'))"
        New-Item -ItemType Directory -Path $stagingDir | Out-Null
        $oracleFiles = @()
        foreach ($f in $foundNames.Values) {
            Copy-Item -LiteralPath $f.FullName -Destination (Join-Path $stagingDir $f.Name)
            $sha256 = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            $oracleFiles += [PSCustomObject]@{ file = $f.Name; sha256 = $sha256; size = $f.Length }
            $sizeKB = [math]::Round($f.Length / 1024)
            Write-Host ("    {0} [{1} KB]" -f $f.Name, $sizeKB)
        }
        if ($WriteOracle -and $inst.Oracle) {
            $OracleArchives += [PSCustomObject]@{
                archive       = $archiveName
                archive_sha256 = $provenance.sources[0].sha256
                oracle_tool   = $provenance.tools[0]
                policy        = $provenance.policy
                oracle_source = "game_data/extract_mac_demos.ps1 -WriteOracle"
                files         = @($oracleFiles)
            }
        }
        Write-ExtractionCompletionManifest -Directory $stagingDir -Provenance $provenance
        Publish-ExtractionDirectory -StagingDirectory $stagingDir -DestinationDirectory $outputDir
        Write-Host ("  Extracted {0} files -> {1}" -f $found.Count, $outputDir) -ForegroundColor Green
    }

    Remove-Item -LiteralPath $tempBase -Recurse -Force -ErrorAction SilentlyContinue
}

if ($WriteOracle) {
    if ($OracleArchives.Count -eq 0 -or $OracleFailures.Count -gt 0) {
        Write-Warning "No oracle archives were extracted, leaving $OracleFile unchanged"
    } else {
        $oracle = [PSCustomObject]@{ archives = @($OracleArchives) }
        [IO.File]::WriteAllText($OracleFile, ($oracle | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
        Write-Host "Wrote StuffIt oracle hashes -> $OracleFile" -ForegroundColor Green
    }
}

Write-Host ""
Write-Host "Done" -ForegroundColor Cyan
if ($failures -gt 0) { exit 1 }
