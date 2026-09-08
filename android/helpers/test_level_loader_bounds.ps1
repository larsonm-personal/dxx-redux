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
        @{ Name = 'comet'; Mission = 'CD - Descent - Levels of the World (USA).json'; File = 'comet.rdl'; ExpectedExit = 0; Repair = 'primary=137 overlay=14924 replaced with primary=137 overlay=43' },
        @{ Name = 'brinsane'; Mission = 'CD - Descent - Levels of the World (USA).json'; File = 'BRINSANE.rdl'; ExpectedExit = 0; Repair = 'primary=32767 overlay=523 replaced with primary=43 overlay=523' },
        @{ Name = 'insane'; Mission = 'CD - Dimensions for Descent (USA).json'; File = 'insane.rdl'; ExpectedExit = 0; Repair = 'primary=32767 overlay=523 replaced with primary=43 overlay=523' },
        @{ Name = 'kcxf2'; Mission = 'kcxf2.json'; File = 'kcxf204.rl2'; ExpectedExit = 0 }
    )) {
    $runRoot = Join-Path $OutputRoot $fixture.Name
    & $pwsh -NoProfile -File $runner -NoBuild -HeadlessExecutable $HeadlessExecutable -MissionJson $fixture.Mission -LevelFileFilter $fixture.File -MaxParallel 1 -OutputRoot $runRoot
    if ($LASTEXITCODE -ne $fixture.ExpectedExit) { throw "Unexpected $($fixture.Name) process result: $LASTEXITCODE" }
    $logs = @(Get-ChildItem -LiteralPath (Join-Path $runRoot 'logs') -File -Filter '*.log')
    if ($logs.Count -ne 1) { throw "Expected exactly one $($fixture.Name) engine run; found $($logs.Count)" }
    $content = Get-Content -LiteralPath $logs[0].FullName -Raw
    $records = Get-Content -LiteralPath (Join-Path $runRoot ('results/' + $fixture.Mission.Replace('.json', '.simulation.json'))) -Raw | ConvertFrom-Json
    $levelResult = @($records.levels | Where-Object level_file -eq $fixture.File)
    if ($levelResult.Count -ne 1) { throw "Expected one compact result for $($fixture.Name)" }
    if ($content -match 'AddressSanitizer|runtime error:') { throw "Sanitizer finding in $($logs[0].FullName)" }
    if ($fixture.Repair) {
        $texture = if ($fixture.Name -eq 'comet') { 14924 } else { 32767 }
        $expectedNote = "invalid texture $texture, 2 occurrences"
        if (@($levelResult[0].notes).Count -ne 1 -or $levelResult[0].notes[0] -cne $expectedNote) {
            throw "$($fixture.Name) did not retain its texture occurrence count in regression JSON"
        }
        if ($content -notmatch [regex]::Escape($fixture.Repair) -or $content -match "Couldn't load level file" -or $content -notmatch 'phase=simulation' -or $content -notmatch 'phase=result') {
            throw "$($fixture.Name) did not log its texture repair and complete simulation"
        }
    } else {
        if ($levelResult[0].notes) { throw 'Clean KCXF2 load unexpectedly has texture notes' }
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
