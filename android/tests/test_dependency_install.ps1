#!/usr/bin/env pwsh
[CmdletBinding()]
param(
    [string]$LinuxPowerShell = $env:DXX_TEST_LINUX_PWSH,
    [string]$WslDistribution = $env:DXX_TEST_WSL_DISTRIBUTION
)

$ErrorActionPreference = 'Stop'
$fixtures = @('test_dependency_temp_files.sh', 'test_jdk_install.sh', 'test_archive_install.sh', 'test_cmakelang_install.sh', 'test_powershell_install.sh', 'test_sdk_provisioning.sh')
if (-not [Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Linux)) {
    # A configured Linux PowerShell lets Windows exercise the actual transaction fixtures
    if ($LinuxPowerShell -and [Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Windows)) {
        $wsl = Get-Command wsl.exe -ErrorAction Stop
        $connection = @()
        if ($WslDistribution) { $connection += @('--distribution', $WslDistribution) }
        $linuxScript = (& $wsl.Source @connection --exec wslpath -u $PSCommandPath | Out-String).Trim()
        if ($LASTEXITCODE -ne 0 -or -not $linuxScript.StartsWith('/')) { throw 'Could not map the installer fixture runner into WSL' }
        $bridge = $linuxScript.Substring(0, $linuxScript.LastIndexOf('/')) + '/dependency_install_wsl.sh'
        & $wsl.Source @connection --exec bash $bridge $LinuxPowerShell $linuxScript @fixtures
        exit $LASTEXITCODE
    }
    Write-Host 'RESULT: SKIP (installer transaction fixtures require Linux and flock; portable dependency checks run separately)'
    exit 2
}
foreach ($test in $fixtures) {
    & bash (Join-Path $PSScriptRoot $test)
    if ($LASTEXITCODE -ne 0) { throw "$test failed with exit code $LASTEXITCODE" }
}
