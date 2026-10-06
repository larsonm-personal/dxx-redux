#!/usr/bin/env pwsh
# Capture a new boss replay and reproducibly revise an accepted combined video
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$ReferenceDirectory,
    [string]$BossAudioDirectory,
    [string]$Serial = 'emulator-5582',
    [switch]$ComposeOnly,
    [switch]$ReuseCapture
)
$ErrorActionPreference = 'Stop'
$python = Join-Path $PSScriptRoot $(if ($IsWindows) { 'temp/store-assets-tools/venv/Scripts/python.exe' } else { 'temp/store-assets-tools/venv/bin/python' })
if (!(Test-Path $python)) { throw 'Provision the store asset environment with generate-store-assets.ps1 first' }
$generator = Join-Path $PSScriptRoot 'helpers/store_asset_boss_video.py'
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if ($ComposeOnly) {
    & $python $generator compose --output $OutputDirectory
    if ($LASTEXITCODE) { throw 'Offline boss preview composition failed' }
    return
}
if (!$ReferenceDirectory -or !$BossAudioDirectory) { throw 'Provide -ReferenceDirectory and -BossAudioDirectory' }
& (Join-Path $PSScriptRoot 'helpers/retain-recent-artifacts.ps1') -Artifacts $OutputDirectory
if (!$ReuseCapture) {
    if ($Serial -notmatch '^emulator-(\d+)$') { throw 'Capture requires an emulator serial' }
    $port = $Matches[1]
    $depBase = (Get-Content (Join-Path (Split-Path $PSScriptRoot) 'dependency_base.txt') -First 1).Trim()
    $sdk = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { Join-Path $depBase 'android-sdk' }
    $adb = Join-Path $sdk $(if ($IsWindows) { 'platform-tools/adb.exe' } else { 'platform-tools/adb' })
    if (!(@(& $adb devices) -match "^$([regex]::Escape($Serial))\s+device$")) {
        $emulator = Join-Path $sdk $(if ($IsWindows) { 'emulator/emulator.exe' } else { 'emulator/emulator' })
        $start = @{
            FilePath = $emulator
            ArgumentList = @('-avd', 'DxxStoreAssets', '-port', $port, '-no-window', '-no-snapshot', '-no-boot-anim', '-gpu', 'host', '-feature', 'GuestAngle')
            RedirectStandardOutput = (Join-Path $PSScriptRoot 'temp/store-assets-tools/boss-emulator.log')
            RedirectStandardError = (Join-Path $PSScriptRoot 'temp/store-assets-tools/boss-emulator-error.log')
        }
        if ($IsWindows) { $start.WindowStyle = 'Hidden' }
        Start-Process @start | Out-Null
        $deadline = (Get-Date).AddMinutes(3)
        do {
            $booted = & $adb -s $Serial shell getprop sys.boot_completed 2>$null
            if ($booted -eq '1') { break }
            if ((Get-Date) -gt $deadline) { throw 'Capture emulator did not boot in three minutes' }
            Start-Sleep -Seconds 2
        } while ($true)
    }
    & $python $generator capture --output $OutputDirectory --serial $Serial
    if ($LASTEXITCODE) { throw 'Native boss capture failed' }
}
& $python $generator prepare --output $OutputDirectory --reference ([IO.Path]::GetFullPath($ReferenceDirectory)) --audio-run ([IO.Path]::GetFullPath($BossAudioDirectory))
if ($LASTEXITCODE) { throw 'Boss preview preparation failed' }
Write-Output "Review: $(Join-Path $OutputDirectory 'index.html')"
