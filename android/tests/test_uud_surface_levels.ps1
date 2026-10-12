#!/usr/bin/env pwsh
# Verify every corrected D1 level surface after actual launcher import in both engines
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [ValidateSet('d1', 'd1-in-d2')][string[]]$Modes = @('d1', 'd1-in-d2'),
    [ValidateSet(3, 17, 18, 20)][int[]]$Levels = @(3, 17, 18, 20)
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
. (Join-Path $PSScriptRoot '../helpers/uud_gameplay_fixture.ps1')
Initialize-AndroidTestTarget -Serial $Serial | Out-Null
Assert-IsolatedPhysicalTestApp
Ensure-EmulatorHealthy | Out-Null
$repo = Split-Path (Split-Path $PSScriptRoot)
$pack = Join-Path $repo 'game_data/mods/xfing/dxx_tp/tmp/plain_texture_dxa/uud1tp-textures.dxa'
$output = Join-Path $repo ('android/temp/uud-surfaces/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repo 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $output -DirectoryPrefix run_
New-Item -ItemType Directory -Force $output | Out-Null
$archive = [IO.Compression.ZipFile]::OpenRead($pack)
try {
    $reader = [IO.StreamReader]::new($archive.GetEntry('patches/d1/level_surface_patch.rfc6902.json').Open())
    try { $operations = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
} finally { $archive.Dispose() }
Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mkdir', '-p', 'files/uud-gameplay') | Out-Null
Adb -AdbArgs @('push', $pack, '/data/local/tmp/uud-surfaces.dxa') | Out-Null
Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/uud-surfaces.dxa', 'files/uud-gameplay/uud1tp-textures.dxa') | Out-Null
Adb -AdbArgs @('shell', 'rm', '-f', '/data/local/tmp/uud-surfaces.dxa') | Out-Null
foreach ($mode in $Modes) {
    foreach ($level in $Levels) {
        $label = "$mode-level-$level"
        $levelName = 'level{0:d2}.rdl' -f $level
        $changes = @($operations | Where-Object { $_.op -eq 'replace' -and $_.path.StartsWith("/levels/$levelName/surfaces/") })
        if (!$changes.Count -and $level -ne 18) { throw "No source-verified surface operations for $levelName" }
        Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        Adb -AdbArgs @('logcat', '-c') | Out-Null
        $game = if ($mode -eq 'd1') { 'd1' } else { 'd2' }
        $params = @{ MODE = $mode; LEVEL = [string]$level; TENS = [string][Math]::Floor($level / 10); ONES = [string]($level % 10); SET = 'uud-surfaces'; MISSION = 'First Strike' }
        & (Join-Path $PSScriptRoot '../helpers/run_test.ps1') -ScriptName test_uud_surface_levels.jsonc -Game $game -Serial $env:ANDROID_SERIAL -LeaveRunning -TimeoutSeconds 300 -Params $params
        if ($LASTEXITCODE -ne 0) { throw "Patched level launch failed: $label; $output" }
        $steps = @()
        foreach ($group in ($changes | Group-Object { $_.value.segment })) {
            $first = $group.Group[0].value
            $steps += @{ action = 'face_view'; segment = $first.segment; side = $first.side; distance = 4; post_delay_ms = 150 }
            $expected = @{ 'position.segment' = $first.segment }
            foreach ($change in $group.Group) {
                $row = $change.value
                $expected["level_scene.segment.sides[$($row.side)].base"] = $row.tmap1
                $expected["level_scene.segment.sides[$($row.side)].overlay"] = $row.rawTmap2
            }
            $steps += @{ action = 'assert'; expect = $expected }
        }
        # Original HOG wall records place clip 9 in level 18, segment 101, side 4
        if ($level -eq 18) {
            $steps += @{ action = 'face_view'; segment = 101; side = 4; distance = 7; post_delay_ms = 200 }
            $steps += @{ action = 'assert'; expect = @{
                    'level_scene.segment.sides[4].wall_clip' = 9
                    'level_scene.segment.sides[4].wall_play_time' = @{ range = @(1, 1) }
                }
            }
            $steps += @{ action = 'send_button'; button = 100; held = 1 }
            $steps += @{ action = 'wait_for'; timeout_ms = 5000; expect = @{ 'level_scene.segment.sides[4].wall_state' = @{ gt = 0 } } }
            $steps += @{ action = 'send_button'; button = 100; pressed = 0 }
        }
        $steps += @{ action = 'introspect'; post_delay_ms = 500 }
        Invoke-UudGameplayProbe -Steps $steps -OutputDirectory $output -Name $label | Out-Null
        $trace = Get-Content -LiteralPath (Join-Path $output "$label.log") -Raw
        if ($changes.Count -and $trace -notmatch "DXA D1 level ${levelName}: applied $($changes.Count) surface changes") { throw "Missing actual level application evidence: $label" }
        Write-Host "Verified $($changes.Count) actual patched surfaces: $label"
        if ($level -eq 18) { Write-Host "Verified corrected one-second door timing and opening: $label" }
    }
}
Write-Host "UUD D1 surface integration passed: $output"
