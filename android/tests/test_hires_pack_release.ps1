#!/usr/bin/env pwsh
# Normal launcher imports and fresh processes verify real packs, residency and disable
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [switch]$Install,
    [string]$ApkPath,
    [string]$PackDirectory,
    [switch]$VerifyAdditiveSprites,
    [ValidateSet('d1', 'd2', 'd1-in-d2')][string[]]$Modes = @('d1', 'd2', 'd1-in-d2'),
    [ValidateSet(128, 256, 512)][int[]]$Resolutions = @(128, 256, 512)
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
Initialize-AndroidTestTarget -Serial $Serial | Out-Null
Assert-IsolatedPhysicalTestApp
Ensure-EmulatorHealthy | Out-Null
$repo = Split-Path (Split-Path $PSScriptRoot)
if (!$PackDirectory) { $PackDirectory = Join-Path $repo 'game_data/mods/d2x-xl' }
. (Join-Path $PSScriptRoot '../helpers/uud_gameplay_fixture.ps1')
$output = Join-Path $repo ('android/temp/hires-release/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repo 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $output -DirectoryPrefix run_
New-Item -ItemType Directory -Force $output | Out-Null
if ($Install) {
    $apk = if ($ApkPath) { $ApkPath } else { Join-Path $repo 'android/app/build/outputs/apk/debug/app-debug.apk' }
    if (!(Test-Path -LiteralPath $apk)) { $apk = Join-Path $repo 'android/app/build/intermediates/apk/debug/app-debug.apk' }
    $result = Adb -AdbArgs @('install', '-r', '-t', $apk) -Seconds 180
    if ($result -notmatch 'Success') { throw "Install failed: $result" }
}
Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mkdir', '-p', 'files/hires-import') | Out-Null
$presentation = Join-Path $repo 'game_data/mods/d2x-xl/d1xr-hires.dxa'
Adb -AdbArgs @('push', $presentation, '/data/local/tmp/hires-presentation.dxa') | Out-Null
Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/hires-presentation.dxa', 'files/hires-import/d1xr-hires.dxa') | Out-Null
Adb -AdbArgs @('shell', 'rm', '-f', '/data/local/tmp/hires-presentation.dxa') | Out-Null
foreach ($mode in $Modes) {
    $contentGame = if ($mode -eq 'd2') { 'd2' } else { 'd1' }
    foreach ($resolution in $Resolutions) {
        $pack = "$contentGame-hires-$resolution-textures-ktx2.dxa"
        $source = Join-Path $PackDirectory $pack
        Adb -AdbArgs @('push', $source, "/data/local/tmp/$pack") -Seconds 120 | Out-Null
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', "/data/local/tmp/$pack", "files/hires-import/$pack") -Seconds 120 | Out-Null
        Adb -AdbArgs @('shell', 'rm', '-f', "/data/local/tmp/$pack") | Out-Null
        # The previous resolution's final off run is the next pack's baseline
        $phases = if ($resolution -eq $Resolutions[0]) { @('baseline', 'on', 'off') } else { @('on', 'off') }
        foreach ($phase in $phases) {
            $command = if ($phase -eq 'on') { 'import_mod' } else { 'delete_mod' }
            $status = if ($phase -eq 'on') { 'active' } else { 'none_loaded' }
            Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
            Adb -AdbArgs @('logcat', '-c') | Out-Null
            $game = if ($mode -eq 'd1') { 'd1' } else { 'd2' }
            $params = @{ MODE = $mode; SET = 'hires-verification'; PACK = $pack; MOD_COMMAND = $command; EXPECTED_STATUS = $status }
            & (Join-Path $PSScriptRoot '../helpers/run_test.ps1') -ScriptName test_hires_pack_release.jsonc -Game $game -Serial $env:ANDROID_SERIAL -LeaveRunning -TimeoutSeconds 240 -Params $params
            $testResult = $LASTEXITCODE
            $json = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/introspect.json')
            $json | Set-Content -LiteralPath (Join-Path $output "$mode-$resolution-$phase.json") -Encoding utf8
            Adb -AdbArgs @('logcat', '-d') -Seconds 30 | Set-Content -LiteralPath (Join-Path $output "$mode-$resolution-$phase.log") -Encoding utf8
            if ($testResult -ne 0) { throw "Hires scenario failed: $mode $resolution $phase; $output" }
            Adb -AdbArgs @('shell', 'screencap', '-p', '/data/local/tmp/hires-video-info.png') | Out-Null
            Adb -AdbArgs @('pull', '/data/local/tmp/hires-video-info.png', (Join-Path $output "$mode-$resolution-$phase.png")) | Out-Null
            Adb -AdbArgs @('shell', 'rm', '-f', '/data/local/tmp/hires-video-info.png') | Out-Null
            $state = $json | ConvertFrom-Json
            if ($phase -eq 'on') {
                $used = @($state.texture_packs.packs | Where-Object { $_.name -eq $pack })
                if ($used.Count -ne 1 -or $used[0].loaded -le 0 -or $used[0].mipmapped -le 0 -or $used[0].max_mips -lt 2) {
                    throw "Missing successful pack and mipmap evidence: $mode $resolution; $output"
                }
                $art = @($state.mod_usage.presentation | Where-Object { $_.source -like '*/d1xr-hires.dxa' })
                if ($mode -ne 'd2' -and (@($art | Where-Object { $_.kind -eq 'font' }).Count -eq 0 -or
                        @($art | Where-Object { $_.kind -eq 'image' -and $_.width -ge 640 }).Count -eq 0)) {
                    throw "D1 presentation pack not used in $mode; $output"
                }
                if ($mode -eq 'd2' -and $art.Count -ne 0) { throw 'D1 presentation pack leaked into D2' }
                if ($VerifyAdditiveSprites -and $mode -eq 'd2') {
                    $spriteState = Invoke-UudGameplayProbe -OutputDirectory $output -Name "d2-$resolution-sprites" -Steps @(
                        @{ action = 'controller_input'; key = 'B'; post_delay_ms = 100 }
                        @{ action = 'controller_input'; key = 'B'; post_delay_ms = 100 }
                        @{ action = 'wait_for'; timeout_ms = 10000; expect = @{ time_paused = $false; game_window_is_front = $true } }
                        # Face the stock level-one robot in segment 35 from the adjoining room
                        # Player-owned muzzle flashes are intentionally hidden by the engine
                        @{ action = 'pose_view'; segment = 76; x = 0.16; y = -73.22; z = 25.61; heading = 0; post_delay_ms = 100 }
                        @{ action = 'wait_for'; timeout_ms = 20000; expect = @{ 'texture_packs.additive_sprite_draws' = @{ gt = 0 } } }
                        @{ action = 'introspect' }
                    )
                    $drawn = @($spriteState.texture_packs.additive_sprites | Where-Object { $_.draws -gt 0 -and $_.source -like "*/$pack" })
                    if (!$drawn.Count) { throw "No actual muzzle sprite draws from $pack" }
                }
            } elseif ($state.texture_packs.loaded -ne 0) {
                throw "Disabled pack still in use: $mode $resolution; $output"
            }
        }
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', "files/hires-import/$pack") | Out-Null
    }
}
Write-Host "Hires release integration passed: $output"
