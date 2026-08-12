#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "byteswap.h"
#include "cntrlcen.h"
#include "console.h"
#include "d1_save_translate.h"
#include "effects.h"
#include "fuelcen.h"
#include "game.h"
#include "gamemine.h"
#include "laser.h"
#include "morph.h"
#include "object.h"
#include "polyobj.h"
#include "powerup.h"
#include "robot.h"
#include "secretarea.h"
#include "segment.h"
#include "switch.h"
#include "textures.h"
#include "wall.h"
#include "weapon.h"
#include "vclip.h"

#define D1_SAVE_VERSION 15
#define D1_SAVE_COMPATIBLE_VERSION D1_SAVE_VERSION
#define D1_SAVE_DESC_LENGTH 20
#define D1_SAVE_THUMBNAIL_W 100
#define D1_SAVE_THUMBNAIL_H 50
#define D1_SAVE_THUMBNAIL_RGB_BYTES (D1_SAVE_THUMBNAIL_W * D1_SAVE_THUMBNAIL_H * 3)
#define D1_SAVE_THUMBNAIL_INDEXED_BYTES (D1_SAVE_THUMBNAIL_W * D1_SAVE_THUMBNAIL_H)
#define D1_SAVE_THUMBNAIL_PALETTE_BYTES (256 * 3)
#define D1_SAVE_THUMBNAIL_RGB_VERSION 8
#define D1_SAVE_THUMBNAIL_PALETTE_VERSION 8
#define D1_SAVE_OBJECT_MOVEMENT_UNION_BYTES sizeof(physics_info)
#define D1_SAVE_OBJECT_CONTROL_UNION_BYTES 30
#define D1_SAVE_OBJECT_RENDER_UNION_BYTES sizeof(polyobj_info)
#define D1_SAVE_AI_LOCAL_RW_BYTES 184
#define D1_SAVE_POINT_SEG_BYTES 16
#define D1_SAVE_AI_CLOAK_INFO_RW_BYTES 16
#define D1_SAVE_MAX_AI_CLOAK_INFO 8
#define D1_SAVE_MAX_AWARENESS_EVENTS 64
#define D1_SAVE_MAX_POLYGON_MODELS 85

typedef struct d1_save_translate_reader {
	const uint8_t *data;
	size_t size;
	size_t pos;
	int swap;
} d1_save_translate_reader;

typedef struct d1_save_translate_side_state {
	short wall_num;
	short tmap_num;
	short tmap_num2;
} d1_save_translate_side_state;

typedef struct d1_save_translate_world_state {
	int num_walls;
	wall walls[MAX_WALLS];
	int num_open_doors;
	active_door active_doors[MAX_DOORS];
	int num_triggers;
	trigger triggers[MAX_TRIGGERS];
	d1_save_translate_side_state sides[MAX_SEGMENTS][MAX_SIDES_PER_SEGMENT];
	int control_center_destroyed;
	int countdown_seconds_left;
	int num_robot_centers;
	matcen_info robot_centers[MAX_ROBOT_CENTERS];
	control_center_triggers control_center_triggers;
	int num_fuelcenters;
	FuelCenter stations[MAX_NUM_FUELCENS];
	fix countdown_timer;
	int control_center_been_hit;
	int control_center_player_been_seen;
	int control_center_next_fire_time;
	int control_center_present;
	int dead_controlcen_object_num;
} d1_save_translate_world_state;

typedef struct d1_save_translate_ai_state {
	int ai_initialized;
	int overall_agitation;
	ai_local local_info[MAX_OBJECTS];
	point_seg point_segs[MAX_POINT_SEGS];
	ai_cloak_info cloak_info[D1_SAVE_MAX_AI_CLOAK_INFO];
	fix64 boss_cloak_start_time;
	fix64 boss_cloak_end_time;
	fix64 last_teleport_time;
	fix boss_teleport_interval;
	fix boss_cloak_interval;
	fix boss_cloak_duration;
	fix64 last_gate_time;
	fix gate_interval;
	fix64 boss_dying_start_time;
	int boss_dying;
	sbyte boss_dying_sound_playing;
	int point_seg_free_index;
	int awareness_count;
	awareness_event awareness_events[D1_SAVE_MAX_AWARENESS_EVENTS];
	vms_vector believed_player_pos;
} d1_save_translate_ai_state;

typedef struct d1_save_translate_runtime_state {
	fix next_laser_fire_delta;
	fix next_missile_fire_delta;
	fix last_laser_fired_delta;
	fix next_flare_fire_delta;
	fix auto_fire_fusion_delta;
	int global_laser_firing_count;
	int global_missile_firing_count;
	int has_rng_state;
	uint rng_state;
	int has_fx_rng_state;
	uint fx_rng_state;
	uint fx_rng_call_count;
	game_d_tick_state d_tick_state;
	object_runtime_state object_state;
	laser_runtime_state laser_state;
	ai_path_runtime_state ai_path_state;
} d1_save_translate_runtime_state;

extern int Do_appearance_effect;
extern void copy_defaults_to_robot(object *objp);

static int d1_save_translate_skip(d1_save_translate_reader *reader, size_t count)
{
	if (!reader || count > reader->size || reader->pos > reader->size - count)
		return 0;
	reader->pos += count;
	return 1;
}

static int d1_save_translate_read_bytes(d1_save_translate_reader *reader,
                                        void *dest, size_t count)
{
	if (!d1_save_translate_skip(reader, count))
		return 0;
	memcpy(dest, reader->data + reader->pos - count, count);
	return 1;
}

static int d1_save_translate_read_s32(d1_save_translate_reader *reader, int *out)
{
	int value;

	if (!d1_save_translate_read_bytes(reader, &value, sizeof(value)))
		return 0;
	if (reader->swap)
		value = SWAPINT(value);
	*out = value;
	return 1;
}

static int d1_save_translate_read_u32(d1_save_translate_reader *reader, uint *out)
{
	int value;

	if (!d1_save_translate_read_s32(reader, &value))
		return 0;
	*out = (uint) value;
	return 1;
}

static int d1_save_translate_read_s16(d1_save_translate_reader *reader, short *out)
{
	short value;

	if (!d1_save_translate_read_bytes(reader, &value, sizeof(value)))
		return 0;
	if (reader->swap)
		value = (short) SWAPSHORT(value);
	*out = value;
	return 1;
}

static int d1_save_translate_read_u16(d1_save_translate_reader *reader, ushort *out)
{
	short value;

	if (!d1_save_translate_read_s16(reader, &value))
		return 0;
	*out = (ushort) value;
	return 1;
}

static int d1_save_translate_read_u8(d1_save_translate_reader *reader, ubyte *out)
{
	return d1_save_translate_read_bytes(reader, out, sizeof(*out));
}

static int d1_save_translate_read_s8(d1_save_translate_reader *reader, sbyte *out)
{
	return d1_save_translate_read_bytes(reader, out, sizeof(*out));
}

static int d1_save_translate_read_fix(d1_save_translate_reader *reader, fix *out)
{
	int value;

	if (!d1_save_translate_read_s32(reader, &value))
		return 0;
	*out = (fix) value;
	return 1;
}

static int d1_save_translate_read_fix64(d1_save_translate_reader *reader,
                                        fix64 *out)
{
	uint low, high;
	uint64_t raw_value;

	if (!d1_save_translate_read_u32(reader, &low) ||
	    !d1_save_translate_read_u32(reader, &high))
		return 0;
	raw_value = ((uint64_t) high << 32) | (uint64_t) low;
	*out = (fix64) raw_value;
	return 1;
}

static int d1_save_translate_read_fixang(d1_save_translate_reader *reader,
                                         fixang *out)
{
	short value;

	if (!d1_save_translate_read_s16(reader, &value))
		return 0;
	*out = (fixang) value;
	return 1;
}

static int d1_save_translate_read_vector(d1_save_translate_reader *reader,
                                         vms_vector *out)
{
	return d1_save_translate_read_fix(reader, &out->x) &&
	       d1_save_translate_read_fix(reader, &out->y) &&
	       d1_save_translate_read_fix(reader, &out->z);
}

static int d1_save_translate_read_matrix(d1_save_translate_reader *reader,
                                         vms_matrix *out)
{
	return d1_save_translate_read_vector(reader, &out->rvec) &&
	       d1_save_translate_read_vector(reader, &out->uvec) &&
	       d1_save_translate_read_vector(reader, &out->fvec);
}

static int d1_save_translate_finish_union(d1_save_translate_reader *reader,
                                          size_t union_start, size_t union_size)
{
	size_t consumed;

	if (!reader || reader->pos < union_start)
		return 0;
	consumed = reader->pos - union_start;
	if (consumed > union_size)
		return 0;
	return d1_save_translate_skip(reader, union_size - consumed);
}

static int d1_save_translate_read_physics_info(
    d1_save_translate_reader *reader,
    d1_save_translate_checkpoint_start *start)
{
	if (!d1_save_translate_read_vector(reader, &start->player_object_velocity) ||
	    !d1_save_translate_read_vector(reader, &start->player_object_thrust) ||
	    !d1_save_translate_read_fix(reader, &start->player_object_mass) ||
	    !d1_save_translate_read_fix(reader, &start->player_object_drag) ||
	    !d1_save_translate_read_fix(reader, &start->player_object_brakes) ||
	    !d1_save_translate_read_vector(reader, &start->player_object_rotvel) ||
	    !d1_save_translate_read_vector(reader, &start->player_object_rotthrust) ||
	    !d1_save_translate_read_fixang(reader, &start->player_object_turnroll) ||
	    !d1_save_translate_read_u16(reader, &start->player_object_phys_flags))
		return 0;
	start->has_player_object_physics = 1;
	return 1;
}

static int d1_save_translate_skip_thumbnail(d1_save_translate_reader *reader,
                                            int version)
{
	if (version >= D1_SAVE_THUMBNAIL_RGB_VERSION)
		return d1_save_translate_skip(reader, D1_SAVE_THUMBNAIL_RGB_BYTES);
	if (!d1_save_translate_skip(reader, D1_SAVE_THUMBNAIL_INDEXED_BYTES))
		return 0;
	if (version >= D1_SAVE_THUMBNAIL_PALETTE_VERSION)
		return d1_save_translate_skip(reader, D1_SAVE_THUMBNAIL_PALETTE_BYTES);
	return 1;
}

static void d1_save_translate_copy_mission_name(char *dest, size_t dest_size,
                                                const char *source)
{
	const char *name = source;
	const char *slash;

	if (!dest || !dest_size)
		return;
	slash = strrchr(source, '/');
	if (slash)
		name = slash + 1;
	snprintf(dest, dest_size, "%s", name);
}

