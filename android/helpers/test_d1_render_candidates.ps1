#!/usr/bin/env pwsh
# Compare actual native/imported render lists with CPU-only candidate collection
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$D1DataDirectory,
    [string]$NativeExecutable = "buildd1/main/test_upstream_compat.exe",
    [string]$ImportedExecutable = "buildd2/main/test_upstream_compat.exe"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$repository = Split-Path (Split-Path $PSScriptRoot)
$assets = (Resolve-Path -LiteralPath $D1DataDirectory).Path
$nativePath = (Resolve-Path -LiteralPath $NativeExecutable).Path
$importedPath = (Resolve-Path -LiteralPath $ImportedExecutable).Path
$outputPath = Join-Path $repository "temp/d1-render-candidate-comparison"
& "$PSScriptRoot/retain-recent-artifacts.ps1" -Artifacts $outputPath
foreach ($run in @(
        @{ Name = "native"; Executable = $nativePath; Arguments = @($assets) },
        @{ Name = "imported"; Executable = $importedPath; Arguments = @($assets) }
    )) {
    $runDirectory = Join-Path $outputPath $run.Name
    New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
    foreach ($name in @("candidates.json")) {
        $file = Join-Path $runDirectory $name
        if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file -Force }
    }
    Push-Location $runDirectory
    try {
        $arguments = $run.Arguments
        & $run.Executable --render-candidates-trace @arguments *> output.log
        if ($LASTEXITCODE -ne 0) {
            throw "$($run.Name) render candidates failed (exit $LASTEXITCODE); see $runDirectory/output.log"
        }
    } finally {
        Pop-Location
    }
}
$native = Get-Content -LiteralPath (Join-Path $outputPath "native/candidates.json") -Raw
$imported = Get-Content -LiteralPath (Join-Path $outputPath "imported/candidates.json") -Raw
if ($native -cne $imported) { throw "Native/imported render candidates differ; see $outputPath" }
$trace = $native | ConvertFrom-Json
Write-Output "PASS: $($trace.Count) prepared candidate and portal lists match native D1"
Write-Output "Each engine also compared every CPU candidate list with its actual renderer and checked live-object/RNG preservation"
Write-Output "Candidate traces: $outputPath"
