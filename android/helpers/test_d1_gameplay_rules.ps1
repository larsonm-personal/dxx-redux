#!/usr/bin/env pwsh
# Compare native/imported door, pickup, damage and replacement-drop operations
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$D1DataDirectory,
    [string]$D2DataDirectory,
    [string]$NativeExecutable = "buildd1/main/test_upstream_compat.exe",
    [string]$ImportedExecutable = "buildd2/main/test_upstream_compat.exe"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$repository = Split-Path (Split-Path $PSScriptRoot)
$assets = (Resolve-Path -LiteralPath $D1DataDirectory).Path
$nativePath = (Resolve-Path -LiteralPath $NativeExecutable).Path
$importedPath = (Resolve-Path -LiteralPath $ImportedExecutable).Path
$outputPath = Join-Path $repository "temp/d1-gameplay-rules-comparison"
& "$PSScriptRoot/retain-recent-artifacts.ps1" -Artifacts $outputPath
$importArguments = @($assets)
if ($D2DataDirectory) { $importArguments += (Resolve-Path -LiteralPath $D2DataDirectory).Path }
foreach ($run in @(
        @{ Name = "native"; Executable = $nativePath; Arguments = @($assets) },
        @{ Name = "imported"; Executable = $importedPath; Arguments = $importArguments }
    )) {
    $runDirectory = Join-Path $outputPath $run.Name
    New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
    foreach ($name in @("rules.json", "rules-d2.json")) {
        $file = Join-Path $runDirectory $name
        if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file -Force }
    }
    Push-Location $runDirectory
    try {
        $arguments = $run.Arguments
        & $run.Executable --gameplay-rules-trace @arguments *> output.log
        if ($LASTEXITCODE -ne 0) {
            throw "$($run.Name) gameplay rules failed (exit $LASTEXITCODE); see $runDirectory/output.log"
        }
    } finally {
        Pop-Location
    }
}
$native = Get-Content -LiteralPath (Join-Path $outputPath "native/rules.json") -Raw
$imported = Get-Content -LiteralPath (Join-Path $outputPath "imported/rules.json") -Raw
if ($native -cne $imported) { throw "Native/imported gameplay rules differ; see $outputPath" }
$trace = $native | ConvertFrom-Json
if ($D2DataDirectory -and !(Test-Path -LiteralPath (Join-Path $outputPath "imported/rules-d2.json"))) {
    throw "Ordinary D2 regression did not finish"
}
Write-Output "PASS: $($trace.doors.Count) door, $($trace.pickups.Count) pickup, $($trace.damage.Count) damage and $($trace.drops.Count) drop cases match native D1"
Write-Output "PASS: $($trace.surfaces.Count) source surfaces, $($trace.contact_motion.motion.Count) motion cases, $($trace.contact_motion.contacts.Count) robot contact cases and $($trace.robot_blasts.Count) robot blasts match native D1"
Write-Output "PASS: $($trace.robot_pairs.Count) robot-pair intersections, $($trace.resource_drops.Count) resource drops and $($trace.secondary_explosions.Count) secondary explosions match native D1"
Write-Output "PASS: $($trace.small_fireballs.Count) attached fireball cases match native D1, including position, size and FX draw count"
Write-Output "PASS: $($trace.reactor_fireballs.Count) dead-reactor burn cases match native D1"
Write-Output "PASS: $($trace.object_orientations.Count) object initialization cases match native D1"
Write-Output "PASS: $($trace.powerup_animation.Count) pickup animation steps match native D1"
Write-Output "PASS: $($trace.weapon_drops.Count) actual weapon-drop and pickup cases match native D1"
Write-Output "PASS: $($trace.volatile_impacts.Count) lava weapon impacts match native D1, including explosion size and robot blast motion"
Write-Output "Gameplay traces: $outputPath"
