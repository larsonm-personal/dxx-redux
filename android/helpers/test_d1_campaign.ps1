#!/usr/bin/env pwsh
# Compare real secret travel, death and save/restore in native and imported D1
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
$outputPath = Join-Path $repository "temp/d1-campaign-comparison"
$importArguments = @($assets)
if ($D2DataDirectory) { $importArguments += (Resolve-Path -LiteralPath $D2DataDirectory).Path }
& "$PSScriptRoot/retain-recent-artifacts.ps1" -Artifacts $outputPath
foreach ($run in @(
        @{ Name = "native"; Executable = $nativePath; Arguments = @($assets) },
        @{ Name = "imported"; Executable = $importedPath; Arguments = $importArguments }
    )) {
    $runDirectory = Join-Path $outputPath $run.Name
    New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
    foreach ($name in @('campaign.json', 'cadence-d2.json', 'cadence-d2-awareness.json')) {
        $file = Join-Path $runDirectory $name
        if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file }
    }
    Push-Location $runDirectory
    try {
        $arguments = $run.Arguments
        & $run.Executable --campaign-trace @arguments *> output.log
        if ($LASTEXITCODE -ne 0) {
            throw "$($run.Name) campaign trace failed (exit $LASTEXITCODE); see $runDirectory/output.log"
        }
        # The native travel path must not depend on D2 secret-world snapshots
        foreach ($snapshot in @("secret.sgb", "secret.sgc")) {
            if (Test-Path -LiteralPath $snapshot) { throw "Unexpected D2 secret snapshot: $runDirectory/$snapshot" }
        }
    } finally {
        Pop-Location
    }
}
$native = Get-Content -LiteralPath (Join-Path $outputPath "native/campaign.json") -Raw
$imported = Get-Content -LiteralPath (Join-Path $outputPath "imported/campaign.json") -Raw
if ($native -cne $imported) { throw "Campaign traces differ; see $outputPath" }
$trace = $native | ConvertFrom-Json
foreach ($phase in $trace) {
    $expectedAwareness = if ($phase.phase -eq 'secret_restore') { 1 } else { 0 }
    if ($phase.pending_awareness -ne $expectedAwareness) {
        throw "Unexpected pending awareness in $($phase.phase) on level $($phase.level)"
    }
}
Write-Output "PASS: $($trace.Count) native campaign observations match, including secret entry, save/restore, death and return"
Write-Output 'PASS: new mines retire pending awareness; ordinary restore preserves the saved queue'
Write-Output "PASS: cadence storage and first-use Fusion awareness, refueling and collision RNG match across native level/ship transitions"
if ($D2DataDirectory) {
    $control = Get-Content -LiteralPath (Join-Path $outputPath 'imported/cadence-d2.json') -Raw | ConvertFrom-Json
    if ($control.Count -ne 3) { throw 'Incomplete ordinary D2 cadence persistence control' }
    Write-Output "PASS: $($control.Count) ordinary D2 loaded-world save/restore cases retain full-width relative cadence clocks"
}
Write-Output "Campaign traces: $outputPath"
