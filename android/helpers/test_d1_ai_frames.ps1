#!/usr/bin/env pwsh
# Compare the public robot-frame operation against the native D1 executable
[CmdletBinding()]
param(
    [string]$NativeExecutable = "buildd1/main/test_upstream_compat.exe",
    [string]$ImportedExecutable = "buildd2/main/test_upstream_compat.exe"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$repository = Split-Path (Split-Path $PSScriptRoot)
$nativePath = (Resolve-Path -LiteralPath $NativeExecutable).Path
$importedPath = (Resolve-Path -LiteralPath $ImportedExecutable).Path
$outputPath = Join-Path $repository "temp/d1-ai-frame-comparison"
& "$PSScriptRoot/retain-recent-artifacts.ps1" -Artifacts $outputPath
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null

foreach ($run in @(
        @{ Name = "native"; Executable = $nativePath },
        @{ Name = "imported"; Executable = $importedPath }
    )) {
    $runDirectory = Join-Path $outputPath $run.Name
    New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
    Push-Location $runDirectory
    try {
        & $run.Executable --robot-frame-trace frames.json *> output.log
        if ($LASTEXITCODE -ne 0) {
            throw "$($run.Name) robot-frame trace failed (exit $LASTEXITCODE); see $runDirectory/output.log"
        }
    } finally {
        Pop-Location
    }
}

$native = Get-Content -LiteralPath (Join-Path $outputPath "native/frames.json") -Raw
$imported = Get-Content -LiteralPath (Join-Path $outputPath "imported/frames.json") -Raw
if ($native -cne $imported) {
    $expected = $native | ConvertFrom-Json
    $actual = $imported | ConvertFrom-Json
    if ($expected.cases.Count -ne $actual.cases.Count) {
        throw "Frame scenario counts differ; see $outputPath"
    }
    for ($case = 0; $case -lt $expected.cases.Count; ++$case) {
        $left = $expected.cases[$case]
        $right = $actual.cases[$case]
        if (($left | ConvertTo-Json -Depth 30 -Compress) -cne ($right | ConvertTo-Json -Depth 30 -Compress)) {
            throw "Frame mismatch: behavior $($left.behavior), state $($left.state), sight $($left.sight), variant $($left.variant); see $outputPath"
        }
    }
    throw "Frame trace formatting differs; see $outputPath"
}
$cases = ($native | ConvertFrom-Json).cases.Count
Write-Output "PASS: $cases scenarios, $($cases * 4) complete native/imported robot frames match"
Write-Output "Traces: $outputPath"
