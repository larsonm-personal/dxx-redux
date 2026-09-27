#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
if (-not [Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Linux)) {
    Write-Host 'RESULT: SKIP (installer transaction fixtures require Linux and flock; portable dependency checks run separately)'
    exit 2
}
foreach ($test in @('test_dependency_temp_files.sh', 'test_jdk_install.sh', 'test_archive_install.sh', 'test_cmakelang_install.sh', 'test_powershell_install.sh', 'test_sdk_provisioning.sh')) {
    & bash (Join-Path $PSScriptRoot $test)
    if ($LASTEXITCODE -ne 0) { throw "$test failed with exit code $LASTEXITCODE" }
}
