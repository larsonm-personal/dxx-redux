#!/usr/bin/env pwsh
# Save, cold resume with installed mods disabled, then re-enable and resume again
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [ValidateSet('d1', 'd2', 'd1-in-d2', 'vertigo')][string[]]$Modes = @('d1', 'd2', 'd1-in-d2', 'vertigo')
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
Initialize-AndroidTestTarget -Serial $Serial | Out-Null
Assert-IsolatedPhysicalTestApp
Ensure-EmulatorHealthy | Out-Null
$repo = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repo 'android/temp/mod-save-toggle'
& (Join-Path $repo 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $output
New-Item -ItemType Directory -Force $output | Out-Null
$packs = @(
    'game_data/mods/xfing/dxx_tp/tmp/plain_texture_dxa/uud1tp-textures.dxa',
    'game_data/mods/xfing/dxx_tp/tmp/plain_texture_dxa/uud2tp-textures.dxa',
    'game_data/mods/xfing/d2x_sp/uud2sp-sounds.dxa'
)
Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mkdir', '-p', 'files/uud-import') | Out-Null
foreach ($relative in $packs) {
    $path = Join-Path $repo $relative
    $name = Split-Path $path -Leaf
    Adb -AdbArgs @('push', $path, "/data/local/tmp/$name") | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', "/data/local/tmp/$name", "files/uud-import/$name") | Out-Null
    Adb -AdbArgs @('shell', 'rm', '-f', "/data/local/tmp/$name") | Out-Null
}
foreach ($mode in $Modes) {
    $template = Read-JsoncFile -Path (Join-Path $repo 'android/game_scripts/test_uud2sp.jsonc')
    $template[0]._info._owner = 'test_mod_save_toggle'
    foreach ($dependency in $template[0]._info._deps) { $dependency.target = 'files/imported/sets/mod-save-verification' }
    foreach ($option in $template[0]._info.params.MODE.options.PSObject.Properties) { $option.Value.SET = 'mod-save-verification' }
    $level = if ($mode -eq 'vertigo') { '2' } else { '1' }
    if ($mode -eq 'vertigo') {
        $vertigo = Read-JsoncFile -Path (Join-Path $repo 'android/game_scripts/test_vertigo_level_metadata.jsonc')
        foreach ($dependency in $vertigo[0]._info._deps | Where-Object { $_.file -like 'd2x.*' }) {
            $dependency | Add-Member NoteProperty target 'files/imported/sets/mod-save-verification' -Force
            $template[0]._info._deps += $dependency
        }
        $template[0]._info.params.MODE.options.d2.MISSION = 'Vertigo'
    }
    $steps = @()
    foreach ($step in $template) {
        if ($step.action -eq 'setup_command' -and $step.args.key -eq 'show_resume_offer') { $step.args.value = $true }
        $steps += $step
        if ($mode -eq 'vertigo' -and $step.action -eq 'select_mission') {
            $steps += @{ action = 'key'; key = 'backspace'; post_delay_ms = 100 }
            $steps += @{ action = 'key'; key = '2'; post_delay_ms = 100 }
        }
        if ($step.action -eq 'wait_for' -and $step.field -eq 'in_game') { break }
    }
    $names = @(if ($mode -in @('d2', 'vertigo')) { 'uud2tp-textures.dxa'; 'uud2sp-sounds.dxa' } else { 'uud1tp-textures.dxa' })
    $steps += @{ action = 'enter_launcher' }
    foreach ($enabled in @($false, $true)) {
        foreach ($name in $names) {
            $steps += @{ action = 'setup_command'; command = 'set_mod_enabled'; args = @{ filename = $name; enabled = $enabled }; post_delay_ms = 500 }
        }
        $steps += @{ action = 'wait_for'; field = 'resume_offer_enabled'; value = 'true'; timeout_ms = 15000 }
        $steps += @{ action = 'tap_button'; text = 'Load Last Save'; launches_game = $true }
        $steps += @{ action = 'wait_for'; field = 'in_game'; value = 'true'; timeout_ms = 30000 }
        $steps += @{ action = 'assert'; expect = @{ 'mod_save.status' = 'presentation_adapted'; current_level_num = $level; in_game = 'true' } }
        $steps += @{ action = 'introspect' }
        $steps += @{ action = 'enter_launcher' }
    }
    if ($names.Count -gt 1) {
        $steps += @{ action = 'setup_command'; command = 'swap_mod_order'; args = @{ first = $names[0]; second = $names[1] }; post_delay_ms = 500 }
        $steps += @{ action = 'wait_for'; field = 'resume_offer_enabled'; value = 'true'; timeout_ms = 15000 }
        $steps += @{ action = 'tap_button'; text = 'Load Last Save'; launches_game = $true }
        $steps += @{ action = 'wait_for'; field = 'in_game'; value = 'true'; timeout_ms = 30000 }
        $steps += @{ action = 'assert'; expect = @{ current_level_num = $level } }
        $steps += @{ action = 'enter_launcher' }
    }
    $steps += @{ action = 'wait_for'; field = 'resume_offer_enabled'; value = 'true'; timeout_ms = 15000 }
    $steps += @{ action = 'tap_button'; text = 'Load Last Save'; launches_game = $true }
    $steps += @{ action = 'wait_for'; field = 'in_game'; value = 'true'; timeout_ms = 30000 }
    $steps += @{ action = 'assert'; expect = @{ 'mod_save.status' = 'exact'; current_level_num = $level } }
    $steps += @{ action = 'introspect' }
    $steps += @{ action = 'enter_launcher' }
    $steps += @{ action = 'wait_for'; field = 'resume_offer_enabled'; value = 'true'; timeout_ms = 15000 }
    $path = Join-Path $output "$mode.jsonc"
    $steps | ConvertTo-Json -Depth 40 | Set-Content -LiteralPath $path -Encoding utf8
    $params = @{
        MODE = $(if ($mode -eq 'vertigo') { 'd2' } else { $mode }); FIRST = $names[0]; SECOND = $(if ($names.Count -gt 1) { $names[1] } else { 'unused-test-mod.dxa' }); SOUND = 'unused-test-mod.dxa'
        FIRST_COMMAND = 'import_mod'; SECOND_COMMAND = $(if ($names.Count -gt 1) { 'import_mod' } else { 'delete_mod' }); EXTRAS_COMMAND = 'delete_mod'
    }
    $game = if ($mode -eq 'd1') { 'd1' } else { 'd2' }
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    & (Join-Path $repo 'android/helpers/run_test.ps1') -ScriptName $path -Game $game -Serial $env:ANDROID_SERIAL -DataEdition retail -LeaveRunning -TimeoutSeconds 300 -Params $params
    $result = $LASTEXITCODE
    Adb -AdbArgs @('logcat', '-d') | Set-Content -LiteralPath (Join-Path $output "$mode.log") -Encoding utf8
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/automation_log.jsonl') | Set-Content -LiteralPath (Join-Path $output "$mode-automation.jsonl") -Encoding utf8
    if ($result -ne 0) { throw "$mode mod save toggle failed; see $output" }
    if ($mode -eq 'd2') {
        # A damaged compatibility block must be rejected without rewriting the save
        $deviceSave = 'files/d2x-redux/Players/save_sets/single/player/d2/player.sg8'
        $encoded = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'base64', $deviceSave)
        $bytes = [Convert]::FromBase64String(($encoded -join ''))
        $end = $bytes.Length - [BitConverter]::ToUInt16($bytes, $bytes.Length - 2)
        if ([Text.Encoding]::ASCII.GetString($bytes, $end - 8, 8) -ne 'DXMODS01') { throw 'Missing mod source block' }
        $bytes[$end - 80] = $bytes[$end - 80] -bxor 1
        $broken = Join-Path $output 'damaged-source.sg8'
        [IO.File]::WriteAllBytes($broken, $bytes)
        Adb -AdbArgs @('push', $broken, '/data/local/tmp/mod-save-damaged.sg8') | Out-Null
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/mod-save-damaged.sg8', $deviceSave) | Out-Null
        Adb -AdbArgs @('shell', 'rm', '-f', '/data/local/tmp/mod-save-damaged.sg8') | Out-Null
        Send-MpCommand -Serial $env:ANDROID_SERIAL -Command tap_button -Extras @('--es', 'text', "'Load Last Save'")
        $timer = [Diagnostics.Stopwatch]::StartNew()
        do {
            $state = Get-GameIntrospection -Serial $env:ANDROID_SERIAL -Fresh
        } while ((!$state -or $state.mod_save.status -ne 'incompatible' -or !@($state.menu.items | Where-Object { $_.text -eq 'OK' }).Count) -and $timer.Elapsed.TotalSeconds -lt 45)
        $state | ConvertTo-Json -Depth 40 | Set-Content -LiteralPath (Join-Path $output 'rejection.json') -Encoding utf8
        if (!$state -or $state.in_game -or $state.mod_save.reason -notmatch 'checksum' -or $state.window_count -ne 2) { throw 'Damaged save did not show a usable compatibility error' }
        Adb -AdbArgs @('shell', 'input', 'keyevent', '66') | Out-Null
        $timer.Restart()
        do {
            $state = Get-GameIntrospection -Serial $env:ANDROID_SERIAL -Fresh
        } while ((!$state -or $state.window_count -ne 1) -and $timer.Elapsed.TotalSeconds -lt 15)
        if (!$state -or $state.window_count -ne 1 -or !@($state.menu.items | Where-Object { $_.text -eq 'Quit' }).Count) { throw 'Compatibility error did not return to one complete menu' }
        $after = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'base64', $deviceSave)
        if ([Convert]::ToBase64String($bytes) -ne (($after -join '') -replace '\s', '')) { throw 'Failed restore modified the source save' }
    }
    Write-Host "$mode installed mod disable, re-enable, and exact resave passed"
}
Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
