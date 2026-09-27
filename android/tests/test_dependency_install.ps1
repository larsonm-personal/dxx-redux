#!/usr/bin/env pwsh
$ErrorActionPreference = 'Stop'
foreach ($test in @('test_dependency_temp_files.sh', 'test_jdk_install.sh', 'test_archive_install.sh', 'test_cmakelang_install.sh', 'test_powershell_install.sh', 'test_sdk_provisioning.sh')) {
    & bash (Join-Path $PSScriptRoot $test)
    if ($LASTEXITCODE -ne 0) { throw "$test failed with exit code $LASTEXITCODE" }
}
