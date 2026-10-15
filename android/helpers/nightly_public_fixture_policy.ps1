#!/usr/bin/env pwsh

# These owners exercise packaged UI or generated document fixtures without game media
# ControllerOverlayChecks uses bundled presets and recording input/music callbacks
# PackageDocumentChecks creates its own DXA; the Published variant is NOT exempt
function Test-SuiteMediaIndependentTest {
    param([Parameter(Mandatory)][object]$Test)

    if ($Test.Type -cne 'ps1' -or $Test.Requires -cne 'emulator') { return $false }
    if ($Test.Name -cne $Test.BaseName) { return $false }
    if ($Test.Name -cnotin @('test_controller_overlay', 'test_package_documents')) { return $false }
    # No caller-supplied variant is admitted, including -Published or future flags
    if ($null -ne $Test.Arguments -and @($Test.Arguments).Count -gt 0) { return $false }
    return $true
}

function Test-SuiteNeedsStandardGameData {
    param([AllowEmptyCollection()][object[]]$Tests = @())

    foreach ($test in $Tests) {
        if ($test.Requires -in @('emulator', 'two_emulators') -and
            -not (Test-SuiteMediaIndependentTest -Test $test)) { return $true }
    }
    return $false
}
