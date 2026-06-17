#ifndef D1_SAVE_TRANSLATE_H
#define D1_SAVE_TRANSLATE_H

#include <stddef.h>
#include <stdint.h>

#include "maths.h"
#include "player.h"
#include "pstypes.h"
#include "vecmat.h"

#define D1_SAVE_TRANSLATE_MAX_MISSION_NAME 128
#define D1_SAVE_TRANSLATE_PRIMARY_WEAPONS 5
#define D1_SAVE_TRANSLATE_SECONDARY_WEAPONS 5

typedef struct d1_save_translate_checkpoint_start {
	char mission_name[D1_SAVE_TRANSLATE_MAX_MISSION_NAME];
	char player_callsign[CALLSIGN_LEN + 1];
	int version;
	int current_level;
	int next_level;
	fix game_time;
	int difficulty;
	int difficulty_changed;
	int difficulty_min;
	int difficulty_max;
	ubyte connected;
	uint flags;
	fix energy;
	fix shields;
	ubyte lives;
	sbyte level;
	ubyte laser_level;
	sbyte starting_level;
	short killer_objnum;
	ushort primary_weapon_flags;
	ushort secondary_weapon_flags;
	ushort primary_ammo[D1_SAVE_TRANSLATE_PRIMARY_WEAPONS];
	ushort secondary_ammo[D1_SAVE_TRANSLATE_SECONDARY_WEAPONS];
	int last_score;
	int score;
	fix time_level;
	fix time_total;
	fix cloak_time;
	fix invulnerable_time;
	short net_killed_total;
	short net_kills_total;
	short num_kills_level;
	short num_kills_total;
	short num_robots_level;
	short num_robots_total;
	ushort hostages_rescued_total;
	ushort hostages_total;
	ubyte hostages_on_board;
	ubyte hostages_level;
	fix homing_object_dist;
	sbyte hours_level;
	sbyte hours_total;
	sbyte primary_weapon;
	sbyte secondary_weapon;
	int checkpoint_swap;
	int object_count;
	size_t object_stream_offset;
	int has_player_object_pose;
	int has_player_object_physics;
	ubyte player_object_type;
	ubyte player_object_id;
	ubyte player_object_control_type;
	ubyte player_object_movement_type;
	ubyte player_object_render_type;
	ubyte player_object_flags;
	short player_object_segnum;
	short player_object_attached_obj;
	fix player_object_size;
	fix player_object_shields;
	vms_vector player_object_pos;
	vms_matrix player_object_orient;
	vms_vector player_object_last_pos;
	sbyte player_object_contains_type;
	sbyte player_object_contains_id;
	sbyte player_object_contains_count;
	sbyte player_object_matcen_creator;
	fix player_object_lifeleft;
	vms_vector player_object_velocity;
	vms_vector player_object_thrust;
	fix player_object_mass;
	fix player_object_drag;
	fix player_object_brakes;
	vms_vector player_object_rotvel;
	vms_vector player_object_rotthrust;
	fixang player_object_turnroll;
	ushort player_object_phys_flags;
} d1_save_translate_checkpoint_start;

int d1_save_translate_read_checkpoint_start(const uint8_t *data, size_t size,
                                            d1_save_translate_checkpoint_start *start);
int d1_save_translate_apply_checkpoint_objects(
    const uint8_t *data, size_t size,
    const d1_save_translate_checkpoint_start *start);
void d1_save_translate_apply_checkpoint_player(
    const d1_save_translate_checkpoint_start *start,
    const char *local_player_callsign);

#endif
