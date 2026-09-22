#!/usr/bin/env pwsh
# Paired native-repeat/imported captures; incomplete evidence is a nonzero result
[CmdletBinding()]
param(
    [string]$DataDir,
    [string[]]$DemoFileName,
    [string]$OutputPath,
    [int]$TimeoutSeconds = 300,
    [ValidateRange(0.01, 1048576)][double]$MinimumFreeSpaceGB = 4,
    [ValidateRange(0.01, 1048576)][double]$RetainedHistoryGB = 8
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot 'input_demo_host_build_guard.ps1')
. (Join-Path $repoRoot 'android/helpers/output_disk_space.ps1')
if (-not $OutputPath) {
    $OutputPath = Join-Path $repoRoot "temp/d1_replay_parity_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
}
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
$retention = @{ Artifacts = $OutputPath; Keep = 2; MinimumFreeSpaceGB = $MinimumFreeSpaceGB; MaxFamilyBytes = [long]($RetainedHistoryGB * 1GB) }
if ([IO.Path]::GetFileName($OutputPath).StartsWith('d1_replay_parity_', [StringComparison]::OrdinalIgnoreCase)) {
    $retention.DirectoryPrefix = 'd1_replay_parity_'
}
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') @retention
Assert-OutputDiskSpace -Paths @($repoRoot, $OutputPath) -MinimumFreeGB $MinimumFreeSpaceGB
$demos = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'android/regression_demos') -Filter '*.dximdemo' -File |
        Where-Object { (Get-InputDemoRecordedGameName -DemoPath $_.FullName) -eq 'd1' } | Sort-Object Name)
if ($DemoFileName) {
    foreach ($name in $DemoFileName) {
        if ($name -cnotin $demos.Name) { throw "Required D1 recording not found: $name" }
    }
    $demos = @($demos | Where-Object Name -CIn $DemoFileName)
}
if (-not $demos.Count) { throw 'No D1 recordings selected' }
$pwsh = (Get-Process -Id $PID).Path
if (-not $DataDir) {
    $resolved = & $pwsh -NoProfile -File (Join-Path $PSScriptRoot 'run_input_demo_replay.ps1') `
        -DemoPath $demos[0].FullName -Game d1 -Mode accelerated -ResolveDataDirOnly
    if ($LASTEXITCODE -ne 0) { throw 'Could not resolve native D1 resources' }
    $line = @($resolved | Where-Object { $_ -like 'Resolved d1 data dir: *' })
    if ($line.Count -ne 1) { throw 'Native data resolver returned no unique directory' }
    $DataDir = $line[0].Substring('Resolved d1 data dir: '.Length)
}
$DataDir = (Resolve-Path -LiteralPath $DataDir).Path
foreach ($game in @('d1', 'd2')) {
    Ensure-InputDemoExecutable -RepoRoot $repoRoot -GameName $game `
        -ExecutablePath (Get-InputDemoExecutablePath -RepoRoot $repoRoot -GameName $game)
}
$arguments = @(
    (Join-Path $PSScriptRoot 'd1_replay_parity.py'), '--repo', $repoRoot, '--data', $DataDir,
    '--native', (Get-InputDemoExecutablePath -RepoRoot $repoRoot -GameName d1),
    '--imported', (Get-InputDemoExecutablePath -RepoRoot $repoRoot -GameName d2),
    '--output', $OutputPath, '--pwsh', $pwsh, '--timeout', $TimeoutSeconds,
    '--minimum-free-gb', $MinimumFreeSpaceGB.ToString([Globalization.CultureInfo]::InvariantCulture)
)
foreach ($demo in $demos) { $arguments += @('--demo', $demo.FullName) }
& python @arguments
exit $LASTEXITCODE
