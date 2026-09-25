#!/usr/bin/env pwsh
# Compare actual death phases, gear drops and consecutive respawns
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
$outputPath = Join-Path $repository 'temp/d1-death-comparison'
& "$PSScriptRoot/retain-recent-artifacts.ps1" -Artifacts $outputPath
foreach ($run in @(
        @{ Name = 'native'; Executable = $nativePath },
        @{ Name = 'imported'; Executable = $importedPath }
    )) {
    $runDirectory = Join-Path $outputPath $run.Name
    New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
    $trace = Join-Path $runDirectory 'death.json'
    if (Test-Path -LiteralPath $trace) { Remove-Item -LiteralPath $trace }
    Push-Location $runDirectory
    try {
        & $run.Executable --death-trace $assets *> output.log
        if ($LASTEXITCODE -ne 0) { throw "$($run.Name) death trace failed (exit $LASTEXITCODE); see $runDirectory/output.log" }
    } finally {
        Pop-Location
    }
}
$native = Get-Content -LiteralPath (Join-Path $outputPath 'native/death.json') -Raw
$imported = Get-Content -LiteralPath (Join-Path $outputPath 'imported/death.json') -Raw
if ($native -cne $imported) { throw "Death gameplay state differs; see $outputPath" }
Write-Output 'PASS: actual native/imported death timing, carried gear drops and consecutive respawns match'
Write-Output "Death traces: $outputPath"
