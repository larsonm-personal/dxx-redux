#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
. (Join-Path (Split-Path $PSScriptRoot) 'helpers/powershell_compat.ps1')
# Load the real pure comparison/configuration functions without launching a game
$tokens = $null
$errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $PSScriptRoot 'run_input_demo_replay.ps1'), [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw ($errors | Out-String) }
foreach ($name in @('ConvertTo-DeepHashtableClone', 'Normalize-D1InD2ExpectedResult', 'Compare-JsonDiff', 'Format-CompareValue', 'Get-D1InD2GameConfig', 'Get-LaunchArguments')) {
    $function = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name }, $true)
    if (-not $function) { throw "Missing comparison function: $name" }
    . ([scriptblock]::Create($function.Extent.Text))
}
function Get-GameConfig {
    param([string]$Name)
    return @{ Name = $Name; RequiredFiles = @("$Name.hog"); RequiredHashes = @("$Name hash"); DefaultDataDirs = @("$Name data") }
}
$config = Get-D1InD2GameConfig
if ($config.Name -cne 'd2' -or $config.RequiredFiles[0] -cne 'd1.hog' -or $config.RequiredHashes[0] -cne 'd1 hash') {
    throw 'Imported replay must use the D2 engine with native D1 data requirements'
}
$arguments = Get-LaunchArguments -Config @{ TitleArg = '-nomovies' } -ResolvedDataDir 'd1 data' -ResolvedDemoPath 'recording' `
    -ActualResultPath 'result' -LaunchMode @{ MaxFps = 200; ExtraArgs = @() } -RenderProfileSelection @{ ExtraArgs = @() } -D1InD2
if ('-d1' -cnotin $arguments -or '-inputdemo-d1-in-d2' -cnotin $arguments) { throw 'Imported replay must explicitly select D1 startup and checkpoint translation' }
$header = @{ game = 'd1'; mission = 'd1' }
$expected = @{ game = 'd1'; mission = 'd1'; player0 = @{ shields = 30 }; position = @{ x = 10 } }
$normalized = Normalize-D1InD2ExpectedResult -Expected $expected -Header $header
if ($normalized.game -cne 'd2' -or $normalized.mission -cne 'descent' -or $expected.game -cne 'd1') {
    throw 'Deterministic identity mapping failed or mutated the reference'
}
foreach ($reference in @(@{ game = 'd1'; mission = 'wrong' }, @{ game = 'd2'; mission = 'wrong' }, @{ game = 'other'; mission = 'd1' })) {
    $rejected = $false
    try { Normalize-D1InD2ExpectedResult -Expected $reference -Header $header | Out-Null } catch { $rejected = $true }
    if (-not $rejected) { throw 'Mismatched reference identity was accepted' }
}
$actual = @{ game = 'd2'; mission = 'wrong'; player0 = @{ shields = 29 }; position = @{ x = 11 } }
$diffs = @(Compare-JsonDiff -Expected $normalized -Actual $actual)
foreach ($field in @('mission', 'shields', 'position.x')) {
    if (-not ($diffs -like "*$field*")) { throw "Comparison hid $field" }
}
Write-Host 'PASS: replay reference identities cannot be copied from actual results'
& python -m unittest discover -s $PSScriptRoot -p test_d1_replay_parity_compare.py
if ($LASTEXITCODE -ne 0) { throw 'Paired replay evidence tests failed' }
