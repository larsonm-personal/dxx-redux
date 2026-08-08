#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$scriptPath = Join-Path $repoRoot 'game_data/fingerprint_music_packs.ps1'
$lines = [System.IO.File]::ReadAllLines($scriptPath)

$buildLines = @($lines | Select-String -SimpleMatch 'cmake --build $buildDir --config Release --target fingerprint_audio')
Assert-True ($buildLines.Count -eq 1) 'Music-pack generation must invoke exactly one fingerprint_audio target build'
$buildIndex = $buildLines[0].LineNumber - 1

$artifactGuardLines = @($lines | Select-String -SimpleMatch 'if (-not (Test-Path $fpExe))')
Assert-True ($artifactGuardLines.Count -eq 1) 'Music-pack generation must validate the built fingerprint_audio artifact'
$artifactGuardIndex = $artifactGuardLines[0].LineNumber - 1
Assert-True ($buildIndex -lt $artifactGuardIndex) 'The target build must run before accepting an existing fingerprint_audio artifact'

$buildFailureLines = @($lines | Select-String -SimpleMatch 'fingerprint_audio build failed with exit code $LASTEXITCODE')
Assert-True ($buildFailureLines.Count -eq 1) 'Music-pack generation must propagate fingerprint_audio build failures'
$buildFailureIndex = $buildFailureLines[0].LineNumber - 1
Assert-True ($buildIndex -lt $buildFailureIndex -and $buildFailureIndex -lt $artifactGuardIndex) `
    'Build failure validation must occur immediately after the target build and before artifact admission'

$configureLines = @($lines | Select-String -SimpleMatch 'cmake -S "$repoRoot/android/app/src/main/cpp/extract" -B $buildDir')
$configureFailureLines = @($lines | Select-String -SimpleMatch 'fingerprint_audio CMake configuration failed with exit code $LASTEXITCODE')
Assert-True ($configureLines.Count -eq 1 -and $configureFailureLines.Count -eq 1) `
    'Music-pack generation must configure the current native source and propagate configuration failures'
Assert-True ($configureLines[0].LineNumber -lt $configureFailureLines[0].LineNumber) `
    'Configuration failure validation must follow configuration'

Write-Host 'Fingerprint music-pack build guard tests passed'
