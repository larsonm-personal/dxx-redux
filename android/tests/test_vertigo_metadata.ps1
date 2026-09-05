#!/usr/bin/env pwsh
# Exercise real Vertigo data through a reused native metadata worker
param(
    [string]$DataDir = '',
    [string]$VertigoDir = '',
    [string]$Worker = ''
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
if (-not $DataDir) { $DataDir = Join-Path $repoRoot 'game_data_to_copy_to_emulator/temp' }
if (-not $VertigoDir) { $VertigoDir = Join-Path $repoRoot 'game_data/CD images/Descent II - The Vertigo Series (USA)/data_tracks/vertigo' }
if (-not $Worker) { $Worker = Join-Path $repoRoot 'buildd2/main/dxx-redux-d2-metadata-worker.exe' }
$fixture = Join-Path $repoRoot 'android/temp/vertigo_metadata_fixture'
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $VertigoDir 'd2x.mn2') -Destination $fixture -Force
[IO.File]::WriteAllBytes((Join-Path $fixture 'd2x.hog'), [Text.Encoding]::ASCII.GetBytes('DHF'))
$start = [Diagnostics.ProcessStartInfo]::new($Worker)
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardInput = $true
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$process = [Diagnostics.Process]::Start($start)
$stderr = $process.StandardError.ReadToEndAsync()
try {
    foreach ($stage in @($fixture, $VertigoDir, $fixture, $VertigoDir)) {
        $request = @{
            schema = 'dxx-level-metadata-request-v1'; request_id = [guid]::NewGuid().ToString('N')
            game = 'd2'; source_type = 'mission_files'; source_name = 'Vertigo regression'
            data_dir = $DataDir; extra_data_dir = $stage; mission_name = 'd2x'
            mission_filename = 'd2x.mn2'; hog_paths = @()
            normal_level_files = @('d2xlvl01.rl2'); secret_level_files = @()
        }
        $process.StandardInput.WriteLine(($request | ConvertTo-Json -Compress))
        $result = $null
        while ($null -eq $result) {
            $read = $process.StandardOutput.ReadLineAsync()
            if (-not $read.Wait(120000)) { throw 'Vertigo metadata worker timed out' }
            $line = $read.Result
            if ($null -eq $line) { throw "Worker closed output: $($stderr.GetAwaiter().GetResult())" }
            if ($line.StartsWith("DXXMETA`t")) { $result = $line.Substring(8) | ConvertFrom-Json }
        }
        if ($stage -eq $fixture) {
            if ($result.status -ne 'failed' -or ($result.problems -join ' ') -notmatch 'd2x.ham') {
                throw "Missing HAM must fail cleanly: $($result | ConvertTo-Json -Depth 4 -Compress)"
            }
        } elseif ($result.status -ne 'ok' -or @($result.levels).Count -ne 1 -or $result.levels[0].status -ne 'ok') {
            throw "Valid descriptor-only Vertigo request failed: $($result | ConvertTo-Json -Depth 4 -Compress)"
        }
    }
    $process.StandardInput.Close()
    if (-not $process.WaitForExit(10000) -or $process.ExitCode -ne 0) { throw 'Worker did not exit cleanly' }
    Write-Output 'PASS: Vertigo descriptor-only loading, missing HAM rejection, and worker mount cleanup'
} finally {
    if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    $process.Dispose()
}