static int d1_save_translate_read_mission(d1_save_translate_reader *reader,
                                          char *mission_name, size_t mission_name_size)
{
	char short_mission[9];

	if (!d1_save_translate_read_bytes(reader, short_mission, sizeof(short_mission)))
		return 0;
	if (short_mission[8] == 1) {
		char long_mission[D1_SAVE_TRANSLATE_MAX_MISSION_NAME];

		if (!d1_save_translate_read_bytes(reader, long_mission, sizeof(long_mission)))
			return 0;
		if (long_mission[sizeof(long_mission) - 1])
			return 0;
		d1_save_translate_copy_mission_name(mission_name, mission_name_size,
		                                    long_mission);
		return 1;
	}
	if (!mission_name || !mission_name_size)
		return 0;
	memcpy(mission_name, short_mission, sizeof(short_mission));
	mission_name[mission_name_size - 1] = '\0';
	return 1;
}

static int d1_save_translate_read_player(d1_save_translate_reader *reader,
                                         d1_save_translate_checkpoint_start *start)
{
	int i;
	uint unused_u32;
	ubyte primary_weapon_flags;
	ubyte secondary_weapon_flags;
	ubyte unused_net_address[6];

	if (!d1_save_translate_read_bytes(reader, start->player_callsign,
	                                  CALLSIGN_LEN + 1) ||
	    !d1_save_translate_read_bytes(reader, unused_net_address,
	                                  sizeof(unused_net_address)) ||
	    !d1_save_translate_read_u8(reader, &start->connected) ||
	    !d1_save_translate_read_u32(reader, &unused_u32) ||
	    !d1_save_translate_read_u32(reader, &unused_u32) ||
	    !d1_save_translate_read_u32(reader, &unused_u32) ||
	    !d1_save_translate_read_u32(reader, &start->flags) ||
	    !d1_save_translate_read_fix(reader, &start->energy) ||
	    !d1_save_translate_read_fix(reader, &start->shields) ||
	    !d1_save_translate_read_u8(reader, &start->lives) ||
	    !d1_save_translate_read_s8(reader, &start->level) ||
	    !d1_save_translate_read_u8(reader, &start->laser_level) ||
	    !d1_save_translate_read_s8(reader, &start->starting_level) ||
	    !d1_save_translate_read_s16(reader, &start->killer_objnum) ||
	    !d1_save_translate_read_u8(reader, &primary_weapon_flags) ||
	    !d1_save_translate_read_u8(reader, &secondary_weapon_flags))
		return 0;
	start->primary_weapon_flags = primary_weapon_flags;
	start->secondary_weapon_flags = secondary_weapon_flags;
	for (i = 0; i < D1_SAVE_TRANSLATE_PRIMARY_WEAPONS; i++)
		if (!d1_save_translate_read_u16(reader, &start->primary_ammo[i]))
			return 0;
	for (i = 0; i < D1_SAVE_TRANSLATE_SECONDARY_WEAPONS; i++)
		if (!d1_save_translate_read_u16(reader, &start->secondary_ammo[i]))
			return 0;
	if (!d1_save_translate_read_s32(reader, &start->last_score) ||
	    !d1_save_translate_read_s32(reader, &start->score) ||
	    !d1_save_translate_read_fix(reader, &start->time_level) ||
	    !d1_save_translate_read_fix(reader, &start->time_total) ||
	    !d1_save_translate_read_fix(reader, &start->cloak_time) ||
	    !d1_save_translate_read_fix(reader, &start->invulnerable_time) ||
	    !d1_save_translate_read_s16(reader, &start->net_killed_total) ||
	    !d1_save_translate_read_s16(reader, &start->net_kills_total) ||
	    !d1_save_translate_read_s16(reader, &start->num_kills_level) ||
	    !d1_save_translate_read_s16(reader, &start->num_kills_total) ||
	    !d1_save_translate_read_s16(reader, &start->num_robots_level) ||
	    !d1_save_translate_read_s16(reader, &start->num_robots_total) ||
	    !d1_save_translate_read_u16(reader, &start->hostages_rescued_total) ||
	    !d1_save_translate_read_u16(reader, &start->hostages_total) ||
	    !d1_save_translate_read_u8(reader, &start->hostages_on_board) ||
	    !d1_save_translate_read_u8(reader, &start->hostages_level) ||
	    !d1_save_translate_read_fix(reader, &start->homing_object_dist) ||
	    !d1_save_translate_read_s8(reader, &start->hours_level) ||
	    !d1_save_translate_read_s8(reader, &start->hours_total))
		return 0;
	start->player_callsign[CALLSIGN_LEN] = '\0';
	return 1;
}

static int d1_save_translate_read_player_object_pose(
    d1_save_translate_reader *reader,
    d1_save_translate_checkpoint_start *start)
{
	int object_count;
	int unused_s32;
	short unused_s16;

	if (!d1_save_translate_read_s32(reader, &unused_s32) ||
	    !d1_save_translate_read_s32(reader, &unused_s32) ||
	    !d1_save_translate_read_s32(reader, &object_count))
		return 0;
	start->object_count = object_count;
	if (object_count <= 0)
		return 1;
	start->object_stream_offset = reader->pos;
	if (!d1_save_translate_read_s32(reader, &unused_s32) ||
	    !d1_save_translate_read_u8(reader, &start->player_object_type) ||
	    !d1_save_translate_read_u8(reader, &start->player_object_id) ||
	    !d1_save_translate_read_s16(reader, &unused_s16) ||
	    !d1_save_translate_read_s16(reader, &unused_s16) ||
	    !d1_save_translate_read_u8(reader, &start->player_object_control_type) ||
	    !d1_save_translate_read_u8(reader, &start->player_object_movement_type) ||
	    !d1_save_translate_read_u8(reader, &start->player_object_render_type) ||
	    !d1_save_translate_read_u8(reader, &start->player_object_flags) ||
	    !d1_save_translate_read_s16(reader, &start->player_object_segnum) ||
	    !d1_save_translate_read_s16(reader, &start->player_object_attached_obj) ||
	    !d1_save_translate_read_vector(reader, &start->player_object_pos) ||
	    !d1_save_translate_read_matrix(reader, &start->player_object_orient) ||
	    !d1_save_translate_read_fix(reader, &start->player_object_size) ||
	    !d1_save_translate_read_fix(reader, &start->player_object_shields) ||
	    !d1_save_translate_read_vector(reader, &start->player_object_last_pos) ||
	    !d1_save_translate_read_s8(reader, &start->player_object_contains_type) ||
	    !d1_save_translate_read_s8(reader, &start->player_object_contains_id) ||
	    !d1_save_translate_read_s8(reader, &start->player_object_contains_count) ||
	    !d1_save_translate_read_s8(reader, &start->player_object_matcen_creator) ||
	    !d1_save_translate_read_fix(reader, &start->player_object_lifeleft))
		return 0;
	if (start->player_object_movement_type == MT_PHYSICS &&
	    !d1_save_translate_read_physics_info(reader, start))
		return 0;
	start->has_player_object_pose = 1;
	return 1;
}

int d1_save_translate_read_checkpoint_start(const uint8_t *data, size_t size,
                                            d1_save_translate_checkpoint_start *start)
{
	char id[4];
	int version;
	int between_levels;
	d1_save_translate_reader reader;

	if (!data || !size || !start)
		return 0;
	memset(start, 0, sizeof(*start));
	reader.data = data;
	reader.size = size;
	reader.pos = 0;
	reader.swap = 0;
	if (!d1_save_translate_read_bytes(&reader, id, sizeof(id)) ||
	    memcmp(id, "DGSS", sizeof(id)))
		return 0;
	if (!d1_save_translate_read_s32(&reader, &version))
		return 0;
	if (version & 0xffff0000) {
		version = SWAPINT(version);
		reader.swap = 1;
	}
	start->checkpoint_swap = reader.swap;
	if (version < D1_SAVE_COMPATIBLE_VERSION || version > D1_SAVE_VERSION)
		return 0;
	start->version = version;
	if (!d1_save_translate_skip(&reader, D1_SAVE_DESC_LENGTH) ||
	    !d1_save_translate_skip_thumbnail(&reader, version) ||
	    !d1_save_translate_read_s32(&reader, &between_levels) ||
	    !d1_save_translate_read_mission(&reader, start->mission_name,
	                                    sizeof(start->mission_name)) ||
	    !d1_save_translate_read_s32(&reader, &start->current_level) ||
	    !d1_save_translate_read_s32(&reader, &start->next_level) ||
	    !d1_save_translate_read_fix(&reader, &start->game_time) ||
	    !d1_save_translate_read_player(&reader, start) ||
	    !d1_save_translate_read_s8(&reader, &start->primary_weapon) ||
	    !d1_save_translate_read_s8(&reader, &start->secondary_weapon) ||
	    !d1_save_translate_read_s32(&reader, &start->difficulty))
		return 0;
	if (start->primary_weapon < 0 ||
	    start->primary_weapon >= D1_SAVE_TRANSLATE_PRIMARY_WEAPONS ||
	    start->secondary_weapon < 0 ||
	    start->secondary_weapon >= D1_SAVE_TRANSLATE_SECONDARY_WEAPONS ||
	    start->difficulty < 0 || start->difficulty >= NDL)
		return 0;
	if (version >= 13) {
		if (!d1_save_translate_read_s32(&reader, &start->difficulty_changed) ||
		    !d1_save_translate_read_s32(&reader, &start->difficulty_min) ||
		    !d1_save_translate_read_s32(&reader, &start->difficulty_max))
			return 0;
	} else {
		start->difficulty_changed = 0;
		start->difficulty_min = start->difficulty;
		start->difficulty_max = start->difficulty;
	}
	if (!d1_save_translate_read_player_object_pose(&reader, start))
		return 0;
	(void) between_levels;
	return 1;
}

static int d1_save_translate_read_object_movement(d1_save_translate_reader *reader,
                                                  object *obj)
{
	size_t union_start = reader->pos;

