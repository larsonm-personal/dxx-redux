#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Runs the Android JVM unit test suite via Gradle.

.EXAMPLE
    .\test_gradle_unit_tests.ps1
#>

$ErrorActionPreference = 'Stop'

. "$PSScriptRoot\..\helpers\test_env.ps1"
. "$PSScriptRoot/../helpers/gradle_test_evidence.ps1"

$androidDir = Split-Path $PSScriptRoot
$gradleScript = Resolve-RegressionGradleWrapper -AndroidDir $androidDir
# Remove prior reports and disable cache restoration for these test tasks, while
# retaining incremental compilation. A successful NO-SOURCE task is not evidence
& $gradleScript -p $androidDir :app:cleanTestDebugUnitTest :mission-metadata-core:cleanTest :proxy-tests:cleanTest :app:testAuthenticationConfig :app:testDebugUnitTest :mission-metadata-core:test :proxy-tests:test --console=plain --no-daemon --no-build-cache
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
foreach ($relativePath in @('app/build/test-results/testDebugUnitTest', 'mission-metadata-core/build/test-results/test', 'proxy-tests/build/test-results/test')) {
    Assert-GradleUnitTestEvidence -ResultsDirectory (Join-Path $androidDir $relativePath)
}
exit 0
