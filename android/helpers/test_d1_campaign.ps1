#!/usr/bin/env pwsh
# Compare real secret travel, death and save/restore in native and imported D1
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
$outputPath = Join-Path $repository "temp/d1-campaign-comparison"
& "$PSScriptRoot/retain-recent-artifacts.ps1" -Artifacts $outputPath
foreach ($run in @(
        @{ Name = "native"; Executable = $nativePath },
        @{ Name = "imported"; Executable = $importedPath }
    )) {
    $runDirectory = Join-Path $outputPath $run.Name
    New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
    Push-Location $runDirectory
    try {
        & $run.Executable --campaign-trace $assets *> output.log
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
Write-Output "PASS: $($trace.Count) native campaign observations match, including secret entry, save/restore, death and return"
Write-Output "Campaign traces: $outputPath"
