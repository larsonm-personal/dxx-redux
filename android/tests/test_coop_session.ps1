param([string]$Serial)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
Ensure-EmulatorHealthy | Out-Null
try {
    if (-not (Test-DeviceOnline -Serial $Serial)) { throw 'Emulator is not online' }
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    $result = Adb -AdbArgs @('shell', 'am', 'instrument', '-w', '-e', 'suite', 'coop_session',
        "$($script:PACKAGE).test/com.dxxredux.app.RecoveryInstrumentation")
    Write-Output $result
    if ($result -notmatch 'PASS: migrated lobby adoption and former-host rejoin' -or
        $result -match 'FAIL:|INSTRUMENTATION_FAILED|Process crashed') { throw 'Co-op session instrumentation failed' }
} finally {
    Adb -AdbArgs @('logcat', '-b', 'all', '-d', '-s', 'MultiplayerForeground:D', 'LobbyService:I', 'AndroidRuntime:E', 'ActivityManager:I') | Write-Output
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
