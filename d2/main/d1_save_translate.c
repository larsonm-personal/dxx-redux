#include <stdio.h>
#include <string.h>

#include "ai.h"
#include "byteswap.h"
#include "cntrlcen.h"
#include "d1_save_translate.h"
#include "effects.h"
#include "fuelcen.h"
#include "game.h"
#include "laser.h"
#include "morph.h"
#include "object.h"
#include "robot.h"
#include "secretarea.h"
#include "segment.h"
#include "switch.h"
#include "wall.h"
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
#define D1_SAVE_OBJECT_MOVEMENT_UNION_BYTES sizeof(physics_info)
#define D1_SAVE_OBJECT_CONTROL_UNION_BYTES 30
#define D1_SAVE_OBJECT_RENDER_UNION_BYTES sizeof(polyobj_info)
#define D1_SAVE_AI_LOCAL_RW_BYTES 184
#define D1_SAVE_POINT_SEG_BYTES 16
#define D1_SAVE_AI_CLOAK_INFO_RW_BYTES 16
#define D1_SAVE_MAX_AI_CLOAK_INFO 8
#define D1_SAVE_MAX_AWARENESS_EVENTS 64

typedef struct d1_save_translate_reader {
	const uint8_t *data;
	size_t size;
	size_t pos;
	int swap;
} d1_save_translate_reader;

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

