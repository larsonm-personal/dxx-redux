#include <stdio.h>
#include <string.h>

#include "console.h"
#include "digi.h"
#include "endlevel.h"
#include "game.h"
#include "gauges.h"
#include "hudmsg.h"
#include "input_demo_hooks.h"
#include "input_demo_recorder.h"
#include "laser.h"
#include "newdemo.h"
#include "piggy.h"
#include "player.h"
#include "powerup.h"
#include "sounds.h"
#include "text.h"
#include "weapon.h"
#include "weapon_cheat_shared.h"
#ifdef DXX_BUILD_DESCENT_II
#include "d1_in_d2/d1_in_d2.h"
#endif

/* D1 grants retain D1 ammo units/cap even in the D2 engine; keep in sync with d1/main/powerup.h */
#define D1_GRANT_VULCAN_AMMO_MAX (392 * 2)
#if !defined(DXX_BUILD_DESCENT_II) && D1_GRANT_VULCAN_AMMO_MAX != VULCAN_AMMO_MAX
#error D1 all-weapons grant ammo cap differs from native D1
#endif

static int weapon_cheat_d1_profile(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return d1_in_d2_use_d1_gameplay();
#else
	return 1;
#endif
}

int weapon_cheat_default_grant(void)
{
#ifdef DXX_BUILD_DESCENT_II
	d1_in_d2_asset_stats stats;
	if (!weapon_cheat_d1_profile())
		return INPUT_DEMO_GRANT_ALL_WEAPONS_D2_HONESTBOB;
	d1_in_d2_get_stats(&stats);
	if (stats.robot_pig_size == D1_SHARE_BIG_PIGSIZE ||
	    stats.robot_pig_size == D1_SHARE_10_PIGSIZE ||
	    stats.robot_pig_size == D1_SHARE_PIGSIZE)
		return INPUT_DEMO_GRANT_ALL_WEAPONS_D1_SCOURGE;
#endif
	return INPUT_DEMO_GRANT_ALL_WEAPONS_D1_BIGRED;
}

static int weapon_cheat_error(const char *message, char *error, size_t error_size)
{
	if (error && error_size)
		snprintf(error, error_size, "%s", message);
	return 0;
}

