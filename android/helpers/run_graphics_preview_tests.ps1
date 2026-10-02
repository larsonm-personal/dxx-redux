#!/usr/bin/env pwsh
# Live isolated previews must never challenge or validate the shared graphics tuple
param(
    [ValidateSet('d1', 'd2')][string]$Game,
    [ValidateSet('level', 'robot')][string]$Preview,
    [string]$Serial = 'emulator-5554'
)

$ErrorActionPreference = 'Stop'
if ($Serial -notmatch '^emulator-[0-9]+$') { throw 'This reset-state fixture requires a provisioned emulator' }
. (Join-Path $PSScriptRoot 'test_helpers.ps1')
$outputDirectory = Join-Path $script:REPO_ROOT ('android/temp/graphics-preview-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
& (Join-Path $PSScriptRoot 'retain-recent-artifacts.ps1') -Artifacts @($outputDirectory)
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$previousSerial = $env:ANDROID_SERIAL
$env:ANDROID_SERIAL = $Serial
$games = if ($Game) { @($Game) } else { @('d1', 'd2') }
$previews = if ($Preview) { @($Preview) } else { @('robot', 'level') }
$results = @()

function Invoke-PreviewDevice {
    param([string[]]$Arguments)
    $output = & $script:ADB -s $Serial @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "adb failed: $($Arguments -join ' '): $output" }
    return ($output -join "`n")
}

function Read-PreviewJson {
    param([string]$Path)
    return (Invoke-PreviewDevice @('shell', 'run-as', $script:PACKAGE, 'cat', $Path) | ConvertFrom-Json)
}

function Assert-PreviewRecord {
    $record = Read-PreviewJson 'files/graphics_safety.json'
    $expected = @{ TexFilt = 1; AnisoLevel = 0; MsaaLevel = 0; MenuTexFilt = 0; HudTexFilt = 0; ResolutionX = 640; ResolutionY = 480; AspectX = 3; AspectY = 4; ColorDepth = 0 }
    foreach ($key in $expected.Keys) {
        if ($null -eq $record.accepted.$key -or $record.accepted.$key -ne $expected[$key]) { throw "Preview changed accepted $key" }
    }
    if ($null -ne $record.attempt -or @($record.pending.PSObject.Properties).Count -ne 0) { throw 'Preview armed or staged a graphics trial' }
    return $record
}