	switch (obj->movement_type) {
	case MT_PHYSICS:
		if (!d1_save_translate_read_vector(reader, &obj->mtype.phys_info.velocity) ||
		    !d1_save_translate_read_vector(reader, &obj->mtype.phys_info.thrust) ||
		    !d1_save_translate_read_fix(reader, &obj->mtype.phys_info.mass) ||
		    !d1_save_translate_read_fix(reader, &obj->mtype.phys_info.drag) ||
		    !d1_save_translate_read_fix(reader, &obj->mtype.phys_info.brakes) ||
		    !d1_save_translate_read_vector(reader, &obj->mtype.phys_info.rotvel) ||
		    !d1_save_translate_read_vector(reader, &obj->mtype.phys_info.rotthrust) ||
		    !d1_save_translate_read_fixang(reader, &obj->mtype.phys_info.turnroll) ||
		    !d1_save_translate_read_u16(reader, &obj->mtype.phys_info.flags))
			return 0;
		break;
	case MT_SPINNING:
		if (!d1_save_translate_read_vector(reader, &obj->mtype.spin_rate))
			return 0;
		break;
	}
	return d1_save_translate_finish_union(reader, union_start,
	                                      D1_SAVE_OBJECT_MOVEMENT_UNION_BYTES);
}

static int d1_save_translate_read_object_control(d1_save_translate_reader *reader,
                                                 object *obj)
{
	int i;
	size_t union_start = reader->pos;

	switch (obj->control_type) {
	case CT_WEAPON:
	{
		fix creation_time;

		if (!d1_save_translate_read_s16(
		        reader, &obj->ctype.laser_info.parent_type) ||
		    !d1_save_translate_read_s16(reader, &obj->ctype.laser_info.parent_num) ||
		    !d1_save_translate_read_s32(
		        reader, &obj->ctype.laser_info.parent_signature) ||
		    !d1_save_translate_read_fix(reader, &creation_time) ||
		    !d1_save_translate_read_s16(reader, &obj->ctype.laser_info.last_hitobj) ||
		    !d1_save_translate_read_s16(reader, &obj->ctype.laser_info.track_goal) ||
		    !d1_save_translate_read_fix(reader, &obj->ctype.laser_info.multiplier))
			return 0;
		obj->ctype.laser_info.creation_time = GameTime64 + (fix64) creation_time;
		obj->ctype.laser_info.creation_framecount = 0;
		memset(obj->ctype.laser_info.hitobj_list, 0,
		       sizeof(obj->ctype.laser_info.hitobj_list));
		if (obj->ctype.laser_info.last_hitobj >= 0 &&
		    obj->ctype.laser_info.last_hitobj < MAX_OBJECTS)
			obj->ctype.laser_info
			    .hitobj_list[obj->ctype.laser_info.last_hitobj] = 1;
		break;
	}
	case CT_EXPLOSION:
		if (!d1_save_translate_read_fix(reader, &obj->ctype.expl_info.spawn_time) ||
		    !d1_save_translate_read_fix(reader, &obj->ctype.expl_info.delete_time) ||
		    !d1_save_translate_read_s16(reader,
		                                &obj->ctype.expl_info.delete_objnum) ||
		    !d1_save_translate_read_s16(reader,
		                                &obj->ctype.expl_info.attach_parent) ||
		    !d1_save_translate_read_s16(reader, &obj->ctype.expl_info.prev_attach) ||
		    !d1_save_translate_read_s16(reader, &obj->ctype.expl_info.next_attach))
			return 0;
		break;
	case CT_AI: {
		short d1_cur_path_index;
		short unused_follow_path_start_seg;
		short unused_follow_path_end_seg;

		if (!d1_save_translate_read_u8(reader, &obj->ctype.ai_info.behavior))
			return 0;
		for (i = 0; i < MAX_AI_FLAGS; i++)
			if (!d1_save_translate_read_s8(reader, &obj->ctype.ai_info.flags[i]))
				return 0;
		obj->ctype.ai_info.SUB_FLAGS = 0;
		if (!d1_save_translate_read_s16(reader, &obj->ctype.ai_info.hide_segment) ||
		    !d1_save_translate_read_s16(reader, &obj->ctype.ai_info.hide_index) ||
		    !d1_save_translate_read_s16(reader, &obj->ctype.ai_info.path_length) ||
		    !d1_save_translate_read_s16(reader, &d1_cur_path_index) ||
		    !d1_save_translate_read_s16(reader, &unused_follow_path_start_seg) ||
		    !d1_save_translate_read_s16(reader, &unused_follow_path_end_seg) ||
		    !d1_save_translate_read_s32(
		        reader, &obj->ctype.ai_info.danger_laser_signature) ||
		    !d1_save_translate_read_s16(reader, &obj->ctype.ai_info.danger_laser_num))
			return 0;
		obj->ctype.ai_info.cur_path_index = (sbyte) d1_cur_path_index;
		obj->ctype.ai_info.dying_sound_playing = 0;
		obj->ctype.ai_info.dying_start_time = 0;
		break;
	}
	case CT_LIGHT:
		if (!d1_save_translate_read_fix(reader, &obj->ctype.light_info.intensity))
			return 0;
		break;
	case CT_POWERUP:
		if (!d1_save_translate_read_s32(reader, &obj->ctype.powerup_info.count))
			return 0;
		obj->ctype.powerup_info.creation_time = 0;
		obj->ctype.powerup_info.flags = 0;
		break;
	}
	return d1_save_translate_finish_union(reader, union_start,
	                                      D1_SAVE_OBJECT_CONTROL_UNION_BYTES);
}

static int d1_save_translate_read_object_render(d1_save_translate_reader *reader,
                                                object *obj)
{
	int i;
	size_t union_start = reader->pos;

	switch (obj->render_type) {
	case RT_MORPH:
	case RT_POLYOBJ:
	case RT_NONE:
		if (!d1_save_translate_read_s32(reader, &obj->rtype.pobj_info.model_num))
			return 0;
		for (i = 0; i < MAX_SUBMODELS; i++)
			if (!d1_save_translate_read_fixang(
			        reader, &obj->rtype.pobj_info.anim_angles[i].p) ||
			    !d1_save_translate_read_fixang(
			        reader, &obj->rtype.pobj_info.anim_angles[i].b) ||
			    !d1_save_translate_read_fixang(
			        reader, &obj->rtype.pobj_info.anim_angles[i].h))
				return 0;
		if (!d1_save_translate_read_s32(reader, &obj->rtype.pobj_info.subobj_flags) ||
		    !d1_save_translate_read_s32(reader,
		                                &obj->rtype.pobj_info.tmap_override) ||
		    !d1_save_translate_read_s32(reader, &obj->rtype.pobj_info.alt_textures))
			return 0;
		break;
	case RT_WEAPON_VCLIP:
	case RT_HOSTAGE:
	case RT_POWERUP:
	case RT_FIREBALL:
		if (!d1_save_translate_read_s32(reader, &obj->rtype.vclip_info.vclip_num) ||
		    !d1_save_translate_read_fix(reader, &obj->rtype.vclip_info.frametime) ||
		    !d1_save_translate_read_s8(reader, &obj->rtype.vclip_info.framenum))
			return 0;
		break;
	}
	return d1_save_translate_finish_union(reader, union_start,
	                                      D1_SAVE_OBJECT_RENDER_UNION_BYTES);
}

static int d1_save_translate_read_object(d1_save_translate_reader *reader,
                                         object *obj)
{
	if (!reader || !obj)
		return 0;
	memset(obj, 0, sizeof(*obj));
	if (!d1_save_translate_read_s32(reader, &obj->signature) ||
	    !d1_save_translate_read_u8(reader, &obj->type) ||
	    !d1_save_translate_read_u8(reader, &obj->id) ||
	    !d1_save_translate_read_s16(reader, &obj->next) ||
	    !d1_save_translate_read_s16(reader, &obj->prev) ||
	    !d1_save_translate_read_u8(reader, &obj->control_type) ||
	    !d1_save_translate_read_u8(reader, &obj->movement_type) ||
	    !d1_save_translate_read_u8(reader, &obj->render_type) ||
	    !d1_save_translate_read_u8(reader, &obj->flags) ||
	    !d1_save_translate_read_s16(reader, &obj->segnum) ||
	    !d1_save_translate_read_s16(reader, &obj->attached_obj) ||
	    !d1_save_translate_read_vector(reader, &obj->pos) ||
	    !d1_save_translate_read_matrix(reader, &obj->orient) ||
	    !d1_save_translate_read_fix(reader, &obj->size) ||
	    !d1_save_translate_read_fix(reader, &obj->shields) ||
	    !d1_save_translate_read_vector(reader, &obj->last_pos) ||
	    !d1_save_translate_read_s8(reader, &obj->contains_type) ||
	    !d1_save_translate_read_s8(reader, &obj->contains_id) ||
	    !d1_save_translate_read_s8(reader, &obj->contains_count) ||
	    !d1_save_translate_read_s8(reader, &obj->matcen_creator) ||
	    !d1_save_translate_read_fix(reader, &obj->lifeleft) ||
	    !d1_save_translate_read_object_movement(reader, obj) ||
	    !d1_save_translate_read_object_control(reader, obj) ||
	    !d1_save_translate_read_object_render(reader, obj))
		return 0;
	return 1;
}

static int d1_save_translate_control_type_valid(ubyte value)
{
	return value == CT_NONE || value == CT_AI || value == CT_EXPLOSION ||
	       value == CT_FLYING || value == CT_SLEW || value == CT_FLYTHROUGH ||
	       value == CT_WEAPON || value == CT_REPAIRCEN || value == CT_MORPH ||
	       value == CT_DEBRIS || value == CT_POWERUP || value == CT_LIGHT ||
	       value == CT_REMOTE || value == CT_CNTRLCEN;
}

static int d1_save_translate_object_index_valid(short value, int object_count)
{
	return value == -1 || (value >= 0 && value < object_count);
}

