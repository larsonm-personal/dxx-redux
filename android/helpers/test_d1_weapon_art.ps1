#!/usr/bin/env pwsh
# Compare actual Spreadfire firing, source pixels and rendered native/imported art
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$D1DataDirectory,
    [string]$D2DataDirectory,
    [string]$NativeExecutable = 'buildd1/main/test_upstream_compat.exe',
    [string]$ImportedExecutable = 'buildd2/main/test_upstream_compat.exe'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repository = Split-Path (Split-Path $PSScriptRoot)
$assets = (Resolve-Path -LiteralPath $D1DataDirectory).Path
$nativePath = (Resolve-Path -LiteralPath $NativeExecutable).Path
$importedPath = (Resolve-Path -LiteralPath $ImportedExecutable).Path
$outputPath = Join-Path $repository 'temp/d1-weapon-art-comparison'
& "$PSScriptRoot/retain-recent-artifacts.ps1" -Artifacts $outputPath
$runs = @(
    @{ Name = 'native'; Executable = $nativePath; Arguments = @($assets) },
    @{ Name = 'imported'; Executable = $importedPath; Arguments = @($assets) }
)
if ($D2DataDirectory) {
    $runs += @{ Name = 'imported-with-d2'; Executable = $importedPath; Arguments = @($assets, (Resolve-Path -LiteralPath $D2DataDirectory).Path) }
}
foreach ($run in $runs) {
    $runDirectory = Join-Path $outputPath $run.Name
    New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
    Push-Location $runDirectory
    try {
        $arguments = $run.Arguments
        & $run.Executable --weapon-art-trace @arguments *> output.log
        if ($LASTEXITCODE -ne 0) { throw "$($run.Name) weapon art trace failed; see $runDirectory/output.log" }
    } finally {
        Pop-Location
    }
}
$manifest = Get-Content -LiteralPath (Join-Path $outputPath 'native/weapon-art.json') -Raw
$frames = $manifest | ConvertFrom-Json
foreach ($run in $runs | Select-Object -Skip 1) {
    $runDirectory = Join-Path $outputPath $run.Name
    if ((Get-Content -LiteralPath (Join-Path $runDirectory 'weapon-art.json') -Raw) -cne $manifest) {
        throw "$($run.Name) projectile state or resource identity differs; see $outputPath"
    }
    foreach ($frame in $frames) {
        foreach ($extension in @('indexed', 'palette', 'rgb')) {
            $filename = "$($frame.frame).$extension"
            $reference = Get-FileHash -LiteralPath (Join-Path $outputPath "native/$filename") -Algorithm SHA256
            $actual = Get-FileHash -LiteralPath (Join-Path $runDirectory $filename) -Algorithm SHA256
            if ($reference.Hash -ne $actual.Hash) { throw "$($run.Name) weapon art differs: $filename; inspect $outputPath" }
        }
    }
}
Write-Output "PASS: $($frames.Count) Spreadfire firing, source and rendered frames match in $($runs.Count - 1) imported configurations"
Write-Output "Weapon traces and PNGs: $outputPath"
