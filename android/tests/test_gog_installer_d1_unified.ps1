#!/usr/bin/env pwsh
<#
.SYNOPSIS
  D1 GOG installer regression test (unified launcher+game script).

.DESCRIPTION
  Pushes the selected D1 GOG installer variant to the emulator, then invokes
  run_test.ps1 with the unified JSONC script that handles both launcher import
  verification and the in-game D1 level-load checks.

  D1 variants covered by this wrapper:
  - game_data\gog installers\setup_descent_1.4a_(16596).exe
  - game_data\gog installers\descent_enUS_1_0_35122.pkg

  Companion D2 regression still targets:
  - game_data\gog installers\setup_descent_2_1.1_(16596).exe
  - game_data\gog installers\descent_2_enUS_1_0_51877.pkg

  Note: The Mac and PC GOG installers appear to ship the same PC game data.
  The Mac package seems to wrap the DOS build rather than include original
  Mac-specific assets, which keeps the D1 exe/pkg test paths unified.

.PARAMETER GogInstallerPath
  Local path to the selected D1 GOG installer (.exe or .pkg). Defaults to the
  known location for the selected installer variant.

.PARAMETER InstallerVariant
  Which D1 installer variant to run: d1_windows_exe or d1_mac_pkg.

.PARAMETER SkipPush
  Skip pushing the selected installer (assumes it's already on the emulator).

.EXAMPLE
  .\test_gog_installer_d1_unified.ps1
  .\test_gog_installer_d1_unified.ps1 -InstallerVariant d1_mac_pkg
  .\test_gog_installer_d1_unified.ps1 -SkipPush
#>
param(
    [Alias('GogExePath')]
    [string]$GogInstallerPath,
    [ValidateSet('d1_windows_exe', 'd1_mac_pkg')]
    [string]$InstallerVariant,
    [switch]$SkipPush,
    [int]$TimeoutSeconds = 300
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\..\helpers\test_helpers.ps1"

$SCRIPT_NAME = 'test_gog_installer_d1_unified.jsonc'

$installerVariants = @{
    d1_windows_exe = @{
        Label = 'D1 Windows GOG .exe'
        FileName = 'setup_descent_1.4a_(16596).exe'
        DevicePath = '/data/local/tmp/setup_descent_1.4a_(16596).exe'
    }
    d1_mac_pkg = @{
        Label = 'D1 Mac GOG .pkg'
        FileName = 'descent_enUS_1_0_35122.pkg'
        DevicePath = '/data/local/tmp/descent_enUS_1_0_35122.pkg'
    }
}

if (-not $InstallerVariant) {
    if ($GogInstallerPath) {
        $leafName = Split-Path $GogInstallerPath -Leaf
        switch -Regex ($leafName) {
            '^setup_descent_1\.4a_\(16596\)\.exe$' { $InstallerVariant = 'd1_windows_exe'; break }
            '^descent_enUS_1_0_35122\.pkg$' { $InstallerVariant = 'd1_mac_pkg'; break }
            '^setup_descent_2_1\.1_\(16596\)\.exe$' {
                Write-Status 'FAIL: This wrapper only drives the D1 installer regression. Use the D2 wrapper for the D2 exe path.' 'Red'
                exit 1
            }
            '^descent_2_enUS_1_0_51877\.pkg$' {
                Write-Status 'FAIL: This wrapper only drives the D1 installer regression. Use the D2 wrapper for the D2 pkg path.' 'Red'
                exit 1
            }
            '\.pkg$' { $InstallerVariant = 'd1_mac_pkg'; break }
            default { $InstallerVariant = 'd1_windows_exe' }
        }
    } else {
        $InstallerVariant = 'd1_windows_exe'
    }
}

$installer = $installerVariants[$InstallerVariant]
if (-not $installer) {
    Write-Status "FAIL: Unknown installer variant: $InstallerVariant" 'Red'
    exit 1
}

# -- Auto-discover D1 GOG installer path ------------------------

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

$testExitCode = 1
try {
    if (-not (Push-VerifiedDeviceFile -LocalPath $GogInstallerPath -DevicePath $installer.DevicePath -SkipPush:$SkipPush)) {
        exit 1
    }

    # -- Run unified test via run_test.ps1 --------------------------

    $runTest = Join-Path (Join-Path (Split-Path $PSScriptRoot) "helpers") "run_test.ps1"
    & $runTest -ScriptName $SCRIPT_NAME -TimeoutSeconds $TimeoutSeconds -Game d1 -Params @{
        INSTALLER_VARIANT = $InstallerVariant
    }
    $testExitCode = $LASTEXITCODE
} finally {
    if (-not $SkipPush) {
        try { Adb -AdbArgs @('shell', "rm -f '$($installer.DevicePath)'") | Out-Null } catch {}
    }
}
exit $testExitCode