static int d1_save_translate_validate_object(const object *obj, int object_count)
{
	if (!obj)
		return 0;
	if (obj->type == OBJ_NONE)
		return 1;
	if (obj->type >= MAX_OBJECT_TYPES ||
	    !d1_save_translate_control_type_valid(obj->control_type) ||
	    (obj->movement_type != MT_NONE && obj->movement_type != MT_PHYSICS &&
	     obj->movement_type != MT_SPINNING) ||
	    obj->render_type > RT_WEAPON_VCLIP ||
	    (obj->type != OBJ_NONE &&
	     (obj->segnum < 0 || obj->segnum > Highest_segment_index)) ||
	    !d1_save_translate_object_index_valid(obj->attached_obj, object_count))
		return 0;

	switch (obj->type) {
	case OBJ_ROBOT:
		if (obj->id >= N_robot_types)
			return 0;
		break;
	case OBJ_PLAYER:
	case OBJ_COOP:
	case OBJ_GHOST:
		if (obj->id >= MAX_PLAYERS)
			return 0;
		break;
	case OBJ_WEAPON:
	case OBJ_FLARE:
		if (obj->id >= N_weapon_types)
			return 0;
		break;
	case OBJ_POWERUP:
		if (obj->id >= N_powerup_types)
			return 0;
		break;
	case OBJ_CNTRLCEN:
		break;
	}

	if (obj->render_type == RT_POLYOBJ || obj->render_type == RT_MORPH) {
		int model_limit = N_polygon_models;
		if (obj->type == OBJ_ROBOT || obj->type == OBJ_PLAYER ||
		    obj->type == OBJ_COOP || obj->type == OBJ_GHOST)
			model_limit = D1_SAVE_MAX_POLYGON_MODELS;
		if (obj->rtype.pobj_info.model_num < 0 ||
		    obj->rtype.pobj_info.model_num >= model_limit)
			return 0;
	}
	if ((obj->render_type == RT_WEAPON_VCLIP || obj->render_type == RT_HOSTAGE ||
	     obj->render_type == RT_POWERUP || obj->render_type == RT_FIREBALL) &&
	    (obj->rtype.vclip_info.vclip_num < 0 ||
	     obj->rtype.vclip_info.vclip_num >= Num_vclips))
		return 0;

	if (obj->control_type == CT_WEAPON &&
	    (!d1_save_translate_object_index_valid(obj->ctype.laser_info.parent_num,
	                                          object_count) ||
	     !d1_save_translate_object_index_valid(obj->ctype.laser_info.last_hitobj,
	                                          object_count) ||
	     !d1_save_translate_object_index_valid(obj->ctype.laser_info.track_goal,
	                                          object_count)))
		return 0;
	if (obj->control_type == CT_EXPLOSION &&
	    (!d1_save_translate_object_index_valid(obj->ctype.expl_info.delete_objnum,
	                                          object_count) ||
	     !d1_save_translate_object_index_valid(obj->ctype.expl_info.attach_parent,
	                                          object_count) ||
	     !d1_save_translate_object_index_valid(obj->ctype.expl_info.prev_attach,
	                                          object_count) ||
	     !d1_save_translate_object_index_valid(obj->ctype.expl_info.next_attach,
	                                          object_count)))
		return 0;
	if (obj->control_type == CT_AI &&
	    !d1_save_translate_object_index_valid(obj->ctype.ai_info.danger_laser_num,
	                                         object_count))
		return 0;
	if (obj->contains_count < -1 ||
	    (obj->contains_count > 0 &&
	     (obj->contains_type < 0 || obj->contains_type >= MAX_OBJECT_TYPES)))
		return 0;
	if (obj->contains_count > 0) {
		switch (obj->contains_type) {
		case OBJ_ROBOT:
			if (obj->contains_id < 0 || obj->contains_id >= N_robot_types)
				return 0;
			break;
		case OBJ_POWERUP:
			if (obj->contains_id < 0 || obj->contains_id >= N_powerup_types)
				return 0;
			break;
		default:
			return 0;
		}
	}
	return 1;
}

static int d1_save_translate_validate_object_references(object *objects,
	int object_count)
{
	int i;
	for (i = 0; i < object_count; i++) {
		object *obj = &objects[i];
		/* Original D1 saves can retain obsolete AI destinations and weapon
		 * references after the referenced segment/object is gone.  D1 tolerated
		 * these until the corresponding AI mode used them; translated state must
		 * not publish those unsafe optional references into D2. */
		if (obj->type == OBJ_ROBOT && obj->control_type == CT_AI) {
			if (!d1_save_translate_object_index_valid(
			        obj->ctype.ai_info.danger_laser_num, object_count)) {
				obj->ctype.ai_info.danger_laser_num = -1;
				obj->ctype.ai_info.danger_laser_signature = 0;
			}
		}
		if (!d1_save_translate_validate_object(obj, object_count)) {
			con_printf(CON_URGENT,
			           "D1 checkpoint translation: invalid object %d type=%u id=%u control=%u movement=%u render=%u seg=%d attached=%d next=%d prev=%d model=%d vclip=%d contains=%d/%d/%d ai_hide=%d ai_danger=%d limits=%d/%d/%d/%d/%d\n",
			           i, obj->type, obj->id, obj->control_type,
			           obj->movement_type, obj->render_type, obj->segnum,
			           obj->attached_obj, obj->next, obj->prev,
			           obj->rtype.pobj_info.model_num,
			           obj->rtype.vclip_info.vclip_num, obj->contains_type,
			           obj->contains_id, obj->contains_count,
			           obj->ctype.ai_info.hide_segment,
			           obj->ctype.ai_info.danger_laser_num, MAX_OBJECT_TYPES,
			           N_robot_types, N_polygon_models, Num_vclips, MAX_PLAYERS);
			return 0;
		}
		if ((obj->type == OBJ_PLAYER || obj->type == OBJ_COOP ||
		     obj->type == OBJ_GHOST) && obj->render_type == RT_POLYOBJ) {
			if (!Player_ship || Player_ship->model_num < 0 ||
			    Player_ship->model_num >= N_polygon_models) {
				con_printf(CON_URGENT, "D1 checkpoint translation: invalid player model at object %d\n", i);
				return 0;
			}
			obj->rtype.pobj_info.model_num = Player_ship->model_num;
		} else if (obj->type == OBJ_ROBOT && obj->render_type == RT_POLYOBJ) {
			int model_num = Robot_info[obj->id].model_num;
			if (model_num < 0 || model_num >= N_polygon_models) {
				con_printf(CON_URGENT, "D1 checkpoint translation: invalid robot model at object %d robot=%u model=%d limit=%d\n", i, obj->id, model_num, N_polygon_models);
				return 0;
			}
			obj->rtype.pobj_info.model_num = model_num;
		}
		if (obj->type == OBJ_CNTRLCEN)
			obj->id = 0;
		if (obj->attached_obj != -1 && objects[obj->attached_obj].type == OBJ_NONE) {
			con_printf(CON_URGENT, "D1 checkpoint translation: stale attachment at object %d target=%d\n", i, obj->attached_obj);
			return 0;
		}
		/* A valid parent slot with a stale signature is a normal orphaned D1
		 * weapon state.  Consumers use the signature mismatch to avoid treating
		 * the current occupant as the original parent. */
		/* As with weapon parents, a valid danger-laser slot with a stale
		 * signature is an inactive historical reference, not corruption. */
	}
	return 1;
}

static int d1_save_translate_read_ai_local(d1_save_translate_reader *reader,
                                           ai_local *local)
{
	int j;
	sbyte value_s8;
	short goal_segment;
	fix time_delta;

	if (!local)
		return 0;
	memset(local, 0, sizeof(*local));
	if (!d1_save_translate_read_s8(reader, &value_s8))
		return 0;
	local->player_awareness_type = value_s8;
	if (!d1_save_translate_read_s8(reader, &value_s8))
		return 0;
	local->retry_count = value_s8;
	if (!d1_save_translate_read_s8(reader, &value_s8))
		return 0;
	local->consecutive_retries = value_s8;
	if (!d1_save_translate_read_s8(reader, &value_s8))
		return 0;
	local->mode = value_s8;
	if (!d1_save_translate_read_s8(reader, &value_s8))
		return 0;
	local->previous_visibility = value_s8;
	if (!d1_save_translate_read_s8(reader, &value_s8))
		return 0;
	local->rapidfire_count = value_s8;
	if (!d1_save_translate_read_s16(reader, &goal_segment))
		return 0;
	local->goal_segment = goal_segment;
	if (!d1_save_translate_skip(reader, 2 * sizeof(fix)) ||
	    !d1_save_translate_read_fix(reader, &local->next_action_time) ||
	    !d1_save_translate_read_fix(reader, &local->next_fire) ||
	    !d1_save_translate_read_fix(reader, &local->player_awareness_time) ||
	    !d1_save_translate_read_fix(reader, &time_delta))
		return 0;
	local->time_player_seen = GameTime64 + (fix64) time_delta;
	if (!d1_save_translate_read_fix(reader, &time_delta))
		return 0;
	local->time_player_sound_attacked = GameTime64 + (fix64) time_delta;
	if (!d1_save_translate_read_fix(reader, &time_delta))
		return 0;
	local->next_misc_sound_time = GameTime64 + (fix64) time_delta;
	if (!d1_save_translate_read_fix(reader, &local->time_since_processed))
		return 0;
	for (j = 0; j < MAX_SUBMODELS; j++)
		if (!d1_save_translate_read_fixang(reader, &local->goal_angles[j].p) ||
		    !d1_save_translate_read_fixang(reader, &local->goal_angles[j].b) ||
		    !d1_save_translate_read_fixang(reader, &local->goal_angles[j].h))
			return 0;
	for (j = 0; j < MAX_SUBMODELS; j++)
		if (!d1_save_translate_read_fixang(reader, &local->delta_angles[j].p) ||
		    !d1_save_translate_read_fixang(reader, &local->delta_angles[j].b) ||
		    !d1_save_translate_read_fixang(reader, &local->delta_angles[j].h))
			return 0;
	for (j = 0; j < MAX_SUBMODELS; j++)
		if (!d1_save_translate_read_s8(reader, &local->goal_state[j]))
			return 0;
	for (j = 0; j < MAX_SUBMODELS; j++)
		if (!d1_save_translate_read_s8(reader, &local->achieved_state[j]))
			return 0;
	return 1;
}

