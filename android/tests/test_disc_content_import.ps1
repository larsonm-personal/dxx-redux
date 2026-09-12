# Android Anniversary import integration; JVM coverage belongs to test_gradle_unit_tests
param([string]$Serial = 'emulator-5554')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$iso = Join-Path $repoRoot 'game_data/CD images/Descent Anniversary (ISO)/descent_anniversary.iso'
if (-not (Test-Path -LiteralPath $iso)) { throw 'Anniversary ISO is required for the device test' }
$env:ANDROID_SERIAL = $Serial
$adb = if ($IsWindows) { 'C:/local/android-sdk/platform-tools/adb.exe' } else { 'adb' }
try {
    & $adb -s $Serial push $iso /data/local/tmp/descent_anniversary.iso
    if ($LASTEXITCODE -ne 0) { throw 'Could not stage Anniversary ISO' }
    & $adb -s $Serial logcat -c
    & (Join-Path $repoRoot 'android/helpers/run_test.ps1') -ScriptName test_anniversary_content_import.jsonc -Game d1
    if ($LASTEXITCODE -ne 0) { throw 'Anniversary device import test failed' }
} finally {
    & $adb -s $Serial shell rm -f /data/local/tmp/descent_anniversary.iso | Out-Null
}