try {
    if (-not (Ensure-StandardGameDataOnDevice -Serial $Serial)) { throw 'Base game data provisioning failed' }
    foreach ($gameId in $games) {
        foreach ($kind in $previews) {
            $caseName = "$gameId-$kind"
            Write-Output "Graphics preview case: $caseName"
            Reset-DeviceGameState -Serial $Serial
            if (-not (Start-SetupActivity -Serial $Serial)) { throw 'Launcher did not become ready' }
            Invoke-PreviewDevice @('logcat', '-c') | Out-Null
            $template = Get-Content (Join-Path $script:ANDROID_ROOT 'game_scripts/test_graphics_preview_eligibility.jsonc') -Raw
            $action = if ($kind -eq 'robot') { 'launch_base_robot_preview' } else { 'launch_random_level_preview' }
            $hog = if ($gameId -eq 'd1') { 'descent.hog' } else { 'descent2.hog' }
            $steps = $template.Replace('${PREVIEW_ACTION}', $action).Replace('${GAME}', $gameId).Replace('${BASE_HOG}', $hog) | ConvertFrom-Json -NoEnumerate
            $scriptPath = Join-Path $outputDirectory "$caseName.json"
            ConvertTo-Json -InputObject $steps -Depth 30 | Set-Content $scriptPath -Encoding utf8NoBOM
            Invoke-PreviewDevice @('push', $scriptPath, '/data/local/tmp/graphics_preview.json') | Out-Null
            Invoke-PreviewDevice @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/graphics_preview.json', 'files/graphics_preview.json') | Out-Null
            $statePath = "files/${kind}_preview_introspect.json"
            Invoke-PreviewDevice @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', $statePath, 'files/automation_result.json') | Out-Null
            Invoke-PreviewDevice @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_AUTOMATE', '--es', 'script', 'graphics_preview.json') | Out-Null
            $timer = [Diagnostics.Stopwatch]::StartNew()
            $first = $null
            while ($timer.Elapsed.TotalSeconds -lt 120) {
                $result = $null
                try { $result = Read-PreviewJson 'files/automation_result.json' } catch { }
                if ($result.result -eq 'FAIL') { throw "Preview launch failed: $($result.reason)" }
                Invoke-PreviewDevice @('shell', 'am', 'broadcast', '-a', "com.dxxredux.$($kind.ToUpperInvariant())_PREVIEW_INTROSPECT", '-p', $script:PACKAGE) | Out-Null
                try { $first = Read-PreviewJson $statePath } catch { }
                if ($first.level_preview.active -and $first.msaa.flip_serial -gt 0) { break }
                Start-Sleep -Milliseconds 250
            }
            if (-not $first.level_preview.active -or $first.msaa.flip_serial -le 0) { throw 'No live preview frames' }
            if ($first.graphics_safety.phase -ne 'disabled') { throw 'Preview initialized graphics confirmation' }
            $first | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $outputDirectory "$caseName-before.json") -Encoding utf8NoBOM
            $record = Assert-PreviewRecord
            $configs = @{}
            foreach ($path in @('files/descent.cfg', 'files/d1x-redux/descent.cfg', 'files/d2x-redux/descent.cfg')) {
                $configs[$path] = Invoke-PreviewDevice @('shell', 'run-as', $script:PACKAGE, 'cat', $path)
                foreach ($expected in @('TexFilt=2', 'MsaaLevel=2', 'ColorDepth=1')) {
                    if ($configs[$path] -notmatch "(?m)^$expected\r?$") { throw "Staged configuration lost: $path $expected" }
                }
            }
            $watch = [Diagnostics.Stopwatch]::StartNew()
            while ($watch.Elapsed.TotalSeconds -lt 6) {
                Assert-PreviewRecord | Out-Null
                Start-Sleep -Milliseconds 250
            }
            Invoke-PreviewDevice @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', $statePath) | Out-Null
            Invoke-PreviewDevice @('shell', 'am', 'broadcast', '-a', "com.dxxredux.$($kind.ToUpperInvariant())_PREVIEW_INTROSPECT", '-p', $script:PACKAGE) | Out-Null
            $fresh = [Diagnostics.Stopwatch]::StartNew()
            $after = $null
            while ($fresh.Elapsed.TotalSeconds -lt 10) {
                try { $after = Read-PreviewJson $statePath; break } catch { Start-Sleep -Milliseconds 100 }
            }
            if ($after.graphics_safety.phase -ne 'disabled' -or $after.msaa.flip_serial -le $first.msaa.flip_serial) { throw 'Preview did not remain active with confirmation disabled' }
            if ($after.framebuffer_probe.gl_error -ne 0 -or $after.framebuffer_probe.visible_pixels -le 0) { throw 'Preview did not produce visible pixels' }
            $after | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $outputDirectory "$caseName-after.json") -Encoding utf8NoBOM
            Invoke-PreviewDevice @('shell', 'input', 'keyevent', 'KEYCODE_BACK') | Out-Null
            if (-not (Wait-SetupActivityReady)) { throw 'Preview did not return to launcher' }
            Assert-PreviewRecord | ConvertTo-Json -Depth 20 | Set-Content (Join-Path $outputDirectory "$caseName-record.json") -Encoding utf8NoBOM
            foreach ($path in $configs.Keys) {
                if ((Invoke-PreviewDevice @('shell', 'run-as', $script:PACKAGE, 'cat', $path)) -cne $configs[$path]) { throw "Preview rewrote staged config: $path" }
            }
            Invoke-PreviewDevice @('logcat', '-d') | Set-Content (Join-Path $outputDirectory "$caseName-logcat.txt") -Encoding utf8NoBOM
            $results += [ordered]@{ game = $gameId; preview = $kind; flips_before = $first.msaa.flip_serial; flips_after = $after.msaa.flip_serial; observed_ms = $watch.ElapsedMilliseconds; passed = $true }
            ConvertTo-Json -InputObject $results -Depth 20 | Set-Content (Join-Path $outputDirectory 'results.json') -Encoding utf8NoBOM
            Write-Output "PASS: $caseName, active pixels and unchanged accepted/staged settings"
        }
    }
} finally {
    Invoke-PreviewDevice @('logcat', '-d') | Set-Content (Join-Path $outputDirectory 'last-logcat.txt') -Encoding utf8NoBOM
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial }
    else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
Write-Output "Graphics preview tests passed: $outputDirectory"
