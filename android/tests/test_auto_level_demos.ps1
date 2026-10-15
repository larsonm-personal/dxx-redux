#!/usr/bin/env pwsh
# Record on Android, then replay the exported file without a companion save
param(
    [ValidateSet('d1', 'd2', 'd1-in-d2')][string]$Game,
    [string]$Serial,
    [ValidateSet('retail', 'shareware')][string]$DataEdition = 'retail',
    [switch]$Cheats,
    [switch]$RecordState,
    [switch]$Secret,
    [string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
if ($Secret -and ($Game -ne 'd2' -or $DataEdition -ne 'retail' -or $Cheats)) {
    throw 'Secret validation requires -Game d2 -DataEdition retail without -Cheats'
}
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $script:REPO_ROOT ('android/temp/auto-level-demos-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
& (Join-Path $PSScriptRoot '../helpers/retain-recent-artifacts.ps1') -Artifacts @($OutputDirectory)
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$priorSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
$diagnostics = if ($RecordState) { 'on' } else { 'off' }
$scriptName = if ($Secret) { 'test_auto_level_secret.jsonc' } elseif ($Cheats) { 'test_auto_level_cheats.jsonc' } else { 'test_auto_level_demos.jsonc' }
$games = if ($Game) { @($Game) } else { @('d1', 'd2', 'd1-in-d2') }
try {
    foreach ($scenario in $games) {
        $gameName = if ($scenario -eq 'd1') { 'd1' } else { 'd2' }
        $missionMode = if ($scenario -eq 'd1-in-d2') { 'd1-in-d2' } else { 'native' }
        $destination = Join-Path $OutputDirectory $scenario
        New-Item -ItemType Directory -Force -Path $destination | Out-Null
        if ($Cheats -and $scenario -eq 'd2') {
            # Use the same grants and live firing sequence through the guided shot
            # Keep the recorder's explicit camera rejection covered independently
            $steps = @(Read-JsoncFile -Path (Join-Path $script:ANDROID_ROOT 'game_scripts/test_auto_level_cheats.jsonc'))
            $guidedIndex = @(0..($steps.Count - 1) | Where-Object {
                    $steps[$_].action -eq 'assert' -and $steps[$_].expect.'player.secondary_weapon' -eq '6'
                })
            if ($guidedIndex.Count -ne 1) { throw 'Expected one guided weapon selection in cheat scenario' }
            $steps[0]._info.vars.d2.GUIDED_FIRE_WHEN = 'd2'
            $probe = @($steps[0..($guidedIndex[0] + 4)]) + @(
                @{ action = 'wait_for'; expect = @{ 'input_demo.recording' = 'false'; 'in_game' = 'true' }; timeout_ms = 5000 },
                @{ action = 'enter_launcher' },
                @{ action = 'setup_command'; command = 'write_bool_pref'; args = @{ key = 'demo_record_each_level'; value = $false } }
            )
            $probePath = Join-Path $destination 'guided-rejection.jsonc'
            ConvertTo-Json -InputObject $probe -Depth 30 | Set-Content -LiteralPath $probePath -Encoding utf8NoBOM
            Adb -AdbArgs @('logcat', '-c') | Out-Null
            & (Join-Path $PSScriptRoot '../helpers/run_test.ps1') -Game $gameName -ScriptName $probePath -DataEdition $DataEdition -Params @{ MISSION_MODE = $missionMode; DIAGNOSTICS = $diagnostics } -TimeoutSeconds 240 *>&1 |
                Tee-Object -FilePath (Join-Path $destination 'guided-rejection.txt')
            $probeExit = $LASTEXITCODE
            $probeLog = Join-Path $destination 'guided-rejection-logcat.txt'
            Adb -AdbArgs @('logcat', '-d') | Set-Content -LiteralPath $probeLog -Encoding utf8NoBOM
            if ($probeExit -ne 0 -or !(Select-String -LiteralPath $probeLog -SimpleMatch 'Input demo recording stopped: guided missile camera is not supported by input recording' -Quiet)) {
                throw 'Guided camera recording rejection was not verified'
            }
            Write-Output "PASS: $scenario guided camera recording rejection"
        }
        Adb -AdbArgs @('logcat', '-c') | Out-Null
        & (Join-Path $PSScriptRoot '../helpers/run_test.ps1') -Game $gameName -ScriptName $scriptName -DataEdition $DataEdition -Params @{ MISSION_MODE = $missionMode; DIAGNOSTICS = $diagnostics } -TimeoutSeconds 240 *>&1 |
            Tee-Object -FilePath (Join-Path $destination 'android.txt')
        $recordingExit = $LASTEXITCODE
        Adb -AdbArgs @('logcat', '-d') | Set-Content (Join-Path $destination 'logcat.txt') -Encoding utf8NoBOM
        if ($recordingExit -ne 0) {
            Adb -AdbArgs @('shell', 'dumpsys', 'activity', 'exit-info', $script:PACKAGE) |
                Set-Content (Join-Path $destination 'activity-exit-info.txt') -Encoding utf8NoBOM
            throw "Automatic demo recording failed for $gameName"
        }
        $remoteDir = "files/${gameName}x-redux/input_demo_recordings/new"
        $listing = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'find', $remoteDir, '-name', '*.dximdemo')
        $sources = @(($listing -join "`n") -split '\r?\n' | Where-Object { $_.Trim() })
        if ($sources.Count -ne 2) { throw "Expected two new $gameName recordings, got $($sources.Count)" }
        $levels = @()
        $localRecordings = @()
        foreach ($source in $sources) {
            $remote = $source.Trim()
            $local = Join-Path $destination ([IO.Path]::GetFileName($remote))
            foreach ($suffix in @('', '.rngtrace.jsonl')) {
                $encoded = Adb -AdbArgs @('exec-out', 'run-as', $script:PACKAGE, 'base64', '-w', '0', ($remote + $suffix))
                [IO.File]::WriteAllBytes(($local + $suffix), [Convert]::FromBase64String(($encoded -join '')))
            }
            $localRecordings += $local
        }
        # Preserve every capture before a replay failure can end the runner
        foreach ($local in $localRecordings) {
            $header = Get-Content -LiteralPath $local -First 1 | ConvertFrom-Json
            if ($header.version -ne 5) { throw 'Automatic recording lacks ordered input format 5' }
            if ($header.start_mode -ne 'new_level' -or !$header.level_start -or $header.start_save) {
                throw 'Automatic recording lacks checkpoint-free level-start metadata'
            }
            if ($header.game -ne $gameName -or ($scenario -eq 'd1-in-d2' -and $header.mission -ne 'descent')) {
                throw "Recording used the wrong engine or mission for $scenario"
            }
            if ($gameName -eq 'd2' -and $header.player_cfg.guidebot_routing_mode -cne 0) {
                throw 'General gameplay recordings must use the documented Original-routing policy'
            }
            $levels += $header.level
            $inventoryLevel = if ($Secret) { -1 } else { 2 }
            if (!$Cheats -and $header.level -eq $inventoryLevel -and ($header.level_start.primary_ammo[1] -ne 777 -or
                    $header.level_start.primary_weapon -ne 2 -or $header.level_start.lives -ne 5 -or
                    $header.level_start.energy -lt (140 * 65536) -or $header.level_start.energy -gt (150 * 65536))) {
                throw 'Level 2 recording did not preserve the carried inventory'
            }
            if ($Cheats) {
                if ($header.level -eq 1) {
                    $cheatCommands = @(Get-Content -LiteralPath $local | ForEach-Object {
                            $row = $_ | ConvertFrom-Json
                            if ($row.type -in @('frame', 'input_tail')) {
                                if ($row.events | Where-Object command -eq 'cheat') { throw 'Unexpected legacy weapon cheat event' }
                                foreach ($op in $row.dispatch.ops) {
                                    $command = $op.event
                                    if ($command.command -eq 'cheat') { throw 'Unexpected legacy weapon cheat command' }
                                    if ($command.command -in @('enable_cheats', 'grant_all_weapons')) {
                                        if ($op.k -cne 'command' -or $command.kind -cne 'direct_command') { throw 'Malformed weapon cheat operation' }
                                        if ($command.command -eq 'grant_all_weapons') { "$($command.command):$($command.grant)" } else { $command.command }
                                    }
                                }
                            }
                        })
                    $expectedCheats = if ($gameName -eq 'd1') {
                        @('enable_cheats', 'grant_all_weapons:d1_scourge', 'grant_all_weapons:d1_bigred', 'grant_all_weapons:d1_bigred')
                    } else {
                        # honestbob resolves the active gameplay profile in weapon_cheat_shared.c
                        $grant = if ($scenario -eq 'd1-in-d2') {
                            if ($DataEdition -eq 'shareware') { 'd1_scourge' } else { 'd1_bigred' }
                        } else { 'd2_honestbob' }
                        @("grant_all_weapons:$grant", "grant_all_weapons:$grant")
                    }
                    if (($cheatCommands -join ',') -cne ($expectedCheats -join ',')) { throw 'Missing or reordered weapon cheat commands' }
                } elseif ($header.level_start.cheats_enabled -ne 1 -or $header.level_start.score -ne 0) {
                    throw 'Next level lost cheat scoring state'
                }
            }
            if (Select-String -LiteralPath $local -Pattern '"type":"checkpoint"' -Quiet) {
                throw 'Automatic recording contains a checkpoint'
            }
            & (Join-Path $PSScriptRoot 'run_input_demo_replay.ps1') -DemoPath $local -Game $gameName -D1Mission:($scenario -eq 'd1-in-d2') -DataEdition $DataEdition -Mode accelerated -Runner windowed-no-present -StrictComparison -CompareRngTrace -CompareStateTrace:$RecordState -TimeoutSeconds 120 *>&1 |
                Tee-Object -FilePath (Join-Path $destination ("replay-level" + $header.level + ".txt"))
            if ($LASTEXITCODE -ne 0) { throw "Independent level demo replay failed for $gameName" }
        }
        $expectedLevels = if ($Secret) { '-1,1' } else { '1,2' }
        if ((($levels | Sort-Object) -join ',') -ne $expectedLevels) { throw "Missing expected level recordings: $expectedLevels" }
        Write-Output "PASS: $scenario automatic level recording and independent replay"
    }
} finally {
    $env:ANDROID_SERIAL = $priorSerial
}