static int d1_save_translate_read_d1_ai_state(
	d1_save_translate_reader *reader, d1_save_translate_ai_state *state)
{
	int i;
	int unused_s32;
	fix time_delta;

	if (!reader || !state)
		return 0;
	memset(state, 0, sizeof(*state));
	if (!d1_save_translate_read_s32(reader, &state->ai_initialized) ||
	    !d1_save_translate_read_s32(reader, &state->overall_agitation))
		return 0;
	for (i = 0; i < MAX_OBJECTS; i++)
		if (!d1_save_translate_read_ai_local(reader, &state->local_info[i]))
			return 0;
	for (i = 0; i < MAX_POINT_SEGS; i++)
		if (!d1_save_translate_read_s32(reader, &state->point_segs[i].segnum) ||
		    !d1_save_translate_read_vector(reader, &state->point_segs[i].point))
			return 0;
	for (i = 0; i < D1_SAVE_MAX_AI_CLOAK_INFO; i++) {
		if (!d1_save_translate_read_fix(reader, &time_delta) ||
		    !d1_save_translate_read_vector(reader,
		                                   &state->cloak_info[i].last_position))
			return 0;
		state->cloak_info[i].last_time = GameTime64 + (fix64) time_delta;
		state->cloak_info[i].last_segment = -1;
	}
	if (!d1_save_translate_read_fix(reader, &time_delta))
		return 0;
	state->boss_cloak_start_time = GameTime64 + (fix64) time_delta;
	if (!d1_save_translate_read_fix(reader, &time_delta))
		return 0;
	state->boss_cloak_end_time = GameTime64 + (fix64) time_delta;
	if (!d1_save_translate_read_fix(reader, &time_delta))
		return 0;
	state->last_teleport_time = GameTime64 + (fix64) time_delta;
	if (!d1_save_translate_read_fix(reader, &state->boss_teleport_interval) ||
	    !d1_save_translate_read_fix(reader, &state->boss_cloak_interval) ||
	    !d1_save_translate_read_fix(reader, &state->boss_cloak_duration) ||
	    !d1_save_translate_read_fix(reader, &time_delta))
		return 0;
	state->last_gate_time = GameTime64 + (fix64) time_delta;
	if (!d1_save_translate_read_fix(reader, &state->gate_interval) ||
	    !d1_save_translate_read_fix(reader, &time_delta))
		return 0;
	state->boss_dying_start_time = time_delta ? GameTime64 + (fix64) time_delta : 0;
	if (!d1_save_translate_read_s32(reader, &state->boss_dying) ||
	    !d1_save_translate_read_s32(reader, &unused_s32))
		return 0;
	state->boss_dying_sound_playing = (sbyte) unused_s32;
	if (!d1_save_translate_skip(reader, 2 * sizeof(int)) ||
	    !d1_save_translate_read_s32(reader, &state->point_seg_free_index))
		return 0;
	if (state->point_seg_free_index < 0 ||
	    state->point_seg_free_index > MAX_POINT_SEGS)
		return 0;
	if (!d1_save_translate_read_s32(reader, &state->awareness_count))
		return 0;
	if (state->awareness_count < 0 ||
	    state->awareness_count > D1_SAVE_MAX_AWARENESS_EVENTS)
		return 0;
	for (i = 0; i < state->awareness_count; i++) {
		short value;

		if (!d1_save_translate_read_s16(reader, &value))
			return 0;
		state->awareness_events[i].segnum = value;
		if (!d1_save_translate_read_s16(reader, &value) ||
		    !d1_save_translate_read_vector(reader,
		                                   &state->awareness_events[i].pos))
			return 0;
		state->awareness_events[i].type = value;
	}
	return d1_save_translate_read_vector(reader, &state->believed_player_pos);
}

static int d1_save_translate_validate_d1_ai_state(
	d1_save_translate_ai_state *state, const object *objects,
	int object_count)
{
	int i;

	if (!state || !objects || state->point_seg_free_index < 0 ||
	    state->point_seg_free_index > MAX_POINT_SEGS ||
	    state->awareness_count < 0 ||
	    state->awareness_count > D1_SAVE_MAX_AWARENESS_EVENTS)
		return 0;
	for (i = 0; i < state->point_seg_free_index; i++)
		if (state->point_segs[i].segnum < 0 ||
		    state->point_segs[i].segnum > Highest_segment_index) {
			con_printf(CON_URGENT, "D1 checkpoint translation: invalid AI path point=%d seg=%d limit=%d\n", i, state->point_segs[i].segnum, Highest_segment_index);
			return 0;
		}
	for (i = 0; i < state->awareness_count; i++)
		if (state->awareness_events[i].segnum < 0 ||
		    state->awareness_events[i].segnum > Highest_segment_index ||
		    state->awareness_events[i].type < PA_NEARBY_ROBOT_FIRED ||
		    state->awareness_events[i].type > PA_WEAPON_ROBOT_COLLISION) {
			con_printf(CON_URGENT, "D1 checkpoint translation: invalid awareness event=%d seg=%d type=%d\n", i, state->awareness_events[i].segnum, state->awareness_events[i].type);
			return 0;
		}
	return 1;
}

static void d1_save_translate_commit_d1_ai_state(
	const d1_save_translate_ai_state *state)
{
	Ai_initialized = state->ai_initialized;
	Overall_agitation = state->overall_agitation;
	memcpy(Ai_local_info, state->local_info, sizeof(state->local_info));
	memcpy(Point_segs, state->point_segs, sizeof(state->point_segs));
	memcpy(Ai_cloak_info, state->cloak_info, sizeof(state->cloak_info));
	Boss_cloak_start_time = state->boss_cloak_start_time;
	Boss_cloak_end_time = state->boss_cloak_end_time;
	Last_teleport_time = state->last_teleport_time;
	Boss_teleport_interval = state->boss_teleport_interval;
	Boss_cloak_interval = state->boss_cloak_interval;
	Boss_cloak_duration = state->boss_cloak_duration;
	Last_gate_time = state->last_gate_time;
	Gate_interval = state->gate_interval;
	Boss_dying_start_time = state->boss_dying_start_time;
	Boss_dying = state->boss_dying;
	Boss_dying_sound_playing = state->boss_dying_sound_playing;
	Point_segs_free_ptr = &Point_segs[state->point_seg_free_index];
	Num_awareness_events = state->awareness_count;
	memcpy(Awareness_events, state->awareness_events,
	       (size_t)state->awareness_count * sizeof(state->awareness_events[0]));
	Believed_player_pos = state->believed_player_pos;
	Believed_player_seg = -1;
	Last_fired_upon_player_pos = state->believed_player_pos;
}

static int d1_save_translate_read_d1_wall(d1_save_translate_reader *reader,
                                          wall *out)
{
	short pad;

	if (!out)
		return 0;
	memset(out, 0, sizeof(*out));
	if (!d1_save_translate_read_s32(reader, &out->segnum) ||
	    !d1_save_translate_read_s32(reader, &out->sidenum) ||
	    !d1_save_translate_read_fix(reader, &out->hps) ||
	    !d1_save_translate_read_s32(reader, &out->linked_wall) ||
	    !d1_save_translate_read_u8(reader, &out->type) ||
	    !d1_save_translate_read_u8(reader, &out->flags) ||
	    !d1_save_translate_read_u8(reader, &out->state) ||
	    !d1_save_translate_read_s8(reader, &out->trigger) ||
	    !d1_save_translate_read_s8(reader, &out->clip_num) ||
	    !d1_save_translate_read_u8(reader, &out->keys) ||
	    !d1_save_translate_read_s16(reader, &pad))
		return 0;
	if (out->linked_wall == 65535)
		out->linked_wall = -1;
	out->controlling_trigger = -1;
	out->cloak_value = 0;
	return 1;
}

static int d1_save_translate_read_d1_active_door(
    d1_save_translate_reader *reader, active_door *out)
{
	if (!out)
		return 0;
	memset(out, 0, sizeof(*out));
	return d1_save_translate_read_s32(reader, &out->n_parts) &&
	       d1_save_translate_read_s16(reader, &out->front_wallnum[0]) &&
	       d1_save_translate_read_s16(reader, &out->front_wallnum[1]) &&
	       d1_save_translate_read_s16(reader, &out->back_wallnum[0]) &&
	       d1_save_translate_read_s16(reader, &out->back_wallnum[1]) &&
	       d1_save_translate_read_fix(reader, &out->time);
}

static ubyte d1_save_translate_trigger_type(short d1_flags)
{
	if (d1_flags & TRIGGER_EXIT)
		return TT_EXIT;
	if (d1_flags & TRIGGER_SECRET_EXIT)
		return TT_SECRET_EXIT;
	if (d1_flags & TRIGGER_CONTROL_DOORS)
		return TT_OPEN_DOOR;
	if (d1_flags & TRIGGER_MATCEN)
		return TT_MATCEN;
	if (d1_flags & TRIGGER_ILLUSION_ON)
		return TT_ILLUSION_ON;
	if (d1_flags & TRIGGER_ILLUSION_OFF)
		return TT_ILLUSION_OFF;
	return TT_OPEN_DOOR;
}

static int d1_save_translate_read_d1_trigger(d1_save_translate_reader *reader,
                                             trigger *out)
{
	int i;
	sbyte unused_type;
	sbyte unused_link_num;
	short d1_flags;

	if (!out)
		return 0;
	memset(out, 0, sizeof(*out));
	if (!d1_save_translate_read_s8(reader, &unused_type) ||
	    !d1_save_translate_read_s16(reader, &d1_flags) ||
	    !d1_save_translate_read_fix(reader, &out->value) ||
	    !d1_save_translate_read_fix(reader, &out->time) ||
	    !d1_save_translate_read_s8(reader, &unused_link_num))
		return 0;
	out->type = d1_save_translate_trigger_type(d1_flags);
	if (d1_flags & TRIGGER_ONE_SHOT)
		out->flags |= TF_ONE_SHOT;
	if (!(d1_flags & TRIGGER_ON))
		out->flags |= TF_DISABLED;
	if (!d1_save_translate_read_s16(reader, &d1_flags))
		return 0;
	out->num_links = (sbyte) d1_flags;
	for (i = 0; i < MAX_WALLS_PER_LINK; i++)
		if (!d1_save_translate_read_s16(reader, &out->seg[i]))
			return 0;
	for (i = 0; i < MAX_WALLS_PER_LINK; i++)
		if (!d1_save_translate_read_s16(reader, &out->side[i]))
			return 0;
	return 1;
}

static int d1_save_translate_read_d1_matcen(d1_save_translate_reader *reader,
                                            matcen_info *out)
{
	if (!out)
		return 0;
	memset(out, 0, sizeof(*out));
	return d1_save_translate_read_s32(reader, &out->robot_flags[0]) &&
	       d1_save_translate_read_fix(reader, &out->hit_points) &&
	       d1_save_translate_read_fix(reader, &out->interval) &&
	       d1_save_translate_read_s16(reader, &out->segnum) &&
	       d1_save_translate_read_s16(reader, &out->fuelcen_num);
}

static int d1_save_translate_read_d1_control_center_triggers(
    d1_save_translate_reader *reader, control_center_triggers *out)
{
	int i;

	if (!out)
		return 0;
	memset(out, 0, sizeof(*out));
	if (!d1_save_translate_read_s16(reader, &out->num_links))
		return 0;
	for (i = 0; i < MAX_CONTROLCEN_LINKS; i++)
		if (!d1_save_translate_read_s16(reader, &out->seg[i]))
			return 0;
	for (i = 0; i < MAX_CONTROLCEN_LINKS; i++)
		if (!d1_save_translate_read_s16(reader, &out->side[i]))
			return 0;
	return 1;
}

