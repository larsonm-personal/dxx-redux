#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Runs the Android JVM unit test suite via Gradle.

.EXAMPLE
    .\test_gradle_unit_tests.ps1
#>

$ErrorActionPreference = 'Stop'

. "$PSScriptRoot\..\helpers\test_env.ps1"

$androidDir = Split-Path $PSScriptRoot
$gradleScript = Resolve-RegressionGradleWrapper -AndroidDir $androidDir
& $gradleScript -p $androidDir :app:testDebugUnitTest --console=plain --no-daemon
exit $LASTEXITCODE
