#!/usr/bin/env pwsh
# Real packs, isolated file set, fresh engine processes: off -> on -> off
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [switch]$Install,
    [string]$ApkPath,
    # A partial rerun reuses the other phases already captured in the evidence directory
    [ValidateSet('off', 'on', 'off-again')]
    [string[]]$Phases = @('off', 'on', 'off-again'),
    [ValidateSet('d1', 'd2', 'd1-in-d2', 'mac-d1')]
    [string[]]$Modes = @('d1', 'd2', 'd1-in-d2', 'mac-d1')
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
Initialize-AndroidTestTarget -Serial $Serial | Out-Null
Assert-IsolatedPhysicalTestApp
Ensure-EmulatorHealthy | Out-Null
$repo = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repo 'temp/dxa-asset-usage'
New-Item -ItemType Directory -Force $output | Out-Null
$packs = Join-Path $repo 'game_data/mods/d2x-xl'
$target = 'files/imported/sets/dxa-verification/.content/mods'
if ($Install) {
    $apk = if ($ApkPath) { $ApkPath } else { Join-Path $repo 'android/app/build/intermediates/apk/debug/app-debug.apk' }
    if (!(Test-Path -LiteralPath $apk)) { $apk = Join-Path $repo 'android/app/build/outputs/apk/debug/app-debug.apk' }
    if (!(Test-Path -LiteralPath $apk)) { throw 'Build the debug APK before running with -Install' }
    $installResult = Adb -AdbArgs @('install', '-r', '-t', $apk) -Seconds 120
    if ($installResult -notmatch 'Success') { throw "APK install failed: $installResult" }
}
Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mkdir', '-p', $target) | Out-Null
foreach ($name in @('d1-hires-sounds.dxa', 'd2-hires-sounds.dxa', 'd1xr-hires.dxa', 'd1xr-mac-demo-sounds.dxa')) {
    $file = Join-Path $packs $name
    if (!(Test-Path -LiteralPath $file)) { throw "Required real pack is missing: $file" }
    Adb -AdbArgs @('push', $file, "/data/local/tmp/$name") -Seconds 60 | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', "/data/local/tmp/$name", "$target/$name") | Out-Null
    Adb -AdbArgs @('shell', 'rm', '-f', "/data/local/tmp/$name") | Out-Null
}
$macTarget = 'files/imported/sets/dxa-mac-verification/.content/mods'
Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mkdir', '-p', $macTarget) | Out-Null
foreach ($name in @('d1xr-hires.dxa', 'd1xr-mac-demo-sounds.dxa')) {
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', "$target/$name", "$macTarget/$name") | Out-Null
}
foreach ($mode in $Modes) {
    foreach ($phase in $Phases) {
        $manifest = @{
            mods = @(
                @{ filename = 'd1-hires-sounds.dxa'; displayName = 'D1 sounds'; game = 'd1'; order = 0; enabled = ($phase -eq 'on'); addedAt = 0; sizeBytes = 0 },
                @{ filename = 'd2-hires-sounds.dxa'; displayName = 'D2 sounds'; game = 'd2'; order = 1; enabled = ($phase -eq 'on'); addedAt = 0; sizeBytes = 0 },
                @{ filename = 'd1xr-hires.dxa'; displayName = 'D1 presentation'; game = 'd1'; order = 2; enabled = ($phase -eq 'on'); addedAt = 0; sizeBytes = 0 }
            )
        }
        $phaseTarget = $target
        if ($mode -eq 'mac-d1') {
            $phaseTarget = $macTarget
            $manifest.mods = @(
                @{ filename = 'd1xr-mac-demo-sounds.dxa'; displayName = 'Mac sounds'; game = 'd1'; order = 0; enabled = ($phase -eq 'on'); addedAt = 0; sizeBytes = 0 },
                @{ filename = 'd1xr-hires.dxa'; displayName = 'D1 presentation'; game = 'd1'; order = 1; enabled = ($phase -eq 'on'); addedAt = 0; sizeBytes = 0 }
            )
        }
        $localManifest = Join-Path $output 'mod_manifest.json'
        $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $localManifest -Encoding utf8
        Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        Adb -AdbArgs @('push', $localManifest, '/data/local/tmp/dxa-mod-manifest.json') | Out-Null
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/dxa-mod-manifest.json', "$phaseTarget/mod_manifest.json") | Out-Null
        Adb -AdbArgs @('logcat', '-c') | Out-Null
        $game = if ($mode -in @('d1', 'mac-d1')) { 'd1' } else { 'd2' }
        $mission = if ($mode -eq 'd2') { 'Counterstrike' } else { 'First Strike' }
        & (Join-Path $PSScriptRoot '../helpers/run_test.ps1') -ScriptName test_dxa_asset_usage.jsonc -Game $game -Serial $env:ANDROID_SERIAL -LeaveRunning -TimeoutSeconds 300 -Params @{ MODE = $mode; MISSION = $mission }
        $result = $LASTEXITCODE
        Adb -AdbArgs @('logcat', '-d') -Seconds 30 | Set-Content -LiteralPath (Join-Path $output "$mode-$phase.log") -Encoding utf8
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/introspect.json') | Set-Content -LiteralPath (Join-Path $output "$mode-$phase.json") -Encoding utf8
        $engineDir = if ($game -eq 'd1') { 'd1x-redux' } else { 'd2x-redux' }
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', "files/$engineDir/mod_usage.json") | Set-Content -LiteralPath (Join-Path $output "$mode-$phase-usage.json") -Encoding utf8
        if ($result -ne 0) { throw "DXA scenario failed: $mode $phase (see $output)" }
    }
}
& python (Join-Path $PSScriptRoot 'verify_dxa_asset_usage.py') $output $packs --modes $Modes
if ($LASTEXITCODE -ne 0) { throw 'DXA payload/playback verification failed' }
Write-Host "DXA asset-use integration passed: $output"
