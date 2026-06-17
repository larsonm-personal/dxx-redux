#include <stdio.h>
#include <string.h>

#include "byteswap.h"
#include "d1_save_translate.h"
#include "game.h"
#include "object.h"
#include "segment.h"
#include "weapon.h"

#define D1_SAVE_VERSION 15
#define D1_SAVE_COMPATIBLE_VERSION 6
#define D1_SAVE_DESC_LENGTH 20
#define D1_SAVE_THUMBNAIL_W 100
#define D1_SAVE_THUMBNAIL_H 50
#define D1_SAVE_THUMBNAIL_RGB_BYTES (D1_SAVE_THUMBNAIL_W * D1_SAVE_THUMBNAIL_H * 3)
#define D1_SAVE_THUMBNAIL_INDEXED_BYTES (D1_SAVE_THUMBNAIL_W * D1_SAVE_THUMBNAIL_H)
#define D1_SAVE_THUMBNAIL_PALETTE_BYTES (256 * 3)
#define D1_SAVE_THUMBNAIL_RGB_VERSION 8
#define D1_SAVE_THUMBNAIL_PALETTE_VERSION 8

typedef struct d1_save_translate_reader {
	const uint8_t *data;
	size_t size;
	size_t pos;
	int swap;
} d1_save_translate_reader;

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
	if (start->has_player_object_pose) {
		int objnum = Players[Player_num].objnum;
		if (objnum >= 0 && objnum <= Highest_object_index) {
			object *obj = &Objects[objnum];

			if (start->player_object_segnum >= 0 &&
			    start->player_object_segnum <= Highest_segment_index &&
			    obj->segnum != start->player_object_segnum)
				obj_relink(objnum, start->player_object_segnum);
			obj->type = start->player_object_type;
			obj->id = start->player_object_id;
			obj->control_type = start->player_object_control_type;
			obj->movement_type = start->player_object_movement_type;
			obj->render_type = start->player_object_render_type;
			obj->flags = start->player_object_flags;
			obj->attached_obj = start->player_object_attached_obj;
			obj->pos = start->player_object_pos;
			obj->orient = start->player_object_orient;
			obj->size = start->player_object_size;
			obj->last_pos = start->player_object_last_pos;
			obj->contains_type = start->player_object_contains_type;
			obj->contains_id = start->player_object_contains_id;
			obj->contains_count = start->player_object_contains_count;
			obj->matcen_creator = start->player_object_matcen_creator;
			obj->lifeleft = start->player_object_lifeleft;
			obj->shields = start->player_object_shields;
			if (start->has_player_object_physics) {
				obj->mtype.phys_info.velocity = start->player_object_velocity;
				obj->mtype.phys_info.thrust = start->player_object_thrust;
				obj->mtype.phys_info.mass = start->player_object_mass;
				obj->mtype.phys_info.drag = start->player_object_drag;
				obj->mtype.phys_info.brakes = start->player_object_brakes;
				obj->mtype.phys_info.rotvel = start->player_object_rotvel;
				obj->mtype.phys_info.rotthrust = start->player_object_rotthrust;
				obj->mtype.phys_info.turnroll = start->player_object_turnroll;
				obj->mtype.phys_info.flags = start->player_object_phys_flags;
			}
			ConsoleObject = obj;
			Viewer = obj;
		}
	}
}
