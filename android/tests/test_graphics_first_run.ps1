#!/usr/bin/env pwsh
# First-run live graphics editor, confirmation, once-per-install and recovery integration
[CmdletBinding()]
param(
    [ValidateSet('d1', 'd2')][string]$Game,
    [ValidateSet('Accept', 'Cancel', 'Back', 'Timeout', 'Previous', 'Unchanged', 'Background', 'Stall', 'Unsupported', 'Crash', 'Rearm')][string[]]$Scenario,
    [switch]$D1InD2,
    [string]$Serial
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_env.ps1"
. "$PSScriptRoot/../helpers/test_helpers.ps1"
. "$PSScriptRoot/../helpers/jsonc.ps1"
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
if ($D1InD2) { $Game = 'd2' }
$output = Join-Path $script:ANDROID_ROOT ('temp/graphics-first-run-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts @($output)
New-Item -ItemType Directory -Path $output -Force | Out-Null
$games = if ($Game) { @($Game) } else { @('d1', 'd2') }
$scenarios = if ($Scenario) { $Scenario } else { @('Accept', 'Cancel', 'Back', 'Timeout', 'Previous', 'Unchanged', 'Background', 'Stall', 'Unsupported', 'Crash', 'Rearm') }

function New-ExpectStep([hashtable]$Expect, [switch]$Wait) {
    if ($Wait) { return @{ action = 'wait_for'; timeout_ms = 15000; expect = $Expect } }
    return @{ action = 'assert'; expect = $Expect }
}

function New-TouchStep([string]$Target) {
    return @{ action = 'controller_input'; graphics_touch = @(@{ target = $Target; action = 'down' }, @{ target = $Target; action = 'up' }); post_delay_ms = 60 }
}

function Get-LaunchSteps([string]$Engine, [switch]$Handled, [switch]$Unsupported, [switch]$Stall) {
    $fixturePath = "$PSScriptRoot/../game_scripts/test_graphics_first_run_fixture.jsonc"
    $mission = if ($Engine -eq 'd1' -or ($D1InD2 -and -not $Handled)) { 'First Strike' } else { 'Counterstrike' }
    $fixtureText = [IO.File]::ReadAllText($fixturePath).Replace('${MISSION}', $mission).Replace('${GAME_NUM}', $(if ($Engine -eq 'd1') { '1' } else { '2' }))
    $fixture = @(ConvertFrom-JsoncText $fixtureText | ConvertFrom-Json)
    foreach ($entry in $fixture) {
        if ($D1InD2 -and -not $Handled -and $entry.action -eq 'enter_game' -and $entry.game -eq 'd2') { $entry.game = 'd1-in-d2' }
        if ($Handled -and ($entry.action -in @('reset_state', 'write_config') -or $entry.command -eq 'graphics_first_run_offered')) { continue }
        if ($entry.expect.'graphics_safety.first_run_pending') {
            $entry.expect.'graphics_safety.first_run_pending' = if ($Handled) { 'false' } else { 'true' }
        }
        if ($Unsupported -and $entry.action -eq 'select' -and $entry.text -eq 'New game') {
            @{ action = 'set_debug'; field = 'gpu_capabilities_disable'; value = '1' }
        }
        if ($Stall -and $entry.action -eq 'select' -and $entry.text -eq 'New game') {
            @{ action = 'set_debug'; field = 'graphics_stall_once'; value = '6500' }
        }
        $entry
    }
}

function Invoke-Case([string]$Name, [string]$Engine, [object[]]$Steps) {
    $path = Join-Path $output "$Engine-$Name.jsonc"
    $log = Join-Path $output "$Engine-$Name.log"
    ConvertTo-Json -InputObject $Steps -Depth 35 | Set-Content -LiteralPath $path -Encoding utf8
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    $extra = @()
    if ($Name.EndsWith('-handled')) { $extra += '-PreserveState' }
    & pwsh -NoProfile -File "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName $path -Game $Engine -Serial $Serial -LeaveRunning -TimeoutSeconds 180 @extra *> $log
    $result = $LASTEXITCODE
    Get-Content -LiteralPath $log | Select-String '^\[.*\] (PASS|FAIL)|SCRIPT_RESULT:' | Select-Object -Last 6 | ForEach-Object { Write-Output $_.Line }
    if ($result -ne 0) { throw "$Engine $Name failed: $log" }
    Write-Output "PASS $Engine $Name ($log)"
}

foreach ($engine in $games) {
    foreach ($case in $scenarios) {
        $steps = [Collections.Generic.List[object]]::new()
        @(Get-LaunchSteps $engine -Unsupported:($case -eq 'Unsupported') -Stall:($case -eq 'Stall')) | ForEach-Object { $steps.Add($_) }
        if ($case -ne 'Stall') {
            $steps.Add((New-ExpectStep -Wait @{ 'graphics_safety.phase' = 'editing'; 'graphics_safety.candidate_ready' = 'true'; 'graphics_safety.current.TexFilt' = '2'; 'graphics_safety.accepted.TexFilt' = '0'; 'graphics_safety.requested.TexFilt' = '0'; 'graphics_safety.first_run_pending' = 'false'; 'time_paused' = 'true' }))
            $steps.Add(@{ action = 'controller_input'; expect_ui = @{ graphics_chooser_open = $true; graphics_preview_tex_filt = 2 } })
            $steps.Add(@{ action = 'wait_ms'; ms = 3000 })
            $steps.Add((New-ExpectStep @{ 'graphics_safety.phase' = 'editing'; 'time_paused' = 'true'; 'graphics_safety.preview_presented_frames' = @{ gte = 5 } }))
        }

        if ($case -eq 'Unsupported') {
            $steps.Add((New-ExpectStep @{ 'graphics_safety.current.AnisoLevel' = '0'; 'graphics_safety.current.MsaaLevel' = '0'; 'graphics_safety.capabilities.aniso_max' = @{ range = @(1, 1) }; 'graphics_safety.capabilities.msaa_4' = '0' }))
        }

        if ($case -eq 'Crash') {
            $steps.Add(@{ action = 'log'; message = 'First-run preview ready for host process-death recovery check' })
            Invoke-Case $case $engine $steps.ToArray()
            $record = (Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/graphics_safety.json')) | ConvertFrom-Json
            if ($record.attempt.phase -ne 5 -or $record.accepted.TexFilt -ne 0 -or $record.attempt.candidate.TexFilt -ne 2) { throw 'Preview was not durably journaled before process death' }
            $state = (Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/introspect.json')) | ConvertFrom-Json
            $caps = $state.graphics_safety.capabilities
            $maxAf = @(0, 2, 4, 8, 16 | Where-Object { $_ -le $caps.aniso_max })[-1]
            $maxMsaa = if ($caps.msaa_4 -ge 4) { 4 } elseif ($caps.msaa_2 -ge 2) { 2 } else { 0 }
            if ($state.graphics_safety.current.AnisoLevel -ne $maxAf -or $state.graphics_safety.current.MsaaLevel -ne $maxMsaa) { throw 'Opening preview did not select the maximum supported product choices' }
            Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        } else {
            if ($case -eq 'Background') {
                $steps.Add(@{ action = 'log'; message = 'SCRIPT_BACKGROUND: first-run-editor duration_s=4' })
                $steps.Add((New-ExpectStep -Wait @{ 'graphics_safety.phase' = 'idle'; 'graphics_safety.current.TexFilt' = '0'; 'android_lifecycle.observed_visibility' = 'foreground'; 'egl_recreate_count' = @{ gte = 1 }; 'android_lifecycle.park_entry_count' = @{ gte = 1 } }))
                $steps.Add(@{ action = 'key'; key = 'escape'; post_delay_ms = 100 })
            } elseif ($case -eq 'Previous') {
                $steps.Add((New-TouchStep 'cancel'))
            } elseif ($case -ne 'Stall') {
                # Real touch controls cycle maximum AF/MSAA to Off, including disabled rows
                $steps.Add((New-TouchStep 'aniso_level'))
                $steps.Add((New-TouchStep 'msaa_level'))
                $steps.Add(@{ action = 'controller_input'; key = 'A'; post_delay_ms = 60 })
                $steps.Add((New-ExpectStep -Wait @{ 'graphics_safety.current.TexFilt' = '0'; 'graphics_safety.current.AnisoLevel' = '0'; 'graphics_safety.current.MsaaLevel' = '0'; 'graphics_safety.phase' = 'editing' }))
                if ($case -ne 'Unchanged') {
                    $steps.Add(@{ action = 'controller_input'; key = 'A'; post_delay_ms = 60 })
                    $steps.Add((New-ExpectStep -Wait @{ 'graphics_safety.current.TexFilt' = '1'; 'graphics_safety.phase' = 'editing'; 'graphics_safety.candidate_ready' = 'true' }))
                }
                if ($case -eq 'Back') { $steps.Add(@{ action = 'controller_input'; key = 'B'; post_delay_ms = 60 }) }
                else { $steps.Add((New-TouchStep 'ok')) }
                $steps.Add(@{ action = 'wait_ms'; ms = 1500 })
                $steps.Add((New-ExpectStep @{ 'graphics_safety.phase' = 'settling'; 'time_paused' = 'true' }))
                if ($case -ne 'Unchanged') {
                    $steps.Add((New-ExpectStep -Wait @{ 'graphics_safety.phase' = 'challenge'; 'graphics_safety.candidate_ready' = 'true'; 'time_paused' = 'true' }))
                    if ($case -in @('Accept', 'Unsupported')) { $steps.Add(@{ action = 'controller_input'; controller_keys = @('DLEFT', 'A'); post_delay_ms = 60 }) }
                    elseif ($case -in @('Cancel', 'Back', 'Rearm')) { $steps.Add(@{ action = 'controller_input'; key = 'B'; post_delay_ms = 60 }) }
                }
            }
            $accepted = if ($case -in @('Accept', 'Unsupported')) { '1' } else { '0' }
            $steps.Add((New-ExpectStep -Wait @{ 'graphics_safety.phase' = 'idle'; 'graphics_safety.current.TexFilt' = $accepted; 'graphics_safety.accepted.TexFilt' = $accepted; 'graphics_safety.requested.TexFilt' = $accepted; 'time_paused' = 'false'; 'graphics_safety.first_run_pending' = 'false' }))
            $steps.Add((New-ExpectStep @{ 'fire_primary_state' = '0'; 'fire_primary_count' = '0' }))
            if ($case -eq 'Rearm') {
                $steps.Add(@{ action = 'log'; message = 'SCRIPT_BACKGROUND: rearm-chooser duration_s=4 rearm_graphics_chooser=true' })
                $steps.Add((New-ExpectStep -Wait @{ 'egl_recreate_count' = @{ gte = 1 }; 'graphics_safety.phase' = 'idle'; 'graphics_safety.first_run_pending' = 'true' }))
                $steps.Add(@{ action = 'key'; key = 'escape'; post_delay_ms = 100 })
                $steps.Add((New-ExpectStep -Wait @{ 'graphics_safety.phase' = 'editing'; 'graphics_safety.current.TexFilt' = '2'; 'graphics_safety.first_run_pending' = 'false'; 'time_paused' = 'true' }))
                $steps.Add(@{ action = 'wait_ms'; ms = 5500 })
                $steps.Add((New-ExpectStep @{ 'graphics_safety.phase' = 'editing'; 'graphics_safety.deadline_ms' = '0' }))
                $steps.Add((New-TouchStep 'cancel'))
                $steps.Add((New-ExpectStep -Wait @{ 'graphics_safety.phase' = 'idle'; 'time_paused' = 'false' }))
                $steps.Add(@{ action = 'wait_ms'; ms = 3000 })
                $steps.Add((New-ExpectStep @{ 'graphics_safety.phase' = 'idle'; 'graphics_safety.first_run_pending' = 'false' }))
            }
            Invoke-Case $case $engine $steps.ToArray()
        }

        $marker = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'no_backup/graphics-first-run-offered')
        if ("$marker".Trim() -ne 'offered') { throw 'First-run marker missing or wrong after the actual chooser draw' }
        # A different engine in a new process shares the real marker; the runner must not seed it
        $otherEngine = if ($engine -eq 'd1') { 'd2' } else { 'd1' }
        $handled = @(Get-LaunchSteps $otherEngine -Handled)
        $expected = if ($case -in @('Accept', 'Unsupported')) { '1' } else { '0' }
        $handled += New-ExpectStep -Wait @{ 'graphics_safety.phase' = 'idle'; 'graphics_safety.first_run_pending' = 'false'; 'graphics_safety.current.TexFilt' = $expected; 'graphics_safety.accepted.TexFilt' = $expected; 'time_paused' = 'false' }
        $handled += @{ action = 'wait_ms'; ms = 3500 }
        $handled += New-ExpectStep @{ 'graphics_safety.phase' = 'idle'; 'time_paused' = 'false' }
        Invoke-Case "$case-handled" $otherEngine $handled
    }
}
Write-Output "First-run graphics integration passed: $output"
