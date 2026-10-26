#!/usr/bin/env pwsh
# Requires an XL pilot APK, retail D1/D2, player "player", and imported original Dragon Pit package
# Verifies opt-in state, save/restore and stock isolation; not normal mission completion
param([Parameter(Mandatory)][string]$Serial)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../temp/xl-classic-lighting-lifecycle'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null

function Invoke-ClassicAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Classic lighting adb failed: $result" }
    return $result
}
function Invoke-ClassicPrivate {
    param([string]$Command)
    Invoke-ClassicAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}

# Exclusive backup: interrupted runs remain visible and are never overwritten
$backup = 'files/xl-classic-lighting-lifecycle-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d1x-redux/descent.cfg', 'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs')
$ready = $false
try {
    Invoke-ClassicAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-ClassicPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-ClassicPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail data, player player and the original Dragon Pit package first' }
    Invoke-ClassicPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-ClassicPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-ClassicAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName test_xl_classic_lighting_lifecycle.jsonc -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 240
    $testExit = $LASTEXITCODE
    Invoke-ClassicAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-ClassicPrivate 'if test -f files/automation_result.json; then cat files/automation_result.json; fi' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-ClassicPrivate 'if test -f files/automation_log.jsonl; then cat files/automation_log.jsonl; fi' | Set-Content "$output/steps.jsonl" -Encoding utf8
    if ($testExit -ne 0) { throw "Classic lighting lifecycle failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne 43 -or $result.total_steps -ne 43) { throw 'Incomplete classic lighting lifecycle result' }
    Invoke-ClassicPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    $state = Get-Content "$output/state.json" -Raw | ConvertFrom-Json
    # The final introspection request may sample another frame; validate the exported frame's receipt
    $probe = $state.pixel_capture
    $view = $probe.main_view
    if ($view.valid -ne $true -or $view.request_generation -ne $probe.request_generation -or
        $view.viewport.Count -ne 4 -or $view.viewport[2] -le 0 -or $view.viewport[3] -le 0 -or
        $view.matrix_scale_fixed.Count -ne 3 -or $view.matrix_scale_fixed[2] -le 0) {
        throw 'Missing or mismatched main-view evidence for the sampled framebuffer'
    }
    $encoded = Invoke-ClassicAdb -Arguments @('exec-out', 'run-as', $script:PACKAGE, 'base64', '-w', '0', 'files/introspect.json.frame')
    $frameBytes = [Convert]::FromBase64String(($encoded -join '').Trim())
    [IO.File]::WriteAllBytes("$output/render.frame", $frameBytes)
    $headerEnd = [Array]::IndexOf($frameBytes, [byte]10)
    if ($headerEnd -lt 1 -or $headerEnd -gt 65536) { throw 'Invalid frame export header' }
    $header = [Text.Encoding]::UTF8.GetString($frameBytes, 0, $headerEnd) | ConvertFrom-Json
    $pixelOffset = $headerEnd + 1
    $pixelCount = $frameBytes.Length - $pixelOffset
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $pixelHash = [Convert]::ToHexString($sha.ComputeHash($frameBytes, $pixelOffset, $pixelCount)).ToLowerInvariant() } finally { $sha.Dispose() }
    if ($header.version -ne 1 -or $header.format -ne 'RGBA8' -or $header.row_order -ne 'bottom_up' -or
        $pixelCount -ne $header.pixel_bytes -or $pixelCount -ne (4L * $header.width * $header.height) -or
        $pixelHash -ne $header.pixel_sha256 -or $pixelHash -ne $state.pixel_capture.pixel_sha256 -or
        $state.pixel_capture.status -ne 'saved' -or $header.main_view.valid -ne $true -or
        $header.request_generation -ne $header.main_view.request_generation -or
        $header.request_generation -ne $state.pixel_capture.request_generation) {
        throw 'Frame export payload or camera receipt does not match the completed capture'
    }
    Invoke-ClassicAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-classic-lighting-lifecycle.png') | Out-Null
    Invoke-ClassicAdb -Arguments @('pull', '/data/local/tmp/xl-classic-lighting-lifecycle.png', "$output/render.png") | Out-Null
    if (Select-String -Path "$output/logcat.txt" -Pattern "ANR in $([regex]::Escape($script:PACKAGE))|FATAL EXCEPTION" -Quiet) {
        throw 'Android reported an ANR or fatal exception despite automation completion'
    }
} finally {
    try {
        Invoke-ClassicAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Exact app-relative state paths only; imported mission assets are preserved
            foreach ($path in $paths) { Invoke-ClassicPrivate "rm -rf $path" | Out-Null }
            Invoke-ClassicPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-ClassicPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Restored state differs; retain $backup for investigation" }
            'Restored state verified byte-for-byte' | Set-Content "$output/restoration.txt"
            Invoke-ClassicPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
Write-Host "PASS: classic lighting lifecycle; rendered reference parity and mission completion remain separate gates"
