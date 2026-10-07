#!/usr/bin/env pwsh
# Exercise runtime texture dispatch with graphics confirmation and actual GPU capabilities
[CmdletBinding()]
param(
    [ValidateSet('d1', 'd2')][string]$Game,
    [string]$Serial
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $script:ANDROID_ROOT ('temp/runtime-texture-options-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts @($output)
New-Item -ItemType Directory -Path $output -Force | Out-Null
$template = "$PSScriptRoot/../game_scripts/test_ogl_runtime_texture_options_unified.jsonc"
$games = if ($Game) { @($Game) } else { @('d1', 'd2') }

function Invoke-TextureCase([string]$Name, [string]$Engine, [object[]]$Steps) {
    $path = Join-Path $output "$Engine-$Name.jsonc"
    $log = Join-Path $output "$Engine-$Name.log"
    ConvertTo-Json -InputObject $Steps -Depth 35 | Set-Content -LiteralPath $path -Encoding utf8NoBOM
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    & pwsh -NoProfile -NonInteractive -File "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName $path -Game $Engine -Serial $Serial -LeaveRunning -TimeoutSeconds 300 *> $log
    $result = $LASTEXITCODE
    Get-Content -LiteralPath $log | Select-String '^\[.*\] (PASS|FAIL)|SCRIPT_RESULT:' | Select-Object -Last 6 | ForEach-Object { Write-Output $_.Line }
    if ($result -ne 0) { throw "$Engine $Name failed: $log" }
}

try {
    foreach ($engine in $games) {
        $steps = @(ConvertFrom-JsoncText ([IO.File]::ReadAllText($template)) | ConvertFrom-Json -AsHashtable)
        $probe = [Collections.Generic.List[object]]::new()
        foreach ($step in $steps) {
            $probe.Add($step)
            if ($step._probe_end) { break }
        }
        if (-not $probe[$probe.Count - 1]._probe_end) { throw 'Missing texture probe boundary' }
        Invoke-TextureCase 'probe' $engine $probe.ToArray()
        $state = Get-GameIntrospection -Serial $Serial
        if (-not $state.gpu_capabilities) { throw 'GPU capability introspection is unavailable' }
        $state.gpu_capabilities | ConvertTo-Json -Depth 15 | Set-Content (Join-Path $output "$engine-capabilities.json") -Encoding utf8NoBOM
        $hasAniso = $state.gpu_capabilities.aniso_max -ge 2
        Write-Output "$engine runtime texture capabilities: AF=$hasAniso"
        $current = @{ TexFilt = 1; AnisoLevel = 0; MsaaLevel = $(if ($engine -eq 'd2') { 2 } else { 0 }); MenuTexFilt = 1; HudTexFilt = 1 }
        $accepted = $current.Clone()
        $cases = [Collections.Generic.List[object]]::new()
        foreach ($step in $steps) {
            if ($step._requires_aniso -and -not $hasAniso) {
                $cases.Add(@{ action = 'log'; message = 'Nonzero AF transition unavailable on this driver' })
                continue
            }
            $confirmation = $step._confirm
            foreach ($key in @('_probe_end', '_confirm', '_requires_aniso')) { $step.Remove($key) }
            $cases.Add($step)
            if (-not $confirmation) { continue }
            $field, $value = $confirmation -split ':'
            $current[$field] = [int]$value
            $changed = @($current.Keys | Where-Object { $current[$_] -ne $accepted[$_] }).Count -gt 0
            # All-off and edits returning to accepted settings have no confirmation dialog
            $allOff = $current.TexFilt -eq 0 -and $current.AnisoLevel -eq 0 -and $current.MsaaLevel -eq 0
            if ($changed -and -not $allOff) {
                $cases.Add(@{
                        action = 'controller_input'; timeout_ms = 10000
                        expect = @{ 'graphics_safety.phase' = 'challenge'; 'graphics_safety.candidate_ready' = 'true' }
                        controller_keys = @('DLEFT', 'A'); post_delay_ms = 50
                    })
                $accepted = $current.Clone()
            }
            $cases.Add(@{
                    action = 'wait_for'; timeout_ms = 5000
                    expect = @{ 'graphics_safety.phase' = 'idle'; "graphics_safety.current.$field" = $value; "graphics_safety.accepted.$field" = [string]$accepted[$field] }
                })
        }
        Invoke-TextureCase 'edits' $engine $cases.ToArray()
        Write-Output "PASS $engine runtime texture options ($output)"
    }
} finally {
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
}
