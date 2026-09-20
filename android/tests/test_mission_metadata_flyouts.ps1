#!/usr/bin/env pwsh
Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/host_metadata_worker.ps1')
. (Join-Path $repoRoot 'android/helpers/standard_game_data.ps1')
$root = Join-Path $repoRoot 'temp/test_mission_metadata_flyouts'
New-Item -ItemType Directory -Force -Path $root | Out-Null
$deps = @(Get-StandardGameDataDeps)
$candidates = @('game_data_to_copy_to_emulator/temp', 'game_data_to_copy_to_emulator/data', 'game_data/extracted/d1 mac extracted' | ForEach-Object { Join-Path $repoRoot $_ })
foreach ($game in @('d1', 'd2')) {
    $names = if ($game -eq 'd1') { @('descent.hog', 'descent.pig') } else { @('descent2.hog', 'descent2.ham', 'groupa.pig') }
    $data = Resolve-StandardGameDataDirectory -Candidates $candidates -Dependencies @($deps | Where-Object file -in $names) -Label $game
    $baselineName = if ($game -eq 'd1') { 'FirstStrike' } else { 'Counterstrike' }
    $baseline = Get-Content (Join-Path $repoRoot "game_data/mission_files/$baselineName.json") -Raw | ConvertFrom-Json
    $worker = New-MetadataWorker -Executable (Join-Path $repoRoot "build$game/main/dxx-redux-$game-metadata-worker.exe")
    try {
        $cases = if ($game -eq 'd1') { @('in_engine', 'invalid_data', 'missing_level') } else { @('video', 'missing_movie', 'long_movie', 'invalid_movie') }
        foreach ($case in $cases) {
            $stage = Join-Path $root "$game-$case"
            New-Item -ItemType Directory -Force -Path $stage | Out-Null
            $request = [ordered]@{
                schema = 'dxx-level-metadata-request-v1'; request_id = [guid]::NewGuid().ToString('N')
                game = $game; source_type = 'active_level'; source_name = $case
                data_dir = $data.Path; extra_data_dir = $stage; mission_name = ''
                level_file = $baseline.levels[0].level_file; level_num = 1
            }
            if ($case -eq 'video') {
                $request.flyout_movie_library = Join-Path $repoRoot 'game_data/d2 1.1 data tracks from cd image tool/OTHER-H.MVL'
            }
            if ($case -eq 'invalid_data') {
                [IO.File]::WriteAllText((Join-Path $stage ([IO.Path]::ChangeExtension($request.level_file, 'end'))), "broken`n")
            }
            if ($case -eq 'missing_level') { $request.level_file = 'absent.rdl' }
            if ($case -eq 'invalid_movie') { [IO.File]::WriteAllText((Join-Path $stage 'esa.mve'), 'truncated') }
            if ($case -eq 'long_movie') {
                $bytes = [Collections.Generic.List[byte]]::new()
                $bytes.AddRange([Text.Encoding]::ASCII.GetBytes("Interplay MVE File$([char]26)"))
                $bytes.AddRange([byte[]]::new(7))
                # One 100 ms timer followed by 900 display opcodes: 90 seconds
                $bytes.AddRange([byte[]]@(10, 0, 0, 0, 6, 0, 2, 0))
                $bytes.AddRange([BitConverter]::GetBytes([uint32]100000))
                $bytes.AddRange([byte[]]@(1, 0))
                foreach ($frame in 1..900) { $bytes.AddRange([byte[]]@(4, 0, 3, 0, 0, 0, 7, 0)) }
                [IO.File]::WriteAllBytes((Join-Path $stage 'esa.mve'), $bytes.ToArray())
            }
            $result = Invoke-MetadataWorker -Worker $worker -Request $request -RawOutputPath (Join-Path $root "$game-$case.json") -LogPath (Join-Path $root "$game-$case.log") -TimeoutSeconds 120
            $flyout = $result.levels[0].flyout
            if ($case -ne 'missing_level') {
                if ($flyout.exit_trigger -ne 'present' -or $flyout.tunnel.status -ne 'present') { throw 'Exit trigger/tunnel evidence missing' }
                if ($game -eq 'd1' -and $flyout.movie.status -ne 'not_applicable_d1') { throw 'D1 incorrectly expects a movie' }
                if ($case -eq 'invalid_data' -and $flyout.presentation.status -ne 'invalid_endlevel_data') { throw 'Invalid exterior hidden by valid tunnel' }
                if ($case -eq 'missing_movie' -and $flyout.movie.status -ne 'missing_movie') { throw 'Missing movie evidence lost' }
                if ($case -eq 'video' -and $flyout.movie.status -ne 'measured') { throw 'Movie evidence missing' }
            }
            if ($case -ne 'in_engine') {
                if ($flyout.PSObject.Properties.Name -contains 'tunnel' -and $flyout.tunnel.PSObject.Properties.Name -contains 'seconds') { throw 'Unused tunnel timing published' }
                if ($flyout.PSObject.Properties.Name -contains 'routes') {
                    foreach ($route in $flyout.routes) {
                        if ($route.PSObject.Properties.Name -contains 'seconds' -or $route.PSObject.Properties.Name -contains 'tunnel_units') { throw 'Unused route estimate published' }
                    }
                }
                if ($case -notin @('video', 'long_movie') -and ($flyout.kind -ne 'none' -or $null -ne $flyout.seconds)) { throw 'Unavailable flyout still advertises playback' }
            }
            switch ($case) {
                in_engine { if ($flyout.kind -ne 'in_engine' -or $flyout.status -ne 'estimated' -or $flyout.seconds -le 5) { throw "Bad in_engine estimate: $($flyout | ConvertTo-Json -Depth 8 -Compress)" } }
                invalid_data { if ($flyout.status -ne 'invalid_endlevel_data') { throw 'Malformed END data was not reported' } }
                missing_level { if ($flyout.status -ne 'level_not_loaded' -or $null -ne $flyout.seconds) { throw 'Missing level acquired a time' } }
                video { if ($flyout.status -ne 'measured' -or $flyout.seconds -lt 16.9 -or $flyout.seconds -gt 17.4) { throw "Bad ESA movie duration: $($flyout | ConvertTo-Json -Compress)" } }
                missing_movie { if ($flyout.status -ne 'missing_movie' -or $null -ne $flyout.seconds) { throw 'Movie mount leaked between requests' } }
                long_movie { if ($flyout.status -ne 'measured' -or $flyout.seconds -ne 90) { throw 'Movie duration was capped or floored' } }
                invalid_movie { if ($flyout.status -ne 'invalid_movie' -or $null -ne $flyout.seconds) { throw 'Malformed movie acquired a time' } }
            }
            Write-Host "PASS $game $case"
        }
    } finally { Stop-MetadataWorkerProcess -Worker $worker }
}
