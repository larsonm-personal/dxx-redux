#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$metadataRoot = Join-Path $repoRoot "game_data\mission_files"
$missionCount = 0

foreach ($file in Get-ChildItem -LiteralPath $metadataRoot -Filter "*.json" -File -Recurse) {
    try {
        $items = @(Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json)
    } catch {
        continue
    }
    foreach ($mission in $items) {
        if (-not $mission.mission_filename) { continue }
        $missionCount++
        $intent = $mission.mission_intent
        if ($null -eq $intent -or $intent -is [string]) {
            throw "$($file.FullName): $($mission.mission_filename) mission_intent is not a structured object"
        }
        foreach ($name in @(
                "classification", "rule", "confidence", "reason", "declarations",
                "normal_levels", "campaign_actor_levels", "arena_like_levels", "solo_like_levels",
                "player_start_min", "player_start_max", "coop_start_min", "coop_start_max",
                "robots", "hostages", "matcens", "guidebots", "powerups", "reactors"
            )) {
            if ($intent.PSObject.Properties.Name -notcontains $name) {
                throw "$($file.FullName): $($mission.mission_filename) mission_intent is missing $name"
            }
        }
        foreach ($name in @("anarchy_only", "normal", "coop", "anarchy", "robo_anarchy", "capture_flag", "hoard")) {
            if ($intent.declarations.PSObject.Properties.Name -notcontains $name) {
                throw "$($file.FullName): $($mission.mission_filename) mission_intent declarations are missing $name"
            }
        }
    }
}
if ($missionCount -eq 0) { throw "No checked mission metadata entries were found" }

Write-Host "PASS: $missionCount checked missions use structured mission_intent output"
