#!/usr/bin/env pwsh
# update_all_fingerprints.ps1 -- Run the full fingerprint + database pipeline.
#
# Steps:
#   1. Fingerprint CD disc tracks         (fingerprint_disc_tracks.ps1)
#   2. Merge disc fingerprints into DB     (update_known_discs_fingerprints.ps1)
#   3. Extract + fingerprint music packs   (fingerprint_music_packs.ps1)
#   4. Merge album entries into DB         (update_known_discs_albums.ps1)
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
#   .\update_all_fingerprints.ps1 -Step merge          # only DB merges (steps 2+4)

param(
    [switch]$Force,
    [switch]$SkipAcoustId,
    [switch]$DryRun,
    [ValidateSet("all", "discs", "packs", "merge")]
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
$runMerge = $Step -eq "merge"

# Step 1: Fingerprint CD disc tracks
if ($runDiscs) {
    $args1 = @{}
    if ($Force) { $args1["Force"] = $true }
    Run-Step "Step 1/4: Fingerprint CD disc tracks" "fingerprint_disc_tracks.ps1" $args1
}

# Step 2: Merge disc fingerprints into known_discs.json5
if ($runDiscs -or $runMerge) {
    $args2 = @{}
    if ($DryRun) { $args2["DryRun"] = $true }
    Run-Step "Step 2/4: Merge disc fingerprints into DB" "update_known_discs_fingerprints.ps1" $args2
}

# Step 3: Extract + fingerprint music packs (+ optional AcoustID)
if ($runPacks) {
    $args3 = @{}
    if ($Force) { $args3["Force"] = $true }
    if ($SkipAcoustId) { $args3["SkipAcoustId"] = $true }
    Run-Step "Step 3/4: Extract + fingerprint music packs" "fingerprint_music_packs.ps1" $args3
}

# Step 4: Merge album entries into known_discs.json5
if ($runPacks -or $runMerge) {
    $args4 = @{}
    if ($DryRun) { $args4["DryRun"] = $true }
    if ($Force) { $args4["Force"] = $true }
    Run-Step "Step 4/4: Merge album entries into DB" "update_known_discs_albums.ps1" $args4
}

Write-Host "`n=========================================="
Write-Host "  All done"
Write-Host "=========================================="
