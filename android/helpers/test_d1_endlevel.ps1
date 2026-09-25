#!/usr/bin/env pwsh
# Compare actual repeated rendered flyouts, private state and RNG in both engines
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$D1DataDirectory,
    [string]$NativeExecutable = 'buildd1/main/test_upstream_compat.exe',
    [string]$ImportedExecutable = 'buildd2/main/test_upstream_compat.exe'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repository = Split-Path (Split-Path $PSScriptRoot)
$assets = (Resolve-Path -LiteralPath $D1DataDirectory).Path
$nativePath = (Resolve-Path -LiteralPath $NativeExecutable).Path
$importedPath = (Resolve-Path -LiteralPath $ImportedExecutable).Path
$outputPath = Join-Path $repository 'temp/d1-endlevel-comparison'
& "$PSScriptRoot/retain-recent-artifacts.ps1" -Artifacts $outputPath
foreach ($run in @(
        @{ Name = 'native'; Executable = $nativePath },
        @{ Name = 'imported'; Executable = $importedPath }
    )) {
    $runDirectory = Join-Path $outputPath $run.Name
    New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
    $trace = Join-Path $runDirectory 'endlevel.json'
    if (Test-Path -LiteralPath $trace) { Remove-Item -LiteralPath $trace }
    Push-Location $runDirectory
    try {
        & $run.Executable --endlevel-trace $assets *> output.log
        if ($LASTEXITCODE -ne 0) { throw "$($run.Name) endlevel trace failed (exit $LASTEXITCODE); see $runDirectory/output.log" }
    } finally {
        Pop-Location
    }
}
$native = Get-Content -LiteralPath (Join-Path $outputPath 'native/endlevel.json') -Raw
$imported = Get-Content -LiteralPath (Join-Path $outputPath 'imported/endlevel.json') -Raw
if ($native -cne $imported) { throw "Rendered endlevel state differs; see $outputPath" }
$runs = $native | ConvertFrom-Json
if ($runs.Count -ne 3) { throw 'Incomplete repeated flyout coverage' }
$frames = ($runs | ForEach-Object { $_.frames.Count } | Measure-Object -Sum).Sum
Write-Output "PASS: 3 actual flyouts, $frames frames of native/imported private state, player/camera motion and RNG match"
Write-Output "Endlevel traces: $outputPath"
