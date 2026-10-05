#!/usr/bin/env pwsh
param([switch]$NoBuild)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/standard_game_data.ps1')
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
$root = Join-Path $repoRoot ('android/temp/flyout-safety/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root -DirectoryPrefix run_
New-Item -ItemType Directory -Force $root | Out-Null
$deps = @(Get-StandardGameDataDeps)
$data = @{}
foreach ($game in @('d1', 'd2')) {
    if (-not $NoBuild) { Invoke-RegressionHostBuild -RepoRoot $repoRoot -Target $game }
    $names = if ($game -eq 'd1') { @('descent.hog', 'descent.pig') } else { @('descent2.hog', 'descent2.ham', 'groupa.pig', 'descent2.s22', 'alien1.pig', 'alien2.pig', 'fire.pig', 'ice.pig', 'water.pig') }
    $resolved = Resolve-StandardGameDataDirectory -Candidates @(Get-StandardGameDataCandidates -RepoRoot $repoRoot -Game $game) -Dependencies @($deps | Where-Object file -in $names) -Label $game
    $data[$game] = $resolved.Path
}
$enemy = Join-Path $root 'ewithin-rebirth.zip'
$archive = [IO.Compression.ZipFile]::OpenRead((Join-Path $repoRoot 'game_data/mission_files/ewithin-versions.zip'))
try {
    $entry = @($archive.Entries | Where-Object Name -eq 'ewithin-rebirth.zip')
    if ($entry.Count -ne 1) { throw 'Expected one anniversary Rebirth fixture' }
    [IO.Compression.ZipFileExtensions]::ExtractToFile($entry[0], $enemy, $true)
} finally { $archive.Dispose() }
$mission = Join-Path $root 'mission'
Expand-Archive -LiteralPath $enemy -DestinationPath $mission -Force
foreach ($case in @(
        @{ Name = 'd1'; Game = 'd1'; Arguments = @('--endlevel-trace', $data.d1) },
        @{ Name = 'd2'; Game = 'd2'; Arguments = @('--endlevel-trace', $data.d1) },
        @{ Name = 'grand-finale'; Game = 'd2'; Arguments = @('--fan-flyout-trace', $data.d1, $data.d2, $mission) }
    )) {
    $directory = Join-Path $root $case.Name
    New-Item -ItemType Directory -Force $directory | Out-Null
    $executable = Join-Path $repoRoot "build$($case.Game)/main/$((Get-RegressionHostExecutableNames -BaseName 'test_upstream_compat')[0])"
    $start = [Diagnostics.ProcessStartInfo]::new($executable)
    $start.WorkingDirectory = $directory
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in $case.Arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $completed = $process.WaitForExit(240000)
        if (-not $completed) {
            $process.Kill($true)
            $process.WaitForExit()
        }
        [IO.File]::WriteAllText((Join-Path $directory 'stdout.log'), $stdout.Result)
        [IO.File]::WriteAllText((Join-Path $directory 'stderr.log'), $stderr.Result)
        if (-not $completed) { throw "Fly-out case $($case.Name) timed out; see $directory" }
        if ($process.ExitCode -ne 0) { throw "Fly-out case $($case.Name) failed ($($process.ExitCode)); see $directory" }
        $report = if ($case.Name -eq 'grand-finale') { 'fan-flyouts.json' } else { 'endlevel.json' }
        $result = @(Get-Content -LiteralPath (Join-Path $directory $report) -Raw | ConvertFrom-Json)
        $expected = if ($case.Name -eq 'grand-finale') { 2 } else { 3 }
        if ($result.Count -ne $expected) { throw 'Incomplete fly-out regression report' }
        Write-Host "PASS $($case.Name): malformed data guards and complete fly-out travel"
    } finally {
        if (-not $process.HasExited) { $process.Kill($true); $process.WaitForExit() }
        $process.Dispose()
    }
}
Write-Host "Fly-out evidence: $root"
