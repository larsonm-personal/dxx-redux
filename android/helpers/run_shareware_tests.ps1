#!/usr/bin/env pwsh
# Run the existing audited scenarios without the full suite's retail provisioning
[CmdletBinding()]
param(
    [ValidateSet('both', 'd1', 'd2')][string]$Game = 'both',
    [string]$Serial,
    [string]$Filter = '*',
    [switch]$ListTests
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'test_helpers.ps1')
$cases = @(
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $script:ANDROID_ROOT 'game_scripts') -Filter 'test_*.jsonc' | Sort-Object Name) {
        $info = Get-TestScriptInfo -ScriptPath $file.FullName
        if (-not $info.PSObject.Properties['_shareware'] -or $info._shareware -ne $true -or $file.BaseName -notlike $Filter) { continue }
        $games = @($info.games | Where-Object { $Game -eq 'both' -or $_ -eq $Game })
        if (-not $games.Count) { continue }
        $owner = if ($info.PSObject.Properties['_standalone'] -and $info._standalone -eq $false) { [string]$info._owner } else { '' }
        [pscustomobject]@{ name = $file.BaseName; games = $games; owner = $owner; path = $file.FullName }
    }
)
if ($ListTests) {
    ConvertTo-Json -InputObject @($cases | Select-Object name, games, owner) -Depth 5
    return
}
if (-not $cases.Count) { throw "No audited public scenarios match $Filter for $Game" }
# No implicit downloads; prepare with setup_shareware_test_data.ps1 -Download
foreach ($assetGame in @('d1', 'd2')) {
    . (Join-Path $PSScriptRoot 'shareware_test_data.ps1')
    Resolve-SharewareTestData -RepoRoot $script:REPO_ROOT -Game $assetGame | Out-Null
}
$failures = @()
$pwsh = Get-RegressionCurrentPwshPath
foreach ($case in $cases) {
    foreach ($engine in $case.games) {
        $arguments = @('-NoProfile', '-NonInteractive', '-File')
        if ($case.owner) {
            $ownerPath = Join-Path $script:ANDROID_ROOT "tests/$($case.owner).ps1"
            if (-not (Test-Path -LiteralPath $ownerPath)) { throw "Missing existing integration owner: $ownerPath" }
            $arguments += @($ownerPath, '-Game', $engine, '-DataEdition', 'shareware')
        } else {
            $arguments += @((Join-Path $PSScriptRoot 'run_test.ps1'), '-ScriptName', $case.path, '-Game', $engine, '-DataEdition', 'shareware')
        }
        if ($Serial) { $arguments += @('-Serial', $Serial) }
        Write-Host "RUN public $($case.name)/$engine"
        & $pwsh @arguments
        if ($LASTEXITCODE -ne 0) {
            $failures += "$($case.name)/$engine (exit $LASTEXITCODE)"
        }
    }
}
if ($failures.Count) { throw "Public scenarios did not pass: $($failures -join ', ')" }
Write-Host "PASS: $($cases.Count) public scenarios"
