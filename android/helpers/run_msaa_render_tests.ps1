#!/usr/bin/env pwsh
# Serial known-color MSAA probes, both games and both requested color depths by default
param(
    [switch]$Install,
    [ValidateSet('d1', 'd2')][string]$Game,
    [ValidateSet('rgb565', 'rgba8888')][string]$ColorDepth,
    [string]$Serial = 'emulator-5554'
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'test_env.ps1')
. (Join-Path $PSScriptRoot 'test_helpers.ps1')
$outputDirectory = Join-Path $script:REPO_ROOT ('android/temp/msaa-render-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
& (Join-Path $PSScriptRoot 'retain-recent-artifacts.ps1') -Artifacts @($outputDirectory)
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$previousSerial = $env:ANDROID_SERIAL
$env:ANDROID_SERIAL = $Serial
$depths = if ($ColorDepth) { @($ColorDepth) } else { @('rgb565', 'rgba8888') }
$passed = $true
try {
    foreach ($depth in $depths) {
        Adb -AdbArgs @('logcat', '-c') | Out-Null
        $runnerPath = (Join-Path $PSScriptRoot 'run_test.ps1').Replace("'", "''")
        $command = "& '$runnerPath' -ScriptName test_msaa_render_and_menu.jsonc -TimeoutSeconds 180 -Params @{color='$depth'}"
        if ($Install) { $command += ' -Install'; $Install = $false }
        if ($Game) { $command += " -Game $Game" }
        & pwsh -NoProfile -NonInteractive -Command $command 2>&1 |
            Tee-Object -FilePath (Join-Path $outputDirectory ($depth + '.txt'))
        if ($LASTEXITCODE -ne 0) { $passed = $false; break }
    }
} finally {
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial }
    else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
Write-Output "MSAA render tests passed: $passed; output: $outputDirectory"
if ($passed) { exit 0 }
exit 1
