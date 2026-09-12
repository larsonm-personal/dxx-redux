#!/usr/bin/env pwsh

# Fixed integration owners always run; scenario families rotate once per UTC day.
# Keep names explicit so new tests require a deliberate coverage decision.
function Get-TestSuiteCoveragePolicy {
    return @{
        saved_routes = @(
            'test_guidebot_saved_world'
            'test_guidebot_secret_transition'
        )
        network_scenarios = @(
            'test_lan'
            'test_lan_lobby_discovery'
            'test_coop_launch_feedback'
            'test_coop_save_compatibility'
        )
        audio_preferences = @(
            'test_music_save_source_restore_d2'
            'test_title_music_skip_pref_unified'
        )
        content_browser = @(
            'test_disc_content_import'
            'test_demo_group_file_set_content'
            'test_mod_loading'
            'test_unified_content_mission_picker'
        )
        core = @(
            'test_acoustid_regeneration'
            'test_active_game_data_reset'
            'test_all_extracts'
            'test_android_saveload_dispatch_unified'
            'test_android_sdk_lifecycle'
            'test_bot_client'
            'test_bounded_extraction'
            'test_bounded_python_runtime'
            'test_castaway_level2_restored_switch_route'
            'test_cd_level_metadata_sources'
            'test_cd_mission_hog_isolation'
            'test_cd_regression_runner'
            'test_clean_old_artifacts'
            'test_clean_workspace'
            'test_client_identity_backup'
            'test_controller_compare_unified'
            'test_coop_start_fanout_mapset'
            'test_counterstrike_level2_trigger21_route'
            'test_cue_iso'
            'test_d2xxl_sound_format'
            'test_d2xxl_tga_layout'
            'test_dep_platform'
            'test_double_launch'
            'test_download_verification'
            'test_extract_all_cds_batch'
            'test_extract_all_gog_batch'
            'test_extract_regression_workflow'
            'test_extract_suite_device_preflight'
            'test_extraction_cache_provenance'
            'test_extraction_publication'
            'test_fingerprint_audio_enumeration'
            'test_fingerprint_manifest_publication'
            'test_fingerprint_mission_zip_budgets'
            'test_fingerprint_music_pack_build_guard'
            'test_fingerprint_source_identity'
            'test_fingerprint_threshold'
            'test_fpcalc_and_acoustid'
            'test_game_data_asset_manifest_writer'
            'test_generate_regression_specs'
            'test_get_deps_runtime_updates'
            'test_gog_installer_d1_unified'
            'test_gog_installer_redbook_unified'
            'test_gradle_unit_tests'
            'test_guidebot_publication_batching'
            'test_guidebot_route_regressions'
            'test_guidebot_simulation_headed_headless_parity'
            'test_guidebot_simulation_reporting'
            'test_guidebot_simulation_runner'
            'test_guidebot_simulation_schema'
            'test_guidebot_simulation_timeout_policy'
            'test_hash_assets_force_completeness'
            'test_headless_process_pool'
            'test_host_metadata_worker'
            'test_host_metadata_workspace'
            'test_input_demo_determinism_matrix'
            'test_input_demo_explicit_path'
            'test_input_demo_host_build_guard'
            'test_input_demo_regressions'
            'test_input_demo_rng_trace_compare'
            'test_input_demo_runtime_smoke'
            'test_input_demo_state_trace_compare'
            'test_jsonc_and_tracklist_parsing'
            'test_lan_broadcast'
            'test_launch_to_automap'
            'test_mac_extract_saf'
            'test_metadata_level_headers'
            'test_metadata_parallel_results'
            'test_mission_archive_variants'
            'test_mission_intent_regression_schema'
            'test_mission_level_names'
            'test_mission_metadata_archive_sources'
            'test_mission_metadata_json_normalization'
            'test_mission_metadata_level_statistics'
            'test_mission_metadata_travel_times'
            'test_mission_metadata_trigger_cycles'
            'test_mission_rar_archive'
            'test_mission_zip_batch'
            'test_mission_zip_batch_publication'
            'test_mission_zip_batch_recovery'
            'test_mp'
            'test_music_track_controls_unified'
            'test_native_host_unit_tests'
            'test_obsidian_level1_objective_markers'
            'test_obsidian_level3_blastable_wall'
            'test_obsidian_level4_closed_trigger_source'
            'test_obsidian_level7_exit_route'
            'test_ogl_runtime_texture_options_unified'
            'test_powershell_51_compatibility'
            'test_random_level_preview'
            'test_regenerate_all_regression_data'
            'test_regression_process_lifetime'
            'test_repository_artifact_policy'
            'test_runtime_targeted_sampling'
            'test_saf_archiver'
            'test_saf_redbook'
            'test_secret_area_baseline'
            'test_secret_area_baseline_diff'
            'test_server_integration'
            'test_standard_game_data_resolution'
            'test_test_helpers_process_wait'
            'test_test_process_output_capture'
            'test_test_report_runtimes'
            'test_test_runner_result'
            'test_test_suite_progress'
            'test_trine2_d1_in_d2_custom_textures'
            'test_unified_file_set_content'
            'test_validate_automation_catalog'
            'test_validate_extract_regression_specs'
            'test_vertigo_metadata'
            'test_windows_mission_metadata_route_masks'
            'test_windows_mission_metadata_runner'
            'test_xfing_asset_validation'
        )
        explicit = @(
            'test_dual_emu'
            'test_dual_emu_setup'
            'test_keyboard_manual'
            'test_lan_discovery'
            'test_level_metadata_benchmark'
            'test_manual_lan_coop'
            'test_skip_every_launch_button_manual_unified'
        )
        extended_graphics = @(
            'test_merged_wall_two_pass_probe'
        )
        gameplay_scenarios = @(
            'test_autosave_resume_missing_pilot_unified'
            'test_abort_game_to_main_menu_d2'
            'test_compute_faster_dialog'
            'test_d2_boss_difficulty_save_restore'
            'test_d2_level7_reactor_water_profile'
            'test_death'
            'test_debug_log_refresh_button'
            'test_levelcomplete_touch_skip'
            'test_quick_record_classic_sidecar'
        )
        graphics_scenarios = @(
            'test_newmenu_render_paths_unified'
            'test_boss_health_bar'
            'test_gles3_shim_vbo_arrays'
            'test_merged_wall_snapshot_regression'
            'test_readable_tiny_help_d2'
            'test_resolution_unified'
            'test_secret_reveal_automap_d2'
            'test_texture_index_level_load'
        )
        host_routes = @(
            'test_guided_shot_annotations'
            'test_primary_target_grates'
            'test_route_regeneration_audit'
            'test_vertigo_metadata_checkpoints'
        )
        input_preferences = @(
            'test_axis_mapping'
            'test_autoselect_crash_unified'
            'test_controls_readability_d2'
            'test_engine_prefs_unified'
            'test_intro_skip_inputs_unified'
            'test_pilot_long_hold_delete_unified'
        )
        launcher = @(
            'test_guidebot_simulation_browser'
            'test_launcher_dpad'
            'test_robot_preview'
        )
        metadata_lifecycle = @(
            'test_compressed_music_metadata'
            'test_level_metadata_hxm_worker_reuse'
            'test_level_metadata_interactive_preemption'
            'test_level_metadata_launcher_zip_reusable'
            'test_level_metadata_request_mount_scope'
            'test_level_metadata_result_cache_reuse'
            'test_lunar_series_revamped_metadata_only'
            'test_mission_launch_cache'
            'test_route_metadata_background_priority'
            'test_route_metadata_import_handoff'
            'test_route_metadata_large_level_budget'
            'test_vertigo_level_metadata'
        )
        packaging = @(
            'test_acoustid_config_packaging'
            'test_xcrash_native_report'
        )
        route_guidance = @(
            'test_automap_objective_readiness_progress'
            'test_counterstrike_level20_guidebot_dropped_notification'
            'test_counterstrike_level20_guidebot_post_boss_exit'
            'test_counterstrike_level20_guidebot_trigger3'
            'test_counterstrike_level21_guidebot_pre_reactor_exit'
            'test_counterstrike_level23_guidebot_trigger_liveness'
            'test_counterstrike_level24_guidebot_gold_key_liveness'
            'test_guidebot_recall_to_ship'
            'test_guidebot_unexplored_goal'
            'test_kcxf2_guidebot_route_next'
            'test_obsidian_level13_open_wall_profile'
            'test_obsidian_level4_post_reactor_keys'
            'test_obsidian_level6_guidebot_switch_grate'
            'test_obsidian_level7_switch_guidance'
        )
    }
}

function Select-TestSuiteCoverage {
    param([Parameter(Mandatory)][object[]]$Tests, [int]$Seed, [switch]$AllScenarios)

    $policy = Get-TestSuiteCoveragePolicy
    $families = @{}
    foreach ($family in $policy.Keys) {
        foreach ($name in $policy[$family]) {
            if ($families.ContainsKey($name)) { throw "Duplicate suite coverage entry: $name" }
            $families[$name] = $family
        }
    }
    foreach ($test in $Tests) {
        $name = if ($test.BaseName) { $test.BaseName } else { $test.Name }
        if (-not $families.ContainsKey($name)) { throw "Test needs a suite coverage family: $name" }
    }
    if ($AllScenarios) { return $Tests }
    foreach ($family in @($policy.Keys | Sort-Object)) {
        $candidates = @($Tests | Where-Object {
                $name = if ($_.BaseName) { $_.BaseName } else { $_.Name }
                $families[$name] -eq $family
            } | Sort-Object Name)
        if ($family -in @('core', 'extended_graphics')) { $candidates }
        elseif ($family -ne 'explicit' -and $candidates.Count) { $candidates[$Seed % $candidates.Count] }
    }
}