static int d1_save_translate_read_d1_fuelcen(d1_save_translate_reader *reader,
                                             FuelCenter *out)
{
	if (!out)
		return 0;
	memset(out, 0, sizeof(*out));
	return d1_save_translate_read_s32(reader, &out->Type) &&
	       d1_save_translate_read_s32(reader, &out->segnum) &&
	       d1_save_translate_read_s8(reader, &out->Flag) &&
	       d1_save_translate_read_s8(reader, &out->Enabled) &&
	       d1_save_translate_read_s8(reader, &out->Lives) &&
	       d1_save_translate_read_s8(reader, &out->dum1) &&
	       d1_save_translate_read_fix(reader, &out->Capacity) &&
	       d1_save_translate_read_fix(reader, &out->MaxCapacity) &&
	       d1_save_translate_read_fix(reader, &out->Timer) &&
	       d1_save_translate_read_fix(reader, &out->Disable_time) &&
	       d1_save_translate_read_vector(reader, &out->Center);
}

static int d1_save_translate_validate_world_state(
	const d1_save_translate_world_state *state, const object *objects,
	int object_count)
{
	int i, j;

	if (!state || !objects)
		return 0;
	for (i = 0; i < state->num_walls; i++) {
		const wall *wallp = &state->walls[i];
		if (wallp->segnum < 0 || wallp->segnum > Highest_segment_index ||
		    wallp->sidenum < 0 || wallp->sidenum >= MAX_SIDES_PER_SEGMENT ||
		    (wallp->linked_wall != -1 &&
		     (wallp->linked_wall < 0 || wallp->linked_wall >= state->num_walls)) ||
		    wallp->type > WALL_CLOAKED || wallp->state > WALL_DOOR_DECLOAKING ||
		    (wallp->trigger != -1 &&
		     (wallp->trigger < 0 || wallp->trigger >= state->num_triggers)) ||
		    (wallp->clip_num != -1 &&
		     (wallp->clip_num < 0 || wallp->clip_num >= Num_wall_anims)) ||
		    (wallp->keys != KEY_NONE && wallp->keys != KEY_BLUE &&
		     wallp->keys != KEY_RED && wallp->keys != KEY_GOLD))
			return 0;
		if (state->sides[wallp->segnum][wallp->sidenum].wall_num != i)
			return 0;
	}
	for (i = 0; i < state->num_open_doors; i++) {
		const active_door *door = &state->active_doors[i];
		if (door->n_parts < 1 || door->n_parts > 2)
			return 0;
		for (j = 0; j < door->n_parts; j++)
			if (door->front_wallnum[j] < 0 ||
			    door->front_wallnum[j] >= state->num_walls ||
			    door->back_wallnum[j] < 0 ||
			    door->back_wallnum[j] >= state->num_walls)
				return 0;
	}
	for (i = 0; i < state->num_triggers; i++) {
		const trigger *triggerp = &state->triggers[i];
		if (triggerp->type >= NUM_TRIGGER_TYPES || triggerp->num_links < 0 ||
		    triggerp->num_links > MAX_WALLS_PER_LINK)
			return 0;
		for (j = 0; j < triggerp->num_links; j++)
			if (triggerp->seg[j] < 0 || triggerp->seg[j] > Highest_segment_index ||
			    triggerp->side[j] < 0 || triggerp->side[j] >= MAX_SIDES_PER_SEGMENT)
				return 0;
	}
	for (i = 0; i <= Highest_segment_index; i++)
		for (j = 0; j < MAX_SIDES_PER_SEGMENT; j++) {
			const d1_save_translate_side_state *sidep = &state->sides[i][j];
			if ((sidep->wall_num != -1 &&
			     (sidep->wall_num < 0 || sidep->wall_num >= state->num_walls)) ||
			    sidep->tmap_num < 0 || sidep->tmap_num >= NumTextures ||
			    (sidep->tmap_num2 & 0x3fff) >= NumTextures)
				return 0;
			if (sidep->wall_num != -1 &&
			    (state->walls[sidep->wall_num].segnum != i ||
			     state->walls[sidep->wall_num].sidenum != j))
				return 0;
		}
	for (i = 0; i < state->num_fuelcenters; i++)
		if (state->stations[i].Type < SEGMENT_IS_NOTHING ||
		    state->stations[i].Type > SEGMENT_IS_GOAL_RED ||
		    state->stations[i].segnum < 0 ||
		    state->stations[i].segnum > Highest_segment_index ||
		    Segments[state->stations[i].segnum].special != state->stations[i].Type)
			return 0;
	for (i = 0; i < state->num_robot_centers; i++)
		if (state->robot_centers[i].segnum < 0 ||
		    state->robot_centers[i].segnum > Highest_segment_index ||
		    state->robot_centers[i].fuelcen_num < 0 ||
		    state->robot_centers[i].fuelcen_num >= state->num_fuelcenters ||
		    state->stations[state->robot_centers[i].fuelcen_num].segnum !=
		        state->robot_centers[i].segnum ||
		    Segments[state->robot_centers[i].segnum].special != SEGMENT_IS_ROBOTMAKER)
			return 0;
	if (state->control_center_triggers.num_links < 0 ||
	    state->control_center_triggers.num_links > MAX_CONTROLCEN_LINKS)
		return 0;
	for (i = 0; i < state->control_center_triggers.num_links; i++)
		if (state->control_center_triggers.seg[i] < 0 ||
		    state->control_center_triggers.seg[i] > Highest_segment_index ||
		    state->control_center_triggers.side[i] < 0 ||
		    state->control_center_triggers.side[i] >= MAX_SIDES_PER_SEGMENT)
			return 0;
	if (state->dead_controlcen_object_num != -1 &&
	    (state->dead_controlcen_object_num < 0 ||
	     state->dead_controlcen_object_num >= object_count ||
	     objects[state->dead_controlcen_object_num].type != OBJ_CNTRLCEN))
		return 0;
	return 1;
}

static int d1_save_translate_read_d1_world_state(
	d1_save_translate_reader *reader, d1_save_translate_world_state *state)
{
	int i;
	int j;

	if (!reader || !state)
		return 0;
	memset(state, 0, sizeof(*state));
	if (!d1_save_translate_read_s32(reader, &state->num_walls) ||
	    state->num_walls < 0 || state->num_walls > MAX_WALLS)
		return 0;
	for (i = 0; i < state->num_walls; i++)
		if (!d1_save_translate_read_d1_wall(reader, &state->walls[i]))
			return 0;

	if (!d1_save_translate_read_s32(reader, &state->num_open_doors) ||
	    state->num_open_doors < 0 || state->num_open_doors > MAX_DOORS)
		return 0;
	for (i = 0; i < state->num_open_doors; i++)
		if (!d1_save_translate_read_d1_active_door(reader,
		                                            &state->active_doors[i]))
			return 0;

	if (!d1_save_translate_read_s32(reader, &state->num_triggers) ||
	    state->num_triggers < 0 || state->num_triggers > MAX_TRIGGERS)
		return 0;
	for (i = 0; i < state->num_triggers; i++)
		if (!d1_save_translate_read_d1_trigger(reader, &state->triggers[i]))
			return 0;

	for (i = 0; i <= Highest_segment_index; i++) {
		for (j = 0; j < MAX_SIDES_PER_SEGMENT; j++) {
			short tmap_num;
			short tmap_num2;
			if (!d1_save_translate_read_s16(reader, &state->sides[i][j].wall_num) ||
			    !d1_save_translate_read_s16(reader,
			                                &tmap_num) ||
			    !d1_save_translate_read_s16(reader,
			                                &tmap_num2))
				return 0;
			state->sides[i][j].tmap_num = convert_d1_tmap_num(tmap_num);
			state->sides[i][j].tmap_num2 =
				tmap_num2 ? convert_d1_tmap_num(tmap_num2) : 0;
		}
	}

	if (!d1_save_translate_read_s32(reader, &state->control_center_destroyed) ||
	    !d1_save_translate_read_s32(reader, &state->countdown_seconds_left) ||
	    !d1_save_translate_read_s32(reader, &state->num_robot_centers) ||
	    state->num_robot_centers < 0 ||
	    state->num_robot_centers > MAX_ROBOT_CENTERS)
		return 0;
	for (i = 0; i < state->num_robot_centers; i++)
		if (!d1_save_translate_read_d1_matcen(reader,
		                                      &state->robot_centers[i]))
			return 0;
	if (!d1_save_translate_read_d1_control_center_triggers(
	        reader, &state->control_center_triggers) ||
	    !d1_save_translate_read_s32(reader, &state->num_fuelcenters) ||
	    state->num_fuelcenters < 0 ||
	    state->num_fuelcenters > MAX_NUM_FUELCENS)
		return 0;
	for (i = 0; i < state->num_fuelcenters; i++) {
		if (!d1_save_translate_read_d1_fuelcen(reader, &state->stations[i]))
			return 0;
		if (state->stations[i].Type == SEGMENT_IS_CONTROLCEN)
			state->countdown_timer = state->stations[i].Timer;
	}

	return d1_save_translate_read_s32(reader, &state->control_center_been_hit) &&
	       d1_save_translate_read_s32(reader,
	                                  &state->control_center_player_been_seen) &&
	       d1_save_translate_read_s32(reader,
	                                  &state->control_center_next_fire_time) &&
	       d1_save_translate_read_s32(reader, &state->control_center_present) &&
	       d1_save_translate_read_s32(reader,
	                                  &state->dead_controlcen_object_num);
}

static void d1_save_translate_commit_d1_world_state(
	const d1_save_translate_world_state *state)
{
	int i, j;

	Num_walls = state->num_walls;
	memcpy(Walls, state->walls, sizeof(state->walls));
	Num_open_doors = state->num_open_doors;
	memcpy(ActiveDoors, state->active_doors, sizeof(state->active_doors));
	Num_triggers = state->num_triggers;
	memcpy(Triggers, state->triggers, sizeof(state->triggers));
	for (i = 0; i <= Highest_segment_index; i++)
		for (j = 0; j < MAX_SIDES_PER_SEGMENT; j++) {
			Segments[i].sides[j].wall_num = state->sides[i][j].wall_num;
			Segments[i].sides[j].tmap_num = state->sides[i][j].tmap_num;
			Segments[i].sides[j].tmap_num2 = state->sides[i][j].tmap_num2;
		}
	Control_center_destroyed = state->control_center_destroyed;
	Countdown_seconds_left = state->countdown_seconds_left;
	Num_robot_centers = state->num_robot_centers;
	memcpy(RobotCenters, state->robot_centers, sizeof(state->robot_centers));
	ControlCenterTriggers = state->control_center_triggers;
	Num_fuelcenters = state->num_fuelcenters;
	memcpy(Station, state->stations, sizeof(state->stations));
	Countdown_timer = state->countdown_timer;
	Control_center_been_hit = state->control_center_been_hit;
	Control_center_player_been_seen = state->control_center_player_been_seen;
	Control_center_next_fire_time = state->control_center_next_fire_time;
	Control_center_present = state->control_center_present;
	Dead_controlcen_object_num = state->dead_controlcen_object_num;
	if (Control_center_destroyed)
		Total_countdown_time = Countdown_timer / F0_5;
	else
		Total_countdown_time = 0;
}

