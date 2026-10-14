#include <string.h>

#include "args.h"
#include "collide.h"
#include "console.h"
#include "fuelcen.h"
#include "game.h"
#include "gameseq.h"
#include "hudmsg.h"
#include "input_demo_level_start.h"
#include "input_demo_newdemo_shared.h"
#include "input_demo_replay.h"
#include "laser.h"
#include "matcen_mode.h"
#include "maths.h"
#include "newdemo.h"
#include "object.h"
#include "player.h"
#include "weapon.h"

static int record_each_level;
static int pending_level;
static input_demo_level_start starting_state;

void input_demo_set_record_each_level(int enabled)
{
	record_each_level = enabled != 0;
}

int input_demo_new_level_pending(void)
{
	return pending_level != 0;
}

const input_demo_level_start *input_demo_recording_level_start(void)
{
	return pending_level ? &starting_state : NULL;
}

static void capture_player(input_demo_level_start *s)
{
	const player *p = &Players[Player_num];
	unsigned int i;
#define CAPTURE_PLAYER(type, name, low, high) s->name = p->name;
	INPUT_DEMO_LEVEL_PLAYER_FIELDS(CAPTURE_PLAYER)
#undef CAPTURE_PLAYER
	for (i = 0; i < MAX_PRIMARY_WEAPONS; ++i)
		s->primary_ammo[i] = p->primary_ammo[i];
	for (i = 0; i < MAX_SECONDARY_WEAPONS; ++i)
		s->secondary_ammo[i] = p->secondary_ammo[i];
}

static void apply_player(const input_demo_level_start *s)
{
	player *p = &Players[Player_num];
	unsigned int i;
#define APPLY_PLAYER(type, name, low, high) p->name = (type) s->name;
	INPUT_DEMO_LEVEL_PLAYER_FIELDS(APPLY_PLAYER)
#undef APPLY_PLAYER
	for (i = 0; i < MAX_PRIMARY_WEAPONS; ++i)
		p->primary_ammo[i] = s->primary_ammo[i];
	for (i = 0; i < MAX_SECONDARY_WEAPONS; ++i)
		p->secondary_ammo[i] = s->secondary_ammo[i];
}

static void capture_runtime(input_demo_level_start *s)
{
	laser_runtime_state laser;
	game_d_tick_state tick;
	laser_get_runtime_state(&laser);
	game_get_d_tick_state(&tick);
	s->cheats_enabled = cheats.enabled;
	s->cheat_wowie = cheats.wowie;
#ifndef DXX_BUILD_DESCENT_II
	s->cheat_wowie2 = cheats.wowie2;
#endif
	s->game_time = GameTime64;
	s->next_laser = Next_laser_fire_time;
	s->last_laser = Last_laser_fired_time;
	s->next_missile = Next_missile_fire_time;
	s->next_flare = Next_flare_fire_time;
	s->spreadfire_toggle = laser.spreadfire_toggle;
	s->missile_gun = laser.missile_gun;
	s->proximity_dropped = laser.proximity_dropped;
	s->helix_orientation = laser.helix_orientation;
	s->smartmines_dropped = laser.smartmines_dropped;
	s->last_omega_fire_time = laser.last_omega_fire_time;
	s->laser_firing_count = Global_laser_firing_count;
	s->missile_firing_count = Global_missile_firing_count;
	s->primary_picked_up = PrimaryWeaponPickedUp;
	s->secondary_picked_up = SecondaryWeaponPickedUp;
	s->tick_count = tick.count;
	s->tick_step = tick.step;
	s->tick_time = tick.timer;
	s->fusion_sound_time = game_get_fusion_next_sound_time();
	s->refuel_sound_time = fuelcen_get_last_sound_time();
	s->collision_sound_time = collide_get_collision_delay_last_play_time();
	s->difficulty_changed = Difficulty_level_changed;
	s->difficulty_min = Difficulty_level_min_seen;
	s->difficulty_max = Difficulty_level_max_seen;
	s->matcen_mode = matcen_mode_get();
#ifdef DXX_BUILD_DESCENT_II
	s->afterburner_charge = Players[Player_num].afterburner_charge;
	s->omega_charge = Omega_charge;
	for (unsigned int i = 0; i < MAX_PRIMARY_WEAPONS; ++i)
		s->primary_last_super[i] = Primary_last_was_super[i];
	for (unsigned int i = 0; i < MAX_SECONDARY_WEAPONS; ++i)
		s->secondary_last_super[i] = Secondary_last_was_super[i];
#endif
}

