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
$arguments = Get-LaunchArguments -Config @{ TitleArg = '-nomovies'; D1Mission = $true } -ResolvedDataDir 'd1 data' -ResolvedDemoPath 'recording' `
    -ActualResultPath 'result' -LaunchMode @{ MaxFps = 200; ExtraArgs = @() } -RenderProfileSelection @{ ExtraArgs = @() }
if ('-d1' -cnotin $arguments -or '-inputdemo-d1-in-d2' -cin $arguments) {
    throw 'Native D2 recordings of D1 missions must select D1 assets without translating demo state'
}
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
. (Join-Path (Split-Path $PSScriptRoot) 'helpers/output_disk_space.ps1')
$spaceRejected = $false
try { Assert-OutputDiskSpace -Paths $PSScriptRoot -MinimumFreeGB 1048576 } catch {
    $spaceRejected = $_.Exception.Message -like 'Insufficient output disk space*'
    if (-not $spaceRejected) { throw }
}
if (-not $spaceRejected) { throw 'Replay disk reserve did not reject insufficient space' }
$waitFunction = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Wait-ForReplayResult' }, $true)
. ([scriptblock]::Create($waitFunction.Extent.Text))
$script:spaceChecks = 0
function Assert-OutputDiskSpace {
    param($Paths, $MinimumFreeGB)
    $script:spaceChecks++
    if ($script:spaceChecks -ge 2) { throw 'synthetic reserve reached during replay' }
}
$script:replayOutputPaths = @($PSScriptRoot)
$MinimumFreeSpaceGB = 4
$spaceRejected = $false
try {
    Wait-ForReplayResult -Process (Get-Process -Id $PID) -ActualResultPath (Join-Path $PSScriptRoot 'missing-disk-guard-result.json') -TimeoutSeconds 5 | Out-Null
} catch {
    $spaceRejected = $_.Exception.Message -eq 'synthetic reserve reached during replay'
    if (-not $spaceRejected) { throw }
}
if (-not $spaceRejected) { throw 'Replay did not check available space during its wait loop' }
Write-Host 'PASS: replay rejects low space before launch and checks its reserve during execution'
. (Join-Path (Split-Path $PSScriptRoot) 'helpers/test_host_platform.ps1')
$python = Resolve-RegressionPythonCommand
if (-not $python) { throw 'Python 3 is required for paired replay evidence tests' }
$pythonPrefix = @($python.PrefixArguments)
& $python.Path @pythonPrefix -m unittest discover -s $PSScriptRoot -p test_d1_replay_parity_compare.py
if ($LASTEXITCODE -ne 0) { throw 'Paired replay evidence tests failed' }

# An explicit native console backend is a capability choice, never a fallback
foreach ($name in @('Get-HeadlessConsoleExe', 'Test-CanUseHeadlessConsoleRunner', 'Resolve-ReplayRunnerSelection')) {
    $function = $ast.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name }, $true)
    . ([scriptblock]::Create($function.Extent.Text))
}
$Runner = 'headless-console'
$ExecutablePath = Join-Path $PSScriptRoot 'deliberately-not-built-console'
foreach ($game in @('d1', 'd2')) {
    foreach ($start in @('new_level', 'save_checkpoint')) {
        $selected = Resolve-ReplayRunnerSelection -RequestedRunner $Runner -ResolvedGame $game -Header @{ start_mode = $start } -LaunchMode @{ Name = 'accelerated' }
        if (-not $selected.UseHeadlessConsole -or $selected.UseNoRender) { throw 'Explicit console selection must stay display-free' }
    }
}
foreach ($case in @('imported', 'realtime', 'pilot', 'unsupported')) {
    $options = @{
        RequestedRunner = $Runner
        ResolvedGame = 'd1'
        Header = @{ start_mode = 'new_level' }
        LaunchMode = @{ Name = 'accelerated' }
    }
    if ($case -eq 'imported') { $options.D1InD2 = $true }
    if ($case -eq 'realtime') { $options.LaunchMode.Name = 'realtime' }
    if ($case -eq 'pilot') { $options.Pilot = 'test' }
    if ($case -eq 'unsupported') { $options.Header.start_mode = 'unsupported' }
    $rejected = $false
    try { Resolve-ReplayRunnerSelection @options | Out-Null } catch { $rejected = $true }
    if (-not $rejected) { throw "Console backend silently accepted $case" }
}
$Runner = 'fast'
$selected = Resolve-ReplayRunnerSelection -RequestedRunner $Runner -ResolvedGame d2 -Header @{ start_mode = 'new_level' } -LaunchMode @{ Name = 'accelerated' }
if ($selected.UseHeadlessConsole) { throw 'An explicit full-game executable must not accidentally select a different console binary' }
Write-Host 'PASS: explicit native console routing fails closed for unsupported combinations'

# A single optional native argument must remain an array, not splat into characters
$publicAst = [Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $PSScriptRoot 'test_compact_public_replays.ps1'), [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw ($errors | Out-String) }
$nativeAssignment = $publicAst.Find({ param($node)
    $node -is [Management.Automation.Language.AssignmentStatementAst] -and $node.Left.Extent.Text -like '*nativeArguments'
}, $true)
if (-not $nativeAssignment) { throw 'Missing public native-only argument construction' }
foreach ($NativeOnly in @($true, $false)) {
    . ([scriptblock]::Create($nativeAssignment.Extent.Text))
    $received = & $python.Path @pythonPrefix -c 'import json,sys; print(json.dumps(sys.argv[1:]))' @nativeArguments
    if ($LASTEXITCODE -ne 0) { throw 'Public native-only argument probe failed' }
    $expectedArguments = if ($NativeOnly) { '["--native-only"]' } else { '[]' }
    if ($received -cne $expectedArguments) { throw "Public native-only argument was not passed intact: $received" }
}
Write-Host 'PASS: public native-only selection preserves the exact Python argument'