static int d1_save_translate_read_d1_state_to_runtime(
	d1_save_translate_reader *reader, d1_save_translate_world_state *world,
	d1_save_translate_ai_state *ai)
{
	if (!d1_save_translate_read_d1_world_state(reader, world) ||
	    !d1_save_translate_read_d1_ai_state(reader, ai))
		return 0;
	if (Highest_segment_index + 1 > MAX_SEGMENTS_ORIGINAL) {
		if (!d1_save_translate_skip(reader, (size_t) Highest_segment_index + 1))
			return 0;
	} else if (!d1_save_translate_skip(reader, MAX_SEGMENTS_ORIGINAL)) {
		return 0;
	}
	return d1_save_translate_skip(reader, 6 * sizeof(int));
}

static int d1_save_translate_skip_weapon_fidelity_state(
	d1_save_translate_reader *reader, const object *objects, int object_count)
{
	int i;

	for (i = 0; i < object_count; i++) {
		if (objects[i].type == OBJ_NONE || objects[i].control_type != CT_WEAPON)
			continue;
		if (!d1_save_translate_skip(reader, sizeof(int) + MAX_OBJECTS))
			return 0;
	}
	return 1;
}

static int d1_save_translate_validate_runtime_allocator(
	const object_runtime_state *state, const object *objects, int object_count)
{
	ubyte seen[MAX_OBJECTS];
	int i;
	int live_count = 0;
	int highest_live = -1;

	if (!state || !objects || object_count <= 0 || object_count > MAX_OBJECTS ||
	    state->num_objects < 0 || state->num_objects > MAX_OBJECTS ||
	    state->highest_object_index < -1 ||
	    state->highest_object_index >= MAX_OBJECTS ||
	    state->signature_seed < 0 ||
	    (state->do_homer_frame != 0 && state->do_homer_frame != 1))
		return 0;
	memset(seen, 0, sizeof(seen));
	for (i = 0; i < MAX_OBJECTS; i++)
		if (i < object_count && objects[i].type != OBJ_NONE) {
			live_count++;
			highest_live = i;
		}
	if (state->num_objects != live_count ||
	    state->highest_object_index != highest_live)
		return 0;
	for (i = state->num_objects; i < MAX_OBJECTS; i++) {
		int objnum = state->free_obj_list[i];
		if (objnum < 0 || objnum >= MAX_OBJECTS || seen[objnum] ||
		    (objnum < object_count && objects[objnum].type != OBJ_NONE))
			return 0;
		seen[objnum] = 1;
	}
	for (i = 0; i < MAX_OBJECTS; i++)
		if ((i >= object_count || objects[i].type == OBJ_NONE) != (seen[i] != 0))
			return 0;
	return 1;
}

static int d1_save_translate_skip_morph_state(d1_save_translate_reader *reader)
{
	int active_morphs;
	size_t morph_bytes;

	if (!d1_save_translate_read_s32(reader, &active_morphs) ||
	    active_morphs < 0 || active_morphs > MAX_MORPH_OBJECTS)
		return 0;
	morph_bytes = sizeof(int) * 2 + (size_t) MAX_VECS * sizeof(vms_vector) * 2 +
	              (size_t) MAX_VECS * sizeof(fix) +
	              (size_t) MAX_SUBMODELS * sizeof(int) * 3 + sizeof(int) +
	              sizeof(ubyte) * 2 + sizeof(physics_info);
	return d1_save_translate_skip(reader, (size_t) active_morphs * morph_bytes);
}

static int d1_save_translate_read_runtime_state(
	d1_save_translate_reader *reader, const object *objects, int object_count,
	d1_save_translate_runtime_state *state)
{
	int i;
	int active_effects;
	fix effect_time_unused;
	fix64 effect_loop_time_unused;

	if (!reader || !objects || !state)
		return 0;
	memset(state, 0, sizeof(*state));
	if (!d1_save_translate_read_fix(reader, &state->next_laser_fire_delta) ||
	    !d1_save_translate_read_fix(reader, &state->next_missile_fire_delta) ||
	    !d1_save_translate_read_fix(reader, &state->last_laser_fired_delta) ||
	    !d1_save_translate_read_fix(reader, &state->next_flare_fire_delta) ||
	    !d1_save_translate_read_fix(reader, &state->auto_fire_fusion_delta) ||
	    !d1_save_translate_read_s32(reader,
	                                &state->global_laser_firing_count) ||
	    !d1_save_translate_read_s32(reader,
	                                &state->global_missile_firing_count) ||
	    !d1_save_translate_read_s32(reader, &state->has_rng_state) ||
	    !d1_save_translate_read_u32(reader, &state->rng_state) ||
	    !d1_save_translate_read_s32(reader, &state->has_fx_rng_state) ||
	    !d1_save_translate_read_u32(reader, &state->fx_rng_state) ||
	    !d1_save_translate_read_u32(reader, &state->fx_rng_call_count) ||
	    !d1_save_translate_read_s32(reader, &state->d_tick_state.count) ||
	    !d1_save_translate_read_s32(reader, &state->d_tick_state.step) ||
	    !d1_save_translate_read_fix(reader, &state->d_tick_state.timer) ||
	    !d1_save_translate_read_s32(reader, &state->object_state.num_objects) ||
	    !d1_save_translate_read_s32(reader,
	                                &state->object_state.highest_object_index))
		return 0;
	if (state->object_state.num_objects < 1 ||
	    state->object_state.num_objects > MAX_OBJECTS ||
	    state->object_state.highest_object_index < 0 ||
	    state->object_state.highest_object_index >= MAX_OBJECTS ||
	    !game_d_tick_state_is_valid(&state->d_tick_state))
		return 0;
	for (i = 0; i < MAX_OBJECTS; i++)
		if (!d1_save_translate_read_s16(
		        reader, &state->object_state.free_obj_list[i]))
			return 0;
	if (!d1_save_translate_read_s32(reader, &state->object_state.signature_seed) ||
	    !d1_save_translate_read_u32(reader,
	                                &state->object_state.homer_frame_count) ||
	    !d1_save_translate_read_fix(
	        reader, &state->object_state.current_homer_frame_time) ||
	    !d1_save_translate_read_s32(reader,
	                                &state->object_state.do_homer_frame) ||
	    !d1_save_translate_read_fix(reader, &state->laser_state.fusion_charge) ||
	    !d1_save_translate_read_s32(reader,
	                                &state->laser_state.spreadfire_toggle) ||
	    !d1_save_translate_read_s32(reader, &state->laser_state.missile_gun) ||
	    !d1_save_translate_read_s32(reader,
	                                &state->laser_state.proximity_dropped) ||
	    !d1_save_translate_skip_weapon_fidelity_state(reader, objects,
	                                                  object_count) ||
	    !d1_save_translate_skip_morph_state(reader) ||
	    !d1_save_translate_skip(reader, sizeof(int) + MAX_STUCK_OBJECTS * 8) ||
	    !d1_save_translate_skip(reader, sizeof(int)) ||
	    !d1_save_translate_read_s32(
	        reader, &state->ai_path_state.last_tick_garbage_collected) ||
	    !d1_save_translate_read_s16(
	        reader, &state->ai_path_state.player_path_length) ||
	    !d1_save_translate_read_s32(
	        reader, &state->ai_path_state.player_hide_index) ||
	    !d1_save_translate_read_s32(
	        reader, &state->ai_path_state.player_cur_path_index) ||
	    !d1_save_translate_read_s32(
	        reader, &state->ai_path_state.player_following_path_flag) ||
	    !d1_save_translate_read_s32(
	        reader, &state->ai_path_state.player_goal_segment) ||
	    !d1_save_translate_read_fix64(reader, &effect_loop_time_unused) ||
	    !d1_save_translate_read_s32(reader, &active_effects))
		return 0;
	if (active_effects < 0 || active_effects > Num_effects)
		return 0;
	for (i = 0; i < active_effects; i++) {
		if (!d1_save_translate_skip(reader, sizeof(int)) ||
		    !d1_save_translate_read_fix(reader, &effect_time_unused) ||
		    !d1_save_translate_skip(reader, sizeof(int) * 5))
			return 0;
	}
	if (!d1_save_translate_skip(reader,
	                            sizeof(int) + SECRET_AREA_MAX_GENERATED))
		return 0;
	if (!d1_save_translate_validate_runtime_allocator(
	        &state->object_state, objects, object_count))
		return 0;
	if (state->ai_path_state.player_goal_segment < -1 ||
	    state->ai_path_state.player_goal_segment > Highest_segment_index ||
	    state->ai_path_state.player_path_length < 0 ||
	    state->ai_path_state.player_path_length > MAX_POINT_SEGS ||
	    !laser_pending_fire_count_is_valid(state->global_laser_firing_count) ||
	    !laser_pending_fire_count_is_valid(state->global_missile_firing_count) ||
	    !laser_runtime_state_is_valid(&state->laser_state))
		return 0;
	return 1;
}

static void d1_save_translate_commit_runtime_state(
	const d1_save_translate_runtime_state *state)
{
	Next_laser_fire_time = GameTime64 + (fix64)state->next_laser_fire_delta;
	Next_missile_fire_time = GameTime64 + (fix64)state->next_missile_fire_delta;
	Last_laser_fired_time = GameTime64 + (fix64)state->last_laser_fired_delta;
	Next_flare_fire_time = GameTime64 + (fix64)state->next_flare_fire_delta;
	Auto_fire_fusion_cannon_time = GameTime64 +
	                               (fix64)state->auto_fire_fusion_delta;
	Global_laser_firing_count = state->global_laser_firing_count;
	Global_missile_firing_count = state->global_missile_firing_count;
	if (state->has_rng_state)
		d_rand_set_state(state->rng_state);
	d_rand_reset_call_count();
	if (state->has_fx_rng_state) {
		d_rand_set_stream_state(D_RNG_FX, state->fx_rng_state);
		d_rand_set_stream_call_count(D_RNG_FX, state->fx_rng_call_count);
	}
	game_set_d_tick_state(&state->d_tick_state);
	object_set_runtime_state(&state->object_state);
	laser_set_runtime_state(&state->laser_state);
	ai_path_set_runtime_state(&state->ai_path_state);
}