static int d1_save_translate_apply_d1_ai_state(d1_save_translate_reader *reader)
{
	int i;
	int point_seg_free_index;
	int awareness_count;
	int unused_s32;
	fix time_delta;

	if (!d1_save_translate_read_s32(reader, &Ai_initialized) ||
	    !d1_save_translate_read_s32(reader, &Overall_agitation))
		return 0;
	for (i = 0; i < MAX_OBJECTS; i++)
		if (!d1_save_translate_read_ai_local(reader, &Ai_local_info[i]))
			return 0;
	for (i = 0; i < MAX_POINT_SEGS; i++)
		if (!d1_save_translate_read_s32(reader, &Point_segs[i].segnum) ||
		    !d1_save_translate_read_vector(reader, &Point_segs[i].point))
			return 0;
	for (i = 0; i < D1_SAVE_MAX_AI_CLOAK_INFO; i++) {
		if (!d1_save_translate_read_fix(reader, &time_delta) ||
		    !d1_save_translate_read_vector(reader, &Ai_cloak_info[i].last_position))
			return 0;
		Ai_cloak_info[i].last_time = GameTime64 + (fix64) time_delta;
		Ai_cloak_info[i].last_segment = -1;
	}
	if (!d1_save_translate_read_fix(reader, &time_delta))
		return 0;
	Boss_cloak_start_time = GameTime64 + (fix64) time_delta;
	if (!d1_save_translate_read_fix(reader, &time_delta))
		return 0;
	Boss_cloak_end_time = GameTime64 + (fix64) time_delta;
	if (!d1_save_translate_read_fix(reader, &time_delta))
		return 0;
	Last_teleport_time = GameTime64 + (fix64) time_delta;
	if (!d1_save_translate_read_fix(reader, &Boss_teleport_interval) ||
	    !d1_save_translate_read_fix(reader, &Boss_cloak_interval) ||
	    !d1_save_translate_read_fix(reader, &Boss_cloak_duration) ||
	    !d1_save_translate_read_fix(reader, &time_delta))
		return 0;
	Last_gate_time = GameTime64 + (fix64) time_delta;
	if (!d1_save_translate_read_fix(reader, &Gate_interval) ||
	    !d1_save_translate_read_fix(reader, &time_delta))
		return 0;
	Boss_dying_start_time = time_delta ? GameTime64 + (fix64) time_delta : 0;
	if (!d1_save_translate_read_s32(reader, &Boss_dying) ||
	    !d1_save_translate_read_s32(reader, &unused_s32))
		return 0;
	Boss_dying_sound_playing = (sbyte) unused_s32;
	if (!d1_save_translate_skip(reader, 2 * sizeof(int)) ||
	    !d1_save_translate_read_s32(reader, &point_seg_free_index))
		return 0;
	if (point_seg_free_index >= 0 && point_seg_free_index <= MAX_POINT_SEGS)
		Point_segs_free_ptr = &Point_segs[point_seg_free_index];
	else
		return 0;
	if (!d1_save_translate_read_s32(reader, &awareness_count))
		return 0;
	if (awareness_count < 0 || awareness_count > D1_SAVE_MAX_AWARENESS_EVENTS)
		return 0;
	Num_awareness_events = 0;
	for (i = 0; i < awareness_count; i++) {
		awareness_event event;
		short value;

		if (!d1_save_translate_read_s16(reader, &value))
			return 0;
		event.segnum = value;
		if (!d1_save_translate_read_s16(reader, &value) ||
		    !d1_save_translate_read_vector(reader, &event.pos))
			return 0;
		event.type = value;
		if (Num_awareness_events < MAX_AWARENESS_EVENTS)
			Awareness_events[Num_awareness_events++] = event;
	}
	if (!d1_save_translate_read_vector(reader, &Believed_player_pos))
		return 0;
	Believed_player_seg = -1;
	Last_fired_upon_player_pos = Believed_player_pos;
	return 1;
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

static int d1_save_translate_apply_d1_world_state(
    d1_save_translate_reader *reader)
{
	int i;
	int j;

	if (!d1_save_translate_read_s32(reader, &Num_walls) ||
	    Num_walls < 0 || Num_walls > MAX_WALLS)
		return 0;
	for (i = 0; i < Num_walls; i++)
		if (!d1_save_translate_read_d1_wall(reader, &Walls[i]))
			return 0;
	for (i = Num_walls; i < MAX_WALLS; i++)
		memset(&Walls[i], 0, sizeof(Walls[i]));

	if (!d1_save_translate_read_s32(reader, &Num_open_doors) ||
	    Num_open_doors < 0 || Num_open_doors > MAX_DOORS)
		return 0;
	for (i = 0; i < Num_open_doors; i++)
		if (!d1_save_translate_read_d1_active_door(reader, &ActiveDoors[i]))
			return 0;
	for (i = Num_open_doors; i < MAX_DOORS; i++)
		memset(&ActiveDoors[i], 0, sizeof(ActiveDoors[i]));

	if (!d1_save_translate_read_s32(reader, &Num_triggers) ||
	    Num_triggers < 0 || Num_triggers > MAX_TRIGGERS)
		return 0;
	for (i = 0; i < Num_triggers; i++)
		if (!d1_save_translate_read_d1_trigger(reader, &Triggers[i]))
			return 0;
	for (i = Num_triggers; i < MAX_TRIGGERS; i++)
		memset(&Triggers[i], 0, sizeof(Triggers[i]));

	for (i = 0; i <= Highest_segment_index; i++) {
		for (j = 0; j < MAX_SIDES_PER_SEGMENT; j++) {
			if (!d1_save_translate_read_s16(reader,
			                                &Segments[i].sides[j].wall_num) ||
			    !d1_save_translate_read_s16(reader,
			                                &Segments[i].sides[j].tmap_num) ||
			    !d1_save_translate_read_s16(reader,
			                                &Segments[i].sides[j].tmap_num2))
				return 0;
		}
	}

	if (!d1_save_translate_read_s32(reader, &Control_center_destroyed) ||
	    !d1_save_translate_read_s32(reader, &Countdown_seconds_left) ||
	    !d1_save_translate_read_s32(reader, &Num_robot_centers) ||
	    Num_robot_centers < 0 || Num_robot_centers > MAX_ROBOT_CENTERS)
		return 0;
	for (i = 0; i < Num_robot_centers; i++)
		if (!d1_save_translate_read_d1_matcen(reader, &RobotCenters[i]))
			return 0;
	for (i = Num_robot_centers; i < MAX_ROBOT_CENTERS; i++)
		memset(&RobotCenters[i], 0, sizeof(RobotCenters[i]));
	if (!d1_save_translate_read_d1_control_center_triggers(
	        reader, &ControlCenterTriggers) ||
	    !d1_save_translate_read_s32(reader, &Num_fuelcenters) ||
	    Num_fuelcenters < 0 || Num_fuelcenters > MAX_NUM_FUELCENS)
		return 0;
	for (i = 0; i < Num_fuelcenters; i++) {
		if (!d1_save_translate_read_d1_fuelcen(reader, &Station[i]))
			return 0;
		if (Station[i].Type == SEGMENT_IS_CONTROLCEN)
			Countdown_timer = Station[i].Timer;
	}
	for (i = Num_fuelcenters; i < MAX_NUM_FUELCENS; i++)
		memset(&Station[i], 0, sizeof(Station[i]));

	if (!d1_save_translate_read_s32(reader, &Control_center_been_hit) ||
	    !d1_save_translate_read_s32(reader, &Control_center_player_been_seen) ||
	    !d1_save_translate_read_s32(reader, &Control_center_next_fire_time) ||
	    !d1_save_translate_read_s32(reader, &Control_center_present) ||
	    !d1_save_translate_read_s32(reader, &Dead_controlcen_object_num))
		return 0;
	if (Control_center_destroyed)
		Total_countdown_time = Countdown_timer / F0_5;
	return 1;
}

static int d1_save_translate_skip_d1_state_to_runtime(
    d1_save_translate_reader *reader)
{
	if (!d1_save_translate_apply_d1_world_state(reader) ||
	    !d1_save_translate_apply_d1_ai_state(reader))
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
    d1_save_translate_reader *reader)
{
	int i;

	for (i = 0; i <= Highest_object_index; i++) {
		if (Objects[i].type == OBJ_NONE || Objects[i].control_type != CT_WEAPON)
			continue;
		if (!d1_save_translate_skip(reader, sizeof(int) + MAX_OBJECTS))
			return 0;
	}
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

static int d1_save_translate_apply_runtime_state(
    d1_save_translate_reader *reader)
{
	int i;
	int has_rng_state;
	int has_fx_rng_state;
	int active_effects;
	uint rng_state;
	uint fx_rng_state;
	uint fx_rng_call_count;
	fix next_laser_fire_delta;
	fix next_missile_fire_delta;
	fix last_laser_fired_delta;
	fix next_flare_fire_delta;
	fix auto_fire_fusion_delta;
	fix effect_time_unused;
	fix64 effect_loop_time_unused;
	game_d_tick_state d_tick_state;
	object_runtime_state object_state;
	laser_runtime_state laser_state;
	ai_path_runtime_state ai_path_state;

	memset(&object_state, 0, sizeof(object_state));
	memset(&laser_state, 0, sizeof(laser_state));
	memset(&ai_path_state, 0, sizeof(ai_path_state));
	if (!d1_save_translate_read_fix(reader, &next_laser_fire_delta) ||
	    !d1_save_translate_read_fix(reader, &next_missile_fire_delta) ||
	    !d1_save_translate_read_fix(reader, &last_laser_fired_delta) ||
	    !d1_save_translate_read_fix(reader, &next_flare_fire_delta) ||
	    !d1_save_translate_read_fix(reader, &auto_fire_fusion_delta) ||
	    !d1_save_translate_read_s32(reader, &Global_laser_firing_count) ||
	    !d1_save_translate_read_s32(reader, &Global_missile_firing_count) ||
	    !d1_save_translate_read_s32(reader, &has_rng_state) ||
	    !d1_save_translate_read_u32(reader, &rng_state) ||
	    !d1_save_translate_read_s32(reader, &has_fx_rng_state) ||
	    !d1_save_translate_read_u32(reader, &fx_rng_state) ||
	    !d1_save_translate_read_u32(reader, &fx_rng_call_count) ||
	    !d1_save_translate_read_s32(reader, &d_tick_state.count) ||
	    !d1_save_translate_read_s32(reader, &d_tick_state.step) ||
	    !d1_save_translate_read_fix(reader, &d_tick_state.timer) ||
	    !d1_save_translate_read_s32(reader, &object_state.num_objects) ||
	    !d1_save_translate_read_s32(reader, &object_state.highest_object_index))
		return 0;
	if (object_state.num_objects < 1 ||
	    object_state.num_objects > MAX_OBJECTS ||
	    object_state.highest_object_index < 0 ||
	    object_state.highest_object_index >= MAX_OBJECTS)
		return 0;
	for (i = 0; i < MAX_OBJECTS; i++)
		if (!d1_save_translate_read_s16(reader, &object_state.free_obj_list[i]))
			return 0;
	if (!d1_save_translate_read_s32(reader, &object_state.signature_seed) ||
	    !d1_save_translate_read_u32(reader, &object_state.homer_frame_count) ||
	    !d1_save_translate_read_fix(reader, &object_state.current_homer_frame_time) ||
	    !d1_save_translate_read_s32(reader, &object_state.do_homer_frame) ||
	    !d1_save_translate_read_fix(reader, &laser_state.fusion_charge) ||
	    !d1_save_translate_read_s32(reader, &laser_state.spreadfire_toggle) ||
	    !d1_save_translate_read_s32(reader, &laser_state.missile_gun) ||
	    !d1_save_translate_read_s32(reader, &laser_state.proximity_dropped) ||
	    !d1_save_translate_skip_weapon_fidelity_state(reader) ||
	    !d1_save_translate_skip_morph_state(reader) ||
	    !d1_save_translate_skip(reader, sizeof(int) + MAX_STUCK_OBJECTS * 8) ||
	    !d1_save_translate_skip(reader, sizeof(int)) ||
	    !d1_save_translate_read_s32(reader,
	                                &ai_path_state.last_tick_garbage_collected) ||
	    !d1_save_translate_read_s16(reader, &ai_path_state.player_path_length) ||
	    !d1_save_translate_read_s32(reader, &ai_path_state.player_hide_index) ||
	    !d1_save_translate_read_s32(reader, &ai_path_state.player_cur_path_index) ||
	    !d1_save_translate_read_s32(
	        reader, &ai_path_state.player_following_path_flag) ||
	    !d1_save_translate_read_s32(reader, &ai_path_state.player_goal_segment) ||
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

	Next_laser_fire_time = GameTime64 + (fix64) next_laser_fire_delta;
	Next_missile_fire_time = GameTime64 + (fix64) next_missile_fire_delta;
	Last_laser_fired_time = GameTime64 + (fix64) last_laser_fired_delta;
	Next_flare_fire_time = GameTime64 + (fix64) next_flare_fire_delta;
	Auto_fire_fusion_cannon_time = GameTime64 + (fix64) auto_fire_fusion_delta;
	if (has_rng_state)
		d_rand_set_state(rng_state);
	d_rand_reset_call_count();
	if (has_fx_rng_state) {
		d_rand_set_stream_state(D_RNG_FX, fx_rng_state);
		d_rand_set_stream_call_count(D_RNG_FX, fx_rng_call_count);
	}
	game_set_d_tick_state(&d_tick_state);
	object_set_runtime_state(&object_state);
	laser_state.helix_orientation = 0;
	laser_state.smartmines_dropped = 0;
	laser_state.last_omega_fire_time = 0;
	laser_set_runtime_state(&laser_state);
	ai_path_state.last_buddy_polish_path_tick = 0;
	ai_path_set_runtime_state(&ai_path_state);
	return 1;
}

int d1_save_translate_apply_checkpoint_objects(
    const uint8_t *data, size_t size,
    const d1_save_translate_checkpoint_start *start)
{
	int i;
	int segnum;
	d1_save_translate_reader reader;

	if (!data || !start || start->object_count <= 0 ||
	    start->object_count > MAX_OBJECTS ||
	    start->object_stream_offset >= size)
		return 0;
	for (segnum = 0; segnum <= Highest_segment_index; segnum++)
		Segments[segnum].objects = -1;
	reset_objects(1);
	init_morphs();
	Do_appearance_effect = 0;
	reader.data = data;
	reader.size = size;
	reader.pos = start->object_stream_offset;
	reader.swap = start->checkpoint_swap;
	for (i = 0; i < start->object_count; i++)
		if (!d1_save_translate_read_object(&reader, &Objects[i]))
			return 0;
	Highest_object_index = start->object_count - 1;
	for (i = 0; i <= Highest_object_index; i++) {
		object *obj = &Objects[i];

		obj->rtype.pobj_info.alt_textures = -1;
		segnum = obj->segnum;
		obj->next = obj->prev = obj->segnum = -1;
		if (obj->type == OBJ_NONE)
			continue;
		if (segnum < 0 || segnum > Highest_segment_index)
			return 0;
		obj_link(i, segnum);
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
	if (!d1_save_translate_skip_d1_state_to_runtime(&reader) ||
	    !d1_save_translate_apply_runtime_state(&reader))
		return 0;
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
