param(
    [string]$Serial,
    [string]$AudioFile,
    [switch]$Install
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot 'test_helpers.ps1')
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
if (-not $AudioFile) {
    $AudioFile = Join-Path $repoRoot "game_data/music/D1 macplay mp3/03 The Escape.mp3"
}
if (-not (Test-Path -LiteralPath $AudioFile)) { throw "Pass -AudioFile with an MP3 longer than 20 seconds" }
$previousSerial = $env:ANDROID_SERIAL
try {
    $Serial = Initialize-AndroidTestTarget -Serial $Serial
    Assert-IsolatedPhysicalTestApp
    Ensure-EmulatorHealthy | Out-Null
    if ($Install -and -not (Install-ApkOnDevice -Serial $Serial)) { throw 'APK installation failed' }
    & $script:ADB -s $Serial push $AudioFile /data/local/tmp/preview-media-test.mp3
    if ($LASTEXITCODE -ne 0) { throw "MP3 push failed" }
    & $script:ADB -s $Serial shell run-as $script:PACKAGE mkdir -p files
    & $script:ADB -s $Serial shell run-as $script:PACKAGE cp /data/local/tmp/preview-media-test.mp3 files/preview-media-test.mp3
    if ($LASTEXITCODE -ne 0) { throw "MP3 staging failed" }
    & $script:ADB -s $Serial logcat -c
    & (Join-Path $PSScriptRoot "run_test.ps1") -ScriptName test_launcher_media_controls.jsonc -Game d2 -Serial $Serial -TimeoutSeconds 180
    $testExitCode = $LASTEXITCODE
} finally {
    & $script:ADB -s $Serial shell run-as $script:PACKAGE rm -f files/preview-media-test.mp3
    & $script:ADB -s $Serial shell rm -f /data/local/tmp/preview-media-test.mp3
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
exit $testExitCode
