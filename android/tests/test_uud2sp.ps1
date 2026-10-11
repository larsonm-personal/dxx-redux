#!/usr/bin/env pwsh
# Launcher imports and real mission re-entry with UUD2SP, UUD2TP and high-res PCM
[CmdletBinding()]
param([string]$Serial = $env:ANDROID_SERIAL)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
Initialize-AndroidTestTarget -Serial $Serial | Out-Null
Assert-IsolatedPhysicalTestApp
Ensure-EmulatorHealthy | Out-Null
$repo = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repo 'android/temp/uud2sp-launcher'
& (Join-Path $repo 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $output
New-Item -ItemType Directory -Force $output | Out-Null
$sound = 'uud2sp-sounds.dxa'
$texture = 'uud2tp-textures.dxa'
$hires = 'd2-hires-sounds.dxa'
Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mkdir', '-p', 'files/uud-import') | Out-Null
foreach ($relative in @(
        "game_data/mods/xfing/d2x_sp/$sound",
        "game_data/mods/xfing/dxx_tp/tmp/plain_texture_dxa/$texture",
        "game_data/mods/d2x-xl/$hires",
        'game_data/mods/d2x-xl/d1xr-hires.dxa'
    )) {
    $path = Join-Path $repo $relative
    $name = Split-Path $path -Leaf
    Adb -AdbArgs @('push', $path, "/data/local/tmp/$name") -Seconds 120 | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', "/data/local/tmp/$name", "files/uud-import/$name") | Out-Null
    Adb -AdbArgs @('shell', 'rm', '-f', "/data/local/tmp/$name") | Out-Null
}
foreach ($phase in @('sound-only', 'sound-first', 'texture-first', 'hires-first', 'disabled')) {
    $order = switch ($phase) {
        'texture-first' { @($texture, $sound, $hires) }
        'hires-first' { @($hires, $texture, $sound) }
        default { @($sound, $texture, $hires) }
    }
    $combined = $phase -in @('sound-first', 'texture-first', 'hires-first')
    $params = @{
        MODE = 'd2'; FIRST = $order[0]; SECOND = $order[1]; SOUND = $order[2]
        FIRST_COMMAND = $(if ($phase -eq 'disabled') { 'delete_mod' } else { 'import_mod' })
        SECOND_COMMAND = $(if ($combined) { 'import_mod' } else { 'delete_mod' })
        EXTRAS_COMMAND = $(if ($combined) { 'import_mod' } else { 'delete_mod' })
    }
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    & (Join-Path $PSScriptRoot '../helpers/run_test.ps1') -ScriptName test_uud2sp.jsonc -Game d2 -Serial $env:ANDROID_SERIAL -LeaveRunning -TimeoutSeconds 300 -Params $params
    $result = $LASTEXITCODE
    $raw = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/introspect.json')
    $raw | Set-Content -LiteralPath (Join-Path $output "$phase.json") -Encoding utf8
    $trace = Adb -AdbArgs @('logcat', '-d') -Seconds 30
    $trace | Set-Content -LiteralPath (Join-Path $output "$phase.log") -Encoding utf8
    if ($result -ne 0) { throw "UUD2SP launcher scenario failed: $phase" }
    $state = $raw | ConvertFrom-Json
    $sounds = @($state.mod_usage.sounds)
    if ($phase -eq 'disabled') {
        if ($sounds.Count -ne 0) { throw 'Disabled sound pack remains resident' }
    } else {
        if ($sounds.Count -ne 208 -or @($sounds | Where-Object { !$_.resident }).Count) { throw "Incomplete UUD2SP PCM residency: $phase" }
        $expectedOwner = if ($phase -eq 'hires-first') { $hires } else { $sound }
        $played = @($sounds | Where-Object { $_.played -and (Split-Path $_.source -Leaf) -eq $expectedOwner })
        if (!$played.Count -or $trace -notmatch 'bank_match=1 cache_input_match=1 cache_output_match=1') { throw "No verified mixer consumption: $phase" }
        if ($phase -ne 'hires-first' -and @($sounds | Where-Object { (Split-Path $_.source -Leaf) -ne $sound }).Count) { throw "Lower-priority PCM replaced bank: $phase" }
        if ($combined) {
            $metadata = @($state.mod_usage.presentation | Where-Object { $_.kind -eq 'metadata' }) | Select-Object -Last 1
            if (!$metadata) { throw "No applied metadata source: $phase" }
            $patch = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', "$($metadata.source)/patches/d2/ham_patch.rfc6902.json")
            $patch | Set-Content -LiteralPath (Join-Path $output "$phase-patch.json") -Encoding utf8
            $operations = $patch | ConvertFrom-Json
            foreach ($index in @(5, 9)) {
                $row = @($operations | Where-Object { $_.op -eq 'replace' -and $_.path -eq "/sections/wclips/$index" })
                if ($row.Count -ne 1 -or $row[0].value.OpenSound -ne 88 -or $row[0].value.CloseSound -ne 89) { throw "Composition lost restored door sounds: $phase clip=$index" }
            }
            if ($trace -notmatch 'DXA metadata: applied' -or $trace -match 'DXA metadata: failed') { throw "Combined HAM not applied: $phase" }
        }
    }
    Write-Host "UUD2SP $phase passed after mission re-entry"
}
Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
Write-Host "UUD2SP launcher acceptance passed: $output"
