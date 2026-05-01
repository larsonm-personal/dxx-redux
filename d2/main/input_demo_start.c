#include <stdio.h>
#include <string.h>

#include "args.h"
#include "game.h"
#include "gameseq.h"
#include "input_demo_replay.h"
#include "input_demo_rng_mode.h"
#include "mission.h"
#include "newmenu.h"
#include "object.h"
#include "physfsx.h"
#include "playsave.h"
#include "player.h"
#include "state.h"
#include "text.h"
#include "input_demo_start.h"

static unsigned int input_demo_replay_hash_u8_sequence(const ubyte *values, int count)
{
	unsigned int hash = 2166136261u;
	int i;

	for (i = 0; i < count; i++) {
		hash ^= values[i];
		hash *= 16777619u;
	}
	return hash;
}

static void input_demo_apply_replay_player_cfg(const input_demo_player_cfg *player_cfg)
{
	if (!player_cfg)
		return;
	PlayerCfg.AutoLeveling = player_cfg->auto_leveling;
	PlayerCfg.PersistentDebris = player_cfg->persistent_debris;
	if (player_cfg->has_headlight_active_default)
		PlayerCfg.HeadlightActiveDefault = player_cfg->headlight_active_default;
	PlayerCfg.NoFireAutoselect = player_cfg->no_fire_autoselect;
	PlayerCfg.CycleAutoselectOnly = player_cfg->cycle_autoselect_only;
	PlayerCfg.SelectAfterFire = player_cfg->select_after_fire;
	PlayerCfg.ClassicAutoselectWeapon = player_cfg->classic_autoselect_weapon;
	memcpy(PlayerCfg.PrimaryOrder, player_cfg->primary_order, MAX_PRIMARY_WEAPONS + 1);
	memcpy(PlayerCfg.SecondaryOrder, player_cfg->secondary_order, MAX_SECONDARY_WEAPONS + 1);
}

int input_demo_load_replay_from_path(const char *demo_path, char *error, size_t error_size)
{
	int engine_mode;
	int demo_mode;
	const char *validation_error;

	if (!demo_path || !demo_path[0]) {
		snprintf(error, error_size, "%s", "missing demo path");
		return 0;
	}
	engine_mode = d_rand_get_replay_mode();
	validation_error = input_demo_rng_mode_validate_metadata_file(demo_path, engine_mode,
		&demo_mode);
	if (validation_error) {
		snprintf(error, error_size, "%s", validation_error);
		return 0;
	}
	if (!input_demo_replay_load(demo_path, error, error_size))
		return 0;
	if (input_demo_replay_game() != INPUT_DEMO_GAME_D2)
	{
		input_demo_replay_unload();
		snprintf(error, error_size, "%s", "Input demo replay currently supports D2 demos only");
		return 0;
	}
	return 1;
}

