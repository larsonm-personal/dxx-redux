# Physical route scenarios: one build, explicit canaries, rotating mission families
function Get-GuidebotRouteRegressionCases {
    @(
        [pscustomobject]@{ File = 'test_bitesize_blastable_grate.ps1'; Family = 'bitesize'; Always = $false }
        [pscustomobject]@{ File = 'test_bitesize_crossed_triggers.ps1'; Family = 'bitesize'; Always = $false }
        [pscustomobject]@{ File = 'test_bitesize_reverse_door.ps1'; Family = 'bitesize'; Always = $false }
        [pscustomobject]@{ File = 'test_castaway_alternative_exit.ps1'; Family = 'castaway'; Always = $false }
        [pscustomobject]@{ File = 'test_castaway_level1_cage_route.ps1'; Family = 'castaway'; Always = $false }
        [pscustomobject]@{ File = 'test_castaway_level3_access_recovery.ps1'; Family = 'castaway'; Always = $false }
        [pscustomobject]@{ File = 'test_castaway_level5_switch_dependencies.ps1'; Family = 'castaway'; Always = $false }
        [pscustomobject]@{ File = 'test_castaway_level6_path_closure.ps1'; Family = 'castaway'; Always = $false }
        [pscustomobject]@{ File = 'test_castaway_level6_trigger_route.ps1'; Family = 'castaway'; Always = $false }
        [pscustomobject]@{ File = 'test_castaway_level7_trigger_dependencies.ps1'; Family = 'castaway'; Always = $true }
        [pscustomobject]@{ File = 'test_castaway_level8_keyed_trigger_route.ps1'; Family = 'castaway'; Always = $false }
        [pscustomobject]@{ File = 'test_castaway_level9_second_boss_route.ps1'; Family = 'castaway'; Always = $false }
        [pscustomobject]@{ File = 'test_counterstrike_implicit_trigger_recovery.ps1'; Family = 'counterstrike'; Always = $false }
        [pscustomobject]@{ File = 'test_counterstrike_keyed_access_recovery.ps1'; Family = 'counterstrike'; Always = $false }
        [pscustomobject]@{ File = 'test_counterstrike_level10_key_carriers.ps1'; Family = 'counterstrike'; Always = $false }
        [pscustomobject]@{ File = 'test_counterstrike_level11_route_waypoints.ps1'; Family = 'counterstrike'; Always = $false }
        [pscustomobject]@{ File = 'test_counterstrike_level12_asymmetric_return_door.ps1'; Family = 'counterstrike'; Always = $false }
        [pscustomobject]@{ File = 'test_counterstrike_level17_carried_key_pickup.ps1'; Family = 'counterstrike'; Always = $false }
        [pscustomobject]@{ File = 'test_counterstrike_level1_route_confirmation.ps1'; Family = 'counterstrike'; Always = $false }
        [pscustomobject]@{ File = 'test_counterstrike_level20_trigger_door_replan.ps1'; Family = 'counterstrike'; Always = $false }
        [pscustomobject]@{ File = 'test_counterstrike_level23_route_progress.ps1'; Family = 'counterstrike'; Always = $false }
        [pscustomobject]@{ File = 'test_counterstrike_level24_final_boss_route.ps1'; Family = 'counterstrike'; Always = $false }
        [pscustomobject]@{ File = 'test_counterstrike_level2_open_locked_route.ps1'; Family = 'counterstrike'; Always = $false }
        [pscustomobject]@{ File = 'test_counterstrike_level6_blue_door_route.ps1'; Family = 'counterstrike'; Always = $false }
        [pscustomobject]@{ File = 'test_crossfire_zero_countdown.ps1'; Family = 'other_archives'; Always = $false }
        [pscustomobject]@{ File = 'test_diehard_countdown_switch.ps1'; Family = 'diehard'; Always = $false }
        [pscustomobject]@{ File = 'test_diehard_directional_frontier.ps1'; Family = 'diehard'; Always = $false }
        [pscustomobject]@{ File = 'test_diehard_exit_prerequisite.ps1'; Family = 'diehard'; Always = $false }
        [pscustomobject]@{ File = 'test_diehard_keyless_frontier.ps1'; Family = 'diehard'; Always = $false }
        [pscustomobject]@{ File = 'test_diehard_native_key_pickup.ps1'; Family = 'diehard'; Always = $false }
        [pscustomobject]@{ File = 'test_diehard_opening_frontier.ps1'; Family = 'diehard'; Always = $false }
        [pscustomobject]@{ File = 'test_diehard_player_assisted_door.ps1'; Family = 'diehard'; Always = $false }
        [pscustomobject]@{ File = 'test_eaf2_reactor_access.ps1'; Family = 'eaf'; Always = $false }
        [pscustomobject]@{ File = 'test_eaf_guided_launch_prerequisites.ps1'; Family = 'eaf'; Always = $false }
        [pscustomobject]@{ File = 'test_entropy2_level4_reactor_links.ps1'; Family = 'entropy'; Always = $false }
        [pscustomobject]@{ File = 'test_entropy2_level5_key_carrier.ps1'; Family = 'entropy'; Always = $false }
        [pscustomobject]@{ File = 'test_ffyl_closed_path_door.ps1'; Family = 'ffyl'; Always = $false }
        [pscustomobject]@{ File = 'test_ffyl_fleeing_guidebot.ps1'; Family = 'ffyl'; Always = $false }
        [pscustomobject]@{ File = 'test_ffyl_forcefield_door.ps1'; Family = 'ffyl'; Always = $false }
        [pscustomobject]@{ File = 'test_guidebot_firststrike_live_objects.ps1'; Family = 'guidebot'; Always = $false }
        [pscustomobject]@{ File = 'test_guidebot_firststrike_long_path.ps1'; Family = 'guidebot'; Always = $true }
        [pscustomobject]@{ File = 'test_guidebot_key_checkpoint.ps1'; Family = 'guidebot'; Always = $false }
        [pscustomobject]@{ File = 'test_guidebot_live_key_pickup.ps1'; Family = 'guidebot'; Always = $true }
        [pscustomobject]@{ File = 'test_guidebot_player_defaults.ps1'; Family = 'guidebot'; Always = $true }
        [pscustomobject]@{ File = 'test_guidebot_precision_recovery.ps1'; Family = 'guidebot'; Always = $true }
        [pscustomobject]@{ File = 'test_guidebot_reactor_lifetime.ps1'; Family = 'guidebot'; Always = $true }
        [pscustomobject]@{ File = 'test_guidebot_successive_frontiers.ps1'; Family = 'guidebot'; Always = $false }
        [pscustomobject]@{ File = 'test_guidebot_waypoint_clearance.ps1'; Family = 'guidebot'; Always = $false }
        [pscustomobject]@{ File = 'test_lostlvls_directional_unlock.ps1'; Family = 'lostlvls'; Always = $false }
        [pscustomobject]@{ File = 'test_lostlvls_door_recess.ps1'; Family = 'lostlvls'; Always = $false }
        [pscustomobject]@{ File = 'test_lostlvls_key_contact.ps1'; Family = 'lostlvls'; Always = $false }
        [pscustomobject]@{ File = 'test_lostlvls_level22_portal.ps1'; Family = 'lostlvls'; Always = $false }
        [pscustomobject]@{ File = 'test_lostlvls_remote_door.ps1'; Family = 'lostlvls'; Always = $false }
        [pscustomobject]@{ File = 'test_mandrill_guidebot_clearance.ps1'; Family = 'other_archives'; Always = $false }
        [pscustomobject]@{ File = 'test_maximum_restoring_wall.ps1'; Family = 'other_archives'; Always = $false }
        [pscustomobject]@{ File = 'test_obsidian_level10_firing_dependencies.ps1'; Family = 'obsidian'; Always = $false }
        [pscustomobject]@{ File = 'test_obsidian_level11_door_route.ps1'; Family = 'obsidian'; Always = $false }
        [pscustomobject]@{ File = 'test_obsidian_level12_switch_approach.ps1'; Family = 'obsidian'; Always = $false }
        [pscustomobject]@{ File = 'test_obsidian_level13_remote_door_route.ps1'; Family = 'obsidian'; Always = $false }
        [pscustomobject]@{ File = 'test_obsidian_level1_route_confirmation.ps1'; Family = 'obsidian'; Always = $false }
        [pscustomobject]@{ File = 'test_obsidian_level5_partial_frontier.ps1'; Family = 'obsidian'; Always = $false }
        [pscustomobject]@{ File = 'test_obsidian_level9_frontier_route.ps1'; Family = 'obsidian'; Always = $false }
        [pscustomobject]@{ File = 'test_obsidian_level9_motion_tolerance.ps1'; Family = 'obsidian'; Always = $false }
        [pscustomobject]@{ File = 'test_obsidian_recovery_collision.ps1'; Family = 'obsidian'; Always = $false }
        [pscustomobject]@{ File = 'test_plutonia_avoidance_endpoint.ps1'; Family = 'plutonia'; Always = $false }
        [pscustomobject]@{ File = 'test_plutonia_level11_boss_grate.ps1'; Family = 'plutonia'; Always = $false }
        [pscustomobject]@{ File = 'test_plutonia_level5_reactor_grate.ps1'; Family = 'plutonia'; Always = $false }
        [pscustomobject]@{ File = 'test_plutonia_narrow_funnel.ps1'; Family = 'plutonia'; Always = $false }
        [pscustomobject]@{ File = 'test_saturn_level15_simulation.ps1'; Family = 'other_archives'; Always = $true }
        [pscustomobject]@{ File = 'test_tew_hidden_door_approach.ps1'; Family = 'tew'; Always = $false }
        [pscustomobject]@{ File = 'test_tew_level13_corner_recovery.ps1'; Family = 'tew'; Always = $false }
        [pscustomobject]@{ File = 'test_tew_level15_planning_budget.ps1'; Family = 'tew'; Always = $false }
        [pscustomobject]@{ File = 'test_tew_level1_adjacent_switch.ps1'; Family = 'tew'; Always = $false }
        [pscustomobject]@{ File = 'test_tew_level20_countdown_door.ps1'; Family = 'tew'; Always = $false }
        [pscustomobject]@{ File = 'test_tew_level23_hidden_keyed_door.ps1'; Family = 'tew'; Always = $false }
        [pscustomobject]@{ File = 'test_tew_level26_timed_switches.ps1'; Family = 'tew'; Always = $false }
        [pscustomobject]@{ File = 'test_tew_secret3_trigger_door.ps1'; Family = 'tew'; Always = $false }
        [pscustomobject]@{ File = 'test_vertigo_level11_door_contact.ps1'; Family = 'vertigo'; Always = $false }
        [pscustomobject]@{ File = 'test_vertigo_level16_narrow_portal.ps1'; Family = 'vertigo'; Always = $false }
        [pscustomobject]@{ File = 'test_vertigo_level6_blue_door_route.ps1'; Family = 'vertigo'; Always = $false }
        [pscustomobject]@{ File = 'test_vertigo_secret3_switch_prerequisites.ps1'; Family = 'vertigo'; Always = $false }
        [pscustomobject]@{ File = 'test_vignettes_level22_frontier.ps1'; Family = 'vignettes'; Always = $false }
        [pscustomobject]@{ File = 'test_vignettes_level8_firing_position.ps1'; Family = 'vignettes'; Always = $false }
    )
}

function Select-GuidebotRouteRegressionCases {
    param(
        [Parameter(Mandatory)][object[]]$Cases,
        [ValidateRange(0, [int]::MaxValue)][int]$Seed,
        [switch]$AllCases
    )
    if ($AllCases) { return $Cases }
    $selected = @($Cases | Where-Object Always)
    foreach ($family in @($Cases | Where-Object { -not $_.Always } | Group-Object Family)) {
        $ordered = @($family.Group | Sort-Object File)
        $selected += $ordered[$Seed % $ordered.Count]
    }
    return $selected | Sort-Object File
}
