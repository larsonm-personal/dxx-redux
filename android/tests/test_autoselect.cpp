#include <cstdio>
#include <cstdlib>
#include <cstring>

#define DXX_REWIND_FILE_WRAPPER 1
extern "C" {
#include "game.h"
#include "kconfig.h"
#include "gameseq.h"
#include "newdemo.h"
#include "object.h"
#include "physfsx.h"
#include "playsave.h"
#include "text.h"
#include "weapon.h"
#include "rewind_file.h"
#undef PHYSFS_file
#undef PHYSFS_read
#undef PHYSFS_write
#define PHYSFS_file  rewind_file
#define PHYSFS_read  rewind_file_read
#define PHYSFS_write rewind_file_write
#include "autoselect_runtime.h"
#undef PHYSFS_file
#undef PHYSFS_read
#undef PHYSFS_write
void init_player_stats_new_ship(ubyte pnum);
#ifdef DXX_BUILD_DESCENT_II
int read_player_d2x(char *filename);
int write_player_d2x(char *filename);
#else
int read_player_d1x(char *filename);
int write_player_d1x(char *filename);
#endif
}

static void require(bool condition, const char *message)
{
	if (!condition) {
		std::fprintf(stderr, "Autoselect integration failure: %s\n", message);
		std::exit(1);
	}
}

static void new_ship(int enabled = 1)
{
	new_player_config();
	PlayerCfg.AutoselectOnlyOnce = enabled;
	std::memset(&Controls, 0, sizeof(Controls));
	reset_auto_select();
	init_player_stats_new_ship(Player_num);
}

static void test_runtime_roundtrip(bool memory)
{
	new_ship();
	Controls.fire_primary_state = Controls.fire_secondary_state = 1;
	require(pick_up_primary(SPREADFIRE_INDEX) && pick_up_secondary(HOMING_INDEX, 1), "pickups while firing");
	require(Players[0].primary_weapon == LASER_INDEX && Players[0].secondary_weapon == CONCUSSION_INDEX, "switches wait for release");
	rewind_memory_buffer buffer = {};
	rewind_file file;
	if (memory)
		rewind_file_init_memory_write(&file, &buffer);
	else
		rewind_file_init_physfs(&file, PHYSFS_openWrite("autoselect-runtime.bin"));
	autoselect_write_runtime_state(&file);
	require(rewind_file_close(&file), "write runtime state");
	PrimaryWeaponPickedUp = SecondaryWeaponPickedUp = 0;
	reset_auto_select();
	if (memory)
		rewind_file_init_memory_read(&file, buffer.data, buffer.size);
	else
		rewind_file_init_physfs(&file, PHYSFS_openRead("autoselect-runtime.bin"));
	require(autoselect_read_runtime_state(&file, 0, 0), "validate without changing the current ship (secret return)");
	require(!PrimaryWeaponPickedUp && !SecondaryWeaponPickedUp && delayed_primary_autoselect_weapon_index == -1, "validation preserves current ship");
	require(rewind_file_seek(&file, 0), "rewind runtime record");
	require(autoselect_read_runtime_state(&file, 0, 1), "restore runtime state");
	require(rewind_file_close(&file), "close runtime state");
	require(PrimaryWeaponPickedUp && SecondaryWeaponPickedUp, "restore consumed first pickups");
	require(pick_up_primary(PLASMA_INDEX) && pick_up_secondary(SMART_INDEX, 1), "collect later weapons while still firing");
	Controls.fire_primary_state = Controls.fire_secondary_state = 0;
	delayed_autoselect();
	require(Players[0].primary_weapon == SPREADFIRE_INDEX && Players[0].secondary_weapon == HOMING_INDEX, "restore queued first switches without replacing them with later pickups");
	rewind_memory_buffer_discard(&buffer);

	int swapped[4] = { 0, static_cast<int>(SWAPINT(1)), -1, -1 };
	rewind_file_init_memory_read(&file, reinterpret_cast<unsigned char *>(swapped), sizeof(swapped));
	require(autoselect_read_runtime_state(&file, 1, 1) && !PrimaryWeaponPickedUp && SecondaryWeaponPickedUp, "swapped save runtime state");
	rewind_file_init_memory_read(&file, reinterpret_cast<unsigned char *>(swapped), sizeof(swapped) - 1);
	require(!autoselect_read_runtime_state(&file, 1, 1) && !PrimaryWeaponPickedUp && SecondaryWeaponPickedUp, "reject truncated state without applying it");
	swapped[2] = SWAPINT(1000);
	rewind_file_init_memory_read(&file, reinterpret_cast<unsigned char *>(swapped), sizeof(swapped));
	require(!autoselect_read_runtime_state(&file, 1, 1), "reject invalid queued weapon");
}

