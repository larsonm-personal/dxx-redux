#!/usr/bin/env pwsh
# extract_mac_demos.ps1 -- Extract game files from Mac StuffIt demo installers.
#
# Requires The Unarchiver CLI installed via android\get_deps\get_unar.sh.
# Usage: .\extract_mac_demos.ps1 [-Force]
param(
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
$RepoRoot = Split-Path $ScriptDir
$DemoDir = Join-Path $ScriptDir "demo installers"

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
    Write-Error "unar not found at $UnarExe`nRun: bash android/get_deps/get_unar.sh"
    exit 1
}

$GameExtensions = @("*.hog", "*.pig", "*.ham", "*.s11", "*.s22", "*.dem", "*.mvl", "*.mn2", "*.msn")

$Installers = @(
    @{
        Archive = "Descent Shareware.sit"
        NestedInstaller = $null
        ExpectFiles = @("descent.hog", "descent.pig")
    },
    @{
        Archive = "Descent II Preview.sit"
        NestedInstaller = "Install Descent II Preview"
        ExpectFiles = @("d2demo.hog", "d2demo.ham", "d2demo.pig", "descent2.s11", "exit.ham")
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
        continue
    }

    if ((Test-Path -LiteralPath $outputDir) -and -not $Force) {
        $existing = Get-GameFiles -RootPath $outputDir
        if ($existing.Count -gt 0) {
            Write-Host ("$baseName already extracted, {0} game files. Use -Force to redo" -f $existing.Count)
            continue
        }
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
    } else {
        $foundNames = @{}
        foreach ($f in $found) { $foundNames[$f.Name.ToLowerInvariant()] = $true }
        $missing = @($inst.ExpectFiles | Where-Object { -not $foundNames.ContainsKey($_.ToLowerInvariant()) })
        if ($missing.Count -gt 0) { Write-Warning "  Missing expected files: $($missing -join ', ')" }
        New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
        foreach ($f in $found) {
            Copy-Item -LiteralPath $f.FullName -Destination (Join-Path $outputDir $f.Name) -Force
            $sizeKB = [math]::Round($f.Length / 1024)
            Write-Host ("    {0} [{1} KB]" -f $f.Name, $sizeKB)
        }
        Write-Host ("  Extracted {0} files -> {1}" -f $found.Count, $outputDir) -ForegroundColor Green
    }

    Remove-Item -LiteralPath $tempBase -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "Done" -ForegroundColor Cyan