#!/usr/bin/env pwsh
# update_all_fingerprints.ps1 -- Run the full fingerprint + database pipeline.
#
# Steps:
#   1. Fingerprint CD disc tracks         (fingerprint_disc_tracks.ps1)
#   2. Merge disc fingerprints into DB     (update_known_discs_fingerprints.ps1)
#   3. Extract + fingerprint music packs   (fingerprint_music_packs.ps1)
#   4. Extract + fingerprint mission ZIP soundtracks
#                                           (fingerprint_mission_zip_music.ps1)
#   5. Generate the album fingerprint DB   (update_known_discs_albums.ps1)
#
# Each step is idempotent and skips work already done unless -Force is passed.
# Individual steps can be selected with -Step.
#
# Usage:
#   .\update_all_fingerprints.ps1                      # run all steps
#   .\update_all_fingerprints.ps1 -Force               # re-run everything
#   .\update_all_fingerprints.ps1 -SkipAcoustId        # skip AcoustID lookups
#   .\update_all_fingerprints.ps1 -Step discs          # only disc fingerprinting + merge
#   .\update_all_fingerprints.ps1 -Step packs          # only music pack extraction + merge
#   .\update_all_fingerprints.ps1 -Step mission-zips   # only mission ZIP music extraction + merge
#   .\update_all_fingerprints.ps1 -Step merge          # only DB merges (steps 2+5)

param(
    [switch]$Force,
    [switch]$SkipAcoustId,
    [switch]$DryRun,
    [ValidateSet("all", "discs", "packs", "mission-zips", "merge")]
    [string]$Step = "all"
)

$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot

function Run-Step {
    param([string]$Label, [string]$Script, [hashtable]$Params = @{})
    Write-Host "`n=========================================="
    Write-Host "  $Label"
    Write-Host "==========================================`n"
    & "$ScriptDir/$Script" @Params
    if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
        Write-Error "$Label failed (exit code $LASTEXITCODE)"
    }
}

$runDiscs = $Step -eq "all" -or $Step -eq "discs"
$runPacks = $Step -eq "all" -or $Step -eq "packs"
$runMissionZips = $Step -eq "all" -or $Step -eq "mission-zips"
$runMerge = $Step -eq "merge"

# Step 1: Fingerprint CD disc tracks
if ($runDiscs) {
    $args1 = @{}
    if ($Force) { $args1["Force"] = $true }
    Run-Step "Step 1/5: Fingerprint CD disc tracks" "fingerprint_disc_tracks.ps1" $args1
}

# Step 2: Merge disc fingerprints into known_discs.json5
if ($runDiscs -or $runMerge) {
    $args2 = @{}
    if ($DryRun) { $args2["DryRun"] = $true }
    Run-Step "Step 2/5: Merge disc fingerprints into DB" "update_known_discs_fingerprints.ps1" $args2
}

# Step 3: Extract + fingerprint music packs (+ optional AcoustID)
if ($runPacks) {
    $args3 = @{}
    if ($Force) { $args3["Force"] = $true }
    if ($SkipAcoustId) { $args3["SkipAcoustId"] = $true }
    Run-Step "Step 3/5: Extract + fingerprint music packs" "fingerprint_music_packs.ps1" $args3
}

# Step 4: Extract + fingerprint mission ZIP soundtracks (+ optional AcoustID)
if ($runMissionZips) {
    $argsMission = @{}
    if ($Force) { $argsMission["Force"] = $true }
    if ($SkipAcoustId) { $argsMission["SkipAcoustId"] = $true }
    Run-Step "Step 4/5: Extract + fingerprint mission ZIP soundtracks" "fingerprint_mission_zip_music.ps1" $argsMission
}

# Step 5: Generate known_albums.json5 from album sidecars
if ($runPacks -or $runMissionZips -or $runMerge) {
    $args4 = @{}
    if ($DryRun) { $args4["DryRun"] = $true }
    if ($Force) { $args4["Force"] = $true }
    Run-Step "Step 5/5: Generate album fingerprint DB" "update_known_discs_albums.ps1" $args4
}

Write-Host "`n=========================================="
Write-Host "  All done"
Write-Host "=========================================="
