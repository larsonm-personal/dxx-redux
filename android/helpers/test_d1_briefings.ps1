#!/usr/bin/env pwsh
# Compare real briefing windows and rendered pages in native and imported D1
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
$outputPath = Join-Path $repository "temp/d1-briefing-comparison"
& "$PSScriptRoot/retain-recent-artifacts.ps1" -Artifacts $outputPath
$runs = @(
    @{ Name = "native"; Executable = $nativePath; Arguments = @($assets) },
    @{ Name = "imported"; Executable = $importedPath; Arguments = @($assets) }
)
if ($D2DataDirectory) {
    $d2Assets = (Resolve-Path -LiteralPath $D2DataDirectory).Path
    $runs += @{ Name = "imported-with-d2"; Executable = $importedPath; Arguments = @($assets, $d2Assets) }
}
foreach ($run in $runs) {
    $runDirectory = Join-Path $outputPath $run.Name
    New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
    # A previously interrupted failure case may leave its deliberate bad override
    foreach ($fixture in @("moon01.pcx", "owned.tex", "broken.tex", "briefing.json")) {
        $fixturePath = Join-Path $runDirectory $fixture
        if (Test-Path -LiteralPath $fixturePath) { Remove-Item -LiteralPath $fixturePath -Force }
    }
    Push-Location $runDirectory
    try {
        $arguments = $run.Arguments
        & $run.Executable --briefing-trace @arguments *> output.log
        if ($LASTEXITCODE -ne 0) {
            throw "$($run.Name) briefing trace failed (exit $LASTEXITCODE); see $runDirectory/output.log"
        }
    } finally {
        Pop-Location
    }
}
$manifest = Get-Content -LiteralPath (Join-Path $outputPath "native/briefing.json") -Raw
$frames = $manifest | ConvertFrom-Json
foreach ($run in $runs | Select-Object -Skip 1) {
    $runDirectory = Join-Path $outputPath $run.Name
    if ((Get-Content -LiteralPath (Join-Path $runDirectory "briefing.json") -Raw) -cne $manifest) {
        throw "$($run.Name) briefing frame manifests differ"
    }
    foreach ($frame in $frames) {
        $reference = Get-FileHash -LiteralPath (Join-Path $outputPath "native/$frame") -Algorithm SHA256
        $actual = Get-FileHash -LiteralPath (Join-Path $runDirectory $frame) -Algorithm SHA256
        if ($reference.Hash -ne $actual.Hash) { throw "$($run.Name) briefing pixels differ: $frame; see matching PNG files in $outputPath" }
    }
}
Write-Output "PASS: $($frames.Count) native briefing frames match in $($runs.Count - 1) imported configurations"
Write-Output "Briefing traces and rendered PNGs: $outputPath"
