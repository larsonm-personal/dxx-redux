#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Runs the Android JVM unit test suite via Gradle.

.EXAMPLE
    .\test_gradle_unit_tests.ps1
#>

$ErrorActionPreference = 'Stop'

. "$PSScriptRoot\..\test_env.ps1"

$androidDir = Split-Path $PSScriptRoot
Push-Location $androidDir
try {
    $gradleScript = Resolve-RegressionGradleWrapper -AndroidDir $androidDir
    & $gradleScript :app:testDebugUnitTest --console=plain --no-daemon
    exit $LASTEXITCODE
} finally {
    Pop-Location
}