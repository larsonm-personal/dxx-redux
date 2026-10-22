#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Regenerate launcher screenshots, demo candidates and a 30-second review video
.DESCRIPTION
    Uses only the dedicated DxxStoreAssets emulator and resets that app sandbox.
    Media and licensed game data stay in ignored android/store-assets directories.
    See store-assets.md for requirements and editing the clip recipe.
#>
[CmdletBinding()]
param(
    [string]$OutputDirectory,
    [switch]$NoBuild,
    [switch]$AllDemos,
    [switch]$ScreenshotsOnly,
    [switch]$AllowUnverifiedAssets,
    [switch]$ComposeOnly,
    [switch]$SelectStillsOnly,
    [switch]$FeaturedOnly,
    [switch]$CombineVideo,
    [string]$PictureDirectory,
    [string]$AudioDirectory,
    [string]$FlyoutDirectory,
    [string]$Recipe
)
$ErrorActionPreference = 'Stop'
if (([int]$ComposeOnly.IsPresent + [int]$SelectStillsOnly.IsPresent + [int]$FeaturedOnly.IsPresent + [int]$CombineVideo.IsPresent) -gt 1) { throw 'Choose only one generation mode' }
if ($ScreenshotsOnly -and ($ComposeOnly -or $SelectStillsOnly -or $FeaturedOnly -or $CombineVideo -or $AllDemos -or $AudioDirectory -or $Recipe)) { throw '-ScreenshotsOnly cannot use demo/video options' }
$repoRoot = Split-Path $PSScriptRoot
$toolsDir = Join-Path $PSScriptRoot 'temp/store-assets-tools'
New-Item -ItemType Directory -Force -Path $toolsDir | Out-Null
if (!$OutputDirectory) {
    if ($ComposeOnly -or $SelectStillsOnly -or $FeaturedOnly -or $CombineVideo) { throw 'Partial generation requires -OutputDirectory' }
    $OutputDirectory = Join-Path $PSScriptRoot ('store-assets/capture_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (!$ComposeOnly -and !$SelectStillsOnly -and !$FeaturedOnly -and !$CombineVideo) {
    if ((Test-Path (Join-Path $OutputDirectory 'build.json')) -or (Test-Path (Join-Path $OutputDirectory 'live-stills.json'))) { throw 'Choose a new output directory for regeneration, or use -ComposeOnly' }
    # Persistent publication directories must survive both cleanup and failed retries
    $storeRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'store-assets'))
    $comparison = if ($IsWindows) { [StringComparison]::OrdinalIgnoreCase } else { [StringComparison]::Ordinal }
    $persistent = $OutputDirectory.Equals($storeRoot, $comparison) -or $OutputDirectory.StartsWith($storeRoot + [IO.Path]::DirectorySeparatorChar, $comparison)
    $retentionTarget = if ($persistent) { $storeRoot } else { $OutputDirectory }
    & (Join-Path $PSScriptRoot 'helpers/retain-recent-artifacts.ps1') -Artifacts $retentionTarget
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
}
$python = (Get-Command python -ErrorAction Stop).Source
$venv = Join-Path $toolsDir 'venv'
$venvPython = Join-Path $venv $(if ($IsWindows) { 'Scripts/python.exe' } else { 'bin/python' })
if (!(Test-Path $venvPython)) {
    & $python -m venv $venv
    if ($LASTEXITCODE) { throw 'Could not create capture Python environment' }
}
& $venvPython -m pip install --disable-pip-version-check 'imageio-ffmpeg==0.6.0' 'Pillow==11.3.0' *> (Join-Path $toolsDir 'python-install.log')
if ($LASTEXITCODE) { throw 'Capture dependencies failed; see temp/store-assets-tools/python-install.log' }
$generator = Join-Path $PSScriptRoot 'helpers/generate_store_assets.py'
if ($CombineVideo) {
    if (!$PictureDirectory -or !$AudioDirectory) { throw '-CombineVideo requires -PictureDirectory and -AudioDirectory' }
    & $venvPython -m pip install --disable-pip-version-check 'numpy==2.2.6' 'opencv-python-headless==4.12.0.88' *> (Join-Path $toolsDir 'video-audit-install.log')
    if ($LASTEXITCODE) { throw 'Video audit dependencies failed' }
    $arguments = @($generator, 'combine-video', '--output', $OutputDirectory, '--picture-run', [IO.Path]::GetFullPath($PictureDirectory), '--audio-run', [IO.Path]::GetFullPath($AudioDirectory))
    if ($FlyoutDirectory) { $arguments += @('--flyout-run', [IO.Path]::GetFullPath($FlyoutDirectory)) }
    & $venvPython @arguments
    if ($LASTEXITCODE) { throw 'Combined video failed; inspect its validation report' }
    Write-Output "Review: $(Join-Path $OutputDirectory 'index.html')"
    return
}
if ($ComposeOnly -or $SelectStillsOnly) {
    $action = if ($SelectStillsOnly) { 'select-stills' } else { 'compose' }
    $arguments = @($generator, $action, '--output', $OutputDirectory)
    if ($Recipe) { $arguments += @('--recipe', [IO.Path]::GetFullPath($Recipe)) }
    & $venvPython @arguments
    if ($LASTEXITCODE) { throw 'Offline asset generation failed' }
    Write-Output "Review: $(Join-Path $OutputDirectory 'index.html')"
    return
}
if (!$ScreenshotsOnly -and !$FeaturedOnly) {
    $sourceArguments = @($generator, 'check-sources', '--output', $OutputDirectory)
    if ($AllDemos) { $sourceArguments += '--all-demos' }
    & $venvPython @sourceArguments
    if ($LASTEXITCODE) { throw 'Full store capture requires qualified format-5 replay sources; use -ScreenshotsOnly for fresh screenshots' }
}
$depBase = (Get-Content (Join-Path $repoRoot 'dependency_base.txt') -First 1).Trim()
$env:JAVA_HOME = Join-Path $depBase 'jdk-21'
$env:ANDROID_HOME = Join-Path $depBase 'android-sdk'
$env:ANDROID_SDK_ROOT = $env:ANDROID_HOME
$env:Path = (Join-Path $env:JAVA_HOME 'bin') + [IO.Path]::PathSeparator + $env:Path
$adb = Join-Path $env:ANDROID_HOME $(if ($IsWindows) { 'platform-tools/adb.exe' } else { 'platform-tools/adb' })
$emulator = Join-Path $env:ANDROID_HOME $(if ($IsWindows) { 'emulator/emulator.exe' } else { 'emulator/emulator' })
$avdManager = Join-Path $env:ANDROID_HOME $(if ($IsWindows) { 'cmdline-tools/latest/bin/avdmanager.bat' } else { 'cmdline-tools/latest/bin/avdmanager' })
# Keep registration beside the disposable emulator instead of depending on a user profile
$env:ANDROID_AVD_HOME = Join-Path $toolsDir 'avd-home'
New-Item -ItemType Directory -Force -Path $env:ANDROID_AVD_HOME | Out-Null
$avdPath = Join-Path $toolsDir 'avd'
if (!(Test-Path (Join-Path $avdPath 'config.ini'))) {
    'no' | & $avdManager create avd --name DxxStoreAssets --package 'system-images;android-34;google_apis;x86_64' --device 'Nexus 5X' --path $avdPath *> (Join-Path $toolsDir 'avd-create.log')
    if ($LASTEXITCODE) { throw 'AVD creation failed; install the API 34 google_apis x86_64 system image' }
}
# Repair a missing descriptor when the existing disk/config survived profile cleanup
[IO.File]::WriteAllText((Join-Path $env:ANDROID_AVD_HOME 'DxxStoreAssets.ini'), "avd.ini.encoding=UTF-8`npath=$avdPath`ntarget=android-34`n")
$configPath = Join-Path $avdPath 'config.ini'
$config = Get-Content -LiteralPath $configPath -Raw
$settings = @{
    'hw.lcd.width' = '1080'; 'hw.lcd.height' = '2400'; 'hw.lcd.density' = '420'
    'hw.ramSize' = '4096'; 'hw.cpu.ncore' = '4'; 'hw.keyboard' = 'no'
    'hw.gpu.enabled' = 'yes'; 'hw.gpu.mode' = 'host'; 'disk.dataPartition.size' = '8G'
}
foreach ($key in $settings.Keys) {
    $pattern = '(?m)^' + [regex]::Escape($key) + '=.*$'
    if ($config -match $pattern) { $config = [regex]::Replace($config, $pattern, $key + '=' + $settings[$key]) }
    else { $config += "`n$key=$($settings[$key])" }
}
[IO.File]::WriteAllText($configPath, $config)
$captureRunning = @(& $adb devices) -match '^emulator-5580\s+device$'
if ($captureRunning) {
    if (!(@(& $adb -s emulator-5580 emu avd name) -contains 'DxxStoreAssets')) { throw 'Port 5580 belongs to another emulator' }
    if ((& $adb -s emulator-5580 shell getprop ro.hardware.egl).Trim() -ne 'angle') {
        # The host GLES translator does not expose anisotropic filtering
        & $adb -s emulator-5580 emu kill | Out-Null
        Start-Sleep -Seconds 3
    }
}
if (!(@(& $adb devices) -match '^emulator-5580\s+device$')) {
    $start = @{
        FilePath = $emulator
        ArgumentList = @('-avd', 'DxxStoreAssets', '-port', '5580', '-no-window', '-no-snapshot', '-no-boot-anim', '-gpu', 'host', '-feature', 'GuestAngle')
        RedirectStandardOutput = (Join-Path $toolsDir 'emulator.log')
        RedirectStandardError = (Join-Path $toolsDir 'emulator-error.log')
    }
    if ($IsWindows) { $start.WindowStyle = 'Hidden' }
    Start-Process @start | Out-Null
}
$deadline = (Get-Date).AddMinutes(3)
do {
    $booted = & $adb -s emulator-5580 shell getprop sys.boot_completed 2>$null
    if ($booted -eq '1') { break }
    if ((Get-Date) -gt $deadline) { throw 'Capture emulator did not boot in three minutes' }
    Start-Sleep -Seconds 2
} while ($true)
if (!(@(& $adb -s emulator-5580 emu avd name) -contains 'DxxStoreAssets')) { throw 'Port 5580 belongs to another emulator' }
if (!$NoBuild) {
    $gradle = Join-Path $PSScriptRoot $(if ($IsWindows) { 'gradlew.bat' } else { 'gradlew' })
    & $gradle -p $PSScriptRoot assembleDebug '-Pandroid.injected.build.abi=x86_64' --no-daemon *> (Join-Path $OutputDirectory 'build.log')
    if ($LASTEXITCODE) { throw "Capture APK build failed; see $OutputDirectory/build.log" }
}
$apk = Join-Path $PSScriptRoot 'app/build/intermediates/apk/debug/app-debug.apk'
if (!(Test-Path $apk)) { $apk = Join-Path $PSScriptRoot 'app/build/outputs/apk/debug/app-debug.apk' }
if (!(Test-Path $apk)) { throw 'Debug APK is missing' }
$action = if ($ScreenshotsOnly) { 'screenshots' } else { 'all' }
$arguments = @($generator, $action, '--output', $OutputDirectory, '--apk', $apk)
if ($FeaturedOnly) {
    & $adb -s emulator-5580 install -r -t $apk | Out-Null
    if ($LASTEXITCODE) { throw 'Could not install capture APK' }
    $arguments = @($generator, 'capture-featured', '--output', $OutputDirectory)
}
if ($AllDemos) { $arguments += '--all-demos' }
if ($AllowUnverifiedAssets) { $arguments += '--allow-unverified-assets' }
if ($Recipe) { $arguments += @('--recipe', [IO.Path]::GetFullPath($Recipe)) }
$captureLog = if ($FeaturedOnly) { 'capture-featured.log' } else { 'capture.log' }
& $venvPython @arguments 2>&1 | Tee-Object -FilePath (Join-Path $OutputDirectory $captureLog)
if ($LASTEXITCODE) { throw "Capture failed; diagnostics remain in $OutputDirectory" }
if ($AudioDirectory -and !$FeaturedOnly) {
    & $venvPython -m pip install --disable-pip-version-check 'numpy==2.2.6' 'opencv-python-headless==4.12.0.88' *> (Join-Path $toolsDir 'video-audit-install.log')
    if ($LASTEXITCODE) { throw 'Video audit dependencies failed' }
    & $venvPython $generator combine-video --output $OutputDirectory --picture-run $OutputDirectory --audio-run ([IO.Path]::GetFullPath($AudioDirectory))
    if ($LASTEXITCODE) { throw 'Combining regenerated visuals with reference audio failed' }
}
Write-Output "Review: $(Join-Path $OutputDirectory 'index.html')"
