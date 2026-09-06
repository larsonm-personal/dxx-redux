#!/usr/bin/env pwsh
# Parameterized sanitizer fixture, deliberately outside the ordinary test auto-discovery directory
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$HeadlessExecutable,
    [Parameter(Mandatory)][string]$OutputRoot
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$pwsh = (Get-Process -Id $PID).Path
foreach ($fixture in @(
        @{ Name = 'comet'; Mission = 'CD - Descent - Levels of the World (USA).json'; File = 'comet.rdl'; ExpectedExit = 1 },
        @{ Name = 'kcxf2'; Mission = 'kcxf2.json'; File = 'kcxf204.rl2'; ExpectedExit = 0 }
    )) {
    $runRoot = Join-Path $OutputRoot $fixture.Name
    & $pwsh -NoProfile -File $runner -NoBuild -HeadlessExecutable $HeadlessExecutable -MissionJson $fixture.Mission -LevelFileFilter $fixture.File -MaxParallel 1 -OutputRoot $runRoot
    if ($LASTEXITCODE -ne $fixture.ExpectedExit) { throw "Unexpected $($fixture.Name) process result: $LASTEXITCODE" }
    $logs = @(Get-ChildItem -LiteralPath (Join-Path $runRoot 'logs') -File -Filter '*.log')
    if ($logs.Count -ne 1) { throw "Expected exactly one $($fixture.Name) engine run; found $($logs.Count)" }
    $content = Get-Content -LiteralPath $logs[0].FullName -Raw
    if ($content -match 'AddressSanitizer|runtime error:') { throw "Sanitizer finding in $($logs[0].FullName)" }
    if ($fixture.Name -eq 'comet') {
        if ($content -notmatch 'Invalid level texture seg=51 side=0 .*overlay=14924' -or $content -notmatch "Couldn't load level file") {
            throw 'Comet did not fail explicitly at the invalid texture reference'
        }
    } else {
        if ($content -match 'Invalid level wall animation|Couldn.t load level file' -or $content -notmatch 'phase=simulation') {
            throw 'KCXF2 did not load its declared sections correctly'
        }
        $actual = Get-Content -LiteralPath (Join-Path $runRoot 'results/kcxf2.json_0_4_kcxf204.rl2_run_1.json') -Raw | ConvertFrom-Json
        if (-not @($actual.objectives | Where-Object label -eq 'blue key').Count) {
            throw 'KCXF2 did not reach its first key with the corrected wall table'
        }
    }
    Write-Host "PASS loader bounds: $($fixture.Name)"
}
exit 0
