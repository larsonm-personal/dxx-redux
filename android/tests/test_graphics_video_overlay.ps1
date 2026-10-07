#!/usr/bin/env pwsh
# Run real Video Info controller edits using the current driver's supported controls
[CmdletBinding()]
param(
    [ValidateSet('d1', 'd2')][string]$Game,
    [string]$Serial
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $script:ANDROID_ROOT ('temp/graphics-video-overlay-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts @($output)
New-Item -ItemType Directory -Path $output -Force | Out-Null
$template = "$PSScriptRoot/../game_scripts/test_graphics_video_overlay.jsonc"
$games = if ($Game) { @($Game) } else { @('d1', 'd2') }

function Invoke-OverlayCase([string]$Name, [string]$Engine, [object[]]$Steps) {
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
        $sections = @{}
        $section = 'setup'
        # Preserve dependency and game-variable declarations for each generated launcher script
        $steps = @(ConvertFrom-JsoncText ([IO.File]::ReadAllText($template)) | ConvertFrom-Json -AsHashtable)
        foreach ($step in $steps) {
            if ($step.ContainsKey('_section')) {
                $section = $step['_section']
                $step.Remove('_section')
            }
            if (-not $sections.ContainsKey($section)) { $sections[$section] = [Collections.Generic.List[object]]::new() }
            $sections[$section].Add($step)
        }
        foreach ($name in @('setup', 'coalesced', 'msaa', 'return')) {
            if (-not $sections.ContainsKey($name)) { throw "Missing overlay fixture section: $name" }
        }
        Invoke-OverlayCase 'probe' $engine $sections.setup.ToArray()
        $state = Get-GameIntrospection -Serial $Serial
        if (-not $state.gpu_capabilities) { throw 'GPU capability introspection is unavailable' }
        $caps = $state.gpu_capabilities
        $caps | ConvertTo-Json -Depth 15 | Set-Content (Join-Path $output "$engine-capabilities.json") -Encoding utf8NoBOM
        $hasAniso = $caps.aniso_max -gt 1
        $hasMsaa = $caps.msaa_2 -ge 2
        Write-Output "$engine Video Info capabilities: AF=$hasAniso MSAA=$hasMsaa"

        $cases = [Collections.Generic.List[object]]::new()
        $sections.setup | ForEach-Object { $cases.Add($_) }
        $cases.Add(@{ action = 'controller_input'; expect_ui = @{ video_aniso_available = $hasAniso; video_msaa_available = $hasMsaa } })
        if ($hasAniso -or $hasMsaa) {
            foreach ($step in $sections.coalesced) {
                if (-not $hasAniso) {
                    # MSAA is the next selectable control when the driver has no AF
                    if ($step.expect_ui) {
                        if ($step.expect_ui.video_selected -eq 'ANISO') { $step.expect_ui.video_selected = 'MSAA' }
                        if ($step.expect_ui.video_aniso -eq 2) {
                            $step.expect_ui.video_aniso = 0
                            $step.expect_ui.video_msaa = 2
                        }
                    }
                    if ($step.expect) {
                        foreach ($kind in @('queued', 'current')) {
                            $afKey = "graphics_safety.$kind.AnisoLevel"
                            if ($step.expect[$afKey] -eq '2') {
                                $step.expect.Remove($afKey)
                                $step.expect["graphics_safety.$kind.MsaaLevel"] = '2'
                                if ($kind -eq 'current') { $step.expect[$afKey] = '0' }
                            }
                        }
                    }
                }
                $cases.Add($step)
            }
        } else {
            Write-Output "${engine}: combined edits unavailable because texture filtering is the only supported control"
        }
        foreach ($step in $sections.msaa) {
            if ($hasMsaa) {
                # The combined-edit case already ends on MSAA when AF is absent
                if (-not $hasAniso -and $step -eq $sections.msaa[0]) { continue }
            } else {
                # Keep Back and timeout coverage using texture filtering on drivers without MSAA
                if ($step -eq $sections.msaa[0]) {
                    if (-not $hasAniso) { continue }
                    $step.key = 'DUP'
                }
                if ($step.expect_ui) {
                    if ($step.expect_ui.video_selected -eq 'MSAA') { $step.expect_ui.video_selected = 'TEX_FILT' }
                    if ($step.expect_ui.video_msaa -eq 2) {
                        $step.expect_ui.video_msaa = 0
                        $step.expect_ui.video_tex_filt = 2
                    }
                }
                if ($step.expect -and $step.expect['graphics_safety.current.MsaaLevel'] -eq '2') {
                    $step.expect['graphics_safety.current.MsaaLevel'] = '0'
                    $step.expect['graphics_safety.current.TexFilt'] = '2'
                }
            }
            $cases.Add($step)
        }
        $upCount = if ($hasMsaa) { [int]$hasAniso + 1 } else { 0 }
        $sections.return[0].controller_keys = @(@('DUP') * $upCount) + @('A', 'A', 'A')
        $sections.return | ForEach-Object { $cases.Add($_) }
        Invoke-OverlayCase 'edits' $engine $cases.ToArray()
        Write-Output "PASS $engine Video Info edits ($output)"
    }
} finally {
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
}
