#!/usr/bin/env pwsh

# These owners need no standard retail game set; explicit owners stage their own public/generated fixtures
# ControllerOverlayChecks uses bundled presets and recording input/music callbacks
# PackageDocumentChecks creates its own DXA; the Published variant is NOT exempt
# Slider checks stay in the launcher; latency uses synthetic loopback peers and save checks
# Mission loading scans an empty-tolerant catalog; its host-launch callback must not run
function Test-SuiteMediaIndependentTest {
    param([Parameter(Mandatory)][object]$Test)

    $arguments = @()
    if ($Test -is [System.Collections.IDictionary]) {
        $arguments = @($Test['Arguments'] | Where-Object { $null -ne $_ })
    } elseif ($Test.PSObject.Properties['Arguments']) {
        $arguments = @($Test.Arguments | Where-Object { $null -ne $_ })
    }
    if ($Test.Name -ceq 'test_launcher_media_controls_generated' -and
        $Test.BaseName -ceq 'test_launcher_media_controls' -and
        $Test.Type -ceq 'ps1' -and $Test.Requires -ceq 'emulator') {
        return $arguments.Count -eq 1 -and $arguments[0] -ceq '-GeneratedAudio'
    }
    if ($Test.Name -ceq 'test_lan_launch_preparation_public_shareware' -and
        $Test.BaseName -ceq 'test_lan_launch_preparation' -and $Test.Type -ceq 'ps1' -and
        $Test.Requires -ceq 'two_emulators') {
        return $arguments.Count -eq 1 -and $arguments[0] -ceq '-PublicShareware'
    }
    if ($Test.Name -ceq 'test_demo_import_public_first_play' -and
        $Test.BaseName -ceq 'test_demo_import' -and $Test.Type -ceq 'ps1' -and
        $Test.Requires -ceq 'emulator') {
        return $arguments.Count -eq 1 -and $arguments[0] -ceq '-PublicFirstPlay'
    }
    if ($Test.Name -cne $Test.BaseName) { return $false }
    $allowed = switch -CaseSensitive ("$($Test.Type):$($Test.Requires)") {
        'ps1:emulator' {
            @('test_controller_overlay', 'test_package_documents', 'test_slider_navigation',
                'test_lobby_latency', 'test_host_dialog_loading',
                'test_android_automap_route_policy',
                'test_android_cd_preview_completion',
                'test_android_config_policy',
                'test_android_merge_cache',
                'test_android_merged_wrap',
                'test_android_music_completion',
                'test_android_ogl_batches',
                'test_android_redbook_completion',
                'test_android_texture_bindings',
                'test_android_texture_labels',
                'test_android_xmodel_mipmaps',
                'test_pcm_ring',
                'test_touch_layout_format',
                'test_distribution_build_info',
                'test_xcrash_native_report',
                'test_lan_transport_recovery')
        }
        'ps1:two_emulators' { @('test_lan_lobby_discovery', 'test_lan_broadcast') }
        'jsonc:emulator' {
            @('test_public_first_play_empty', 'test_public_first_play_d1',
                'test_public_first_play_d1_in_d2', 'test_public_first_play_d2', 'test_public_first_play_mission',
                'test_compute_faster_dialog', 'test_debug_log_refresh_button',
                'test_controller_raw_inputs', 'test_controller_trigger_axis_priority',
                'test_launcher_media_controls',
                'test_demo_group_file_set_content',
                'test_unified_file_set_content',
                'test_soundfont_download_catalog')
        }
        default { @() }
    }
    if ($Test.Name -cnotin $allowed) { return $false }
    # JSONC discovery supplies only its computed timeout, which does not change fixtures
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
