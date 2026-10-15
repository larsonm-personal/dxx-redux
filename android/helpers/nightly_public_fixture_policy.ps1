#!/usr/bin/env pwsh

# These owners exercise packaged UI or generated document fixtures without game media
# ControllerOverlayChecks uses bundled presets and recording input/music callbacks
# PackageDocumentChecks creates its own DXA; the Published variant is NOT exempt
# Slider checks stay in the launcher; latency uses synthetic loopback peers and save checks
# Mission loading scans an empty-tolerant catalog; its host-launch callback must not run
function Test-SuiteMediaIndependentTest {
    param([Parameter(Mandatory)][object]$Test)

    if ($Test.Name -cne $Test.BaseName) { return $false }
    $allowed = switch -CaseSensitive ("$($Test.Type):$($Test.Requires)") {
        'ps1:emulator' {
            @('test_controller_overlay', 'test_package_documents', 'test_slider_navigation',
                'test_lobby_latency', 'test_host_dialog_loading')
        }
        'jsonc:emulator' {
            @('test_compute_faster_dialog', 'test_debug_log_refresh_button',
                'test_controller_raw_inputs', 'test_controller_trigger_axis_priority')
        }
        default { @() }
    }
    if ($Test.Name -cnotin $allowed) { return $false }
    # JSONC discovery supplies only its computed timeout, which does not change fixtures
    $arguments = @()
    if ($Test -is [System.Collections.IDictionary]) {
        $arguments = @($Test['Arguments'] | Where-Object { $null -ne $_ })
    } elseif ($Test.PSObject.Properties['Arguments']) {
        $arguments = @($Test.Arguments | Where-Object { $null -ne $_ })
    }
    if ($Test.Type -ceq 'jsonc' -and $arguments.Count -eq 2 -and
        $arguments[0] -ceq '-TimeoutSeconds' -and [string]$arguments[1] -cmatch '^[1-9][0-9]{0,4}$') {
        return [int]$arguments[1] -le 86400
    }
    # No caller-supplied variant is admitted, including -Published or future flags
    if ($arguments.Count -gt 0) { return $false }
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