static int d1_save_translate_validate_runtime_ai_path(
	const d1_save_translate_runtime_state *runtime,
	const d1_save_translate_ai_state *ai)
{
	const ai_path_runtime_state *path;

	if (!runtime || !ai)
		return 0;
	path = &runtime->ai_path_state;
	if (path->player_hide_index == -1)
		return path->player_path_length == 0;
	if (path->player_hide_index < 0 || path->player_path_length <= 0 ||
	    path->player_cur_path_index < 0 ||
	    path->player_cur_path_index >= path->player_path_length ||
	    path->player_hide_index > ai->point_seg_free_index ||
	    path->player_path_length >
	        ai->point_seg_free_index - path->player_hide_index)
		return 0;
	return 1;
}

static int d1_save_translate_validate_checkpoint_object_links(
	const object *objects, int object_count)
{
	int i;

	for (i = 0; i < object_count; i++) {
		const object *obj = &objects[i];
		int cursor, steps;

		if (obj->type == OBJ_NONE)
			continue;
		if (obj->segnum < 0 || obj->segnum > Highest_segment_index)
			return 0;
		if (obj->prev != -1) {
			if (obj->prev < 0 || obj->prev >= object_count)
				return 0;
			if (objects[obj->prev].type == OBJ_NONE ||
			    objects[obj->prev].segnum != obj->segnum ||
			    objects[obj->prev].next != i)
				return 0;
		}
		if (obj->next != -1) {
			if (obj->next < 0 || obj->next >= object_count)
				return 0;
			if (objects[obj->next].type == OBJ_NONE ||
			    objects[obj->next].segnum != obj->segnum ||
			    objects[obj->next].prev != i)
				return 0;
		}

		/* More than one historical list head in a segment is harmless: the
		 * native loader used the last one.  A cycle is not harmless because
		 * engine traversals would never terminate. */
		cursor = i;
		for (steps = 0; cursor != -1 && steps <= object_count; steps++)
			cursor = objects[cursor].next;
		if (cursor != -1)
			return 0;
	}

	return 1;
}

static void d1_save_translate_rebuild_checkpoint_object_links(
	object *objects, int object_count)
{
	short heads[MAX_SEGMENTS];
	int i;

	for (i = 0; i <= Highest_segment_index; i++)
		heads[i] = -1;
	for (i = 0; i < object_count; i++) {
		object *obj = &objects[i];
		short previous_head;

		if (obj->type == OBJ_NONE)
			continue;
		previous_head = heads[obj->segnum];
		obj->prev = -1;
		obj->next = previous_head;
		if (previous_head != -1)
			objects[previous_head].prev = (short)i;
		heads[obj->segnum] = (short)i;
	}
}

static void d1_save_translate_commit_checkpoint_object_links(void)
{
	int i;

	for (i = 0; i <= Highest_segment_index; i++)
		Segments[i].objects = -1;
	for (i = 0; i <= Highest_object_index; i++) {
		Objects[i].rtype.pobj_info.alt_textures = -1;
		if (Objects[i].type != OBJ_NONE && Objects[i].prev == -1)
			Segments[Objects[i].segnum].objects = (short)i;
	}
}

int d1_save_translate_apply_checkpoint_objects(
    const uint8_t *data, size_t size,
    const d1_save_translate_checkpoint_start *start)
{
	int i;
	object *translated_objects = NULL;
	d1_save_translate_world_state *world = NULL;
	d1_save_translate_ai_state *ai = NULL;
	d1_save_translate_runtime_state *runtime = NULL;
	d1_save_translate_reader reader;
	const char *failure = "allocation";

	if (!data || !start || start->object_count <= 0 ||
	    start->object_count > MAX_OBJECTS ||
	    start->object_stream_offset >= size)
		return 0;
	translated_objects = (object *)calloc((size_t)start->object_count,
	                                      sizeof(*translated_objects));
	world = (d1_save_translate_world_state *)calloc(1, sizeof(*world));
	ai = (d1_save_translate_ai_state *)calloc(1, sizeof(*ai));
	runtime = (d1_save_translate_runtime_state *)calloc(1, sizeof(*runtime));
	if (!translated_objects || !world || !ai || !runtime)
		goto fail;
	reader.data = data;
	reader.size = size;
	reader.pos = start->object_stream_offset;
	reader.swap = start->checkpoint_swap;
	failure = "object decoding";
	for (i = 0; i < start->object_count; i++)
		if (!d1_save_translate_read_object(&reader, &translated_objects[i]))
			goto fail;
	failure = "object references";
	if (!d1_save_translate_validate_object_references(translated_objects,
	                                                 start->object_count))
		goto fail;
	/* Match the native D1 restore: preserve a complete checkpoint link graph,
	 * but rebuild legacy/stale next/prev links from safe segment membership. */
	if (!d1_save_translate_validate_checkpoint_object_links(
	        translated_objects, start->object_count)) {
		d1_save_translate_rebuild_checkpoint_object_links(
		    translated_objects, start->object_count);
	}
	failure = "world or AI decoding";
	if (!d1_save_translate_read_d1_state_to_runtime(&reader, world, ai))
		goto fail;
	failure = "world references";
	if (!d1_save_translate_validate_world_state(
	        world, translated_objects, start->object_count))
		goto fail;
	failure = "AI references";
	if (!d1_save_translate_validate_d1_ai_state(
	        ai, translated_objects, start->object_count))
		goto fail;
	failure = "runtime state";
	if (!d1_save_translate_read_runtime_state(
	        &reader, translated_objects, start->object_count, runtime))
		goto fail;
	if (!d1_save_translate_validate_runtime_ai_path(runtime, ai))
		goto fail;

	reset_objects(1);
	init_morphs();
	Do_appearance_effect = 0;
	memcpy(Objects, translated_objects,
	       (size_t)start->object_count * sizeof(*translated_objects));
	Highest_object_index = start->object_count - 1;
	d1_save_translate_commit_checkpoint_object_links();
	for (i = 0; i <= Highest_object_index; i++) {
		object *obj = &Objects[i];

		if (obj->type == OBJ_NONE)
			continue;
		if (obj->type == OBJ_ROBOT && Robot_info[obj->id].boss_flag) {
			fix save_shields = obj->shields;

			copy_defaults_to_robot(obj);
			if (save_shields > 0 && save_shields <= obj->shields)
				obj->shields = save_shields;
			else
				obj->shields /= 2;
		}
	}
	special_reset_objects();
	d1_save_translate_commit_d1_world_state(world);
	d1_save_translate_commit_d1_ai_state(ai);
	d1_save_translate_commit_runtime_state(runtime);
	free(runtime);
	free(ai);
	free(world);
	free(translated_objects);
	return 1;

fail:
	con_printf(CON_URGENT, "D1 checkpoint translation rejected %s\n", failure);
	free(runtime);
	free(ai);
	free(world);
	free(translated_objects);
	return 0;
}

void d1_save_translate_apply_checkpoint_player(
    const d1_save_translate_checkpoint_start *start,
    const char *local_player_callsign)
{
	int i;
	int objnum;

	if (!start)
		return;
	objnum = Players[Player_num].objnum;
	if (local_player_callsign && local_player_callsign[0]) {
		strncpy(Players[Player_num].callsign, local_player_callsign, CALLSIGN_LEN);
		Players[Player_num].callsign[CALLSIGN_LEN] = '\0';
	} else {
		strncpy(Players[Player_num].callsign, start->player_callsign, CALLSIGN_LEN);
		Players[Player_num].callsign[CALLSIGN_LEN] = '\0';
	}
	Players[Player_num].connected = start->connected;
	Players[Player_num].objnum = objnum;
	Players[Player_num].flags = start->flags;
	Players[Player_num].energy = start->energy;
	Players[Player_num].shields = start->shields;
	Players[Player_num].lives = start->lives;
	Players[Player_num].level = start->level;
	Players[Player_num].laser_level = start->laser_level;
	Players[Player_num].starting_level = start->starting_level;
	Players[Player_num].killer_objnum = start->killer_objnum;
	Players[Player_num].primary_weapon_flags = start->primary_weapon_flags;
	Players[Player_num].secondary_weapon_flags = start->secondary_weapon_flags;
	for (i = 0; i < MAX_PRIMARY_WEAPONS; i++)
		Players[Player_num].primary_ammo[i] =
		    i < D1_SAVE_TRANSLATE_PRIMARY_WEAPONS ? start->primary_ammo[i] : 0;
	for (i = 0; i < MAX_SECONDARY_WEAPONS; i++)
		Players[Player_num].secondary_ammo[i] =
		    i < D1_SAVE_TRANSLATE_SECONDARY_WEAPONS ? start->secondary_ammo[i] : 0;
	Players[Player_num].last_score = start->last_score;
	Players[Player_num].score = start->score;
	Players[Player_num].time_level = start->time_level;
	Players[Player_num].time_total = start->time_total;
	Players[Player_num].cloak_time = GameTime64 + start->cloak_time;
	Players[Player_num].invulnerable_time = GameTime64 + start->invulnerable_time;
	Players[Player_num].KillGoalCount = 0;
	Players[Player_num].net_killed_total = start->net_killed_total;
	Players[Player_num].net_kills_total = start->net_kills_total;
	Players[Player_num].num_kills_level = start->num_kills_level;
	Players[Player_num].num_kills_total = start->num_kills_total;
	Players[Player_num].num_robots_level = start->num_robots_level;
	Players[Player_num].num_robots_total = start->num_robots_total;
	Players[Player_num].hostages_rescued_total = start->hostages_rescued_total;
	Players[Player_num].hostages_total = start->hostages_total;
	Players[Player_num].hostages_on_board = start->hostages_on_board;
	Players[Player_num].hostages_level = start->hostages_level;
	Players[Player_num].homing_object_dist = start->homing_object_dist;
	Players[Player_num].hours_level = start->hours_level;
	Players[Player_num].hours_total = start->hours_total;
	Players[Player_num].primary_weapon = start->primary_weapon;
	Players[Player_num].secondary_weapon = start->secondary_weapon;
	select_weapon(Players[Player_num].primary_weapon, 0, 0, 0);
	select_weapon(Players[Player_num].secondary_weapon, 1, 0, 0);
	if (objnum >= 0 && objnum <= Highest_object_index) {
		object *obj = &Objects[objnum];

		ConsoleObject = obj;
		Viewer = obj;
	}
}
