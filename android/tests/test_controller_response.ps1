# Run the physical-controller response integration regression in both engines
param([string]$Serial = $(if ($env:ANDROID_SERIAL) { $env:ANDROID_SERIAL } else { 'emulator-5554' }))

$ErrorActionPreference = 'Stop'
$previousSerial = $env:ANDROID_SERIAL
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$adb = 'C:\local\android-sdk\platform-tools\adb.exe'
if (Get-Command adb -ErrorAction SilentlyContinue) { $adb = 'adb' }
try {
    $env:ANDROID_SERIAL = $Serial
    foreach ($game in @('d1', 'd2')) {
        $log = Join-Path $repoRoot "temp/controller-response-$game.log"
        & $adb -s $Serial logcat -c
        & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName test_controller_response.jsonc -Game $game -TimeoutSeconds 180 *> $log
        $code = $LASTEXITCODE
        Get-Content -LiteralPath $log -Tail 25 | Where-Object { $_ -notmatch '^\{' }
        if ($code -ne 0) { throw "Controller response $game failed: $log" }
    }
} finally {
    $env:ANDROID_SERIAL = $previousSerial
}
