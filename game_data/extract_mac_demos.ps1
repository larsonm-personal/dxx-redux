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

function Get-ToolConfigValue {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Name
    )

    foreach ($line in Get-Content -LiteralPath $Path) {
        if ($line -match "^$Name=(.+)$") { return $Matches[1] }
    }
    throw "$Name not found in $Path"
}

$confFile = Join-Path (Join-Path (Join-Path $RepoRoot "android") "get_deps") "tool_versions.conf"
$unarDirName = Get-ToolConfigValue -Path $confFile -Name "UNAR_DIR_NAME"

$_depBaseFile = Join-Path $RepoRoot "dependency_base.txt"
if (-not (Test-Path -LiteralPath $_depBaseFile)) {
    Write-Error "dependency_base.txt not found at $_depBaseFile"
    exit 1
}
$DEP_BASE = (Get-Content -LiteralPath $_depBaseFile -First 1).Trim()
$UnarExe = Join-Path (Join-Path $DEP_BASE $unarDirName) "unar.exe"
if (-not (Test-Path -LiteralPath $UnarExe)) {
    Write-Error "unar not found at $UnarExe`nRun: bash android/get_deps/helpers/get_unar.sh"
    exit 1
}

$GameExtensions = @("*.hog", "*.pig", "*.ham", "*.s11", "*.s22", "*.dem", "*.mvl", "*.mn2", "*.msn")

$Installers = @(
    @{
        Archive = "Descent Shareware.sit"
        NestedInstaller = $null
        ExpectFiles = @("descent.hog", "descent.pig")
        Oracle = $true
    },
    @{
        Archive = "Descent_demo.HQX"
        NestedInstaller = $null
        ExpectFiles = @("descent.hog", "descent.pig")
        Oracle = $false
    },
    @{
        Archive = "descent_demo.sit_.hqx"
        NestedInstaller = $null
        ExpectFiles = @("descent.hog", "descent.pig")
        Oracle = $false
    },
    @{
        Archive = "Descent II Preview.sit"
        NestedInstaller = "Install Descent II Preview"
        ExpectFiles = @("d2demo.hog", "d2demo.ham", "d2demo.pig", "descent2.s11", "exit.ham")
        Oracle = $true
    },
    @{
        Archive = "descent2preview.sit"
        NestedInstaller = $null
        ExpectFiles = @("d2demo.hog", "d2demo.ham", "d2demo.pig", "descent2.s11", "exit.ham")
        Oracle = $true
    },
    @{
        Archive = "descent2preview.sit_.hqx"
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

    if ((Test-ExtractionCompletionManifest -Directory $outputDir) -and -not $Force -and -not $WriteOracle) {
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
                oracle_tool   = "unar"
                oracle_source = "game_data/extract_mac_demos.ps1 -WriteOracle"
                files         = @($oracleFiles)
            }
        }
        [PSCustomObject]@{
            source = $archiveName
            files = @(Get-ChildItem -LiteralPath $stagingDir -File | Sort-Object Name | ForEach-Object {
                    [PSCustomObject]@{
                        name = $_.Name
                        size = $_.Length
                        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                    }
                })
        } | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $stagingDir ".extraction-complete.json") -NoNewline
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
        $oracle | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OracleFile -NoNewline -Encoding utf8
        Write-Host "Wrote StuffIt oracle hashes -> $OracleFile" -ForegroundColor Green
    }
}

Write-Host ""
Write-Host "Done" -ForegroundColor Cyan
if ($failures -gt 0) { exit 1 }
