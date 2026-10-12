#!/usr/bin/env pwsh
# A truncated runtime tail must leave one usable menu after a cold resume
[CmdletBinding()]
param([string]$Serial = $env:ANDROID_SERIAL)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
Initialize-AndroidTestTarget -Serial $Serial | Out-Null
Assert-IsolatedPhysicalTestApp
Ensure-EmulatorHealthy | Out-Null
$repo = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repo ('android/temp/startup-restore-failure/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repo 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $output -DirectoryPrefix run_
New-Item -ItemType Directory -Force $output | Out-Null

# Reuse the maintained retail provisioning and initial exit-autosave flow
$template = Read-JsoncFile -Path (Join-Path $repo 'android/game_scripts/test_autosave_resume_missing_pilot_unified.jsonc')
$template[0]._info | Add-Member -Force NoteProperty _standalone $false
$template[0]._info | Add-Member -Force NoteProperty _owner 'test_startup_restore_failure'
$prepare = @()
$enteredLevel = $false
foreach ($step in $template) {
    $prepare += $step
    if ($step.action -eq 'wait_for' -and $step.field -eq 'in_game' -and $step.value -eq 'true') { $enteredLevel = $true }
    if ($enteredLevel -and $step.action -eq 'enter_launcher') { break }
}
if (!$enteredLevel) { throw 'Autosave preparation template no longer enters a level' }
$prepare += @{ action = 'wait_for'; field = 'resume_offer_enabled'; value = 'true'; timeout_ms = 10000 }
$prepare += @{ action = 'setup_command'; command = 'write_bool_pref'; args = @{ key = 'dlog_game logs_enabled'; value = $true }; post_delay_ms = 250 }
$prepare += @{ action = 'introspect' }
$preparePath = Join-Path $output 'prepare.jsonc'
$prepare | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $preparePath -Encoding utf8

foreach ($game in @('d1', 'd2')) {
    & (Join-Path $repo 'android/helpers/run_test.ps1') -ScriptName $preparePath -Game $game -Serial $env:ANDROID_SERIAL -DataEdition retail -LeaveRunning -TimeoutSeconds 180
    if ($LASTEXITCODE -ne 0) { throw "Could not create $game autosave" }
    $engineDir = "$($game)x-redux"
    $mission = if ($game -eq 'd1') { 'default' } else { 'd2' }
    $relative = "Players/save_sets/single/player/$mission/player.sg8"
    $deviceSave = "files/$engineDir/$relative"
    $encoded = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'base64', $deviceSave)
    $bytes = [Convert]::FromBase64String(($encoded -join ''))
    [IO.File]::WriteAllBytes((Join-Path $output "$game-original.sg8"), $bytes)
    # Native metadata footer: tag u32, version u16, trailer length u16
    if ($bytes.Length -lt 8 -or [BitConverter]::ToUInt32($bytes, $bytes.Length - 8) -ne 0x44584153) { throw 'Missing native save footer' }
    $nativeEnd = $bytes.Length - [BitConverter]::ToUInt16($bytes, $bytes.Length - 2)
    if ([Text.Encoding]::ASCII.GetString($bytes, $nativeEnd - 8, 8) -eq 'DXMODS01') {
        $nativeEnd -= 80 + [BitConverter]::ToUInt32($bytes, $nativeEnd - 16)
        # Exercise the legacy runtime EOF path rather than the new metadata preflight
        $legacyVersion = if ($game -eq 'd1') { 19 } else { 41 }
        [BitConverter]::GetBytes([int]$legacyVersion).CopyTo($bytes, 4)
    }
    if ($nativeEnd -lt 30000 -or $nativeEnd -ge $bytes.Length) { throw 'Invalid native save boundary' }
    # Preserve the complete core world; force checked EOF in the final runtime record
    $broken = Join-Path $output "$game-truncated.sg8"
    [IO.File]::WriteAllBytes($broken, $bytes[0..($nativeEnd - 5)])
    if (Get-AppProcessId "$($script:PACKAGE):game") { throw "$game did not exit before cold resume" }
    Adb -AdbArgs @('push', $broken, '/data/local/tmp/dxx-startup-restore.sg8') | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/dxx-startup-restore.sg8', $deviceSave) | Out-Null
    Adb -AdbArgs @('shell', 'rm', '-f', '/data/local/tmp/dxx-startup-restore.sg8') | Out-Null
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    # Keep the launcher candidate captured from the intact save, then cold-start the engine
    Send-MpCommand -Serial $env:ANDROID_SERIAL -Command 'tap_button' -Extras @('--es', 'text', "'Load Last Save'")
    $deadline = [Diagnostics.Stopwatch]::StartNew()
    $state = $null
    while ($deadline.Elapsed.TotalSeconds -lt 45) {
        $state = Get-GameIntrospection -Serial $env:ANDROID_SERIAL -Fresh
        if ($state -and !$state.in_game -and @($state.menu.items | Where-Object { $_.text -eq 'Quit' }).Count) { break }
    }
    $state | ConvertTo-Json -Depth 40 | Set-Content -LiteralPath (Join-Path $output "$game-menu.json") -Encoding utf8
    $log = Adb -AdbArgs @('logcat', '-d')
    $log | Set-Content -LiteralPath (Join-Path $output "$game-restore.log") -Encoding utf8
    if ($log -notmatch 'runtime (restore )?validation failed' -or $log -notmatch 'restore menu request consumed') {
        throw "$game did not exercise post-world restore rejection; see $output"
    }
    if (!$state -or $state.in_game -or $state.window_count -ne 1) { throw "$game recovery left an incomplete or duplicate menu; see $output" }

    # Use native menu geometry, including menu scaling, to exercise newmenu_mouse
    $quit = @($state.menu.items | Where-Object { $_.text -eq 'Quit' }) | Select-Object -First 1
    if (!$quit) { throw "$game recovery menu has no Quit" }
    $x = $quit.x + $quit.w / 2
    $y = $quit.y + $quit.h / 2
    if ($state.menu_scale.active) {
        $x = $state.menu_scale.dst.x + ($x - $state.menu_scale.src.x) * $state.menu_scale.scale
        $y = $state.menu_scale.dst.y + ($y - $state.menu_scale.src.y) * $state.menu_scale.scale
    }
    $x = [int][Math]::Round($x * $state.resolution.display_width / $state.resolution.render_width)
    $y = [int][Math]::Round($y * $state.resolution.display_height / $state.resolution.render_height)
    Adb -AdbArgs @('shell', 'input', 'tap', "$x", "$y") | Out-Null
    $deadline.Restart()
    do {
        Start-Sleep -Milliseconds 200
        $pidText = Adb -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game")
    } while ($pidText -and $deadline.Elapsed.TotalSeconds -lt 15)
    $log = Adb -AdbArgs @('logcat', '-d')
    $log | Set-Content -LiteralPath (Join-Path $output "$game-quit.log") -Encoding utf8
    if ($pidText -or $log -match 'Fatal signal|FATAL EXCEPTION') { throw "$game touch Quit failed or crashed; see $output" }
    Write-Host "$game failed startup restore and touch Quit passed; artifacts: $output"
}
