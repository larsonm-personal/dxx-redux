#!/usr/bin/env pwsh
param([string]$Serial = 'emulator-5554', [switch]$Install)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$env:ANDROID_SERIAL = $Serial
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
if ($Serial -notmatch '^emulator-\d+$') { throw 'Use a disposable test emulator' }
& $ADB -s $Serial shell am force-stop com.dxxredux.app
# Mission publications are regenerated from the test manifest; release previous large copies first
& $ADB -s $Serial shell run-as com.dxxredux.app rm -rf /data/user/0/com.dxxredux.app/files/d1x-redux/.mission_assets /data/user/0/com.dxxredux.app/files/d2x-redux/.mission_assets
if ($Install) {
    $apk = @('android/app/build/intermediates/apk/debug/app-debug.apk', 'android/app/build/outputs/apk/debug/app-debug.apk') |
        ForEach-Object { Get-Item -LiteralPath (Join-Path $repoRoot $_) -ErrorAction SilentlyContinue } |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    & $ADB -s $Serial install -r -t $apk.FullName
    if ($LASTEXITCODE -ne 0) { throw 'APK installation failed' }
}
$fixtures = Join-Path $repoRoot 'android/temp/guidebot_mission_metadata'
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $fixtures
New-Item -ItemType Directory -Force $fixtures | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead((Join-Path $repoRoot 'game_data/mission_files/ewithin-versions.zip'))
try {
    $entry = @($archive.Entries | Where-Object Name -eq 'ewithin-rebirth.zip')
    if ($entry.Count -ne 1) { throw 'Expected one Rebirth archive' }
    [IO.Compression.ZipFileExtensions]::ExtractToFile($entry[0], (Join-Path $fixtures 'ewithin-rebirth.zip'), $true)
} finally { $archive.Dispose() }
$target = 'files/imported/sets/default/.content/mods'
& $ADB -s $Serial shell run-as com.dxxredux.app mkdir -p $target
foreach ($file in @((Join-Path $fixtures 'ewithin-rebirth.zip'))) {
    $name = Split-Path -Leaf $file
    & $ADB -s $Serial push $file "/data/local/tmp/$name"
    & $ADB -s $Serial shell run-as com.dxxredux.app cp "/data/local/tmp/$name" "$target/$name"
    if ($LASTEXITCODE -ne 0) { throw "Could not stage $name" }
    & $ADB -s $Serial shell rm -f "/data/local/tmp/$name"
}
# Force the in-game worker path instead of reusing a route produced by launcher analysis
& $ADB -s $Serial shell run-as com.dxxredux.app rm -rf files/d2x-redux/route-cache
& $ADB -s $Serial logcat -c
& (Join-Path $repoRoot 'android/helpers/run_test.ps1') -ScriptName test_guidebot_mission_metadata.jsonc -Game d2 -TimeoutSeconds 360
if ($LASTEXITCODE -ne 0) { throw 'Enemy Within route metadata regression failed' }
& $ADB -s $Serial logcat -d | Set-Content -LiteralPath (Join-Path $fixtures 'logcat.txt') -Encoding utf8
Write-Host 'PASS Enemy Within active mission metadata and yellow-key switch guidance'
& (Join-Path $PSScriptRoot 'test_random_level_preview.ps1') -Serial $Serial -Game d2 `
    -MissionFile descent_maximum_fixed.zip -LevelNum -5 -ExpectedFirstObjective 'Open door' -CloseWithCommand -TimeoutSeconds 300
if ($LASTEXITCODE -ne 0) { throw 'Maximum S5 preview regression failed' }
