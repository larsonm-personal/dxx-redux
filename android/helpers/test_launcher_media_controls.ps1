param(
    [string]$Serial = "emulator-5554",
    [string]$AudioFile,
    [switch]$Install
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
if (-not $AudioFile) {
    $AudioFile = Join-Path $repoRoot "game_data/music/D1 macplay mp3/03 The Escape.mp3"
}
if (-not (Test-Path -LiteralPath $AudioFile)) { throw "Pass -AudioFile with an MP3 longer than 20 seconds" }
$adb = "C:/local/android-sdk/platform-tools/adb.exe"
if (Get-Command adb -ErrorAction SilentlyContinue) { $adb = (Get-Command adb).Source }
$previousSerial = $env:ANDROID_SERIAL
try {
    $env:ANDROID_SERIAL = $Serial
    if ($Install) {
        $apkCandidates = @(
            (Join-Path $repoRoot "android/app/build/outputs/apk/debug/app-debug.apk"),
            (Join-Path $repoRoot "android/app/build/intermediates/apk/debug/app-debug.apk")
        )
        $apk = $apkCandidates | Get-Item -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if (-not $apk) { throw "Build the debug APK before running with -Install" }
        & $adb install -r -t $apk.FullName
        if ($LASTEXITCODE -ne 0) { throw "APK installation failed" }
    }
    & $adb push $AudioFile /data/local/tmp/preview-media-test.mp3
    if ($LASTEXITCODE -ne 0) { throw "MP3 push failed" }
    & $adb shell run-as com.dxxredux.app mkdir -p files
    & $adb shell run-as com.dxxredux.app cp /data/local/tmp/preview-media-test.mp3 files/preview-media-test.mp3
    if ($LASTEXITCODE -ne 0) { throw "MP3 staging failed" }
    & $adb logcat -c
    & (Join-Path $PSScriptRoot "run_test.ps1") -ScriptName test_launcher_media_controls.jsonc -Game d2 -TimeoutSeconds 180
    $testExitCode = $LASTEXITCODE
} finally {
    & $adb shell run-as com.dxxredux.app rm -f files/preview-media-test.mp3
    & $adb shell rm -f /data/local/tmp/preview-media-test.mp3
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
exit $testExitCode