void test_autoselect()
{
	std::fprintf(stderr, "Testing Autoselect Only Once pickups and persistence\n");
	Player_num = 0;
	Game_mode = 0;
	Newdemo_state = ND_STATE_NORMAL;
	ConsoleObject = &Objects[0];
	ConsoleObject->type = OBJ_PLAYER;
	Players[0].objnum = 0;
	for (int i = 0; i < N_TEXT_STRINGS; ++i)
		Text_string[i] = const_cast<char *>("weapon");
	new_player_config();
	require(!PlayerCfg.AutoselectOnlyOnce, "new pilots default to normal autoselection");
	for (int enabled = 0; enabled <= 1; ++enabled) {
		new_ship(enabled);
		require(!PrimaryWeaponPickedUp && !SecondaryWeaponPickedUp, "new ship resets both categories");
		require(pick_up_primary(SPREADFIRE_INDEX) && Players[0].primary_weapon == SPREADFIRE_INDEX, "first primary switches");
		require(!SecondaryWeaponPickedUp, "primary pickup leaves secondary allowance alone");
		require(pick_up_primary(PLASMA_INDEX), "later primary still enters inventory");
		require(Players[0].primary_weapon == (enabled ? SPREADFIRE_INDEX : PLASMA_INDEX), "later primary respects setting");
		require(pick_up_secondary(HOMING_INDEX, 1) && Players[0].secondary_weapon == HOMING_INDEX, "first secondary switches independently");
		require(pick_up_secondary(SMART_INDEX, 1), "later secondary still enters inventory");
		require(Players[0].secondary_weapon == (enabled ? HOMING_INDEX : SMART_INDEX), "later secondary respects setting");
	}
	Players[0].secondary_ammo[Players[0].secondary_weapon] = 0;
	require(pick_up_secondary(MEGA_INDEX, 1) && Players[0].secondary_weapon == MEGA_INDEX, "empty selected missile still permits fallback");
	select_weapon(PLASMA_INDEX, 0, 0, 0);
	require(Players[0].primary_weapon == PLASMA_INDEX, "manual selection remains available");
	init_player_stats_new_ship(1);
	require(PrimaryWeaponPickedUp && SecondaryWeaponPickedUp, "another player's new ship does not reset local flags");
	new_ship();
	require(pick_up_primary(LASER_INDEX) && !PrimaryWeaponPickedUp, "current laser pickup does not consume allowance");
	Players[0].primary_weapon_flags |= 1 << SPREADFIRE_INDEX;
	require(!pick_up_primary(SPREADFIRE_INDEX) && !PrimaryWeaponPickedUp, "duplicate primary does not consume allowance");
	require(pick_up_secondary(CONCUSSION_INDEX, 1) && !SecondaryWeaponPickedUp, "current secondary refill does not consume allowance");
	require(pick_up_secondary(PROXIMITY_INDEX, 1) && SecondaryWeaponPickedUp && Players[0].secondary_weapon == CONCUSSION_INDEX, "below-cutoff pickup consumes first pickup without switching");
	require(pick_up_secondary(HOMING_INDEX, 1) && Players[0].secondary_weapon == CONCUSSION_INDEX, "later pickup remains suppressed after below-cutoff pickup");
	new_ship();
	PlayerCfg.SelectAfterFire = 0;
	PlayerCfg.NoFireAutoselect = 1;
	Controls.fire_primary_state = 1;
	require(pick_up_primary(SPREADFIRE_INDEX) && PrimaryWeaponPickedUp, "no-fire pickup consumes first pickup");
	Controls.fire_primary_state = 0;
	delayed_autoselect();
	require(Players[0].primary_weapon == LASER_INDEX, "no-fire setting without delay does not switch");
	test_runtime_roundtrip(false);
	test_runtime_roundtrip(true);

	char pilot[] = "autoonce.plx";
	PlayerCfg.AutoselectOnlyOnce = 1;
#ifdef DXX_BUILD_DESCENT_II
	require(write_player_d2x(pilot) == 1, "write pilot setting");
	new_player_config();
	require(read_player_d2x(pilot) == 0, "read pilot setting");
#else
	require(write_player_d1x(pilot) == 1, "write pilot setting");
	new_player_config();
	require(read_player_d1x(pilot) == 0, "read pilot setting");
#endif
	require(PlayerCfg.AutoselectOnlyOnce == 1, "pilot text preserves setting");
	new_ship(0);
}
