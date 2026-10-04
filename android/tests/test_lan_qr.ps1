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
    Adb -AdbArgs @('shell', 'pm', 'grant', $script:PACKAGE, 'android.permission.CAMERA') | Out-Null
    $result = Adb -Seconds 120 -AdbArgs @('shell', 'am', 'instrument', '-w', '-e', 'suite', 'lan_qr',
        "$($script:PACKAGE).test/com.dxxredux.app.RecoveryInstrumentation")
    Write-Output $result
    if ($result -notmatch 'PASS: QR reveal' -or $result -match 'FAIL:|INSTRUMENTATION_FAILED|Process crashed') {
        throw 'LAN QR instrumentation failed'
    }
} finally {
    Adb -AdbArgs @('logcat', '-d') | Out-File "$PSScriptRoot/../temp/lan_qr_device.log" -Encoding utf8
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
