#!/usr/bin/env pwsh
# Published mission/level sound ownership, actual PCM and disk/rewind isolation
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$FixtureDirectory
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-audio-scopes-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
& python "$PSScriptRoot/../XL_compat/tests/create_audio_scope_package.py" $FixtureDirectory "$output/xl-audio-scopes.zip"
if ($LASTEXITCODE -ne 0) { throw 'Cannot create the pinned scoped-audio package' }
function Invoke-AudioScopesAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "AudioScopes adb failed: $result" }
    return $result
}
function Invoke-AudioScopesPrivate {
    param([string]$Command)
    Invoke-AudioScopesAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-audio-scopes-backup'
$paths = @('files/d2x-redux', 'files/d1x-redux',
    'files/imported/sets/default/.content', 'files/imported/sets/default/.content_projection',
    'files/descent.cfg', 'files/file_sets.json', 'shared_prefs', 'files/route_metadata_precompute.json')
$ready = $false
try {
    Invoke-AudioScopesAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-AudioScopesPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-AudioScopesPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-AudioScopesPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = ([string](Invoke-AudioScopesPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;")) -split '\r?\n' | Where-Object { $_ } | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-AudioScopesAdb -Arguments @('push', "$output/xl-audio-scopes.zip", '/data/local/tmp/xl-audio-scopes.zip') | Out-Null
    Invoke-AudioScopesPrivate 'mkdir -p files/imported/sets/default/.content/mods && cp /data/local/tmp/xl-audio-scopes.zip files/imported/sets/default/.content/mods/xl-audio-scopes.zip' | Out-Null
    Invoke-AudioScopesAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_audio_scopes.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-AudioScopesAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-AudioScopesPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-AudioScopesPrivate 'if test -f files/automation_log.jsonl; then cat files/automation_log.jsonl; fi' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-AudioScopesPrivate 'if test -f files/introspect.json; then cat files/introspect.json; fi' | Set-Content "$output/state.json" -Encoding utf8
    Invoke-AudioScopesPrivate 'if test -f files/d2x-redux/.mission_assets.json; then cat files/d2x-redux/.mission_assets.json; fi' | Set-Content "$output/mission-assets.json" -Encoding utf8
    if ($testExit -ne 0) { throw "AudioScopes runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete AudioScopes runtime result' }
    Write-Host "PASS: Published scoped audio Android PCM, normal/secret levels, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-AudioScopesAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; restore both games and the published package catalog
            foreach ($path in $paths) { Invoke-AudioScopesPrivate "rm -rf $path" | Out-Null }
            Invoke-AudioScopesPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = ([string](Invoke-AudioScopesPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;")) -split '\r?\n' | Where-Object { $_ } | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "AudioScopes test state restoration differs; backup retained at $backup" }
            Invoke-AudioScopesPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-AudioScopesAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-audio-scopes.zip') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