static void apply_runtime(const input_demo_level_start *s)
{
	laser_runtime_state laser;
	game_d_tick_state tick = { s->tick_count, s->tick_step, s->tick_time };
	cheats.enabled = s->cheats_enabled;
	cheats.wowie = s->cheat_wowie;
#ifndef DXX_BUILD_DESCENT_II
	cheats.wowie2 = s->cheat_wowie2;
#endif
	laser_get_runtime_state(&laser);
	laser.spreadfire_toggle = s->spreadfire_toggle;
	laser.missile_gun = s->missile_gun;
	laser.proximity_dropped = s->proximity_dropped;
	laser.helix_orientation = s->helix_orientation;
	laser.smartmines_dropped = s->smartmines_dropped;
#ifdef DXX_BUILD_DESCENT_II
	laser.last_omega_fire_time = s->last_omega_fire_time;
	Players[Player_num].afterburner_charge = s->afterburner_charge;
	Omega_charge = s->omega_charge;
	for (unsigned int i = 0; i < MAX_PRIMARY_WEAPONS; ++i)
		Primary_last_was_super[i] = (ubyte) s->primary_last_super[i];
	for (unsigned int i = 0; i < MAX_SECONDARY_WEAPONS; ++i)
		Secondary_last_was_super[i] = (ubyte) s->secondary_last_super[i];
#endif
	laser_set_runtime_state(&laser);
	Next_laser_fire_time = s->next_laser;
	Last_laser_fired_time = s->last_laser;
	Next_missile_fire_time = s->next_missile;
	Next_flare_fire_time = s->next_flare;
	Global_laser_firing_count = s->laser_firing_count;
	Global_missile_firing_count = s->missile_firing_count;
	PrimaryWeaponPickedUp = s->primary_picked_up;
	SecondaryWeaponPickedUp = s->secondary_picked_up;
	game_set_d_tick_state(&tick);
	game_set_fusion_next_sound_time(s->fusion_sound_time);
	fuelcen_set_last_sound_time(s->refuel_sound_time);
	collide_set_collision_delay_last_play_time(s->collision_sound_time);
	difficulty_restore_history(s->difficulty_changed, s->difficulty_min, s->difficulty_max);
	matcen_mode_set(s->matcen_mode);
}

void input_demo_begin_new_level(int level)
{
	const input_demo_level_start *replay = input_demo_replay_level_start();
	pending_level = 0;
	if (replay) {
		apply_player(replay);
		apply_runtime(replay);
		GameTime64 = replay->game_time;
		d_rand_set_state(replay->rng_state);
		d_rand_set_call_count(replay->rng_calls);
		return;
	}
	if (!record_each_level || (Game_mode & GM_MULTI) ||
	    Newdemo_state != ND_STATE_NORMAL || input_demo_replay_is_loaded() ||
	    GameArg.SysInputDemoNoRender)
		return;
	memset(&starting_state, 0, sizeof(starting_state));
	if (!d_rand_get_state(&starting_state.rng_state))
		return;
	starting_state.rng_calls = d_rand_get_call_count();
	capture_player(&starting_state);
	capture_runtime(&starting_state);
	pending_level = level;
}

void input_demo_end_new_level(int level)
{
	const input_demo_level_start *replay = input_demo_replay_level_start();
	object_runtime_state objects;
	object_get_runtime_state(&objects);
	if (replay) {
		objects.homer_frame_count = replay->homer_frame_count;
		objects.current_homer_frame_time = replay->homer_frame_time;
		objects.do_homer_frame = replay->do_homer_frame;
		object_set_runtime_state(&objects);
		apply_runtime(replay);
		return;
	}
	if (pending_level == level && Current_level_num == level) {
		starting_state.homer_frame_count = objects.homer_frame_count;
		starting_state.homer_frame_time = objects.current_homer_frame_time;
		starting_state.do_homer_frame = objects.do_homer_frame;
		capture_runtime(&starting_state);
		if (!input_demo_start_quick_recording_common())
			HUD_init_message(HM_DEFAULT, "Could not start level demo");
	}
	pending_level = 0;
}
