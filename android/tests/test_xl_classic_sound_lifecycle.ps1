#!/usr/bin/env pwsh
# Requires an XL pilot APK, retail D1/D2, player "player", and imported original Dragon Pit package
# Verifies natural sound completion and interrupted playback across restore; not normal mission completion
param([Parameter(Mandatory)][string]$Serial)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = (Join-Path $PSScriptRoot '../temp/xl-classic-sound-lifecycle')
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null

function Invoke-ClassicAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Classic sound adb failed: $result" }
    return $result
}
function Invoke-ClassicPrivate {
    param([string]$Command)
    Invoke-ClassicAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}

# Exclusive backup: interrupted runs remain visible and are never overwritten
$backup = 'files/xl-classic-sound-lifecycle-backup'
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
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName test_xl_classic_sound_lifecycle.jsonc -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 240
    $testExit = $LASTEXITCODE
    Invoke-ClassicAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-ClassicPrivate 'if test -f files/automation_result.json; then cat files/automation_result.json; fi' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-ClassicPrivate 'if test -f files/automation_log.jsonl; then cat files/automation_log.jsonl; fi' | Set-Content "$output/steps.jsonl" -Encoding utf8
    if ($testExit -ne 0) { throw "Classic sound lifecycle failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne 42 -or $result.total_steps -ne 42) { throw 'Incomplete classic sound lifecycle result' }
    foreach ($file in @('store-audio.wav', 'store-music.wav', 'store-audio.json')) {
        $encoded = Invoke-ClassicAdb -Arguments @('exec-out', 'run-as', $script:PACKAGE, 'base64', '-w', '0', "files/$file")
        [IO.File]::WriteAllBytes("$output/$file", [Convert]::FromBase64String(($encoded -join '').Trim()))
    }
    Invoke-ClassicPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    $state = Get-Content "$output/state.json" -Raw | ConvertFrom-Json
    $context = $state.mod_usage.scoped_sound_context
    if (-not $context.current -or -not $context.archive.EndsWith('/drgpit.hog') -or -not $context.mission_key) {
        throw 'Missing current mounted-HOG sound context after restore'
    }
    foreach ($sample in @(80, 82)) {
        $entries = @($state.mod_usage.sounds | Where-Object sample -eq $sample)
        if ($entries.Count -ne 1) { throw "Missing unique scoped sample $sample after restore" }
        $entry = $entries[0]
        $selected = $entry.scoped_selection
        $expectedName = if ($sample -eq 80) { 'boss01' } else { 'boss02' }
        if (-not $entry.resident -or $selected.identity -ne 'drgpit' -or $selected.signed_ordinal -ne 1 -or
            $selected.scope -ne 'mission' -or $selected.sample_name -ne $expectedName -or
            $selected.path -ne "mods/drgpit/sounds/$expectedName.wav" -or $selected.path -ne $entry.file -or
            $selected.archive -ne $context.archive -or $selected.mission_key -ne $context.mission_key -or
            $selected.generation -ne $context.generation -or $selected.bank_rate -le 0) {
            throw "Scoped sample $sample selection does not match the restored active context"
        }
    }
    Invoke-ClassicAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-classic-sound-lifecycle.png') | Out-Null
    Invoke-ClassicAdb -Arguments @('pull', '/data/local/tmp/xl-classic-sound-lifecycle.png', "$output/render.png") | Out-Null
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
Write-Host "PASS: classic sound lifecycle; rendered reference parity and mission completion remain separate gates"





