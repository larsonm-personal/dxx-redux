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
    [ValidateRange(0.000001, 1.0)][double]$SampleFraction = 1.0,
    [ValidateRange(0, [int]::MaxValue)][int]$SampleSeed = 0,
    [string]$SampleStatePath,
    [ValidateSet("all", "discs", "packs", "mission-zips", "merge")]
    [string]$Step = "all"
)

$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot
$RepoRoot = Split-Path $ScriptDir -Parent
. (Join-Path $RepoRoot 'android\helpers\runtime_targeted_sampling.ps1')

function Select-FingerprintSampleNames {
    param([object[]]$Items, [double]$Fraction, [int]$Seed, [string]$RingName)
    if ($Items.Count -eq 0) { return @() }
    return @(Select-RuntimeHashRingFractionItems -Items $Items -Fraction $Fraction -Seed $Seed `
            -StatePath $SampleStatePath -RingName $RingName |
            Select-Object -ExpandProperty Name)
}

$discNames = $null
$albumNames = $null
$missionZipNames = $null
if ($SampleFraction -lt 1.0) {
    if ($SampleSeed -eq 0) { $SampleSeed = Get-Random -Minimum 1 -Maximum ([int]::MaxValue) }
    $discItems = @(Get-ChildItem (Join-Path $ScriptDir 'CD images') -Directory | Sort-Object Name)
    $albumItems = @(Get-ChildItem (Join-Path $ScriptDir 'music') -File | Where-Object Extension -Match '^\.(zip|7z|DXA)$' |
            ForEach-Object {
                $name = $_.BaseName
                if ($name -match '^(.+?) - ') { $name = $Matches[1] }
                [pscustomobject]@{ Name = $name }
            } | Sort-Object Name -Unique)
    $missionItems = @(Get-ChildItem (Join-Path $ScriptDir 'mission_files') -File |
            Where-Object Extension -Match '^\.(zip|7z|rar)$' | Sort-Object Name)
    $discNames = Select-FingerprintSampleNames $discItems $SampleFraction ($SampleSeed -bxor 101) 'regenerate:fingerprints:discs'
    $albumNames = Select-FingerprintSampleNames $albumItems $SampleFraction ($SampleSeed -bxor 202) 'regenerate:fingerprints:packs'
    $missionZipNames = Select-FingerprintSampleNames $missionItems $SampleFraction ($SampleSeed -bxor 303) 'regenerate:fingerprints:missions'
    Write-Host ("Fingerprint sample: discs {0}/{1}, packs {2}/{3}, mission archives {4}/{5} ({6:P1}), seed {7}" -f
        $discNames.Count, $discItems.Count, $albumNames.Count, $albumItems.Count,
        $missionZipNames.Count, $missionItems.Count, $SampleFraction, $SampleSeed)
}

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
    if ($null -ne $discNames) { $args1['FolderNames'] = $discNames }
    Run-Step "Step 1/5: Fingerprint CD disc tracks" "fingerprint_disc_tracks.ps1" $args1
}

# Step 2: Merge disc fingerprints into known_discs.jsonc
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
    if ($null -ne $albumNames) { $args3['Albums'] = $albumNames }
    Run-Step "Step 3/5: Extract + fingerprint music packs" "fingerprint_music_packs.ps1" $args3
}

# Step 4: Extract + fingerprint mission ZIP soundtracks (+ optional AcoustID)
if ($runMissionZips) {
    $argsMission = @{}
    if ($Force) { $argsMission["Force"] = $true }
    if ($SkipAcoustId) { $argsMission["SkipAcoustId"] = $true }
    if ($null -ne $missionZipNames) { $argsMission['Zips'] = $missionZipNames }
    Run-Step "Step 4/5: Extract + fingerprint mission ZIP soundtracks" "fingerprint_mission_zip_music.ps1" $argsMission
}

# Step 5: Generate known_albums.jsonc from album sidecars
if ($runPacks -or $runMissionZips -or $runMerge) {
    $args4 = @{}
    if ($DryRun) { $args4["DryRun"] = $true }
    if ($Force) { $args4["Force"] = $true }
    Run-Step "Step 5/5: Generate album fingerprint DB" "update_known_discs_albums.ps1" $args4
}

Write-Host "`n=========================================="
Write-Host "  All done"
Write-Host "=========================================="
