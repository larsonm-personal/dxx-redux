param([string]$Serial, [switch]$Published)
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
    $timeout = if ($Published) { 300 } else { 120 }
    $result = Adb -Seconds $timeout -AdbArgs @('shell', 'am', 'instrument', '-w', '-e', 'suite', 'package_documents',
        '-e', 'published', $Published.IsPresent.ToString().ToLowerInvariant(),
        "$($script:PACKAGE).test/com.dxxredux.app.RecoveryInstrumentation")
    Write-Output $result
    if ($result -notmatch 'PASS: package documents' -or $result -match 'FAIL:|INSTRUMENTATION_FAILED|Process crashed') {
        throw 'Package documents instrumentation failed'
    }
} finally {
    Adb -AdbArgs @('logcat', '-d') | Out-File "$PSScriptRoot/../temp/package_documents_device.log" -Encoding utf8
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
