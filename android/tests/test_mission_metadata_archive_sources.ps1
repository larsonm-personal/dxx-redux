#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot "android\helpers\mission_metadata_archive_sources.ps1")

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) "dxx-mission-sources-$([guid]::NewGuid().ToString('N'))"
try {
    $primary = New-Item -ItemType Directory -Force -Path (Join-Path $testRoot "game_data\mission_files")
    $secondary = New-Item -ItemType Directory -Force -Path (Join-Path $primary.FullName "d2xxl_downloads")
    New-Item -ItemType File -Path (Join-Path $primary.FullName "primary.zip") | Out-Null
    New-Item -ItemType File -Path (Join-Path $secondary.FullName "secondary.7z") | Out-Null

    $sources = @(Get-MissionMetadataArchiveSources -RepoRoot $testRoot)
    Assert-True ($sources.Count -eq 2) "Expected two configured mission archive sources"
    Assert-True ($sources[0].Directory -eq $primary.FullName) "Primary source path is incorrect"
    Assert-True ($sources[1].Directory -eq $secondary.FullName) "D2X-XL source path is incorrect"

    $available = @(Get-AvailableMissionMetadataArchiveSources -Sources $sources)
    Assert-True ($available.Count -eq 2) "Expected both populated mission archive sources"

    Remove-Item -LiteralPath (Join-Path $secondary.FullName "secondary.7z")
    $available = @(Get-AvailableMissionMetadataArchiveSources -Sources $sources)
    Assert-True ($available.Count -eq 1) "An empty optional source should be skipped"

    Remove-Item -LiteralPath (Join-Path $primary.FullName "primary.zip")
    $requiredFailure = $false
    try {
        Get-AvailableMissionMetadataArchiveSources -Sources $sources | Out-Null
    } catch {
        $requiredFailure = $_.Exception.Message -like "No mission archives found*"
    }
    Assert-True $requiredFailure "An empty required source should fail"
} finally {
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}

Write-Host "Mission metadata archive source tests passed"
