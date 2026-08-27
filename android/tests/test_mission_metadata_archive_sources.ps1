#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot "android\helpers\mission_archive_sources.ps1")

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) "dxx-mission-sources-$([guid]::NewGuid().ToString('N'))"
try {
    $primary = New-Item -ItemType Directory -Force -Path (Join-Path $testRoot "game_data\mission_files")
    $secondary = New-Item -ItemType Directory -Force -Path (Join-Path $primary.FullName "d2xxl_downloads")
    New-Item -ItemType File -Path (Join-Path $primary.FullName "primary.zip") | Out-Null
    New-Item -ItemType File -Path (Join-Path $primary.FullName "complete.7z") | Out-Null
    New-Item -ItemType File -Path (Join-Path $primary.FullName "complete.json") | Out-Null
    New-Item -ItemType File -Path (Join-Path $secondary.FullName "secondary.7z") | Out-Null
    New-Item -ItemType File -Path (Join-Path $secondary.FullName "complete.zip") | Out-Null
    New-Item -ItemType File -Path (Join-Path $secondary.FullName "complete.json") | Out-Null
    New-Item -ItemType File -Path (Join-Path $secondary.FullName "secondary.rar") | Out-Null

    $sources = @(Get-MissionArchiveSources -RepoRoot $testRoot)
    Assert-True ($sources.Count -eq 2) "Expected two configured mission archive sources"
    Assert-True ($sources[0].Directory -eq $primary.FullName) "Primary source path is incorrect"
    Assert-True ($sources[1].Directory -eq $secondary.FullName) "D2X-XL source path is incorrect"
    Assert-True ($sources[1].FingerprintAlbumPrefix -eq 'Mission ZIP - D2X-XL - ') `
        "D2X-XL fingerprint output should have a collision-safe prefix"

    $available = @(Get-AvailableMissionArchiveSources -Sources $sources)
    Assert-True ($available.Count -eq 2) "Expected both populated mission archive sources"

    $primaryMissing = @(Get-MissingMissionMetadataArchives -Source $sources[0])
    Assert-True (($primaryMissing.Name -join ',') -eq 'primary.zip') `
        "Primary missing selection should exclude archives with matching JSON"
    $secondaryMissing = @(Get-MissingMissionMetadataArchives -Source $sources[1])
    Assert-True (($secondaryMissing.Name -join ',') -eq 'secondary.7z') `
        "Special-source missing selection should support .7z and exclude matching JSON"

    $fingerprintItems = @($sources | ForEach-Object { Get-MissionArchiveSampleItems -Source $_ })
    Assert-True ($fingerprintItems.Count -eq 5) "Fingerprint discovery should include both archive sources and RAR files"
    Assert-True ($fingerprintItems.Name -contains 'mission_files/primary.zip') `
        "Primary fingerprint sample keys should include their source"
    Assert-True ($fingerprintItems.Name -contains 'd2xxl_downloads/secondary.7z') `
        "D2X-XL fingerprint sample keys should include their source"

    Remove-Item -LiteralPath (Join-Path $secondary.FullName "secondary.7z"), `
    (Join-Path $secondary.FullName "complete.zip"), (Join-Path $secondary.FullName "secondary.rar")
    $available = @(Get-AvailableMissionArchiveSources -Sources $sources)
    Assert-True ($available.Count -eq 1) "An empty optional source should be skipped"

    Remove-Item -LiteralPath (Join-Path $primary.FullName "primary.zip"), `
    (Join-Path $primary.FullName "complete.7z")
    $requiredFailure = $false
    try {
        Get-AvailableMissionArchiveSources -Sources $sources | Out-Null
    } catch {
        $requiredFailure = $_.Exception.Message -like "No mission archives found*"
    }
    Assert-True $requiredFailure "An empty required source should fail"
} finally {
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}

Write-Host "Mission archive source tests passed"