int input_demo_start_loaded_replay(void)
{
	char replay_error[256] = "";
	char mission_name[PATH_MAX] = "";
	char local_checkpoint_name[PATH_MAX] = "";
	const char *start_mode;
	const char *checkpoint_name;
	const uint8_t *checkpoint_data;
	size_t checkpoint_size;
	PHYSFS_file *checkpoint_file = NULL;
	const char *checkpoint_base_name;
	input_demo_player_cfg replay_player_cfg;
	char local_player_callsign[CALLSIGN_LEN + 1] = "";
	int have_replay_player_cfg = 0;

	if (!input_demo_replay_is_loaded())
	{
		printf("Input demo replay is not loaded\n");
		return 1;
	}
	if (Player_num >= 0 && Player_num < MAX_PLAYERS)
	{
		strncpy(local_player_callsign, Players[Player_num].callsign, CALLSIGN_LEN);
		local_player_callsign[CALLSIGN_LEN] = '\0';
	}
	have_replay_player_cfg = input_demo_replay_get_player_cfg(&replay_player_cfg);
	start_mode = input_demo_replay_start_mode();
	if (!start_mode)
	{
		printf("Input demo replay metadata is missing start_mode\n");
		input_demo_replay_unload();
		return 1;
	}
	if (!input_demo_replay_mission())
	{
		printf("Input demo replay metadata is missing mission\n");
		input_demo_replay_unload();
		return 1;
	}
	snprintf(mission_name, sizeof(mission_name), "%s", input_demo_replay_mission());
	if (!strcmp(start_mode, "new_level")) {
		if (!load_mission_by_name(mission_name))
		{
			printf("Input demo replay could not load mission: %s\n", mission_name);
			input_demo_replay_unload();
			return 1;
		}
		Difficulty_level = input_demo_replay_difficulty();
		if (have_replay_player_cfg)
			input_demo_apply_replay_player_cfg(&replay_player_cfg);
		printf("Input demo replay starting: %s level %d, %u frames\n",
			mission_name, input_demo_replay_level(), input_demo_replay_frame_count());
		input_demo_set_skip_level_intro(1);
		StartNewGame(input_demo_replay_level());
		return 0;
	}
	if (strcmp(start_mode, "save_checkpoint") != 0)
	{
		printf("Input demo replay start_mode not supported: %s\n", start_mode);
		input_demo_replay_unload();
		return 1;
	}
	checkpoint_name = input_demo_replay_checkpoint_save_name();
	checkpoint_data = input_demo_replay_checkpoint_data();
	checkpoint_size = input_demo_replay_checkpoint_size();
	if (!input_demo_replay_has_checkpoint() || !checkpoint_name || !checkpoint_name[0] || !checkpoint_data || !checkpoint_size)
	{
		printf("Input demo replay is missing checkpoint data\n");
		input_demo_replay_unload();
		return 1;
	}
	checkpoint_base_name = strrchr(checkpoint_name, '/');
	if (checkpoint_base_name)
		checkpoint_base_name++;
	else
		checkpoint_base_name = checkpoint_name;
	if (GameArg.SysUsePlayersDir)
		snprintf(local_checkpoint_name, sizeof(local_checkpoint_name), "Players/%s", checkpoint_base_name);
	else
		snprintf(local_checkpoint_name, sizeof(local_checkpoint_name), "%s", checkpoint_base_name);
	printf("Input demo replay checkpoint temp path: recorded=%s local=%s\n",
		checkpoint_name, local_checkpoint_name);
	if (!strncmp(local_checkpoint_name, "Players/", 8))
		PHYSFS_mkdir("Players");
	PHYSFS_delete(local_checkpoint_name);
	checkpoint_file = PHYSFS_openWrite(local_checkpoint_name);
	if (!checkpoint_file)
	{
		printf("Input demo replay could not write checkpoint file: %s\n", local_checkpoint_name);
		input_demo_replay_unload();
		return 1;
	}
	if (PHYSFS_writeBytes(checkpoint_file, checkpoint_data, checkpoint_size) != (PHYSFS_sint64) checkpoint_size)
	{
		PHYSFS_close(checkpoint_file);
		PHYSFS_delete(local_checkpoint_name);
		printf("Input demo replay could not write checkpoint bytes: %s\n", local_checkpoint_name);
		input_demo_replay_unload();
		return 1;
	}
	PHYSFS_close(checkpoint_file);
	checkpoint_file = NULL;
	if (!state_restore_all_sub(local_checkpoint_name, 0))
	{
		PHYSFS_delete(local_checkpoint_name);
		printf("Input demo replay could not restore checkpoint: %s\n", local_checkpoint_name);
		input_demo_replay_unload();
		return 1;
	}
	PHYSFS_delete(local_checkpoint_name);
	{
		int player_cfg_result;
		int replay_auto_level = -1;
		const char *replay_callsign;
		unsigned int primary_order_hash;
		unsigned int secondary_order_hash;
		fix player_mass = 0, player_drag = 0, player_brakes = 0;
		unsigned int player_phys_flags = 0;
		fix ship_mass = 0, ship_drag = 0, ship_brakes = 0;
		fix ship_max_thrust = 0, ship_max_rotthrust = 0, ship_wiggle = 0;

		if (ConsoleObject)
			replay_auto_level = (ConsoleObject->mtype.phys_info.flags & PF_LEVELLING) ? 1 : 0;
		if (!Players[Player_num].callsign[0] && local_player_callsign[0])
		{
			strncpy(Players[Player_num].callsign, local_player_callsign, CALLSIGN_LEN);
			Players[Player_num].callsign[CALLSIGN_LEN] = '\0';
		}
		if (Players[Player_num].callsign[0])
		{
			new_player_config();
			player_cfg_result = read_player_file();
		}
		else
			player_cfg_result = -1;
		if (have_replay_player_cfg)
			input_demo_apply_replay_player_cfg(&replay_player_cfg);
		else if (replay_auto_level >= 0)
			PlayerCfg.AutoLeveling = replay_auto_level;
		primary_order_hash = input_demo_replay_hash_u8_sequence(PlayerCfg.PrimaryOrder, MAX_PRIMARY_WEAPONS + 1);
		secondary_order_hash = input_demo_replay_hash_u8_sequence(PlayerCfg.SecondaryOrder, MAX_SECONDARY_WEAPONS + 1);
		replay_callsign = Players[Player_num].callsign[0] ? Players[Player_num].callsign : "<empty>";
		if (ConsoleObject) {
			player_mass = ConsoleObject->mtype.phys_info.mass;
			player_drag = ConsoleObject->mtype.phys_info.drag;
			player_brakes = ConsoleObject->mtype.phys_info.brakes;
			player_phys_flags = ConsoleObject->mtype.phys_info.flags;
		}
		if (Player_ship) {
			ship_mass = Player_ship->mass;
			ship_drag = Player_ship->drag;
			ship_brakes = Player_ship->brakes;
			ship_max_thrust = Player_ship->max_thrust;
			ship_max_rotthrust = Player_ship->max_rotthrust;
			ship_wiggle = Player_ship->wiggle;
		}
		con_printf(CON_NORMAL, "Input demo replay player config: callsign=%s result=%d auto_level=%d debris=%d headlight_default=%d autoselect=(nofire=%d,after=%d,cycle=%d,classic=%d) order_hash=(0x%x,0x%x) player_flags=0x%x phys=(%d,%d,%d,0x%x) ship=(%d,%d,%d,%d,%d,%d)\n",
			replay_callsign, player_cfg_result, PlayerCfg.AutoLeveling,
			PlayerCfg.PersistentDebris, PlayerCfg.HeadlightActiveDefault,
			PlayerCfg.NoFireAutoselect, PlayerCfg.SelectAfterFire,
			PlayerCfg.CycleAutoselectOnly, PlayerCfg.ClassicAutoselectWeapon,
			primary_order_hash, secondary_order_hash,
			Players[Player_num].flags,
			player_mass, player_drag, player_brakes, player_phys_flags,
			ship_mass, ship_drag, ship_brakes, ship_max_thrust, ship_max_rotthrust, ship_wiggle);
	}
	if (d_stricmp(Current_mission_filename, mission_name) || Current_level_num != input_demo_replay_level() ||
		Difficulty_level != input_demo_replay_difficulty())
	{
		printf("Input demo replay checkpoint restore mismatch: mission=%s level=%d difficulty=%d\n",
			Current_mission_filename, Current_level_num, Difficulty_level);
		input_demo_replay_unload();
		return 1;
	}
	printf("Input demo replay starting: %s level %d, %u frames\n",
		mission_name, input_demo_replay_level(), input_demo_replay_frame_count());
	return 0;
}