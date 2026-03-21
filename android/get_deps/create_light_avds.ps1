#!/usr/bin/env pwsh
# create_light_avds.ps1 -- Create two lightweight Nexus 5X AVDs for testing.
# These replace the heavy Pixel_6_API_34 / Pixel_6b_API_34 AVDs.
#
# Settings: 1536 MB RAM, 1280x720 @ 320dpi, 2 cores, no cameras/gps/nfc/cell,
#           4 GB SD card (for CD images), 8 GB data partition.

param([switch]$Force)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$depBase = (Get-Content (Join-Path $repoRoot "dependency_base.txt") -First 1).Trim()
$sdkDir = "$depBase\android-sdk"
$avdManager = "$sdkDir\cmdline-tools\latest\bin\avdmanager.bat"
$javaHome = "$depBase\jdk-21"

if (-not (Test-Path $avdManager)) {
    Write-Error "avdmanager not found at $avdManager"
    exit 1
}

$env:ANDROID_HOME = $sdkDir
$env:ANDROID_SDK_ROOT = $sdkDir
$env:JAVA_HOME = $javaHome

$image = "system-images;android-34;google_apis;x86_64"
$avds = @(
    @{ Name = "Nexus5X_Light_1"; Display = "Nexus 5X Light 1" },
    @{ Name = "Nexus5X_Light_2"; Display = "Nexus 5X Light 2" }
)

foreach ($avd in $avds) {
    $name = $avd.Name
    Write-Host "--- Creating AVD: $name ---"

    # Check if it already exists
    $existing = & $avdManager list avd 2>&1 | Out-String
    if ($existing -match "Name: $name" -and -not $Force) {
        Write-Host "  AVD '$name' already exists. Use -Force to recreate."
        continue
    }

    # Delete if Force
    if ($Force) {
        $ErrorActionPreference = "Continue"
        & $avdManager delete avd --name $name 2>&1 | Out-Null
        $ErrorActionPreference = "Stop"
    }

    # Create with Nexus 5X device profile
    Write-Host "  Creating AVD..."
    $ErrorActionPreference = "Continue"
    "no" | & $avdManager create avd `
        --name $name `
        --package $image `
        --device "Nexus 5X" `
        --force 2>&1 | Out-String | Write-Host
    $ErrorActionPreference = "Stop"

    # Patch config.ini with lightweight settings
    $avdDir = "$env:USERPROFILE\.android\avd\${name}.avd"
    $configFile = "$avdDir\config.ini"
    if (-not (Test-Path $configFile)) {
        Write-Error "config.ini not found at $configFile"
        continue
    }

    # Read existing config, apply overrides
    $config = Get-Content $configFile
    $overrides = @{
        "PlayStore.enabled"          = "no"
        "hw.cpu.ncore"               = "2"
        "hw.ramSize"                 = "1536"
        "hw.lcd.width"               = "1280"
        "hw.lcd.height"              = "720"
        "hw.lcd.density"             = "320"
        "hw.camera.back"             = "none"
        "hw.camera.front"            = "none"
        "hw.gps"                     = "no"
        "hw.nfc"                     = "no"
        "hw.gsmModem"                = "no"
        "hw.radio"                   = "no"
        "hw.sim"                     = "no"
        "hw.accelerometer"           = "yes"
        "hw.accelerometer_uncalibrated" = "yes"
        "hw.gyroscope"               = "yes"
        "hw.sensors.gyroscope_uncalibrated" = "yes"
        "hw.sensors.heart_rate"      = "no"
        "hw.sensors.humidity"        = "no"
        "hw.sensors.light"           = "no"
        "hw.sensors.magnetic_field"  = "yes"
        "hw.sensors.magnetic_field_uncalibrated" = "yes"
        "hw.sensors.orientation"     = "yes"
        "hw.sensors.pressure"        = "no"
        "hw.sensors.proximity"       = "no"
        "hw.sensors.temperature"     = "no"
        "hw.gpu.enabled"             = "yes"
        "hw.gpu.mode"                = "host"
        "hw.keyboard"                = "yes"
        "hw.sdCard"                  = "yes"
        "sdcard.size"                = "4096M"
        "disk.dataPartition.size"    = "4G"
        "showDeviceFrame"            = "no"
        "skin.dynamic"               = "yes"
        "skin.name"                  = "1280x720"
        "skin.path"                  = "_no_skin"
        "hw.audioInput"              = "no"
        "hw.audioOutput"             = "yes"
        "vm.heapSize"                = "256M"
    }

    # Apply overrides: replace existing lines or append
    $appliedKeys = @{}
    $newConfig = foreach ($line in $config) {
        $key = ($line -split '=', 2)[0].Trim()
        if ($overrides.ContainsKey($key)) {
            "$key = $($overrides[$key])"
            $appliedKeys[$key] = $true
        } else {
            $line
        }
    }
    # Append any keys that weren't already in the config
    foreach ($kv in $overrides.GetEnumerator()) {
        if (-not $appliedKeys.ContainsKey($kv.Key)) {
            $newConfig += "$($kv.Key) = $($kv.Value)"
        }
    }

    $newConfig | Set-Content $configFile -Encoding UTF8
    Write-Host "  Applied lightweight config to $configFile"
    Write-Host "  Done: $name"
    Write-Host ""
}

Write-Host "=== All AVDs created ==="
Write-Host "Launch with:"
Write-Host "  emulator -avd Nexus5X_Light_1 -no-snapshot-save -gpu host"
Write-Host "  emulator -avd Nexus5X_Light_2 -no-snapshot-save -gpu host"
