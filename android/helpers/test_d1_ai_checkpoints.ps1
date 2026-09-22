#!/usr/bin/env pwsh
# Compare actual native checkpoints and restored robot frames in both engines
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$D1DataDirectory,
    [switch]$CustomAssets,
    [ValidateSet(1, 7, 27)][int]$Level = 1,
    [string]$NativeExecutable = "buildd1/main/test_upstream_compat.exe",
    [string]$ImportedExecutable = "buildd2/main/test_upstream_compat.exe"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$repository = Split-Path (Split-Path $PSScriptRoot)
$assets = (Resolve-Path -LiteralPath $D1DataDirectory).Path
$nativePath = (Resolve-Path -LiteralPath $NativeExecutable).Path
$importedPath = (Resolve-Path -LiteralPath $ImportedExecutable).Path
$outputPath = Join-Path $repository $(if ($CustomAssets) { "temp/d1-custom-checkpoint-comparison" } else { "temp/d1-ai-checkpoint-comparison" })
if ($Level -ne 1) { $outputPath += "-level$Level" }
if ($CustomAssets -and $Level -ne 1) { throw "Custom checkpoint fixtures use level 1" }
$traceOption = if ($CustomAssets) { "--custom-checkpoint-frame-trace" } else { "--checkpoint-frame-trace" }
& "$PSScriptRoot/retain-recent-artifacts.ps1" -Artifacts $outputPath
$nativeDirectory = Join-Path $outputPath "native"
foreach ($run in @(
        @{ Name = "native"; Executable = $nativePath },
        @{ Name = "imported"; Executable = $importedPath }
    )) {
    $runDirectory = Join-Path $outputPath $run.Name
    New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
    Push-Location $runDirectory
    try {
        & $run.Executable $traceOption $assets $nativeDirectory $Level *> output.log
        if ($LASTEXITCODE -ne 0) {
            throw "$($run.Name) checkpoint frame trace failed (exit $LASTEXITCODE); see $runDirectory/output.log"
        }
    } finally {
        Pop-Location
    }
}
$native = Get-Content -LiteralPath (Join-Path $nativeDirectory "frames.json") -Raw
$imported = Get-Content -LiteralPath (Join-Path $outputPath "imported/frames.json") -Raw
if ($native -cne $imported) {
    throw "Restored robot frames differ; see $outputPath"
}
$trace = $native | ConvertFrom-Json
$cases = $trace.cases.Count
Write-Output "PASS: $cases native checkpoint scenarios, $($cases * 4) restored robot frames match"
Write-Output "PASS: $($trace.fresh_textures.Count) side texture pairs match after fresh load and each checkpoint restore"
Write-Output "PASS: native trigger actions, source state, values and links match after fresh load and each checkpoint restore"
if ($Level -ne 1) {
    Write-Output "PASS: $($trace.boss_checkpoints.Count) boss checkpoints preserve exact health and physics across all difficulties"
}
Write-Output "PASS: $($trace.reactor_guns.Count) reactor gun positions and directions match after fresh load and every checkpoint restore"
Write-Output "PASS: $($trace.hidden_reactors.Count) hidden boss-level reactors retain their type, control and presentation after every restore"
if ($CustomAssets) {
    Write-Output "PASS: custom robot/model/joint definitions, pixels and samples match after fresh load and every checkpoint reload"
}
Write-Output "Checkpoints and traces: $outputPath"
