#!/usr/bin/env pwsh
Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/host_metadata_worker.ps1')
$root = Join-Path $repoRoot 'android/temp/metadata-level-header-test'
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root
New-Item -ItemType Directory -Path $root -Force | Out-Null
foreach ($game in @('d1', 'd2')) {
    $extension = if ($game -eq 'd1') { 'rdl' } else { 'rl2' }
    $worker = New-MetadataWorker -Executable (Join-Path $repoRoot "build$game/main/dxx-redux-$game-metadata-worker.exe")
    try {
        foreach ($case in @('future', 'truncated', 'signature', 'zero')) {
            $file = "$case.$extension"
            $bytes = [Text.Encoding]::ASCII.GetBytes('LVLP') + [BitConverter]::GetBytes([int]23)
            if ($case -eq 'truncated') { $bytes = [Text.Encoding]::ASCII.GetBytes('LVLP') }
            if ($case -eq 'signature') { $bytes[0] = 0 }
            if ($case -eq 'zero') { $bytes = [Text.Encoding]::ASCII.GetBytes('LVLP') + [BitConverter]::GetBytes([int]0) }
            [IO.File]::WriteAllBytes((Join-Path $root $file), $bytes)
            $request = @{
                schema = 'dxx-level-metadata-request-v1'; request_id = [guid]::NewGuid().ToString('N')
                game = $game; source_type = 'level'; source_name = $case
                data_dir = Join-Path $repoRoot 'game_data_to_copy_to_emulator/temp'
                extra_data_dir = $root; level_file = $file; level_num = 1
            }
            $result = Invoke-MetadataWorker -Worker $worker -Request $request `
                -RawOutputPath (Join-Path $root "$game-$case.json") -LogPath (Join-Path $root "$game-$case.log") -TimeoutSeconds 10
            $expected = if ($case -eq 'future') { 'unsupported_format' } else { 'invalid_input' }
            if ($result.status -ne 'failed' -or @($result.levels).Count -ne 1 -or $result.levels[0].failure_kind -ne $expected) {
                throw "Incorrect $game result for $case"
            }
            if ($case -eq 'future' -and ($result.levels[0].problems -join ' ') -notmatch 'unsupported level version 23') {
                throw 'Unsupported format must identify the rejected version'
            }
        }
    } finally { Stop-MetadataWorkerProcess -Worker $worker }
}
Write-Host 'Native D1/D2 metadata rejects unsupported and malformed headers without hanging or losing worker state'
