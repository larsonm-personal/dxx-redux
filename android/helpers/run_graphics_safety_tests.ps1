#!/usr/bin/env pwsh
# Runs the graphics confirmation test serially for both engines by default
param(
    [switch]$Install,
    [ValidateSet('d1', 'd2')][string]$Game,
    [string]$ScriptName = 'test_graphics_settings_confirmation.jsonc',
    [string]$Serial = 'emulator-5554'
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'test_env.ps1')
. (Join-Path $PSScriptRoot 'test_helpers.ps1')
$outputDirectory = Join-Path $script:REPO_ROOT ('android/temp/graphics-safety-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$outputFile = Join-Path $outputDirectory 'results.txt'
& (Join-Path $PSScriptRoot 'retain-recent-artifacts.ps1') -Artifacts @($outputDirectory)
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$previousSerial = $env:ANDROID_SERIAL
$env:ANDROID_SERIAL = $Serial
$runnerArguments = @('-File', (Join-Path $PSScriptRoot 'run_test.ps1'), '-ScriptName', $ScriptName, '-TimeoutSeconds', '180')
if ($Install) { $runnerArguments += '-Install' }
if ($Game) { $runnerArguments += @('-Game', $Game) }
try {
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    & pwsh @runnerArguments 2>&1 | Tee-Object -FilePath $outputFile
    $testExitCode = $LASTEXITCODE
} finally {
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial }
    else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
Write-Output "Graphics safety test exit: $testExitCode; output: $outputFile"
exit $testExitCode
