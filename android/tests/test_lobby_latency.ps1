param([string]$Serial = 'emulator-5554')
$ErrorActionPreference = 'Stop'
if ($Serial -notmatch '^emulator-\d+$') { throw 'Run this fixture only on an emulator' }
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$env:ANDROID_SERIAL = $Serial
try {
    if (-not (Test-DeviceOnline -Serial $Serial)) { throw 'Emulator is not online' }
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    $result = Adb -Seconds 120 -AdbArgs @('shell', 'am', 'instrument', '-w', '-e', 'suite', 'lobby_latency',
        'com.dxxredux.app.test/com.dxxredux.app.RecoveryInstrumentation')
    Write-Output $result
    if ($result -notmatch 'PASS: responsive discovery' -or
        $result -match 'FAIL:|INSTRUMENTATION_FAILED|Process crashed') { throw 'Lobby latency instrumentation failed' }
} finally {
    Adb -AdbArgs @('logcat', '-b', 'all', '-d', '-s', 'LobbyLatencyChecks:I', 'LobbyService:D', 'AndroidRuntime:E') | Write-Output
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
