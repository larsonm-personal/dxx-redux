#!/usr/bin/env pwsh
# Extended-HOG import, metadata, native/JVM music selection, playback and engine activation
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../../temp/xl-hog-launcher-android-$(Get-Date -Format 'yyyyMMdd-HHmmss')"))
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
& python "$PSScriptRoot/../XL_compat/tests/create_hog_launcher_fixture.py" $Hog $output
if ($LASTEXITCODE -ne 0) { throw 'HOG launcher fixture generation failed' }
$fixture = Get-Content "$output/fixture.json" -Raw | ConvertFrom-Json -AsHashtable
$steps = Get-Content "$PSScriptRoot/../game_scripts/test_xl_hog_launcher.jsonc" -Raw | ConvertFrom-Json -AsHashtable
foreach ($step in $steps) {
    if ($step.action -eq 'test_hog_launcher') {
        foreach ($key in $fixture.Keys) { $step[$key] = $fixture[$key] }
    }
}
$scriptPath = Join-Path $output 'test_xl_hog_launcher_generated.jsonc'
ConvertTo-Json -InputObject $steps -Depth 20 | Set-Content -LiteralPath $scriptPath -Encoding utf8
function Invoke-HogLauncherAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "HOG launcher adb failed: $result" }
    return $result
}
function Invoke-HogLauncherPrivate {
    param([string]$Command)
    Invoke-HogLauncherAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-hog-launcher-backup'
$paths = @(
    'files/imported/sets/default/.content', 'files/imported/sets/default/.content_projection',
    'files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/missions',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/route_metadata_precompute.json', 'files/xl-hog-launcher.zip', 'files/hog_launcher_result.json',
    'files/mission_zip_import_xl-hog-launcher.json'
)
$ready = $false
try {
    Invoke-HogLauncherAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    if (([string](Invoke-HogLauncherPrivate 'if test -f files/file_sets.json; then cat files/file_sets.json; fi')).Trim()) {
        $sets = ([string](Invoke-HogLauncherPrivate 'cat files/file_sets.json')) | ConvertFrom-Json
        if ($sets.active -and $sets.active -ne 'default') { throw 'Use the provisioned default test file set' }
    }
    $existing = @($paths | Where-Object { ([string](Invoke-HogLauncherPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-HogLauncherPrivate "mkdir $backup" | Out-Null
    Invoke-HogLauncherPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = ([string](Invoke-HogLauncherPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;")) -split '\r?\n' | Where-Object { $_ } | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-HogLauncherAdb -Arguments @('push', "$output/xl-hog-launcher.zip", '/data/local/tmp/xl-hog-launcher.zip') | Out-Null
    Invoke-HogLauncherPrivate 'cp /data/local/tmp/xl-hog-launcher.zip files/xl-hog-launcher.zip' | Out-Null
    Invoke-HogLauncherAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName $scriptPath -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 600
    $testExit = $LASTEXITCODE
    Invoke-HogLauncherAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-HogLauncherPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    # Launcher failures can precede engine logging and the successful preview receipt
    Invoke-HogLauncherPrivate 'if test -f files/automation_log.jsonl; then cat files/automation_log.jsonl; fi' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-HogLauncherPrivate 'if test -f files/hog_launcher_result.json; then cat files/hog_launcher_result.json; fi' | Set-Content "$output/launcher.json" -Encoding utf8
    if ($testExit -ne 0) { throw "HOG launcher failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete HOG launcher result' }
} finally {
    try {
        Invoke-HogLauncherAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; restore the saved configuration and owned mod catalogs
            foreach ($path in $paths) { Invoke-HogLauncherPrivate "rm -rf $path" | Out-Null }
            Invoke-HogLauncherPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = ([string](Invoke-HogLauncherPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;")) -split '\r?\n' | Where-Object { $_ } | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "HOG launcher state restoration differs; backup retained at $backup" }
            Invoke-HogLauncherPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-HogLauncherAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-hog-launcher.zip') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
Write-Host "PASS: extended HOG launcher and engine activation ($($result.total_steps) steps); protected state restored"
