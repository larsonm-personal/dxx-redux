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
    if ($testExitCode -ne 0) {
        # Keep the original failure while retaining platform routing evidence
        try {
            $setup = Adb-Timeout -AdbArgs @('shell', 'settings', 'get', 'secure', 'user_setup_complete') -Seconds 15
            $provisioned = Adb-Timeout -AdbArgs @('shell', 'settings', 'get', 'global', 'device_provisioned') -Seconds 15
            Write-Host "Media routing prerequisites: user_setup_complete=$setup device_provisioned=$provisioned"
            $sessions = Adb-Timeout -AdbArgs @('shell', 'dumpsys', 'media_session') -Seconds 15
            Write-Host 'Media routing state after failed preview controls:'
            ($sessions -split '\r?\n' | Select-Object -First 40) | ForEach-Object { Write-Host $_ }
            $routing = Adb-Timeout -AdbArgs @('logcat', '-d', '-v', 'threadtime', '-s', 'MediaSessionService:V', 'MediaSessionStack:V', '*:S') -Seconds 15
            Write-Host 'Recent platform media-key routing:'
            ($routing -split '\r?\n' | Select-Object -Last 40) | ForEach-Object { Write-Host $_ }
        } catch {
            Write-Warning "Media routing diagnostics unavailable: $_"
        }
    }
} finally {
    & $script:ADB -s $Serial shell run-as $script:PACKAGE rm -f files/preview-media-test.mp3
    & $script:ADB -s $Serial shell rm -f /data/local/tmp/preview-media-test.mp3
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
exit $testExitCode
