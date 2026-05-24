#!/usr/bin/env pwsh
<#
.SYNOPSIS
  GOG Installer + Redbook regression test (unified launcher+game script).

.DESCRIPTION
  Pushes the selected D2 GOG installer variant to the emulator, then invokes
  run_test.ps1 with the unified JSON5 script that handles both launcher setup
  and in-game verification.

  D2 variants covered by this wrapper:
  - game_data\gog installers\setup_descent_2_1.1_(16596).exe
  - game_data\gog installers\descent_2_enUS_1_0_51877.pkg

    Companion D1 regression targets:
  - game_data\gog installers\setup_descent_1.4a_(16596).exe
  - game_data\gog installers\descent_enUS_1_0_35122.pkg

  Note: The Mac and PC GOG installers appear to ship the same PC game data.
  The Mac package seems to wrap the DOS build rather than include original
  Mac-specific assets, which should keep the exe/pkg test paths mostly shared.

.PARAMETER GogInstallerPath
  Local path to the selected D2 GOG installer (.exe or .pkg). Defaults to the
  known location for the selected installer variant.

.PARAMETER InstallerVariant
  Which D2 installer variant to run: d2_windows_exe or d2_mac_pkg.

.PARAMETER SkipPush
  Skip pushing the selected installer (assumes it's already on the emulator).

.EXAMPLE
  .\test_gog_installer_redbook_unified.ps1
  .\test_gog_installer_redbook_unified.ps1 -InstallerVariant d2_mac_pkg
  .\test_gog_installer_redbook_unified.ps1 -SkipPush
#>
param(
    [Alias('GogExePath')]
    [string]$GogInstallerPath,
    [ValidateSet('d2_windows_exe', 'd2_mac_pkg')]
    [string]$InstallerVariant,
    [switch]$SkipPush,
    [int]$TimeoutSeconds = 300
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\..\test_helpers.ps1"

$SCRIPT_NAME = 'test_gog_installer_redbook_unified.json5'

$installerVariants = @{
    d2_windows_exe = @{
        Label = 'D2 Windows GOG .exe'
        FileName = 'setup_descent_2_1.1_(16596).exe'
        DevicePath = '/data/local/tmp/setup_descent_2_1.1_(16596).exe'
    }
    d2_mac_pkg = @{
        Label = 'D2 Mac GOG .pkg'
        FileName = 'descent_2_enUS_1_0_51877.pkg'
        DevicePath = '/data/local/tmp/descent_2_enUS_1_0_51877.pkg'
    }
}

if (-not $InstallerVariant) {
    if ($GogInstallerPath) {
        $leafName = Split-Path $GogInstallerPath -Leaf
        switch -Regex ($leafName) {
            '^setup_descent_2_1\.1_\(16596\)\.exe$' { $InstallerVariant = 'd2_windows_exe'; break }
            '^descent_2_enUS_1_0_51877\.pkg$' { $InstallerVariant = 'd2_mac_pkg'; break }
            '^setup_descent_1\.4a_\(16596\)\.exe$' {
                Write-Status 'FAIL: This wrapper only drives the D2 installer redbook test. The D1 exe path is tracked for the later D1-specific test.' 'Red'
                exit 1
            }
            '^descent_enUS_1_0_35122\.pkg$' {
                Write-Status 'FAIL: This wrapper only drives the D2 installer redbook test. The D1 pkg path is tracked for the later D1-specific test.' 'Red'
                exit 1
            }
            '\.pkg$' { $InstallerVariant = 'd2_mac_pkg'; break }
            default { $InstallerVariant = 'd2_windows_exe' }
        }
    } else {
        $InstallerVariant = 'd2_windows_exe'
    }
}

$installer = $installerVariants[$InstallerVariant]
if (-not $installer) {
    Write-Status "FAIL: Unknown installer variant: $InstallerVariant" 'Red'
    exit 1
}

# -- Auto-discover D2 GOG installer path ------------------------

if (-not $GogInstallerPath) {
    $repoRoot = Split-Path (Split-Path $PSScriptRoot)
    $candidate = Join-Path $repoRoot "game_data\gog installers\$($installer.FileName)"
    if (Test-Path $candidate) {
        $GogInstallerPath = $candidate
    } else {
        Write-Status "FAIL: $($installer.Label) not found at $candidate. Pass -GogInstallerPath" 'Red'
        exit 1
    }
}

if (-not (Test-Path $GogInstallerPath)) {
    Write-Status "FAIL: Installer not found: $GogInstallerPath" 'Red'
    exit 1
}

# -- Push selected installer to device ---------------------------

Ensure-EmulatorHealthy
Write-Status "Using installer variant: $($installer.Label)"

if (-not $SkipPush) {
    Write-Status 'Pushing installer to device...'
    $localSize = (Get-Item $GogInstallerPath).Length
    $deviceSize = Adb-Timeout -AdbArgs @("shell", "stat -c %s '$($installer.DevicePath)'") -Seconds 5 2>$null
    if ($deviceSize -and $deviceSize -match '^\d+$' -and [long]$deviceSize -eq $localSize) {
        Write-Status 'Installer already on device with correct size, skipping push'
    } else {
        Adb -AdbArgs @('push', $GogInstallerPath, $installer.DevicePath)
        Write-Status 'Push complete'
    }
} else {
    Write-Status 'Skipping push (-SkipPush)'
}

# Verify file is on device
$check = Adb-Timeout -AdbArgs @("shell", "ls -la '$($installer.DevicePath)'") -Seconds 5 2>$null
if (-not $check) {
    Write-Status "FAIL: Installer not found on device at $($installer.DevicePath)" 'Red'
    exit 1
}

# -- Run unified test via run_test.ps1 --------------------------

$runTest = Join-Path (Split-Path $PSScriptRoot) "run_test.ps1"
& $runTest -ScriptName $SCRIPT_NAME -TimeoutSeconds $TimeoutSeconds -Game d2 -Params @{
    INSTALLER_VARIANT = $InstallerVariant
}
exit $LASTEXITCODE