int weapon_cheat_apply(const input_demo_replay_direct_command_event *event,
                       int validate_only, char *error, size_t error_size)
{
	int i, grant, d1_profile = weapon_cheat_d1_profile();
	player *ship;
	if (!event || event->value1 || event->text[0] ||
	    (event->kind != INPUT_DEMO_REPLAY_DIRECT_COMMAND_ENABLE_CHEATS &&
	     event->kind != INPUT_DEMO_REPLAY_DIRECT_COMMAND_GRANT_ALL_WEAPONS))
		return weapon_cheat_error("invalid weapon cheat command", error, error_size);
	grant = event->value0;
	if (event->kind == INPUT_DEMO_REPLAY_DIRECT_COMMAND_ENABLE_CHEATS) {
		if (grant || !d1_profile)
			return weapon_cheat_error("cheat enablement requires the D1 profile", error, error_size);
	} else if (!input_demo_all_weapons_grant_supported(grant) ||
	           ((grant == INPUT_DEMO_GRANT_ALL_WEAPONS_D2_HONESTBOB) == d1_profile)) {
		return weapon_cheat_error("all-weapons grant differs from the active gameplay profile", error, error_size);
	}
	if ((Game_mode & GM_MULTI) || Endlevel_sequence || Player_num < 0 || Player_num >= MAX_PLAYERS)
		return weapon_cheat_error("weapon cheats require active local single-player gameplay", error, error_size);
	if (validate_only)
		return 1;
	ship = &Players[Player_num];
	if (d1_profile && !cheats.enabled)
		HUD_init_message(HM_DEFAULT, TXT_CHEATS_ENABLED);
	cheats.enabled = 1;
	digi_play_sample(SOUND_CHEATER, F1_0);
	ship->score = 0;
	if (!grant)
		return 1;

#ifndef DXX_BUILD_DESCENT_II
	if (grant == INPUT_DEMO_GRANT_ALL_WEAPONS_D1_BIGRED)
		cheats.wowie2 = !cheats.wowie2;
	else
#endif
		cheats.wowie = !cheats.wowie;
	if (grant == INPUT_DEMO_GRANT_ALL_WEAPONS_D1_BIGRED)
		HUD_init_message(HM_DEFAULT, "SUPER %s", TXT_WOWIE_ZOWIE);
	else
		HUD_init_message_literal(HM_DEFAULT, TXT_WOWIE_ZOWIE);

	if (d1_profile) {
		/* Preserve both native D1 variants, including scourge's additive masks */
		const int count = grant == INPUT_DEMO_GRANT_ALL_WEAPONS_D1_SCOURGE ? 3 : 5;
		if (count == 3) {
			ship->primary_weapon_flags |= 0xff ^ (HAS_PLASMA_FLAG | HAS_FUSION_FLAG);
			ship->secondary_weapon_flags |= 0xff ^ (HAS_SMART_FLAG | HAS_MEGA_FLAG);
		} else {
			ship->primary_weapon_flags = 0xff;
			ship->secondary_weapon_flags = 0xff;
		}
		for (i = 0; i < count; ++i) {
			ship->primary_ammo[i] = i == VULCAN_INDEX ? D1_GRANT_VULCAN_AMMO_MAX : Primary_ammo_max[i];
			ship->secondary_ammo[i] = Secondary_ammo_max[i];
		}
	} else {
#ifdef DXX_BUILD_DESCENT_II
		if (Piggy_hamfile_version < 3) {
			ship->primary_weapon_flags = ~((1 << PHOENIX_INDEX) | (1 << OMEGA_INDEX) | (1 << FUSION_INDEX) | HAS_FLAG(SUPER_LASER_INDEX));
			ship->secondary_weapon_flags = ~((1 << SMISSILE4_INDEX) | (1 << MEGA_INDEX) | (1 << SMISSILE5_INDEX));
		} else {
			ship->primary_weapon_flags = 0xffff ^ HAS_FLAG(SUPER_LASER_INDEX);
			ship->secondary_weapon_flags = 0xffff;
		}
		for (i = 0; i < MAX_PRIMARY_WEAPONS; ++i)
			ship->primary_ammo[i] = Primary_ammo_max[i];
		for (i = 0; i < MAX_SECONDARY_WEAPONS; ++i)
			ship->secondary_ammo[i] = Secondary_ammo_max[i];
		if (Piggy_hamfile_version < 3) {
			ship->secondary_ammo[SMISSILE4_INDEX] = 0;
			ship->secondary_ammo[SMISSILE5_INDEX] = 0;
			ship->secondary_ammo[MEGA_INDEX] = 0;
		}
#endif
	}
	if (Newdemo_state == ND_STATE_RECORDING)
		newdemo_record_laser_level(ship->laser_level, MAX_LASER_LEVEL);
	ship->energy = MAX_ENERGY;
	ship->laser_level = MAX_LASER_LEVEL;
#ifdef DXX_BUILD_DESCENT_II
	if (!d1_profile)
		ship->laser_level = MAX_SUPER_LASER_LEVEL;
#endif
	ship->flags |= PLAYER_FLAGS_QUAD_LASERS;
	update_laser_weapon_info();
	return 1;
}

void weapon_cheat_cancel_capture(const char *reason)
{
	if (!input_demo_recorder_is_active())
		return;
	con_printf(CON_NORMAL, "Input demo recording stopped: %s\n", reason);
	HUD_init_message(HM_DEFAULT, "Input recording stopped: %s", reason);
	input_demo_recorder_cancel();
}

void weapon_cheat_execute_live(int grant)
{
	char error[256] = "";
	input_demo_replay_direct_command_event event;
	memset(&event, 0, sizeof(event));
	event.kind = grant ? INPUT_DEMO_REPLAY_DIRECT_COMMAND_GRANT_ALL_WEAPONS : INPUT_DEMO_REPLAY_DIRECT_COMMAND_ENABLE_CHEATS;
	event.value0 = grant;
	if (!weapon_cheat_apply(&event, 1, error, sizeof(error))) {
		weapon_cheat_cancel_capture(error);
		return;
	}
	if (input_demo_recorder_is_active()) {
		if (Player_is_dead || !input_demo_recorder_uses_ordered_input())
			weapon_cheat_cancel_capture("weapon cheat requires living ordered input capture");
		else {
			input_demo_record_input_context();
			if (input_demo_recorder_is_active() &&
			    !(grant ? input_demo_recorder_stage_direct_command_grant_all_weapons(grant, error, sizeof(error)) : input_demo_recorder_stage_direct_command_enable_cheats(error, sizeof(error))))
				weapon_cheat_cancel_capture(error);
		}
	}
	/* Capture failure must not change ordinary gameplay */
	weapon_cheat_apply(&event, 0, error, sizeof(error));
}
