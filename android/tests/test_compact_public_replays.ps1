#!/usr/bin/env pwsh
# Compact, ordinary-start public replays; no downloads or expectation updates
[CmdletBinding()]
param(
    [ValidateSet('headless', 'graphics')][string]$RunMode = 'headless',
    [switch]$NativeOnly,
    [string]$D1Exe,
    [string]$D2Exe,
    [string]$D1DataDir,
    [string]$D2DataDir,
    [string]$CorpusDirectory,
    [string]$Output,
    [ValidateRange(1, 3600)][int]$TimeoutSeconds = 120,
    [ValidateRange(0.01, 1048576)][double]$MinimumFreeSpaceGB = 4
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/shareware_test_data.ps1')
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
. (Join-Path $PSScriptRoot 'input_demo_host_build_guard.ps1')
$D1DataDir = Resolve-SharewareTestData -RepoRoot $repoRoot -Game d1 -DataDir $D1DataDir
$D2DataDir = Resolve-SharewareTestData -RepoRoot $repoRoot -Game d2 -DataDir $D2DataDir
foreach ($game in @('d1', 'd2')) {
    $variable = $game.ToUpperInvariant() + 'Exe'
    if (-not (Get-Variable -Name $variable -ValueOnly)) {
        $console = $RunMode -eq 'headless'
        Ensure-InputDemoGameBuild -RepoRoot $repoRoot -GameName $game -PreferHeadlessConsole:$console
        Set-Variable -Name $variable -Value (Get-InputDemoExecutablePath -RepoRoot $repoRoot -GameName $game -PreferHeadlessConsole:$console)
    }
}
foreach ($path in @($D1Exe, $D2Exe)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Replay executable not found: $path" }
}
$D1Exe = (Resolve-Path -LiteralPath $D1Exe).Path
$D2Exe = (Resolve-Path -LiteralPath $D2Exe).Path
if (-not $CorpusDirectory) { $CorpusDirectory = Join-Path $PSScriptRoot 'fixtures/public_level_replays' }
$CorpusDirectory = (Resolve-Path -LiteralPath $CorpusDirectory).Path
if (-not $Output) { $Output = Join-Path $repoRoot "temp/compact_public_replays_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')" }
$Output = [IO.Path]::GetFullPath($Output)
if (Test-Path -LiteralPath $Output) { throw "Use a new output directory: $Output" }
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $Output -MinimumFreeSpaceGB $MinimumFreeSpaceGB
$python = Resolve-RegressionPythonCommand
if (-not $python) { throw 'Python 3 is required for compact public replay verification' }
$prefix = @($python.PrefixArguments)
$pwsh = (Get-Process -Id $PID).Path
[string[]]$nativeArguments = if ($NativeOnly) { @('--native-only') } else { @() }
& $python.Path @prefix (Join-Path $PSScriptRoot 'run_compact_public_replays.py') --repo $repoRoot --corpus $CorpusDirectory --output $Output `
    --data-d1 $D1DataDir --data-d2 $D2DataDir --exe-d1 $D1Exe --exe-d2 $D2Exe --pwsh $pwsh `
    @nativeArguments --run-mode $RunMode --timeout $TimeoutSeconds --minimum-free-gb $MinimumFreeSpaceGB.ToString([Globalization.CultureInfo]::InvariantCulture)
if ($LASTEXITCODE -ne 0) { throw "Compact public replay fidelity or strict paired comparison failed; see $Output" }
Write-Output "PASS: compact native public scenes twice ($RunMode), exact live-capture frame/diagnostic/terminal/SIM fidelity and natural native exit"
if ($RunMode -eq 'graphics' -and -not $NativeOnly) {
    Write-Output 'PASS: scoped imported D1 flight gameplay-state/SIM-RNG parity; other imported scenes are unqualified probes; diagnostic/object/world and full simulation qualification remain INCOMPLETE'
} else {
    Write-Output 'NOT RUN: imported D1 parity, covered by the separate full graphics pass; full simulation qualification remains INCOMPLETE'
}
