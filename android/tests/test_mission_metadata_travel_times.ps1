#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/powershell_compat.ps1')
. (Join-Path $repoRoot 'android/helpers/mission_archive_sources.ps1')
$missionSources = @(Get-AvailableMissionArchiveSources -Sources (Get-MissionArchiveSources -RepoRoot $repoRoot))
$mismatches = @()
$checked = 0

@($missionSources | ForEach-Object { Get-MissionMetadataFiles -Source $_ }) |
    ForEach-Object {
        $metadataFile = $_
        $missions = @(ConvertFrom-CompatibleJsonItems -Json (Get-Content -LiteralPath $metadataFile.FullName -Raw))
        foreach ($mission in $missions) {
            foreach ($level in @($mission.levels)) {
                if ($null -eq $level.travel_time_seconds -or $null -eq $level.travel_time_text) {
                    continue
                }

                $seconds = [int]$level.travel_time_seconds
                $expected = "{0}M:{1:d2}S" -f [Math]::Floor($seconds / 60), ($seconds % 60)
                $checked++
                if ($level.travel_time_text -ne $expected) {
                    $mismatches += "{0} level {1}: {2} seconds is '{3}', expected '{4}'" -f `
                        $metadataFile.MissionMetadataKey, $level.level_num, $seconds, $level.travel_time_text, $expected
                }
            }
        }
    }

if ($mismatches.Count -gt 0) {
    $reported = @($mismatches | Select-Object -First 20)
    $remaining = $mismatches.Count - $reported.Count
    $suffix = if ($remaining -gt 0) { "`n... and $remaining more" } else { "" }
    throw "Found $($mismatches.Count) mission metadata travel time mismatches:`n$($reported -join "`n")$suffix"
}

Write-Host "PASS: $checked mission metadata travel times match their numeric values"
