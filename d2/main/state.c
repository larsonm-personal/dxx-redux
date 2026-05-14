/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Functions to save/restore game state.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "pstypes.h"
#include "inferno.h"
#include "segment.h"
#include "textures.h"
#include "wall.h"
#include "object.h"
#include "gamemine.h"
#include "dxxerror.h"
#include "gamefont.h"
#include "gameseg.h"
#include "switch.h"
#include "game.h"
#include "effects.h"
#include "newmenu.h"
#include "fuelcen.h"
#include "hash.h"
#include "key.h"
#include "piggy.h"
#include "player.h"
#include "cntrlcen.h"
#include "morph.h"
#include "weapon.h"
#include "render.h"
#include "gameseq.h"
#include "gauges.h"
#include "newdemo.h"
#include "automap.h"
#include "piggy.h"
#include "paging.h"
#include "titles.h"
#include "text.h"
#include "mission.h"
#include "pcx.h"
#include "u_mem.h"
#include "args.h"
#include "ai.h"
#include "fireball.h"
#include "controls.h"
#include "laser.h"
#include "state.h"
#include "collide.h"
#include "multi.h"
#include "escort.h"
#include "gr.h"
#include "palette.h"
#ifdef OGL
#include "ogl_init.h"
#endif
#include "physfsx.h"
#include "input_demo_hooks.h"
#include "input_demo_replay.h"
#ifdef __ANDROID__
#include "android_save_meta.h"
#include "coop_save.h"
#include "coop_indicator_lines.h"
#include "android_log.h"
#include "config.h"
#endif

#define STATE_VERSION 27
#define STATE_COMPATIBLE_VERSION 20
#define STATE_RUNTIME_VERSION 23
#define STATE_FIDELITY_VERSION 23
#define STATE_EFFECT_RUNTIME_VERSION 24
#define STATE_AI_PATH_RUNTIME_VERSION 25
#define STATE_OBJECT_SIGNATURE_RUNTIME_VERSION 26
#define STATE_FX_RNG_RUNTIME_VERSION 27
// 0 - Put DGSS (Descent Game State Save) id at tof.
// 1 - Added Difficulty level save
// 2 - Added cheats.enabled flag
// 3 - Added between levels save.
// 4 - Added mission support
// 5 - Mike changed ai and object structure.
// 6 - Added buggin' cheat save
// 7 - Added other cheat saves and game_id.
// 8 - Added AI stuff for escort and thief.
// 9 - Save palette with screen shot
// 12- Saved last_was_super array
// 13- Saved palette flash stuff
// 14- Save cloaking wall stuff
// 15- Save additional ai info
// 16- Save Light_subtracted
// 17- New marker save
// 18- Took out saving of old cheat status
// 19- Saved cheats.enabled flag
// 20- First_secret_visit
// 22- Omega_charge
// 23- Store thumbnail as raw RGB instead of indexed + palette
//     Save deterministic runtime state for checkpoint fidelity:
//     transient weapon, morph, wall, reactor, afterburner state,
//     transient AI awareness and aim state
// 24- Save effect loop time and runtime effect overrides so animated
//     wall/object textures restore deterministically from saves/checkpoints
// 25- Save AI path allocator timing and player path cursors for replay checkpoints
// 26- Save object signature seed for deterministic post-checkpoint object creation
// 27- Save FX RNG state and call count for deterministic checkpoint replay

#define NUM_SAVES 10
#define THUMBNAIL_W 100
#define THUMBNAIL_H 50
#define THUMBNAIL_PALETTE_BYTES (256*3)
#define THUMBNAIL_RGB_BYTES (THUMBNAIL_W * THUMBNAIL_H * 3)
#define STATE_THUMBNAIL_PALETTE_VERSION 9
#define STATE_THUMBNAIL_RGB_VERSION 23
#define DESC_LENGTH 20

extern void apply_all_changed_light(void);

extern int Do_appearance_effect;

extern int Physics_cheat_flag;

int state_save_all_sub(char *filename, char *desc);
int state_restore_all_sub(char *filename, int secret_restore);

int state_runtime_version(void)
{
	return STATE_RUNTIME_VERSION;
}

extern int First_secret_visit;

#ifdef __ANDROID__
static uint32_t state_time_to_seconds(fix time_value, sbyte hours_value)
{
	if (hours_value < 0)
		hours_value = 0;
	if (time_value < 0)
		time_value = 0;
	return (uint32_t)hours_value * 3600u + (uint32_t)f2i(time_value);
}

static int state_read_android_save_meta(PHYSFS_file *fp, PHYSFS_sint64 file_len,
	android_save_meta_disk *meta)
{
	PHYSFS_sint64 saved_pos;
	PHYSFS_sint64 meta_start;
	android_save_meta_footer footer;

	if (file_len < (PHYSFS_sint64)sizeof(footer))
		return 0;

	saved_pos = PHYSFS_tell(fp);
	meta_start = file_len - (PHYSFS_sint64)sizeof(footer);
	if (!PHYSFS_seek(fp, meta_start))
		goto fail;
	if (PHYSFS_read(fp, &footer, sizeof(footer), 1) != 1)
		goto fail;
	if (footer.tag != ANDROID_SAVE_META_TAG ||
		footer.version != ANDROID_SAVE_META_VERSION ||
		footer.trailer_bytes != sizeof(*meta) ||
		file_len < (PHYSFS_sint64)footer.trailer_bytes)
		goto fail;
	meta_start = file_len - (PHYSFS_sint64)footer.trailer_bytes;
	if (!PHYSFS_seek(fp, meta_start))
		goto fail;
	if (PHYSFS_read(fp, meta, sizeof(*meta), 1) != 1)
		goto fail;
	if (!PHYSFS_seek(fp, saved_pos))
		return 0;
	return android_save_meta_is_valid(meta);

fail:
	PHYSFS_seek(fp, saved_pos);
	return 0;
}

static void state_android_copy_log_string(char *out, size_t out_size,
	const char *in, size_t in_size)
{
	size_t i = 0;

	if (!out_size)
		return;
	while (i + 1 < out_size && i < in_size && in[i]) {
		unsigned char ch = (unsigned char)in[i];
		out[i] = (ch >= 32 && ch < 127) ? (char)ch : '?';
		i++;
	}
	out[i] = '\0';
}

static int state_android_digest_save_file(PHYSFS_file *fp, PHYSFS_sint64 *out_len,
	unsigned *out_hash)
{
	PHYSFS_sint64 saved_pos = PHYSFS_tell(fp);
	PHYSFS_sint64 file_len = PHYSFS_fileLength(fp);
	PHYSFS_sint64 remaining = file_len;
	unsigned char buf[4096];
	unsigned hash = 2166136261u;

	if (file_len < 0 || !PHYSFS_seek(fp, 0))
		goto fail;
	while (remaining > 0) {
		PHYSFS_uint32 want = (remaining > (PHYSFS_sint64)sizeof(buf)) ?
			(PHYSFS_uint32)sizeof(buf) : (PHYSFS_uint32)remaining;
		PHYSFS_sint64 got = PHYSFS_read(fp, buf, 1, want);
		PHYSFS_uint32 i;

		if (got != want)
			goto fail;
		for (i = 0; i < want; i++)
			hash = (hash ^ buf[i]) * 16777619u;
		remaining -= got;
	}
	if (!PHYSFS_seek(fp, saved_pos))
		return 0;
	*out_len = file_len;
	*out_hash = hash;
	return 1;

fail:
	PHYSFS_seek(fp, saved_pos);
	return 0;
}

static void state_android_log_save_metadata(const char *filename, PHYSFS_file *fp)
{
	PHYSFS_sint64 file_len = PHYSFS_fileLength(fp);
	android_save_meta_disk meta;
	unsigned save_hash = 0;
	int have_digest = state_android_digest_save_file(fp, &file_len, &save_hash);
	int have_meta = state_read_android_save_meta(fp, file_len, &meta);

	if (have_meta) {
		char callsign[ANDROID_SAVE_META_CALLSIGN_LEN + 1];
		char desc[ANDROID_SAVE_META_DESC_LEN + 1];
		char mission[ANDROID_SAVE_META_MISSION_LEN + 1];
		char level_name[ANDROID_SAVE_META_LEVEL_NAME_LEN + 1];

		state_android_copy_log_string(callsign, sizeof(callsign), meta.callsign, sizeof(meta.callsign));
		state_android_copy_log_string(desc, sizeof(desc), meta.description, sizeof(meta.description));
		state_android_copy_log_string(mission, sizeof(mission), meta.mission_name, sizeof(meta.mission_name));
		state_android_copy_log_string(level_name, sizeof(level_name), meta.level_name, sizeof(meta.level_name));
		debug_log(DLOG_GAME,
			"restore save metadata: game=d2 file='%s' file_len=%lld digest_ok=%d fnv1a32=%08x meta_game=%u kind=%u wall=%llu callsign='%s' desc='%s' mission='%s' level=%d level_name='%s' level_seconds=%u total_seconds=%u thumb_format=%u thumb=%ux%u",
			filename, (long long)file_len, have_digest, save_hash, (unsigned)meta.game_id,
			(unsigned)meta.save_kind, (unsigned long long)meta.wall_clock_unix_seconds,
			callsign, desc, mission, (int)meta.level_num, level_name,
			(unsigned)meta.level_seconds, (unsigned)meta.total_seconds,
			(unsigned)meta.thumbnail_format, (unsigned)meta.thumbnail_width,
			(unsigned)meta.thumbnail_height);
	} else {
		debug_log(DLOG_GAME,
			"restore save metadata: game=d2 file='%s' file_len=%lld digest_ok=%d fnv1a32=%08x have_android_meta=0",
			filename, (long long)file_len, have_digest, save_hash);
	}
}

static int g_android_save_meta_kind = ANDROID_SAVE_META_KIND_MANUAL;
#endif

static int g_android_save_blank_thumbnail = 0;

static fix state_time_to_delta_fix(fix64 time_value)
{
	fix64 delta = time_value - GameTime64;
	fix64 min_delta = (fix64)(F1_0 * (-18000));
	fix64 max_delta = (fix64)(F1_0 * 18000);

	if (delta < min_delta)
		return (fix)min_delta;
	if (delta > max_delta)
		return (fix)max_delta;
	return (fix)delta;
}

static void state_write_time_delta(PHYSFS_file *fp, fix64 time_value)
{
	fix delta = state_time_to_delta_fix(time_value);
	PHYSFS_write(fp, &delta, sizeof(delta), 1);
}

static fix64 state_read_time_delta(PHYSFS_file *fp, int swap)
{
	return (fix64)PHYSFSX_readSXE32(fp, swap);
}

static void state_write_fix64_value(PHYSFS_file *fp, fix64 value)
{
	PHYSFS_uint32 low, high;
	PHYSFS_uint64 raw_value;

	raw_value = (PHYSFS_uint64)value;
	low = (PHYSFS_uint32)(raw_value & 0xffffffffu);
	high = (PHYSFS_uint32)(raw_value >> 32);
	PHYSFS_write(fp, &low, sizeof(low), 1);
	PHYSFS_write(fp, &high, sizeof(high), 1);
}

static fix64 state_read_fix64_value(PHYSFS_file *fp, int swap)
{
	PHYSFS_uint32 low, high;
	PHYSFS_uint64 raw_value;

	low = (PHYSFS_uint32)PHYSFSX_readSXE32(fp, swap);
	high = (PHYSFS_uint32)PHYSFSX_readSXE32(fp, swap);
	raw_value = ((PHYSFS_uint64)high << 32) | (PHYSFS_uint64)low;
	return (fix64)raw_value;
}

static void state_write_physics_info(PHYSFS_file *fp, physics_info *phys_info)
{
	PHYSFSX_writeVector(fp, &phys_info->velocity);
	PHYSFSX_writeVector(fp, &phys_info->thrust);
	PHYSFS_write(fp, &phys_info->mass, sizeof(phys_info->mass), 1);
	PHYSFS_write(fp, &phys_info->drag, sizeof(phys_info->drag), 1);
	PHYSFS_write(fp, &phys_info->brakes, sizeof(phys_info->brakes), 1);
	PHYSFSX_writeVector(fp, &phys_info->rotvel);
	PHYSFSX_writeVector(fp, &phys_info->rotthrust);
	PHYSFS_write(fp, &phys_info->turnroll, sizeof(phys_info->turnroll), 1);
	PHYSFS_write(fp, &phys_info->flags, sizeof(phys_info->flags), 1);
}

static void state_read_physics_info(PHYSFS_file *fp, int swap, physics_info *phys_info)
{
	PHYSFSX_readVectorX(fp, &phys_info->velocity, swap);
	PHYSFSX_readVectorX(fp, &phys_info->thrust, swap);
	phys_info->mass = PHYSFSX_readSXE32(fp, swap);
	phys_info->drag = PHYSFSX_readSXE32(fp, swap);
	phys_info->brakes = PHYSFSX_readSXE32(fp, swap);
	PHYSFSX_readVectorX(fp, &phys_info->rotvel, swap);
	PHYSFSX_readVectorX(fp, &phys_info->rotthrust, swap);
	phys_info->turnroll = (fixang)PHYSFSX_readSXE16(fp, swap);
	phys_info->flags = (ushort)PHYSFSX_readSXE16(fp, swap);
}

static void state_clear_stuck_object_state(void)
{
	int i;

	Num_stuck_objects = 0;
	for (i = 0; i < MAX_STUCK_OBJECTS; i++) {
		Stuck_objects[i].objnum = -1;
		Stuck_objects[i].wallnum = -1;
		Stuck_objects[i].signature = 0;
	}
}

static void state_clear_controlcen_runtime_state(void)
{
	Last_time_cc_vis_check = 0;
	controlcen_death_silence = 0;
}

static void state_clear_afterburner_runtime_state(void)
{
	memset(Last_afterburner_time, 0, sizeof(Last_afterburner_time));
}

static int state_effect_has_runtime_override(int effect_num, fix64 effect_loop_time)
{
	int frame_count;
	fix time_left;
	eclip *ec;

	ec = &Effects[effect_num];
	effect_get_loop_state(ec, effect_loop_time, &frame_count, &time_left);
	if (ec->flags & (EF_ONE_SHOT | EF_STOPPED))
		return 1;
	if (ec->segnum != -1)
		return 1;
	if (ec->frame_count != frame_count)
		return 1;
	if (ec->time_left != time_left)
		return 1;
	return 0;
}

static void state_write_effect_runtime_state(PHYSFS_file *fp, fix64 effect_loop_time)
{
	int count, dynamic_flags, i;

	count = 0;
	state_write_fix64_value(fp, effect_loop_time);
	for (i = 0; i < Num_effects; i++)
		if (state_effect_has_runtime_override(i, effect_loop_time))
			count++;

	PHYSFS_write(fp, &count, sizeof(count), 1);
	for (i = 0; i < Num_effects; i++) {
		if (!state_effect_has_runtime_override(i, effect_loop_time))
			continue;

		dynamic_flags = Effects[i].flags & (EF_ONE_SHOT | EF_STOPPED);
		PHYSFS_write(fp, &i, sizeof(i), 1);
		PHYSFS_write(fp, &Effects[i].time_left, sizeof(Effects[i].time_left), 1);
		PHYSFS_write(fp, &Effects[i].frame_count, sizeof(Effects[i].frame_count), 1);
		PHYSFS_write(fp, &dynamic_flags, sizeof(dynamic_flags), 1);
		PHYSFS_write(fp, &Effects[i].segnum, sizeof(Effects[i].segnum), 1);
		PHYSFS_write(fp, &Effects[i].sidenum, sizeof(Effects[i].sidenum), 1);
		PHYSFS_write(fp, &Effects[i].dest_bm_num, sizeof(Effects[i].dest_bm_num), 1);
	}
}

static void state_read_effect_runtime_state(PHYSFS_file *fp, int swap, int apply, int version, fix64 default_effect_loop_time)
{
	fix64 effect_loop_time;
	int count, dynamic_flags, i;

	effect_loop_time = default_effect_loop_time;
	if (version >= STATE_EFFECT_RUNTIME_VERSION)
		effect_loop_time = state_read_fix64_value(fp, swap);

	if (version < STATE_EFFECT_RUNTIME_VERSION) {
		if (apply)
			reset_special_effects_to_time(effect_loop_time);
		return;
	}

	count = PHYSFSX_readSXE32(fp, swap);
	if (apply)
		reset_special_effects_to_time(effect_loop_time);

	for (i = 0; i < count; i++) {
		int dest_bm_num, effect_num, frame_count, segnum, sidenum;
		fix time_left;

		effect_num = PHYSFSX_readSXE32(fp, swap);
		time_left = PHYSFSX_readSXE32(fp, swap);
		frame_count = PHYSFSX_readSXE32(fp, swap);
		dynamic_flags = PHYSFSX_readSXE32(fp, swap);
		segnum = PHYSFSX_readSXE32(fp, swap);
		sidenum = PHYSFSX_readSXE32(fp, swap);
		dest_bm_num = PHYSFSX_readSXE32(fp, swap);

		if (!apply)
			continue;
		if (effect_num < 0 || effect_num >= Num_effects)
			continue;

		Effects[effect_num].time_left = time_left;
		Effects[effect_num].frame_count = frame_count;
		Effects[effect_num].flags =
			(Effects[effect_num].flags & ~(EF_ONE_SHOT | EF_STOPPED)) |
			(dynamic_flags & (EF_ONE_SHOT | EF_STOPPED));
		Effects[effect_num].segnum = segnum;
		Effects[effect_num].sidenum = sidenum;
		Effects[effect_num].dest_bm_num = dest_bm_num;
	}

	if (!apply)
		return;

	for (i = 0; i < Num_effects; i++)
		effect_apply_bitmap_state(i);
}

static void state_write_weapon_fidelity_state(PHYSFS_file *fp)
{
	int i;

	for (i = 0; i <= Highest_object_index; i++) {
		if (Objects[i].type == OBJ_NONE || Objects[i].control_type != CT_WEAPON)
			continue;
		PHYSFS_write(fp, &Objects[i].ctype.laser_info.creation_framecount,
			sizeof(Objects[i].ctype.laser_info.creation_framecount), 1);
		PHYSFS_write(fp, Objects[i].ctype.laser_info.hitobj_list,
			sizeof(Objects[i].ctype.laser_info.hitobj_list[0]), MAX_OBJECTS);
	}
}

static void state_read_weapon_fidelity_state(PHYSFS_file *fp, int swap, int apply)
{
	int i;

	for (i = 0; i <= Highest_object_index; i++) {
		int creation_framecount;
		ubyte scratch_hitobj_list[MAX_OBJECTS];
		ubyte *hitobj_list;

		if (Objects[i].type == OBJ_NONE || Objects[i].control_type != CT_WEAPON)
			continue;

		creation_framecount = PHYSFSX_readSXE32(fp, swap);
		hitobj_list = apply ? Objects[i].ctype.laser_info.hitobj_list : scratch_hitobj_list;
		PHYSFS_read(fp, hitobj_list, sizeof(hitobj_list[0]), MAX_OBJECTS);
		if (apply)
			Objects[i].ctype.laser_info.creation_framecount = creation_framecount;
	}
}

static int state_morph_slot_is_active(morph_data *md)
{
	return md->obj != NULL && md->obj->type != OBJ_NONE && md->obj->signature == md->Morph_sig;
}

static void state_write_morph_state(PHYSFS_file *fp)
{
	int active_morphs = 0;
	int i, j;

	for (i = 0; i < MAX_MORPH_OBJECTS; i++)
		if (state_morph_slot_is_active(&morph_objects[i]))
			active_morphs++;

	PHYSFS_write(fp, &active_morphs, sizeof(active_morphs), 1);
	for (i = 0; i < MAX_MORPH_OBJECTS; i++) {
		int objnum;
		morph_data *md;

		if (!state_morph_slot_is_active(&morph_objects[i]))
			continue;

		md = &morph_objects[i];
		objnum = (int)(md->obj - Objects);
		PHYSFS_write(fp, &objnum, sizeof(objnum), 1);
		PHYSFS_write(fp, &md->Morph_sig, sizeof(md->Morph_sig), 1);
		for (j = 0; j < MAX_VECS; j++)
			PHYSFSX_writeVector(fp, &md->morph_vecs[j]);
		for (j = 0; j < MAX_VECS; j++)
			PHYSFSX_writeVector(fp, &md->morph_deltas[j]);
		for (j = 0; j < MAX_VECS; j++)
			PHYSFS_write(fp, &md->morph_times[j], sizeof(md->morph_times[j]), 1);
		for (j = 0; j < MAX_SUBMODELS; j++)
			PHYSFS_write(fp, &md->submodel_active[j], sizeof(md->submodel_active[j]), 1);
		for (j = 0; j < MAX_SUBMODELS; j++)
			PHYSFS_write(fp, &md->n_morphing_points[j], sizeof(md->n_morphing_points[j]), 1);
		for (j = 0; j < MAX_SUBMODELS; j++)
			PHYSFS_write(fp, &md->submodel_startpoints[j], sizeof(md->submodel_startpoints[j]), 1);
		PHYSFS_write(fp, &md->n_submodels_active, sizeof(md->n_submodels_active), 1);
		PHYSFS_write(fp, &md->morph_save_control_type, sizeof(md->morph_save_control_type), 1);
		PHYSFS_write(fp, &md->morph_save_movement_type, sizeof(md->morph_save_movement_type), 1);
		state_write_physics_info(fp, &md->morph_save_phys_info);
	}
}

static void state_read_morph_state(PHYSFS_file *fp, int swap, int apply)
{
	int active_morphs = PHYSFSX_readSXE32(fp, swap);
	int next_slot = 0;
	int i, j;

	for (i = 0; i < active_morphs; i++) {
		int objnum = PHYSFSX_readSXE32(fp, swap);
		int morph_sig = PHYSFSX_readSXE32(fp, swap);
		morph_data *md = NULL;

		if (apply && next_slot < MAX_MORPH_OBJECTS && objnum >= 0 && objnum <= Highest_object_index &&
			Objects[objnum].type != OBJ_NONE && Objects[objnum].signature == morph_sig &&
			Objects[objnum].render_type == RT_MORPH) {
			md = &morph_objects[next_slot++];
			memset(md, 0, sizeof(*md));
			md->obj = &Objects[objnum];
			md->Morph_sig = morph_sig;
		}

		for (j = 0; j < MAX_VECS; j++) {
			vms_vector value;
			PHYSFSX_readVectorX(fp, &value, swap);
			if (md)
				md->morph_vecs[j] = value;
		}
		for (j = 0; j < MAX_VECS; j++) {
			vms_vector value;
			PHYSFSX_readVectorX(fp, &value, swap);
			if (md)
				md->morph_deltas[j] = value;
		}
		for (j = 0; j < MAX_VECS; j++) {
			fix value = PHYSFSX_readSXE32(fp, swap);
			if (md)
				md->morph_times[j] = value;
		}
		for (j = 0; j < MAX_SUBMODELS; j++) {
			int value = PHYSFSX_readSXE32(fp, swap);
			if (md)
				md->submodel_active[j] = value;
		}
		for (j = 0; j < MAX_SUBMODELS; j++) {
			int value = PHYSFSX_readSXE32(fp, swap);
			if (md)
				md->n_morphing_points[j] = value;
		}
		for (j = 0; j < MAX_SUBMODELS; j++) {
			int value = PHYSFSX_readSXE32(fp, swap);
			if (md)
				md->submodel_startpoints[j] = value;
		}
		{
			int n_submodels_active = PHYSFSX_readSXE32(fp, swap);
			ubyte morph_save_control_type;
			ubyte morph_save_movement_type;
			physics_info morph_save_phys_info;

			PHYSFS_read(fp, &morph_save_control_type, sizeof(morph_save_control_type), 1);
			PHYSFS_read(fp, &morph_save_movement_type, sizeof(morph_save_movement_type), 1);
			state_read_physics_info(fp, swap, &morph_save_phys_info);
			if (md) {
				md->n_submodels_active = n_submodels_active;
				md->morph_save_control_type = morph_save_control_type;
				md->morph_save_movement_type = morph_save_movement_type;
				md->morph_save_phys_info = morph_save_phys_info;
			}
		}
	}
}

static void state_write_stuck_object_state(PHYSFS_file *fp)
{
	int i;

	PHYSFS_write(fp, &Num_stuck_objects, sizeof(Num_stuck_objects), 1);
	for (i = 0; i < MAX_STUCK_OBJECTS; i++) {
		PHYSFS_write(fp, &Stuck_objects[i].objnum, sizeof(Stuck_objects[i].objnum), 1);
		PHYSFS_write(fp, &Stuck_objects[i].wallnum, sizeof(Stuck_objects[i].wallnum), 1);
		PHYSFS_write(fp, &Stuck_objects[i].signature, sizeof(Stuck_objects[i].signature), 1);
	}
}

static void state_read_stuck_object_state(PHYSFS_file *fp, int swap, int apply)
{
	int saved_num_stuck_objects = PHYSFSX_readSXE32(fp, swap);
	int i;

	for (i = 0; i < MAX_STUCK_OBJECTS; i++) {
		short objnum = (short)PHYSFSX_readSXE16(fp, swap);
		short wallnum = (short)PHYSFSX_readSXE16(fp, swap);
		int signature = PHYSFSX_readSXE32(fp, swap);

		if (!apply)
			continue;

		Stuck_objects[i].objnum = objnum;
		Stuck_objects[i].wallnum = wallnum;
		Stuck_objects[i].signature = signature;
	}

	if (apply) {
		if (saved_num_stuck_objects < 0)
			saved_num_stuck_objects = 0;
		else if (saved_num_stuck_objects > MAX_STUCK_OBJECTS)
			saved_num_stuck_objects = MAX_STUCK_OBJECTS;
		Num_stuck_objects = saved_num_stuck_objects;
	}
}

static void state_write_controlcen_runtime_state(PHYSFS_file *fp)
{
	state_write_time_delta(fp, Last_time_cc_vis_check);
	PHYSFS_write(fp, &controlcen_death_silence, sizeof(controlcen_death_silence), 1);
}

static void state_read_controlcen_runtime_state(PHYSFS_file *fp, int swap, int apply)
{
	fix64 last_time_cc_vis_check = GameTime64 + state_read_time_delta(fp, swap);
	fix death_silence = PHYSFSX_readSXE32(fp, swap);

	if (!apply)
		return;

	Last_time_cc_vis_check = last_time_cc_vis_check;
	controlcen_death_silence = death_silence;
}

static void state_write_afterburner_runtime_state(PHYSFS_file *fp)
{
	int i;

	for (i = 0; i < MAX_OBJECTS; i++)
		state_write_time_delta(fp, Last_afterburner_time[i]);
}

static void state_read_afterburner_runtime_state(PHYSFS_file *fp, int swap, int apply)
{
	int i;

	for (i = 0; i < MAX_OBJECTS; i++) {
		fix64 last_afterburner_time = GameTime64 + state_read_time_delta(fp, swap);
		if (apply)
			Last_afterburner_time[i] = last_afterburner_time;
	}
}

static void state_log_checkpoint_allocator_snapshot(const char *label,
	const object_runtime_state *object_state)
{
	char message[256];

	if (!label || !object_state)
		return;

	snprintf(message, sizeof(message),
		"stage=%s num=%d highest=%d sig_seed=%d free0=%d free1=%d free2=%d free3=%d",
		label,
		object_state->num_objects,
		object_state->highest_object_index,
		object_state->signature_seed,
		object_state->free_obj_list[0],
		object_state->free_obj_list[1],
		object_state->free_obj_list[2],
		object_state->free_obj_list[3]);
	message[sizeof(message) - 1] = 0;
	input_demo_append_replay_probe_message("checkpoint_allocator", NULL,
		message);
}

static void state_write_runtime_state(PHYSFS_file *fp)
{
	object_runtime_state object_state;
	ai_path_runtime_state ai_path_state;
	game_d_tick_state d_tick_state;
	laser_runtime_state laser_state;
	int has_rng_state = 0;
	int has_fx_rng_state = 0;
	int i;
	unsigned int fx_rng_call_count = 0;
	unsigned int fx_rng_state = 0;
	unsigned int rng_state = 0;

	object_get_runtime_state(&object_state);
	ai_path_get_runtime_state(&ai_path_state);
	game_get_d_tick_state(&d_tick_state);
	laser_get_runtime_state(&laser_state);
	has_rng_state = d_rand_get_state(&rng_state);
	has_fx_rng_state = d_rand_get_stream_state(D_RNG_FX, &fx_rng_state);
	fx_rng_call_count = d_rand_get_stream_call_count(D_RNG_FX);

	state_write_time_delta(fp, Next_laser_fire_time);
	state_write_time_delta(fp, Next_missile_fire_time);
	state_write_time_delta(fp, Last_laser_fired_time);
	state_write_time_delta(fp, Next_flare_fire_time);
	state_write_time_delta(fp, Auto_fire_fusion_cannon_time);
	PHYSFS_write(fp, &Global_laser_firing_count, sizeof(Global_laser_firing_count), 1);
	PHYSFS_write(fp, &Global_missile_firing_count, sizeof(Global_missile_firing_count), 1);
	PHYSFS_write(fp, &has_rng_state, sizeof(has_rng_state), 1);
	PHYSFS_write(fp, &rng_state, sizeof(rng_state), 1);
	PHYSFS_write(fp, &has_fx_rng_state, sizeof(has_fx_rng_state), 1);
	PHYSFS_write(fp, &fx_rng_state, sizeof(fx_rng_state), 1);
	PHYSFS_write(fp, &fx_rng_call_count, sizeof(fx_rng_call_count), 1);
	PHYSFS_write(fp, &d_tick_state.count, sizeof(d_tick_state.count), 1);
	PHYSFS_write(fp, &d_tick_state.step, sizeof(d_tick_state.step), 1);
	PHYSFS_write(fp, &d_tick_state.timer, sizeof(d_tick_state.timer), 1);
	PHYSFS_write(fp, &object_state.num_objects, sizeof(object_state.num_objects), 1);
	PHYSFS_write(fp, &object_state.highest_object_index, sizeof(object_state.highest_object_index), 1);
	for (i = 0; i < MAX_OBJECTS; i++)
		PHYSFS_write(fp, &object_state.free_obj_list[i], sizeof(object_state.free_obj_list[i]), 1);
	PHYSFS_write(fp, &object_state.signature_seed, sizeof(object_state.signature_seed), 1);
	PHYSFS_write(fp, &object_state.homer_frame_count, sizeof(object_state.homer_frame_count), 1);
	PHYSFS_write(fp, &object_state.current_homer_frame_time, sizeof(object_state.current_homer_frame_time), 1);
	PHYSFS_write(fp, &object_state.do_homer_frame, sizeof(object_state.do_homer_frame), 1);
	PHYSFS_write(fp, &laser_state.fusion_charge, sizeof(laser_state.fusion_charge), 1);
	PHYSFS_write(fp, &laser_state.spreadfire_toggle, sizeof(laser_state.spreadfire_toggle), 1);
	PHYSFS_write(fp, &laser_state.missile_gun, sizeof(laser_state.missile_gun), 1);
	PHYSFS_write(fp, &laser_state.proximity_dropped, sizeof(laser_state.proximity_dropped), 1);
	PHYSFS_write(fp, &laser_state.helix_orientation, sizeof(laser_state.helix_orientation), 1);
	PHYSFS_write(fp, &laser_state.smartmines_dropped, sizeof(laser_state.smartmines_dropped), 1);
	state_write_time_delta(fp, laser_state.last_omega_fire_time);
	state_write_weapon_fidelity_state(fp);
	state_write_morph_state(fp);
	state_write_stuck_object_state(fp);
	state_write_controlcen_runtime_state(fp);
	state_write_afterburner_runtime_state(fp);
	PHYSFS_write(fp, &ai_path_state.last_tick_garbage_collected, sizeof(ai_path_state.last_tick_garbage_collected), 1);
	PHYSFS_write(fp, &ai_path_state.last_buddy_polish_path_tick, sizeof(ai_path_state.last_buddy_polish_path_tick), 1);
	PHYSFS_write(fp, &ai_path_state.player_path_length, sizeof(ai_path_state.player_path_length), 1);
	PHYSFS_write(fp, &ai_path_state.player_hide_index, sizeof(ai_path_state.player_hide_index), 1);
	PHYSFS_write(fp, &ai_path_state.player_cur_path_index, sizeof(ai_path_state.player_cur_path_index), 1);
	PHYSFS_write(fp, &ai_path_state.player_following_path_flag, sizeof(ai_path_state.player_following_path_flag), 1);
	PHYSFS_write(fp, &ai_path_state.player_goal_segment, sizeof(ai_path_state.player_goal_segment), 1);
	state_write_effect_runtime_state(fp, GameTime64);
}

static void state_read_runtime_state(PHYSFS_file *fp, int swap, int secret_restore, int version)
{
	object_runtime_state object_state;
	ai_path_runtime_state ai_path_state;
	game_d_tick_state d_tick_state;
	laser_runtime_state laser_state;
	int apply_runtime_state = !secret_restore;
	fix64 next_laser_fire_time = GameTime64 + state_read_time_delta(fp, swap);
	fix64 next_missile_fire_time = GameTime64 + state_read_time_delta(fp, swap);
	fix64 last_laser_fired_time = GameTime64 + state_read_time_delta(fp, swap);
	fix64 next_flare_fire_time = GameTime64 + state_read_time_delta(fp, swap);
	fix64 auto_fire_fusion_cannon_time = GameTime64 + state_read_time_delta(fp, swap);
	int have_legacy_fx_rng_seed = 0;
	int has_rng_state;
	int has_fx_rng_state = 0;
	int i;
	unsigned int fx_rng_call_count = 0;
	unsigned int fx_rng_state = 0;
	unsigned int legacy_fx_rng_call_count = 0;
	unsigned int legacy_fx_rng_state = 0;
	unsigned int rng_state;

	Global_laser_firing_count = PHYSFSX_readSXE32(fp, swap);
	Global_missile_firing_count = PHYSFSX_readSXE32(fp, swap);
	has_rng_state = PHYSFSX_readSXE32(fp, swap);
	rng_state = (unsigned int)PHYSFSX_readSXE32(fp, swap);
	if (version >= STATE_FX_RNG_RUNTIME_VERSION) {
		has_fx_rng_state = PHYSFSX_readSXE32(fp, swap);
		fx_rng_state = (unsigned int)PHYSFSX_readSXE32(fp, swap);
		fx_rng_call_count = (unsigned int)PHYSFSX_readSXE32(fp, swap);
	}
	d_tick_state.count = PHYSFSX_readSXE32(fp, swap);
	d_tick_state.step = PHYSFSX_readSXE32(fp, swap);
	d_tick_state.timer = PHYSFSX_readSXE32(fp, swap);
	object_state.num_objects = PHYSFSX_readSXE32(fp, swap);
	object_state.highest_object_index = PHYSFSX_readSXE32(fp, swap);
	for (i = 0; i < MAX_OBJECTS; i++)
		object_state.free_obj_list[i] = (short)PHYSFSX_readSXE16(fp, swap);
	if (version >= STATE_OBJECT_SIGNATURE_RUNTIME_VERSION)
		object_state.signature_seed = PHYSFSX_readSXE32(fp, swap);
	else
		object_state.signature_seed = 0;
	object_state.homer_frame_count = (unsigned int)PHYSFSX_readSXE32(fp, swap);
	object_state.current_homer_frame_time = PHYSFSX_readSXE32(fp, swap);
	object_state.do_homer_frame = PHYSFSX_readSXE32(fp, swap);
	laser_state.fusion_charge = PHYSFSX_readSXE32(fp, swap);
	laser_state.spreadfire_toggle = PHYSFSX_readSXE32(fp, swap);
	laser_state.missile_gun = PHYSFSX_readSXE32(fp, swap);
	laser_state.proximity_dropped = PHYSFSX_readSXE32(fp, swap);
	laser_state.helix_orientation = PHYSFSX_readSXE32(fp, swap);
	laser_state.smartmines_dropped = PHYSFSX_readSXE32(fp, swap);
	laser_state.last_omega_fire_time = (int)(GameTime64 + state_read_time_delta(fp, swap));
	if (version >= STATE_FIDELITY_VERSION) {
		state_read_weapon_fidelity_state(fp, swap, apply_runtime_state);
		state_read_morph_state(fp, swap, apply_runtime_state);
		state_read_stuck_object_state(fp, swap, apply_runtime_state);
		state_read_controlcen_runtime_state(fp, swap, apply_runtime_state);
		state_read_afterburner_runtime_state(fp, swap, apply_runtime_state);
	}
	if (version >= STATE_AI_PATH_RUNTIME_VERSION) {
		ai_path_state.last_tick_garbage_collected = PHYSFSX_readSXE32(fp, swap);
		ai_path_state.last_buddy_polish_path_tick = PHYSFSX_readSXE32(fp, swap);
		ai_path_state.player_path_length = PHYSFSX_readSXE16(fp, swap);
		ai_path_state.player_hide_index = PHYSFSX_readSXE32(fp, swap);
		ai_path_state.player_cur_path_index = PHYSFSX_readSXE32(fp, swap);
		ai_path_state.player_following_path_flag = PHYSFSX_readSXE32(fp, swap);
		ai_path_state.player_goal_segment = PHYSFSX_readSXE32(fp, swap);
	}
	state_read_effect_runtime_state(fp, swap, apply_runtime_state, version, GameTime64);

	if (secret_restore)
		return;

	Next_laser_fire_time = next_laser_fire_time;
	Next_missile_fire_time = next_missile_fire_time;
	Last_laser_fired_time = last_laser_fired_time;
	Next_flare_fire_time = next_flare_fire_time;
	Auto_fire_fusion_cannon_time = auto_fire_fusion_cannon_time;
	if (has_rng_state)
		d_rand_set_state(rng_state);
	d_rand_reset_call_count();
	if (has_fx_rng_state) {
		d_rand_set_stream_state(D_RNG_FX, fx_rng_state);
		d_rand_set_stream_call_count(D_RNG_FX, fx_rng_call_count);
	} else if (input_demo_replay_has_checkpoint()) {
		have_legacy_fx_rng_seed = input_demo_replay_get_legacy_fx_rng_seed(
			&legacy_fx_rng_state,
			&legacy_fx_rng_call_count);
		if (have_legacy_fx_rng_seed) {
			d_rand_set_stream_state(D_RNG_FX, legacy_fx_rng_state);
			d_rand_set_stream_call_count(D_RNG_FX, legacy_fx_rng_call_count);
		}
	}
	game_set_d_tick_state(&d_tick_state);
	object_set_runtime_state(&object_state);
	state_log_checkpoint_allocator_snapshot("post_apply", &object_state);
	if (version >= STATE_AI_PATH_RUNTIME_VERSION)
		ai_path_set_runtime_state(&ai_path_state);
	laser_set_runtime_state(&laser_state);
	rebuild_guided_missile_state();
	if (input_demo_replay_has_checkpoint())
		input_demo_log_checkpoint_runtime_restore(
			GameTime64,
			Next_laser_fire_time - GameTime64,
			Next_missile_fire_time - GameTime64,
			Last_laser_fired_time - GameTime64,
			Next_flare_fire_time - GameTime64,
			Auto_fire_fusion_cannon_time - GameTime64,
			Global_laser_firing_count,
			Global_missile_firing_count,
			laser_state.spreadfire_toggle,
			laser_state.missile_gun,
			laser_state.helix_orientation,
			laser_state.proximity_dropped,
			laser_state.smartmines_dropped,
			laser_state.last_omega_fire_time - GameTime64,
			d_tick_state.count,
			d_tick_state.step,
			d_tick_state.timer,
			rng_state,
			has_rng_state);
}

static void state_log_checkpoint_ai_restore_state(void)
{
	int i;

	if (!input_demo_replay_is_loaded() || !input_demo_replay_has_checkpoint())
		return;

	for (i = 0; i <= Highest_object_index; i++) {
		object *obj = &Objects[i];
		ai_local *ailp;
		char probe[768];

		if ((obj->type != OBJ_ROBOT) || (obj->control_type != CT_AI))
			continue;

		ailp = &Ai_local_info[i];
		snprintf(probe, sizeof(probe),
			"behavior=%d mode=%d cur_state=%d goal_state=%d gun=%d "
			"player_seg=%d believed_seg=%d goal_seg=%d prev_vis=%d aware=%d "
			"aware_time=%d retry=%d retry_chain=%d rapid=%d skip=%d "
			"seen=%lld since=%d next_action=%d next_fire=%d next_fire2=%d "
			"path_index=%d path_length=%d hide=%d dir=%d pos=(%d,%d,%d) vel=(%d,%d,%d)",
			obj->ctype.ai_info.behavior,
			ailp->mode,
			obj->ctype.ai_info.CURRENT_STATE,
			obj->ctype.ai_info.GOAL_STATE,
			obj->ctype.ai_info.CURRENT_GUN,
			ConsoleObject ? ConsoleObject->segnum : -1,
			Believed_player_seg,
			ailp->goal_segment,
			ailp->previous_visibility,
			ailp->player_awareness_type,
			ailp->player_awareness_time,
			ailp->retry_count,
			ailp->consecutive_retries,
			ailp->rapidfire_count,
			obj->ctype.ai_info.SKIP_AI_COUNT,
			(long long)ailp->time_player_seen,
			ailp->time_since_processed,
			ailp->next_action_time,
			ailp->next_fire,
			ailp->next_fire2,
			obj->ctype.ai_info.cur_path_index,
			obj->ctype.ai_info.path_length,
			obj->ctype.ai_info.hide_index,
			obj->ctype.ai_info.PATH_DIR,
			obj->pos.x,
			obj->pos.y,
			obj->pos.z,
			obj->mtype.phys_info.velocity.x,
			obj->mtype.phys_info.velocity.y,
			obj->mtype.phys_info.velocity.z);
		input_demo_append_replay_probe_message("restore_ai", obj, probe);
	}
}

int sc_last_item= 0;

char dgss_id[4] = "DGSS";

uint state_game_id;

static int state_thumbnail_has_palette(int version)
{
	return version >= STATE_THUMBNAIL_PALETTE_VERSION && version < STATE_THUMBNAIL_RGB_VERSION;
}

static int state_thumbnail_is_rgb(int version)
{
	return version >= STATE_THUMBNAIL_RGB_VERSION;
}

static void state_skip_thumbnail(PHYSFS_file *fp, int version)
{
	if (state_thumbnail_is_rgb(version)) {
		PHYSFS_seek(fp, PHYSFS_tell(fp) + THUMBNAIL_RGB_BYTES);
		return;
	}
	PHYSFS_seek(fp, PHYSFS_tell(fp) + THUMBNAIL_W * THUMBNAIL_H);
	if (state_thumbnail_has_palette(version))
		PHYSFS_seek(fp, PHYSFS_tell(fp) + THUMBNAIL_PALETTE_BYTES);
}

static grs_bitmap *state_read_thumbnail(PHYSFS_file *fp, int version)
{
	grs_bitmap *bmp;

	bmp = gr_create_bitmap(THUMBNAIL_W, THUMBNAIL_H);
	if (!bmp) {
		state_skip_thumbnail(fp, version);
		return NULL;
	}

	if (state_thumbnail_is_rgb(version)) {
		// Read packed 6-bit RGB and quantize to the currently active OGL
		// palette so the bitmap indices match what ogl_ubitblt_i uploads
		// through gr_current_pal at draw time. This sidesteps the
		// gr_palette / gr_current_pal split and any stale Computed_colors
		// cache, which were the root cause of garbled preview thumbnails.
		ubyte *rgb = d_malloc(THUMBNAIL_RGB_BYTES);
		int i;
		if (!rgb) {
			PHYSFS_seek(fp, PHYSFS_tell(fp) + THUMBNAIL_RGB_BYTES);
			gr_free_bitmap(bmp);
			return NULL;
		}
		PHYSFS_read(fp, rgb, THUMBNAIL_RGB_BYTES, 1);
		for (i = 0; i < THUMBNAIL_W * THUMBNAIL_H; i++)
			bmp->bm_data[i] = gr_find_closest_color_current(rgb[i*3], rgb[i*3+1], rgb[i*3+2]);
		d_free(rgb);
		return bmp;
	}

	PHYSFS_read(fp, bmp->bm_data, THUMBNAIL_W * THUMBNAIL_H, 1);
	if (state_thumbnail_has_palette(version)) {
		ubyte pal[THUMBNAIL_PALETTE_BYTES];
		int i;

		PHYSFS_read(fp, pal, 3, 256);
		for (i = 0; i < THUMBNAIL_W * THUMBNAIL_H; i++) {
			ubyte idx = bmp->bm_data[i];
			bmp->bm_data[i] = gr_find_closest_color_current(pal[idx*3], pal[idx*3+1], pal[idx*3+2]);
		}
	}

	return bmp;
}

static void state_write_blank_thumbnail(PHYSFS_file *fp)
{
	ubyte *zero = d_malloc(THUMBNAIL_RGB_BYTES);
	if (!zero)
		return;
	memset(zero, 0, THUMBNAIL_RGB_BYTES);
	PHYSFS_write(fp, zero, THUMBNAIL_RGB_BYTES, 1);
	d_free(zero);
}

// Following functions convert object to object_rw and back to be written to/read from Savegames. Mostly object differs to object_rw in terms of timer values (fix/fix64). as we reset GameTime64 for writing so it can fit into fix it's not necessary to increment savegame version. But if we once store something else into object which might be useful after restoring, it might be handy to increment Savegame version and actually store these new infos.
// turn object to object_rw to be saved to Savegame.
void state_object_to_object_rw(object *obj, object_rw *obj_rw)
{
	obj_rw->signature     = obj->signature;
	obj_rw->type          = obj->type;
	obj_rw->id            = obj->id;
	obj_rw->next          = obj->next;
	obj_rw->prev          = obj->prev;
	obj_rw->control_type  = obj->control_type;
	obj_rw->movement_type = obj->movement_type;
	obj_rw->render_type   = obj->render_type;
	obj_rw->flags         = obj->flags;
	obj_rw->segnum        = obj->segnum;
	obj_rw->attached_obj  = obj->attached_obj;
	obj_rw->pos.x         = obj->pos.x;
	obj_rw->pos.y         = obj->pos.y;
	obj_rw->pos.z         = obj->pos.z;
	obj_rw->orient.rvec.x = obj->orient.rvec.x;
	obj_rw->orient.rvec.y = obj->orient.rvec.y;
	obj_rw->orient.rvec.z = obj->orient.rvec.z;
	obj_rw->orient.fvec.x = obj->orient.fvec.x;
	obj_rw->orient.fvec.y = obj->orient.fvec.y;
	obj_rw->orient.fvec.z = obj->orient.fvec.z;
	obj_rw->orient.uvec.x = obj->orient.uvec.x;
	obj_rw->orient.uvec.y = obj->orient.uvec.y;
	obj_rw->orient.uvec.z = obj->orient.uvec.z;
	obj_rw->size          = obj->size;
	obj_rw->shields       = obj->shields;
	obj_rw->last_pos.x    = obj->last_pos.x;
	obj_rw->last_pos.y    = obj->last_pos.y;
	obj_rw->last_pos.z    = obj->last_pos.z;
	obj_rw->contains_type = obj->contains_type;
	obj_rw->contains_id   = obj->contains_id;
	obj_rw->contains_count= obj->contains_count;
	obj_rw->matcen_creator= obj->matcen_creator;
	obj_rw->lifeleft      = obj->lifeleft;
	
	switch (obj_rw->movement_type)
	{
		case MT_PHYSICS:
			obj_rw->mtype.phys_info.velocity.x  = obj->mtype.phys_info.velocity.x;
			obj_rw->mtype.phys_info.velocity.y  = obj->mtype.phys_info.velocity.y;
			obj_rw->mtype.phys_info.velocity.z  = obj->mtype.phys_info.velocity.z;
			obj_rw->mtype.phys_info.thrust.x    = obj->mtype.phys_info.thrust.x;
			obj_rw->mtype.phys_info.thrust.y    = obj->mtype.phys_info.thrust.y;
			obj_rw->mtype.phys_info.thrust.z    = obj->mtype.phys_info.thrust.z;
			obj_rw->mtype.phys_info.mass        = obj->mtype.phys_info.mass;
			obj_rw->mtype.phys_info.drag        = obj->mtype.phys_info.drag;
			obj_rw->mtype.phys_info.brakes      = obj->mtype.phys_info.brakes;
			obj_rw->mtype.phys_info.rotvel.x    = obj->mtype.phys_info.rotvel.x;
			obj_rw->mtype.phys_info.rotvel.y    = obj->mtype.phys_info.rotvel.y;
			obj_rw->mtype.phys_info.rotvel.z    = obj->mtype.phys_info.rotvel.z;
			obj_rw->mtype.phys_info.rotthrust.x = obj->mtype.phys_info.rotthrust.x;
			obj_rw->mtype.phys_info.rotthrust.y = obj->mtype.phys_info.rotthrust.y;
			obj_rw->mtype.phys_info.rotthrust.z = obj->mtype.phys_info.rotthrust.z;
			obj_rw->mtype.phys_info.turnroll    = obj->mtype.phys_info.turnroll;
			obj_rw->mtype.phys_info.flags       = obj->mtype.phys_info.flags;
			break;
			
		case MT_SPINNING:
			obj_rw->mtype.spin_rate.x = obj->mtype.spin_rate.x;
			obj_rw->mtype.spin_rate.y = obj->mtype.spin_rate.y;
			obj_rw->mtype.spin_rate.z = obj->mtype.spin_rate.z;
			break;
	}
	
	switch (obj_rw->control_type)
	{
		case CT_WEAPON:
			obj_rw->ctype.laser_info.parent_type      = obj->ctype.laser_info.parent_type;
			obj_rw->ctype.laser_info.parent_num       = obj->ctype.laser_info.parent_num;
			obj_rw->ctype.laser_info.parent_signature = obj->ctype.laser_info.parent_signature;
			if (obj->ctype.laser_info.creation_time - GameTime64 < F1_0*(-18000))
				obj_rw->ctype.laser_info.creation_time = F1_0*(-18000);
			else
				obj_rw->ctype.laser_info.creation_time = obj->ctype.laser_info.creation_time - GameTime64;
			obj_rw->ctype.laser_info.last_hitobj      = obj->ctype.laser_info.last_hitobj;
			obj_rw->ctype.laser_info.track_goal       = obj->ctype.laser_info.track_goal;
			obj_rw->ctype.laser_info.multiplier       = obj->ctype.laser_info.multiplier;
			break;
			
		case CT_EXPLOSION:
			obj_rw->ctype.expl_info.spawn_time    = obj->ctype.expl_info.spawn_time;
			obj_rw->ctype.expl_info.delete_time   = obj->ctype.expl_info.delete_time;
			obj_rw->ctype.expl_info.delete_objnum = obj->ctype.expl_info.delete_objnum;
			obj_rw->ctype.expl_info.attach_parent = obj->ctype.expl_info.attach_parent;
			obj_rw->ctype.expl_info.prev_attach   = obj->ctype.expl_info.prev_attach;
			obj_rw->ctype.expl_info.next_attach   = obj->ctype.expl_info.next_attach;
			break;
			
		case CT_AI:
		{
			int i;
			obj_rw->ctype.ai_info.behavior               = obj->ctype.ai_info.behavior; 
			for (i = 0; i < MAX_AI_FLAGS; i++)
				obj_rw->ctype.ai_info.flags[i]       = obj->ctype.ai_info.flags[i]; 
			obj_rw->ctype.ai_info.hide_segment           = obj->ctype.ai_info.hide_segment;
			obj_rw->ctype.ai_info.hide_index             = obj->ctype.ai_info.hide_index;
			obj_rw->ctype.ai_info.path_length            = obj->ctype.ai_info.path_length;
			obj_rw->ctype.ai_info.cur_path_index         = obj->ctype.ai_info.cur_path_index;
			obj_rw->ctype.ai_info.dying_sound_playing    = obj->ctype.ai_info.dying_sound_playing;
			obj_rw->ctype.ai_info.danger_laser_num       = obj->ctype.ai_info.danger_laser_num;
			obj_rw->ctype.ai_info.danger_laser_signature = obj->ctype.ai_info.danger_laser_signature;
			if (obj->ctype.ai_info.dying_start_time == 0) // if bot not dead, anything but 0 will kill it
				obj_rw->ctype.ai_info.dying_start_time = 0;
			else
				obj_rw->ctype.ai_info.dying_start_time = obj->ctype.ai_info.dying_start_time - GameTime64;
			break;
		}
			
		case CT_LIGHT:
			obj_rw->ctype.light_info.intensity = obj->ctype.light_info.intensity;
			break;
			
		case CT_POWERUP:
			obj_rw->ctype.powerup_info.count         = obj->ctype.powerup_info.count;
			if (obj->ctype.powerup_info.creation_time - GameTime64 < F1_0*(-18000))
				obj_rw->ctype.powerup_info.creation_time = F1_0*(-18000);
			else
				obj_rw->ctype.powerup_info.creation_time = obj->ctype.powerup_info.creation_time - GameTime64;
			obj_rw->ctype.powerup_info.flags         = obj->ctype.powerup_info.flags;
			break;
	}
	
	switch (obj_rw->render_type)
	{
		case RT_MORPH:
		case RT_POLYOBJ:
		case RT_NONE: // HACK below
		{
			int i;
			if (obj->render_type == RT_NONE && obj->type != OBJ_GHOST) // HACK: when a player is dead or not connected yet, clients still expect to get polyobj data - even if render_type == RT_NONE at this time. Here it's not important, but it might be for Multiplayer Savegames.
				break;
			obj_rw->rtype.pobj_info.model_num                = obj->rtype.pobj_info.model_num;
			for (i=0;i<MAX_SUBMODELS;i++)
			{
				obj_rw->rtype.pobj_info.anim_angles[i].p = obj->rtype.pobj_info.anim_angles[i].p;
				obj_rw->rtype.pobj_info.anim_angles[i].b = obj->rtype.pobj_info.anim_angles[i].b;
				obj_rw->rtype.pobj_info.anim_angles[i].h = obj->rtype.pobj_info.anim_angles[i].h;
			}
			obj_rw->rtype.pobj_info.subobj_flags             = obj->rtype.pobj_info.subobj_flags;
			obj_rw->rtype.pobj_info.tmap_override            = obj->rtype.pobj_info.tmap_override;
			obj_rw->rtype.pobj_info.alt_textures             = obj->rtype.pobj_info.alt_textures;
			break;
		}
			
		case RT_WEAPON_VCLIP:
		case RT_HOSTAGE:
		case RT_POWERUP:
		case RT_FIREBALL:
			obj_rw->rtype.vclip_info.vclip_num = obj->rtype.vclip_info.vclip_num;
			obj_rw->rtype.vclip_info.frametime = obj->rtype.vclip_info.frametime;
			obj_rw->rtype.vclip_info.framenum  = obj->rtype.vclip_info.framenum;
			break;
			
		case RT_LASER:
			break;
			
	}
}

// turn object_rw to object after reading from Savegame
void state_object_rw_to_object(object_rw *obj_rw, object *obj)
{
	obj->signature     = obj_rw->signature;
	obj->type          = obj_rw->type;
	obj->id            = obj_rw->id;
	obj->next          = obj_rw->next;
	obj->prev          = obj_rw->prev;
	obj->control_type  = obj_rw->control_type;
	obj->movement_type = obj_rw->movement_type;
	obj->render_type   = obj_rw->render_type;
	obj->flags         = obj_rw->flags;
	obj->segnum        = obj_rw->segnum;
	obj->attached_obj  = obj_rw->attached_obj;
	obj->pos.x         = obj_rw->pos.x;
	obj->pos.y         = obj_rw->pos.y;
	obj->pos.z         = obj_rw->pos.z;
	obj->orient.rvec.x = obj_rw->orient.rvec.x;
	obj->orient.rvec.y = obj_rw->orient.rvec.y;
	obj->orient.rvec.z = obj_rw->orient.rvec.z;
	obj->orient.fvec.x = obj_rw->orient.fvec.x;
	obj->orient.fvec.y = obj_rw->orient.fvec.y;
	obj->orient.fvec.z = obj_rw->orient.fvec.z;
	obj->orient.uvec.x = obj_rw->orient.uvec.x;
	obj->orient.uvec.y = obj_rw->orient.uvec.y;
	obj->orient.uvec.z = obj_rw->orient.uvec.z;
	obj->size          = obj_rw->size;
	obj->shields       = obj_rw->shields;
	obj->last_pos.x    = obj_rw->last_pos.x;
	obj->last_pos.y    = obj_rw->last_pos.y;
	obj->last_pos.z    = obj_rw->last_pos.z;
	obj->contains_type = obj_rw->contains_type;
	obj->contains_id   = obj_rw->contains_id;
	obj->contains_count= obj_rw->contains_count;
	obj->matcen_creator= obj_rw->matcen_creator;
	obj->lifeleft      = obj_rw->lifeleft;
	
	switch (obj->movement_type)
	{
		case MT_PHYSICS:
			obj->mtype.phys_info.velocity.x  = obj_rw->mtype.phys_info.velocity.x;
			obj->mtype.phys_info.velocity.y  = obj_rw->mtype.phys_info.velocity.y;
			obj->mtype.phys_info.velocity.z  = obj_rw->mtype.phys_info.velocity.z;
			obj->mtype.phys_info.thrust.x    = obj_rw->mtype.phys_info.thrust.x;
			obj->mtype.phys_info.thrust.y    = obj_rw->mtype.phys_info.thrust.y;
			obj->mtype.phys_info.thrust.z    = obj_rw->mtype.phys_info.thrust.z;
			obj->mtype.phys_info.mass        = obj_rw->mtype.phys_info.mass;
			obj->mtype.phys_info.drag        = obj_rw->mtype.phys_info.drag;
			obj->mtype.phys_info.brakes      = obj_rw->mtype.phys_info.brakes;
			obj->mtype.phys_info.rotvel.x    = obj_rw->mtype.phys_info.rotvel.x;
			obj->mtype.phys_info.rotvel.y    = obj_rw->mtype.phys_info.rotvel.y;
			obj->mtype.phys_info.rotvel.z    = obj_rw->mtype.phys_info.rotvel.z;
			obj->mtype.phys_info.rotthrust.x = obj_rw->mtype.phys_info.rotthrust.x;
			obj->mtype.phys_info.rotthrust.y = obj_rw->mtype.phys_info.rotthrust.y;
			obj->mtype.phys_info.rotthrust.z = obj_rw->mtype.phys_info.rotthrust.z;
			obj->mtype.phys_info.turnroll    = obj_rw->mtype.phys_info.turnroll;
			obj->mtype.phys_info.flags       = obj_rw->mtype.phys_info.flags;
			break;
			
		case MT_SPINNING:
			obj->mtype.spin_rate.x = obj_rw->mtype.spin_rate.x;
			obj->mtype.spin_rate.y = obj_rw->mtype.spin_rate.y;
			obj->mtype.spin_rate.z = obj_rw->mtype.spin_rate.z;
			break;
	}
	
	switch (obj->control_type)
	{
		case CT_WEAPON:
			obj->ctype.laser_info.parent_type      = obj_rw->ctype.laser_info.parent_type;
			obj->ctype.laser_info.parent_num       = obj_rw->ctype.laser_info.parent_num;
			obj->ctype.laser_info.parent_signature = obj_rw->ctype.laser_info.parent_signature;
			obj->ctype.laser_info.creation_time    = GameTime64 + (fix64)obj_rw->ctype.laser_info.creation_time;
			obj->ctype.laser_info.creation_framecount = 0;
			memset(obj->ctype.laser_info.hitobj_list, 0, sizeof(obj->ctype.laser_info.hitobj_list));
			obj->ctype.laser_info.last_hitobj      = obj_rw->ctype.laser_info.last_hitobj;
			if (obj->ctype.laser_info.last_hitobj >= 0)
				obj->ctype.laser_info.hitobj_list[obj->ctype.laser_info.last_hitobj] = 1; // restore most recent hitobj to hitobj_list
			obj->ctype.laser_info.track_goal       = obj_rw->ctype.laser_info.track_goal;
			obj->ctype.laser_info.multiplier       = obj_rw->ctype.laser_info.multiplier;
			break;
			
		case CT_EXPLOSION:
			obj->ctype.expl_info.spawn_time    = obj_rw->ctype.expl_info.spawn_time;
			obj->ctype.expl_info.delete_time   = obj_rw->ctype.expl_info.delete_time;
			obj->ctype.expl_info.delete_objnum = obj_rw->ctype.expl_info.delete_objnum;
			obj->ctype.expl_info.attach_parent = obj_rw->ctype.expl_info.attach_parent;
			obj->ctype.expl_info.prev_attach   = obj_rw->ctype.expl_info.prev_attach;
			obj->ctype.expl_info.next_attach   = obj_rw->ctype.expl_info.next_attach;
			break;
			
		case CT_AI:
		{
			int i;
			obj->ctype.ai_info.behavior               = obj_rw->ctype.ai_info.behavior; 
			for (i = 0; i < MAX_AI_FLAGS; i++)
				obj->ctype.ai_info.flags[i]       = obj_rw->ctype.ai_info.flags[i]; 
			obj->ctype.ai_info.hide_segment           = obj_rw->ctype.ai_info.hide_segment;
			obj->ctype.ai_info.hide_index             = obj_rw->ctype.ai_info.hide_index;
			obj->ctype.ai_info.path_length            = obj_rw->ctype.ai_info.path_length;
			obj->ctype.ai_info.cur_path_index         = obj_rw->ctype.ai_info.cur_path_index;
			obj->ctype.ai_info.dying_sound_playing    = obj_rw->ctype.ai_info.dying_sound_playing;
			obj->ctype.ai_info.danger_laser_num       = obj_rw->ctype.ai_info.danger_laser_num;
			obj->ctype.ai_info.danger_laser_signature = obj_rw->ctype.ai_info.danger_laser_signature;
			obj->ctype.ai_info.dying_start_time       = obj_rw->ctype.ai_info.dying_start_time;
			break;
		}
			
		case CT_LIGHT:
			obj->ctype.light_info.intensity = obj_rw->ctype.light_info.intensity;
			break;
			
		case CT_POWERUP:
			obj->ctype.powerup_info.count         = obj_rw->ctype.powerup_info.count;
			obj->ctype.powerup_info.creation_time = GameTime64 + (fix64)obj_rw->ctype.powerup_info.creation_time;
			obj->ctype.powerup_info.flags         = obj_rw->ctype.powerup_info.flags;
			break;
		case CT_CNTRLCEN:
		{
			// Boss levels keep a hidden CT_CNTRLCEN placeholder as OBJ_GHOST/RT_NONE.
			// Only rebuild gun points for the live reactor object.
			int i = 0;
			reactor *reactor;
			if (obj->type != OBJ_CNTRLCEN || obj->render_type != RT_POLYOBJ)
				break;
			reactor = get_reactor_definition(obj->id);
			for (i=0; i<reactor->n_guns; i++)
				calc_controlcen_gun_point(reactor, obj, i);
			break;
		}
	}
	
	switch (obj->render_type)
	{
		case RT_MORPH:
		case RT_POLYOBJ:
		case RT_NONE: // HACK below
		{
			int i;
			if (obj->render_type == RT_NONE && obj->type != OBJ_GHOST) // HACK: when a player is dead or not connected yet, clients still expect to get polyobj data - even if render_type == RT_NONE at this time. Here it's not important, but it might be for Multiplayer Savegames.
				break;
			obj->rtype.pobj_info.model_num                = obj_rw->rtype.pobj_info.model_num;
			for (i=0;i<MAX_SUBMODELS;i++)
			{
				obj->rtype.pobj_info.anim_angles[i].p = obj_rw->rtype.pobj_info.anim_angles[i].p;
				obj->rtype.pobj_info.anim_angles[i].b = obj_rw->rtype.pobj_info.anim_angles[i].b;
				obj->rtype.pobj_info.anim_angles[i].h = obj_rw->rtype.pobj_info.anim_angles[i].h;
			}
			obj->rtype.pobj_info.subobj_flags             = obj_rw->rtype.pobj_info.subobj_flags;
			obj->rtype.pobj_info.tmap_override            = obj_rw->rtype.pobj_info.tmap_override;
			obj->rtype.pobj_info.alt_textures             = obj_rw->rtype.pobj_info.alt_textures;
			break;
		}
			
		case RT_WEAPON_VCLIP:
		case RT_HOSTAGE:
		case RT_POWERUP:
		case RT_FIREBALL:
			obj->rtype.vclip_info.vclip_num = obj_rw->rtype.vclip_info.vclip_num;
			obj->rtype.vclip_info.frametime = obj_rw->rtype.vclip_info.frametime;
			obj->rtype.vclip_info.framenum  = obj_rw->rtype.vclip_info.framenum;
			break;
			
		case RT_LASER:
			break;
			
	}
}

static void state_relink_objects_by_index(void)
{
	int i, segnum;
	object *obj;

	for (i=0; i<=Highest_object_index; i++ ) {
		obj = &Objects[i];
		obj->rtype.pobj_info.alt_textures = -1;
		segnum = obj->segnum;
		obj->next = obj->prev = obj->segnum = -1;
		if ( obj->type != OBJ_NONE )
			obj_link(i,segnum);
	}
}

static int state_restore_segment_object_links(void)
{
	int i, segnum, objnum;
	ubyte seen[MAX_OBJECTS];

	memset(seen, 0, sizeof(seen));

	for (segnum=0; segnum <= Highest_segment_index; segnum++)
		Segments[segnum].objects = -1;

	for (i=0; i<=Highest_object_index; i++) {
		object *obj = &Objects[i];

		obj->rtype.pobj_info.alt_textures = -1;
		if (obj->type == OBJ_NONE)
			continue;
		if (obj->segnum < 0 || obj->segnum > Highest_segment_index)
			return 0;
		if (obj->prev == -1) {
			if (Segments[obj->segnum].objects != -1)
				return 0;
			Segments[obj->segnum].objects = i;
		}
	}

	for (i=0; i<=Highest_object_index; i++) {
		object *obj = &Objects[i];

		if (obj->type == OBJ_NONE)
			continue;
		segnum = obj->segnum;
		if (obj->prev == -1) {
			if (Segments[segnum].objects != i)
				return 0;
		} else {
			if (obj->prev < 0 || obj->prev > Highest_object_index)
				return 0;
			if (Objects[obj->prev].type == OBJ_NONE ||
				Objects[obj->prev].segnum != segnum ||
				Objects[obj->prev].next != i)
				return 0;
		}
		if (obj->next != -1) {
			if (obj->next < 0 || obj->next > Highest_object_index)
				return 0;
			if (Objects[obj->next].type == OBJ_NONE ||
				Objects[obj->next].segnum != segnum ||
				Objects[obj->next].prev != i)
				return 0;
		}
	}

	for (segnum=0; segnum <= Highest_segment_index; segnum++)
		for (objnum=Segments[segnum].objects; objnum!=-1; objnum=Objects[objnum].next) {
			if (objnum < 0 || objnum > Highest_object_index)
				return 0;
			if (seen[objnum])
				return 0;
			if (Objects[objnum].type == OBJ_NONE || Objects[objnum].segnum != segnum)
				return 0;
			seen[objnum] = 1;
		}

	for (i=0; i<=Highest_object_index; i++)
		if (Objects[i].type != OBJ_NONE && !seen[i])
			return 0;

	return 1;
}

// Following functions convert player to player_rw and back to be written to/read from Savegames. player only differ to player_rw in terms of timer values (fix/fix64). as we reset GameTime64 for writing so it can fit into fix it's not necessary to increment savegame version. But if we once store something else into object which might be useful after restoring, it might be handy to increment Savegame version and actually store these new infos.
// turn player to player_rw to be saved to Savegame.
void state_player_to_player_rw(player *pl, player_rw *pl_rw)
{
	int i=0;
	memcpy(pl_rw->callsign, pl->callsign, CALLSIGN_LEN+1);
	memcpy(pl_rw->net_address, pl->net_address, 6);
	pl_rw->connected                 = pl->connected;
	pl_rw->objnum                    = pl->objnum;
	pl_rw->n_packets_got             = pl->n_packets_got;
	pl_rw->n_packets_sent            = pl->n_packets_sent;
	pl_rw->flags                     = pl->flags;
	pl_rw->energy                    = pl->energy;
	pl_rw->shields                   = pl->shields;
	pl_rw->lives                     = pl->lives;
	pl_rw->level                     = pl->level;
	pl_rw->laser_level               = pl->laser_level;
	pl_rw->starting_level            = pl->starting_level;
	pl_rw->killer_objnum             = pl->killer_objnum;
	pl_rw->primary_weapon_flags      = pl->primary_weapon_flags;
	pl_rw->secondary_weapon_flags    = pl->secondary_weapon_flags;
	for (i = 0; i < MAX_PRIMARY_WEAPONS; i++)
		pl_rw->primary_ammo[i]   = pl->primary_ammo[i];
	for (i = 0; i < MAX_SECONDARY_WEAPONS; i++)
		pl_rw->secondary_ammo[i] = pl->secondary_ammo[i];
	pl_rw->last_score                = pl->last_score;
	pl_rw->score                     = pl->score;
	pl_rw->time_level                = pl->time_level;
	pl_rw->time_total                = pl->time_total;
	if (pl->cloak_time - GameTime64 < F1_0*(-18000))
		pl_rw->cloak_time        = F1_0*(-18000);
	else
		pl_rw->cloak_time        = pl->cloak_time - GameTime64;
	if (pl->invulnerable_time - GameTime64 < F1_0*(-18000))
		pl_rw->invulnerable_time = F1_0*(-18000);
	else
		pl_rw->invulnerable_time = pl->invulnerable_time - GameTime64;
	pl_rw->KillGoalCount             = pl->KillGoalCount;
	pl_rw->net_killed_total          = pl->net_killed_total;
	pl_rw->net_kills_total           = pl->net_kills_total;
	pl_rw->num_kills_level           = pl->num_kills_level;
	pl_rw->num_kills_total           = pl->num_kills_total;
	pl_rw->num_robots_level          = pl->num_robots_level;
	pl_rw->num_robots_total          = pl->num_robots_total;
	pl_rw->hostages_rescued_total    = pl->hostages_rescued_total;
	pl_rw->hostages_total            = pl->hostages_total;
	pl_rw->hostages_on_board         = pl->hostages_on_board;
	pl_rw->hostages_level            = pl->hostages_level;
	pl_rw->homing_object_dist        = pl->homing_object_dist;
	pl_rw->hours_level               = pl->hours_level;
	pl_rw->hours_total               = pl->hours_total;
}

// turn player_rw to player after reading from Savegame
void state_player_rw_to_player(player_rw *pl_rw, player *pl)
{
	int i=0;
	memcpy(pl->callsign, pl_rw->callsign, CALLSIGN_LEN+1);
	memcpy(pl->net_address, pl_rw->net_address, 6);
	pl->connected                 = pl_rw->connected;
	pl->objnum                    = pl_rw->objnum;
	pl->n_packets_got             = pl_rw->n_packets_got;
	pl->n_packets_sent            = pl_rw->n_packets_sent;
	pl->flags                     = pl_rw->flags;
	pl->energy                    = pl_rw->energy;
	pl->shields                   = pl_rw->shields;
	pl->lives                     = pl_rw->lives;
	pl->level                     = pl_rw->level;
	pl->laser_level               = pl_rw->laser_level;
	pl->starting_level            = pl_rw->starting_level;
	pl->killer_objnum             = pl_rw->killer_objnum;
	pl->primary_weapon_flags      = pl_rw->primary_weapon_flags;
	pl->secondary_weapon_flags    = pl_rw->secondary_weapon_flags;
	for (i = 0; i < MAX_PRIMARY_WEAPONS; i++)
		pl->primary_ammo[i]   = pl_rw->primary_ammo[i];
	for (i = 0; i < MAX_SECONDARY_WEAPONS; i++)
		pl->secondary_ammo[i] = pl_rw->secondary_ammo[i];
	pl->last_score                = pl_rw->last_score;
	pl->score                     = pl_rw->score;
	pl->time_level                = pl_rw->time_level;
	pl->time_total                = pl_rw->time_total;
	pl->cloak_time                = pl_rw->cloak_time;
	pl->invulnerable_time         = pl_rw->invulnerable_time;
	pl->KillGoalCount             = pl_rw->KillGoalCount;
	pl->net_killed_total          = pl_rw->net_killed_total;
	pl->net_kills_total           = pl_rw->net_kills_total;
	pl->num_kills_level           = pl_rw->num_kills_level;
	pl->num_kills_total           = pl_rw->num_kills_total;
	pl->num_robots_level          = pl_rw->num_robots_level;
	pl->num_robots_total          = pl_rw->num_robots_total;
	pl->hostages_rescued_total    = pl_rw->hostages_rescued_total;
	pl->hostages_total            = pl_rw->hostages_total;
	pl->hostages_on_board         = pl_rw->hostages_on_board;
	pl->hostages_level            = pl_rw->hostages_level;
	pl->homing_object_dist        = pl_rw->homing_object_dist;
	pl->hours_level               = pl_rw->hours_level;
	pl->hours_total               = pl_rw->hours_total;
}


//-------------------------------------------------------------------
int state_callback(newmenu *menu, d_event *event, grs_bitmap *sc_bmp[])
{
	newmenu_item *items = newmenu_get_items(menu);
	int citem = newmenu_get_citem(menu);
	
	if ( (citem > 0) && (event->type == EVENT_NEWMENU_DRAW) )
	{
		if ( sc_bmp[citem-1] )	{
			int preview_x = (grd_curcanv->cv_bitmap.bm_w/2)-FSPACX(THUMBNAIL_W/2);

		#ifndef OGL
			grs_canvas *save_canv = grd_curcanv;
			grs_canvas *temp_canv = gr_create_canvas(FSPACX(THUMBNAIL_W),FSPACY(THUMBNAIL_H));
		#else
			int preview_y = items[0].y-FSPACY(3);
			int preview_w = FSPACX(THUMBNAIL_W);
			int preview_h = FSPACY(THUMBNAIL_H);

			/* D2 thumbnails are already remapped into the current palette when loaded.
			 * Upload them with the transient indexed blit path so OGL uses gr_current_pal
			 * instead of the cached bitmap uploader's default palette. */
			ogl_ubitblt_i(preview_w, preview_h, preview_x, preview_y,
				THUMBNAIL_W, THUMBNAIL_H, 0, 0, sc_bmp[citem-1], &grd_curcanv->cv_bitmap, 1);
		#endif

		#ifndef OGL
			grs_point vertbuf[3] = {{0,0}, {0,0}, {i2f(THUMBNAIL_W*2),i2f(THUMBNAIL_H*24/10)} };
			gr_set_current_canvas(temp_canv);
			scale_bitmap(sc_bmp[citem-1], vertbuf, 0);
			gr_set_current_canvas( save_canv );
			gr_bitmap( preview_x, items[0].y-3, &temp_canv->cv_bitmap);
			gr_free_canvas(temp_canv);
		#endif
		}
		
		return 1;
	}
	
	return 0;
}

#if 0
void rpad_string( char * string, int max_chars )
{
	int i, end_found;

	end_found = 0;
	for( i=0; i<max_chars; i++ )	{
		if ( *string == 0 )
			end_found = 1;
		if ( end_found )
			*string = ' ';
		string++;
	}
	*string = 0;		// NULL terminate
}
#endif

static int state_default_item = 0;
//Since state_default_item should ALWAYS point to a valid savegame slot, we use this to check if we once already actually SAVED a game. If yes, state_quick_item will be equal state_default_item, otherwise it should be -1 on every new mission and tell us we need to select a slot for quicksave.
int state_quick_item = -1;

/* Present a menu for selection of a savegame filename.
 * For saving, dsc should be a pre-allocated buffer into which the new
 * savegame description will be stored.
 * For restoring, dsc should be NULL, in which case empty slots will not be
 * selectable and savagames descriptions will not be editable.
 */
#ifdef __ANDROID__
static void state_android_restore_player_flight_state(void)
{
	object *obj;
	int objnum = Players[Player_num].objnum;

	if (objnum < 0 || objnum > Highest_object_index)
		return;

	Viewer = ConsoleObject = &Objects[objnum];
	obj = ConsoleObject;
	if (obj->type == OBJ_GHOST)
		obj->type = OBJ_PLAYER;
	if (obj->type != OBJ_PLAYER)
		return;

	if (Player_is_dead || obj->control_type != CT_FLYING || obj->movement_type != MT_PHYSICS ||
	    !(obj->mtype.phys_info.flags & PF_USES_THRUST))
		debug_log(DLOG_GAME,
			"restore controls repaired: D2 obj=%d ct=%d mt=%d flags=0x%x dead=%d",
			objnum, obj->control_type, obj->movement_type,
			obj->mtype.phys_info.flags, Player_is_dead);

	Player_is_dead = 0;
	obj->control_type = CT_FLYING;
	obj->movement_type = MT_PHYSICS;
	obj->mtype.phys_info.flags |= PF_TURNROLL | PF_LEVELLING | PF_WIGGLE | PF_USES_THRUST;
}
#endif

int state_get_savegame_filename(char * fname, char * dsc, char * caption, int blind_save)
{
	PHYSFS_file * fp;
	int i, choice, version, nsaves;
	newmenu_item m[NUM_SAVES+1];
	char filename[NUM_SAVES][PATH_MAX];
	char desc[NUM_SAVES][DESC_LENGTH + 16];
	grs_bitmap *sc_bmp[NUM_SAVES];
	char id[5], dummy_callsign[CALLSIGN_LEN+1];
	int valid;

	nsaves=0;
	m[0].type = NM_TYPE_TEXT; m[0].text = "\n\n\n\n";
	for (i=0;i<NUM_SAVES; i++ )	{
		sc_bmp[i] = NULL;
		snprintf( filename[i], PATH_MAX, GameArg.SysUsePlayersDir? "Players/%s.%sg%x" : "%s.%sg%x", Players[Player_num].callsign, (Game_mode & GM_MULTI_COOP)?"m":"s", i );
		valid = 0;
		fp = PHYSFSX_openReadBuffered(filename[i]);
#ifdef __ANDROID__
		/* Autosave slots use COOP_AUTOSAVE_CALLSIGN instead of the
		 * player's callsign so they survive callsign changes */
		if (!fp && (Game_mode & GM_MULTI_COOP) &&
		    i >= COOP_AUTOSAVE_SLOT_FIRST && i < COOP_AUTOSAVE_SLOT_FIRST + COOP_AUTOSAVE_SLOT_COUNT) {
			snprintf(filename[i], PATH_MAX, GameArg.SysUsePlayersDir ? "Players/%s.mg%x" : "%s.mg%x",
				COOP_AUTOSAVE_CALLSIGN, i);
			fp = PHYSFSX_openReadBuffered(filename[i]);
		}
#endif
		if ( fp ) {
			//Read id
			PHYSFS_read(fp, id, sizeof(char) * 4, 1);
			if ( !memcmp( id, dgss_id, 4 )) {
				//Read version
				PHYSFS_read(fp, &version, sizeof(int), 1);
				// In case it's Coop, read state_game_id & callsign as well
				if (Game_mode & GM_MULTI_COOP)
				{
					PHYSFS_seek(fp, PHYSFS_tell(fp) + sizeof(PHYSFS_sint32)); // skip state_game_id
					PHYSFS_read(fp, &dummy_callsign, sizeof(char)*CALLSIGN_LEN+1, 1);
				}
				if ((version >= STATE_COMPATIBLE_VERSION) || (SWAPINT(version) >= STATE_COMPATIBLE_VERSION)) {
					// Read description
					PHYSFS_read(fp, desc[i], sizeof(char) * DESC_LENGTH, 1);
					//rpad_string( desc[i], DESC_LENGTH-1 );
					if (dsc == NULL) m[i+1].type = NM_TYPE_MENU;
					// Read thumbnail
					sc_bmp[i] = state_read_thumbnail(fp, version);
					nsaves++;
					valid = 1;
				}
			}
			PHYSFS_close(fp);
		} 
		if (!valid) {
			strcpy( desc[i], TXT_EMPTY );
			//rpad_string( desc[i], DESC_LENGTH-1 );
			if (dsc == NULL) m[i+1].type = NM_TYPE_TEXT;
		}
		if (dsc != NULL) {
			m[i+1].type = NM_TYPE_INPUT_MENU;
		}
		m[i+1].text_len = DESC_LENGTH-1;
		m[i+1].text = desc[i];
	}

	if ( dsc == NULL && nsaves < 1 )	{
		nm_messagebox( NULL, 1, "Ok", "No saved games were found!" );
		return 0;
	}

	sc_last_item = -1;

	if (blind_save && state_quick_item < 0)
		blind_save = 0;		// haven't picked a slot yet

	if (blind_save)
		choice = state_default_item + 1;
	else {
#ifdef __ANDROID__
		extern volatile int g_saveload_menu_active;
		g_saveload_menu_active = 1;
#endif
		choice = newmenu_do2( NULL, caption, NUM_SAVES+1, m, (int (*)(newmenu *, d_event *, void *))state_callback, sc_bmp, state_default_item + 1, NULL );
#ifdef __ANDROID__
		g_saveload_menu_active = 0;
#endif
	}

	for (i=0; i<NUM_SAVES; i++ )	{
		if ( sc_bmp[i] )
			gr_free_bitmap( sc_bmp[i] );
	}

	if (choice > 0) {
		strcpy( fname, filename[choice-1] );
		if ( dsc != NULL ) strcpy( dsc, desc[choice-1] );
		state_quick_item = state_default_item = choice - 1;
		return choice;
	}
	return 0;
}

int state_get_save_file(char * fname, char * dsc, int blind_save)
{
	return state_get_savegame_filename(fname, dsc, "Save Game", blind_save);
}

int state_get_restore_file(char * fname)
{
	return state_get_savegame_filename(fname, NULL, "Select Game to Restore", 0);
}

#define	DESC_OFFSET	8

//	-----------------------------------------------------------------------------------
//	Imagine if C had a function to copy a file...
int copy_file(char *old_file, char *new_file)
{
	sbyte	*buf;
	int		buf_size;
	PHYSFS_file *in_file, *out_file;

	out_file = PHYSFS_openWrite(new_file);

	if (out_file == NULL)
		return -1;

	in_file = PHYSFS_openRead(old_file);

	if (in_file == NULL)
		return -2;

	buf_size = PHYSFS_fileLength(in_file);
	while (buf_size && !(buf = d_malloc(buf_size)))
		buf_size /= 2;
	if (buf_size == 0)
		return -5;	// likely to be an empty file

	while (!PHYSFS_eof(in_file))
	{
		int bytes_read;

		bytes_read = PHYSFS_read(in_file, buf, 1, buf_size);
		if (bytes_read < 0)
			Error("Cannot read from file <%s>: %s", old_file, PHYSFS_getLastError());

		Assert(bytes_read == buf_size || PHYSFS_eof(in_file));

		if (PHYSFS_write(out_file, buf, 1, bytes_read) < bytes_read)
			Error("Cannot write to file <%s>: %s", new_file, PHYSFS_getLastError());
	}

	d_free(buf);

	if (!PHYSFS_close(in_file))
	{
		PHYSFS_close(out_file);
		return -3;
	}

	if (!PHYSFS_close(out_file))
		return -4;

	return 0;
}

extern int Final_boss_is_dead;

//	-----------------------------------------------------------------------------------
int state_save_all(int secret_save, char *filename_override, int blind_save)
{
	int	rval, filenum = -1;
	char	filename[PATH_MAX], desc[DESC_LENGTH+1];

	if ((Current_level_num < 0) && (secret_save == 0)) {
		HUD_init_message_literal(HM_DEFAULT,  "Can't save in secret level!" );
		return 0;
	}

	if (Final_boss_is_dead)		//don't allow save while final boss is dying
		return 0;

	if ( Game_mode & GM_MULTI )
	{
		if (Game_mode & GM_MULTI_COOP)
			multi_initiate_save_game();
		return 0;
	}

	//	If this is a secret save and the control center has been destroyed, don't allow
	//	return to the base level.
	if (secret_save && (Control_center_destroyed)) {
		PHYSFS_delete(SECRETB_FILENAME);
		return 0;
	}

	stop_time();

	memset(&filename, '\0', PATH_MAX);
	memset(&desc, '\0', DESC_LENGTH+1);
	if (secret_save == 1) {
		filename_override = filename;
		sprintf(filename_override, SECRETB_FILENAME);
	} else if (secret_save == 2) {
		filename_override = filename;
		sprintf(filename_override, SECRETC_FILENAME);
	} else {
		if (!(filenum = state_get_save_file(filename, desc, blind_save)))
		{
			start_time();
			return 0;
		}
	}
		
	//	MK, 1/1/96
	//	Do special secret level stuff.
	//	If secret.sgc exists, then copy it to Nsecret.sgc (where N = filenum).
	//	If it doesn't exist, then delete Nsecret.sgc
	if (!secret_save && !(Game_mode & GM_MULTI_COOP)) {
		int	rval;
		char	temp_fname[PATH_MAX], fc;

		if (filenum != -1) {

			if (filenum >= 10)
				fc = (filenum-10) + 'a';
			else
				fc = '0' + filenum;

			sprintf(temp_fname, GameArg.SysUsePlayersDir? "Players/%csecret.sgc" : "%csecret.sgc", fc);

			if (PHYSFSX_exists(temp_fname,0))
			{
				if (!PHYSFS_delete(temp_fname))
					Error("Cannot delete file <%s>: %s", temp_fname, PHYSFS_getLastError());
			}

			if (PHYSFSX_exists(SECRETC_FILENAME,0))
			{
				rval = copy_file(SECRETC_FILENAME, temp_fname);
				Assert(rval == 0);	//	Oops, error copying secret.sgc to temp_fname!
				(void)rval;
			}
		}
	}

	rval = state_save_all_sub(filename, desc);

	if (rval && !secret_save)
		HUD_init_message_literal(HM_DEFAULT, "Game saved");

	return rval;
}

#ifdef __ANDROID__
static int android_is_real_pilot_callsign(const char *callsign)
{
	return callsign && callsign[0] && strcmp(callsign, COOP_AUTOSAVE_CALLSIGN);
}

static int android_find_fallback_pilot_callsign(char *callsign)
{
	char **list;
	char **entry;
	int found = 0;
	static const char *const types[] = { ".plr", NULL };

	if (!callsign)
		return 0;
	callsign[0] = '\0';
	list = PHYSFSX_findFiles(GameArg.SysUsePlayersDir ? "Players/" : "", types);
	if (!list)
		return 0;
	for (entry = list; *entry; entry++) {
		char candidate[CALLSIGN_LEN + 1];
		char *dot = strstr(*entry, ".plr");
		size_t len;

		if (!dot || dot == *entry || dot[4])
			continue;
		len = (size_t) (dot - *entry);
		if (len > CALLSIGN_LEN)
			continue;
		memset(candidate, 0, sizeof(candidate));
		memcpy(candidate, *entry, len);
		if (!android_is_real_pilot_callsign(candidate))
			continue;
		strncpy(callsign, candidate, CALLSIGN_LEN);
		callsign[CALLSIGN_LEN] = '\0';
		found = 1;
		break;
	}
	PHYSFS_freeList(list);
	return found;
}

static void android_repair_player_callsign_for_autosave(const char *game_name)
{
	char fallback[CALLSIGN_LEN + 1];
	const char *repair_callsign = GameCfg.LastPlayer;

	if (android_is_real_pilot_callsign(Players[Player_num].callsign))
		return;
	if (!android_is_real_pilot_callsign(repair_callsign)) {
		if (android_find_fallback_pilot_callsign(fallback))
			repair_callsign = fallback;
		else
			repair_callsign = "player";
	}
	debug_log(DLOG_GAME, "autosave repairing %s pilot callsign current='%s' last='%s' repair='%s'",
		game_name, Players[Player_num].callsign, GameCfg.LastPlayer, repair_callsign);
	strncpy(Players[Player_num].callsign, repair_callsign, CALLSIGN_LEN);
	Players[Player_num].callsign[CALLSIGN_LEN] = '\0';
	strncpy(GameCfg.LastPlayer, repair_callsign, CALLSIGN_LEN);
	GameCfg.LastPlayer[CALLSIGN_LEN] = '\0';
}

int state_android_save_to_slot(int slotnum, const char *desc, int save_kind)
{
	int result;
	int prev_kind = g_android_save_meta_kind;
	int prev_blank = g_android_save_blank_thumbnail;
	char filename[PATH_MAX], save_desc[DESC_LENGTH + 1];
	char temp_fname[PATH_MAX], fc;

	if (!desc || slotnum < 0 || slotnum >= NUM_SAVES) {
		debug_log(DLOG_GAME, "autosave skipped: invalid D2 slot request");
		return 0;
	}
	if (Current_level_num < 0) {
		debug_log(DLOG_GAME, "autosave skipped: D2 secret level is active");
		return 0;
	}
	if (Final_boss_is_dead) {
		debug_log(DLOG_GAME, "autosave skipped: D2 final boss death sequence is active");
		return 0;
	}
	if (Game_mode & GM_MULTI) {
		debug_log(DLOG_GAME, "autosave skipped: D2 multiplayer is active");
		return 0;
	}
	android_repair_player_callsign_for_autosave("D2");

	stop_time();
	memset(filename, 0, sizeof(filename));
	memset(save_desc, 0, sizeof(save_desc));
	strncpy(save_desc, desc, DESC_LENGTH);
	snprintf(filename, PATH_MAX, GameArg.SysUsePlayersDir ? "Players/%s.sg%x" : "%s.sg%x",
		Players[Player_num].callsign, slotnum);
	if (slotnum >= 10)
		fc = (slotnum - 10) + 'a';
	else
		fc = '0' + slotnum;
	sprintf(temp_fname, GameArg.SysUsePlayersDir ? "Players/%csecret.sgc" : "%csecret.sgc", fc);
	if (PHYSFSX_exists(temp_fname, 0)) {
		if (!PHYSFS_delete(temp_fname))
			Error("Cannot delete file <%s>: %s", temp_fname, PHYSFS_getLastError());
	}
	if (PHYSFSX_exists(SECRETC_FILENAME, 0)) {
		int copy_result = copy_file(SECRETC_FILENAME, temp_fname);
		Assert(copy_result == 0);
		(void)copy_result;
	}
	g_android_save_meta_kind = save_kind;
	g_android_save_blank_thumbnail = 1;
	result = state_save_all_sub(filename, save_desc);
	g_android_save_meta_kind = prev_kind;
	g_android_save_blank_thumbnail = prev_blank;
	if (!result)
		debug_log(DLOG_GAME, "autosave failed: D2 slot %d", slotnum);
	return result;
}
#endif

extern	fix	Flash_effect;
extern fix64 Time_flash_last_played;


int state_save_all_sub(char *filename, char *desc)
{
	int i,j;
	PHYSFS_file *fp;
	grs_canvas * cnv;
	char mission_filename[9];
#ifdef OGL
	GLint gl_draw_buffer;
#endif
	fix tmptime32 = 0;

	#ifndef NDEBUG
	if (GameArg.SysUsePlayersDir && strncmp(filename, "Players/", 8))
		Int3();
	#endif

	fp = PHYSFSX_openWriteBuffered(filename);
	if ( !fp ) {
		nm_messagebox(NULL, 1, TXT_OK, "Error writing savegame.\nPossibly out of disk\nspace.");
		start_time();
		return 0;
	}

//Save id
	PHYSFS_write(fp, dgss_id, sizeof(char) * 4, 1);

//Save version
	i = STATE_VERSION;
	PHYSFS_write(fp, &i, sizeof(int), 1);

// Save Coop state_game_id and this Player's callsign. Oh the redundancy... we have this one later on but Coop games want to read this before loading a state so for easy access save this here, too
	if (Game_mode & GM_MULTI_COOP)
	{
		PHYSFS_write(fp, &state_game_id, sizeof(uint), 1);
		PHYSFS_write(fp, &Players[Player_num].callsign, sizeof(char)*CALLSIGN_LEN+1, 1);
	}

//Save description
	PHYSFS_write(fp, desc, sizeof(char) * DESC_LENGTH, 1);

// Save the current screen shot...
	if (g_android_save_blank_thumbnail) {
		state_write_blank_thumbnail(fp);
	} else {
	cnv = gr_create_canvas( THUMBNAIL_W, THUMBNAIL_H );
	if ( cnv )
	{
#ifdef OGL
		ubyte *buf;
		ubyte *rgb;
		int k;
#endif
		grs_canvas * cnv_save;
		cnv_save = grd_curcanv;

		gr_set_current_canvas( cnv );

		render_frame(0, 0);

#if defined(OGL)
		buf = d_malloc(THUMBNAIL_W * THUMBNAIL_H * 4);
		rgb = d_malloc(THUMBNAIL_RGB_BYTES);
#ifndef OGLES
 		glGetIntegerv(GL_DRAW_BUFFER, &gl_draw_buffer);
 		glReadBuffer(gl_draw_buffer);
#endif
		ogl_prepare_framebuffer_readback();
		glReadPixels(0, SHEIGHT - THUMBNAIL_H, THUMBNAIL_W, THUMBNAIL_H, GL_RGBA, GL_UNSIGNED_BYTE, buf);
		// Store as 6-bit RGB (Descent palette range), Y-flipped so the
		// thumbnail reads top-down at preview time.
		k = THUMBNAIL_H;
		for (i = 0; i < THUMBNAIL_W * THUMBNAIL_H; i++) {
			int dst;
			if (!(j = i % THUMBNAIL_W))
				k--;
			dst = (THUMBNAIL_W * k + j) * 3;
			rgb[dst]     = buf[4*i]     / 4;
			rgb[dst + 1] = buf[4*i + 1] / 4;
			rgb[dst + 2] = buf[4*i + 2] / 4;
		}
		PHYSFS_write(fp, rgb, THUMBNAIL_RGB_BYTES, 1);
		d_free(rgb);
		d_free(buf);
#else
		{
			ubyte *rgb = d_malloc(THUMBNAIL_RGB_BYTES);
			if (rgb) {
				for (i = 0; i < THUMBNAIL_W * THUMBNAIL_H; i++) {
					ubyte idx = cnv->cv_bitmap.bm_data[i];
					rgb[i*3]     = gr_palette[idx*3];
					rgb[i*3 + 1] = gr_palette[idx*3 + 1];
					rgb[i*3 + 2] = gr_palette[idx*3 + 2];
				}
				PHYSFS_write(fp, rgb, THUMBNAIL_RGB_BYTES, 1);
				d_free(rgb);
			} else {
				state_write_blank_thumbnail(fp);
			}
		}
#endif

		gr_set_current_canvas(cnv_save);
		gr_free_canvas( cnv );
	}
	else
	{
		state_write_blank_thumbnail(fp);
	}
	}

// Save the Between levels flag...
	i = 0;
	PHYSFS_write(fp, &i, sizeof(int), 1);

// Save the mission info...
	memset(&mission_filename, '\0', 9);
	snprintf(mission_filename, 9, "%s", Current_mission_filename); // Current_mission_filename is not necessarily 9 bytes long so for saving we use a proper string - preventing corruptions
	PHYSFS_write(fp, &mission_filename, 9 * sizeof(char), 1);

//Save level info
	PHYSFS_write(fp, &Current_level_num, sizeof(int), 1);
	PHYSFS_write(fp, &Next_level_num, sizeof(int), 1);

//Save GameTime
// NOTE: GameTime now is GameTime64 with fix64 since GameTime could only last 9 hrs. To even help old Savegames, we do not increment Savegame version but rather RESET GameTime64 to 0 on every save! ALL variables based on GameTime64 now will get the current GameTime64 value substracted and saved to fix size as well.
	tmptime32 = 0;
	PHYSFS_write(fp, &tmptime32, sizeof(fix), 1);

//Save player info
	//PHYSFS_write(fp, &Players[Player_num], sizeof(player), 1);
	{
		player_rw *pl_rw;
		CALLOC(pl_rw, player_rw, 1);
		state_player_to_player_rw(&Players[Player_num], pl_rw);
		PHYSFS_write(fp, pl_rw, sizeof(player_rw), 1);
		d_free(pl_rw);
	}

// Save the current weapon info
	PHYSFS_write(fp, &Players[Player_num].primary_weapon, sizeof(sbyte), 1);
	PHYSFS_write(fp, &Players[Player_num].secondary_weapon, sizeof(sbyte), 1);

// Save the difficulty level
	PHYSFS_write(fp, &Difficulty_level, sizeof(int), 1);

// Save cheats enabled
	PHYSFS_write(fp, &cheats.enabled, sizeof(int), 1);

//Save object info
	i = Highest_object_index+1;
	PHYSFS_write(fp, &i, sizeof(int), 1);
	//PHYSFS_write(fp, Objects, sizeof(object), i);
	for (i = 0; i <= Highest_object_index; i++)
	{
		object_rw *obj_rw;
		CALLOC(obj_rw, object_rw, 1);
		state_object_to_object_rw(&Objects[i], obj_rw);
		PHYSFS_write(fp, obj_rw, sizeof(object_rw), 1);
		d_free(obj_rw);
	}
	
//Save wall info
	i = Num_walls;
	PHYSFS_write(fp, &i, sizeof(int), 1);
	PHYSFS_write(fp, Walls, sizeof(wall), i);

//Save exploding wall info
	i = MAX_EXPLODING_WALLS;
	PHYSFS_write(fp, &i, sizeof(int), 1);
	PHYSFS_write(fp, expl_wall_list, sizeof(*expl_wall_list), i);

//Save door info
	i = Num_open_doors;
	PHYSFS_write(fp, &i, sizeof(int), 1);
	PHYSFS_write(fp, ActiveDoors, sizeof(active_door), i);

//Save cloaking wall info
	i = Num_cloaking_walls;
	PHYSFS_write(fp, &i, sizeof(int), 1);
	PHYSFS_write(fp, CloakingWalls, sizeof(cloaking_wall), i);

//Save trigger info
	PHYSFS_write(fp, &Num_triggers, sizeof(int), 1);
	PHYSFS_write(fp, Triggers, sizeof(trigger), Num_triggers);

//Save tmap info
	for (i = 0; i <= Highest_segment_index; i++)
	{
		for (j = 0; j < 6; j++)
		{
			PHYSFS_write(fp, &Segments[i].sides[j].wall_num, sizeof(short), 1);
			PHYSFS_write(fp, &Segments[i].sides[j].tmap_num, sizeof(short), 1);
			PHYSFS_write(fp, &Segments[i].sides[j].tmap_num2, sizeof(short), 1);
		}
	}

// Save the fuelcen info
	PHYSFS_write(fp, &Control_center_destroyed, sizeof(int), 1);
	PHYSFS_write(fp, &Countdown_timer, sizeof(int), 1);
	PHYSFS_write(fp, &Num_robot_centers, sizeof(int), 1);
	PHYSFS_write(fp, RobotCenters, sizeof(matcen_info), Num_robot_centers);
	PHYSFS_write(fp, &ControlCenterTriggers, sizeof(control_center_triggers), 1);
	PHYSFS_write(fp, &Num_fuelcenters, sizeof(int), 1);
	PHYSFS_write(fp, Station, sizeof(FuelCenter), Num_fuelcenters);

// Save the control cen info
	PHYSFS_write(fp, &Control_center_been_hit, sizeof(int), 1);
	PHYSFS_write(fp, &Control_center_player_been_seen, sizeof(int), 1);
	PHYSFS_write(fp, &Control_center_next_fire_time, sizeof(int), 1);
	PHYSFS_write(fp, &Control_center_present, sizeof(int), 1);
	PHYSFS_write(fp, &Dead_controlcen_object_num, sizeof(int), 1);

// Save the AI state
	ai_save_state( fp );

// Save the automap visited info
	if ( Highest_segment_index+1 > MAX_SEGMENTS_ORIGINAL )
	{
		PHYSFS_write(fp, Automap_visited, sizeof(ubyte), Highest_segment_index + 1);
	}
	else
		PHYSFS_write(fp, Automap_visited, sizeof(ubyte), MAX_SEGMENTS_ORIGINAL);

	PHYSFS_write(fp, &state_game_id, sizeof(uint), 1);
	i = 0;
	PHYSFS_write(fp, &cheats.rapidfire, sizeof(int), 1);
	PHYSFS_write(fp, &i, sizeof(int), 1); // was Lunacy
	PHYSFS_write(fp, &i, sizeof(int), 1); // was Lunacy, too... and one was Ugly robot stuff a long time ago...

	// Save automap marker info

	PHYSFS_write(fp, MarkerObject, sizeof(MarkerObject) ,1);
	PHYSFS_write(fp, &Players[0].callsign[0], sizeof(char), (NUM_MARKERS)*(CALLSIGN_LEN+1)); // PHYSFS_write(fp, MarkerOwner, sizeof(MarkerOwner), 1); MarkerOwner is obsolete
	PHYSFS_write(fp, MarkerMessage, sizeof(MarkerMessage), 1);

	PHYSFS_write(fp, &Players[Player_num].afterburner_charge, sizeof(fix), 1);

	//save last was super information
	PHYSFS_write(fp, &Primary_last_was_super, sizeof(Primary_last_was_super), 1);
	PHYSFS_write(fp, &Secondary_last_was_super, sizeof(Secondary_last_was_super), 1);

	//	Save flash effect stuff
	PHYSFS_write(fp, &Flash_effect, sizeof(int), 1);
	if (Time_flash_last_played - GameTime64 < F1_0*(-18000))
		tmptime32 = F1_0*(-18000);
	else
		tmptime32 = Time_flash_last_played - GameTime64;
	PHYSFS_write(fp, &tmptime32, sizeof(fix), 1);
	PHYSFS_write(fp, &PaletteRedAdd, sizeof(int), 1);
	PHYSFS_write(fp, &PaletteGreenAdd, sizeof(int), 1);
	PHYSFS_write(fp, &PaletteBlueAdd, sizeof(int), 1);
	if ( Highest_segment_index+1 > MAX_SEGMENTS_ORIGINAL )
	{
		PHYSFS_write(fp, Light_subtracted, sizeof(Light_subtracted[0]), Highest_segment_index + 1);
	}
	else
		PHYSFS_write(fp, Light_subtracted, sizeof(Light_subtracted[0]), MAX_SEGMENTS_ORIGINAL);
	PHYSFS_write(fp, &First_secret_visit, sizeof(First_secret_visit), 1);
	PHYSFS_write(fp, &Omega_charge, sizeof(Omega_charge), 1);

// Save Coop Info
	if (Game_mode & GM_MULTI_COOP)
	{
		for (i = 0; i < MAX_PLAYERS; i++) // I know, I know we only allow 4 players in coop. I screwed that up. But if we ever allow 8 players in coop, who's gonna laugh then?
		{
			player_rw *pl_rw;
			CALLOC(pl_rw, player_rw, 1);
			state_player_to_player_rw(&Players[i], pl_rw);
			PHYSFS_write(fp, pl_rw, sizeof(player_rw), 1);
			d_free(pl_rw);
		}
		PHYSFS_write(fp, &Netgame.mission_title, sizeof(char), MISSION_NAME_LEN+1);
		PHYSFS_write(fp, &Netgame.mission_name, sizeof(char), 9);
		PHYSFS_write(fp, &Netgame.levelnum, sizeof(int), 1);
		PHYSFS_write(fp, &Netgame.difficulty, sizeof(ubyte), 1);
		PHYSFS_write(fp, &Netgame.game_status, sizeof(ubyte), 1);
		PHYSFS_write(fp, &Netgame.numplayers, sizeof(ubyte), 1);
		PHYSFS_write(fp, &Netgame.max_numplayers, sizeof(ubyte), 1);
		PHYSFS_write(fp, &Netgame.numconnected, sizeof(ubyte), 1);
		PHYSFS_write(fp, &Netgame.level_time, sizeof(int), 1);
	}

	state_write_runtime_state(fp);

#ifdef __ANDROID__
	coop_write_save_metadata(fp);
	{
		android_save_meta_disk android_meta;
		android_save_meta_write_params android_params;
		char android_desc[DESC_LENGTH + 1];

		memset(&android_params, 0, sizeof(android_params));
		memcpy(android_desc, desc, DESC_LENGTH);
		android_desc[DESC_LENGTH] = '\0';
		android_params.game_id = ANDROID_SAVE_META_GAME_D2;
		android_params.save_kind = g_android_save_meta_kind;
		android_params.callsign = Players[Player_num].callsign;
		android_params.description = android_desc;
		android_params.mission_name = mission_filename;
		android_params.level_num = Current_level_num;
		android_params.level_name = Current_level_name;
		android_params.level_seconds = state_time_to_seconds(
			Players[Player_num].time_level, Players[Player_num].hours_level);
		android_params.total_seconds = state_time_to_seconds(
			Players[Player_num].time_total, Players[Player_num].hours_total);
		if (android_save_meta_build(&android_meta, &android_params))
			PHYSFS_write(fp, &android_meta, sizeof(android_meta), 1);
	}
#endif

	PHYSFS_close(fp);
	
	start_time();

	return 1;
}

//	-----------------------------------------------------------------------------------
//	Set the player's position from the globals Secret_return_segment and Secret_return_orient.
void set_pos_from_return_segment(void)
{
	int	plobjnum = Players[Player_num].objnum;

	compute_segment_center(&Objects[plobjnum].pos, &Segments[Secret_return_segment]);
	obj_relink(plobjnum, Secret_return_segment);
	reset_player_object();
	Objects[plobjnum].orient = Secret_return_orient;
}

//	-----------------------------------------------------------------------------------
int state_restore_all(int in_game, int secret_restore, char *filename_override)
{
	char filename[PATH_MAX];
	int	filenum = -1;

	if (in_game && (Current_level_num < 0) && (secret_restore == 0)) {
		HUD_init_message_literal(HM_DEFAULT,  "Can't restore in secret level!" );
		return 0;
	}

	if ( Newdemo_state == ND_STATE_RECORDING )
		newdemo_stop_recording(0);

	if ( Newdemo_state != ND_STATE_NORMAL )
		return 0;

	if ( Game_mode & GM_MULTI )
	{
		if (Game_mode & GM_MULTI_COOP)
			multi_initiate_restore_game();
		return 0;
	}

	stop_time();

	if (filename_override) {
		strcpy(filename, filename_override);
		filenum = NUM_SAVES+1; // place outside of save slots
	} else if (!(filenum = state_get_restore_file(filename)))	{
		start_time();
		return 0;
	}
	
	//	MK, 1/1/96
	//	Do special secret level stuff.
	//	If Nsecret.sgc (where N = filenum) exists, then copy it to secret.sgc.
	//	If it doesn't exist, then delete secret.sgc
	if (!secret_restore) {
		int	rval;
		char	temp_fname[PATH_MAX], fc;

		if (filenum != -1) {
			if (filenum >= 10)
				fc = (filenum-10) + 'a';
			else
				fc = '0' + filenum;
			
			snprintf(temp_fname, PATH_MAX, GameArg.SysUsePlayersDir? "Players/%csecret.sgc" : "%csecret.sgc", fc);

			if (PHYSFSX_exists(temp_fname,0))
			{
				rval = copy_file(temp_fname, SECRETC_FILENAME);
				Assert(rval == 0);	//	Oops, error copying temp_fname to secret.sgc!
				(void)rval;
			} else
				PHYSFS_delete(SECRETC_FILENAME);
		}
	}

	if ( !secret_restore && in_game ) {
		int choice;
		choice =  nm_messagebox( NULL, 2, "Yes", "No", "Restore Game?" );
		if ( choice != 0 )	{
			start_time();
			return 0;
		}
	}

	start_time();

	return state_restore_all_sub(filename, secret_restore);
}

int state_restore_all_path(int in_game, char *filename_override)
{
	return state_restore_all(in_game, 0, filename_override);
}

extern void init_player_stats_new_ship(ubyte pnum);

void ShowLevelIntro(int level_num);

extern void do_cloak_invul_secret_stuff(fix64 old_gametime);
extern void copy_defaults_to_robot(object *objp);

int state_restore_all_sub(char *filename, int secret_restore)
{
	int version,i, j, segnum, coop_player_got[MAX_PLAYERS], coop_org_objnum = Players[Player_num].objnum;
	object * obj;
	PHYSFS_file *fp;
	int swap = 0;	// if file is not endian native, have to swap all shorts and ints
	int current_level;
	char mission[128];
	char desc[DESC_LENGTH+1];
	char id[5];
	char org_callsign[CALLSIGN_LEN+16];
	fix tmptime32 = 0;
	fix64	old_gametime = GameTime64;
	short TempTmapNum[MAX_SEGMENTS][MAX_SIDES_PER_SEGMENT];
	short TempTmapNum2[MAX_SEGMENTS][MAX_SIDES_PER_SEGMENT];

	#ifndef NDEBUG
	if (GameArg.SysUsePlayersDir && strncmp(filename, "Players/", 8))
		Int3();
	#endif

	fp = PHYSFSX_openReadBuffered(filename);
	if ( !fp ) {
		con_printf(CON_URGENT, "restore: could not open '%s'\n", filename);
#ifdef __ANDROID__
		debug_log(DLOG_GAME, "restore open failed: game=d2 file='%s' current_callsign='%s' game_mode=%d",
			filename, Players[Player_num].callsign, Game_mode);
#endif
		return 0;
	}
#ifdef __ANDROID__
	debug_log(DLOG_GAME, "restore open: game=d2 file='%s' current_callsign='%s' player_num=%d game_mode=%d secret_restore=%d",
		filename, Players[Player_num].callsign, Player_num, Game_mode, secret_restore);
	state_android_log_save_metadata(filename, fp);
#endif

//Read id
	PHYSFS_read(fp, id, sizeof(char) * 4, 1);
	if ( memcmp( id, dgss_id, 4 )) {
		con_printf(CON_URGENT, "restore: bad save id in '%s'\n", filename);
#ifdef __ANDROID__
		debug_log(DLOG_GAME, "restore bad id: game=d2 file='%s' id='%c%c%c%c'",
			filename, id[0], id[1], id[2], id[3]);
#endif
		PHYSFS_close(fp);
		return 0;
	}

//Read version
	//Check for swapped file here, as dgss_id is written as a string (i.e. endian independent)
	PHYSFS_read(fp, &version, sizeof(int), 1);
	if (version & 0xffff0000)
	{
		swap = 1;
		version = SWAPINT(version);
	}
#ifdef __ANDROID__
	debug_log(DLOG_GAME, "restore version: game=d2 file='%s' version=%d swap=%d compatible=%d runtime=%d",
		filename, version, swap, STATE_COMPATIBLE_VERSION, STATE_RUNTIME_VERSION);
#endif

	if (version < STATE_COMPATIBLE_VERSION)	{
		con_printf(CON_URGENT, "restore: unsupported save version %d in '%s'\n", version, filename);
#ifdef __ANDROID__
		debug_log(DLOG_GAME, "restore unsupported version: game=d2 file='%s' version=%d compatible=%d",
			filename, version, STATE_COMPATIBLE_VERSION);
#endif
		PHYSFS_close(fp);
		return 0;
	}

// Read Coop state_game_id. Oh the redundancy... we have this one later on but Coop games want to read this before loading a state so for easy access we have this here
	if (Game_mode & GM_MULTI_COOP)
	{
		char saved_callsign[CALLSIGN_LEN+1];
		state_game_id = PHYSFSX_readSXE32(fp, swap);
		PHYSFS_read(fp, &saved_callsign, sizeof(char)*CALLSIGN_LEN+1, 1);
#ifdef __ANDROID__
		if (strcmp(saved_callsign, Players[Player_num].callsign) &&
		    strcmp(saved_callsign, COOP_AUTOSAVE_CALLSIGN))
#else
		if (strcmp(saved_callsign, Players[Player_num].callsign))
#endif
		{
			con_printf(CON_URGENT, "restore: coop callsign mismatch '%s' vs '%s'\n",
				saved_callsign, Players[Player_num].callsign);
#ifdef __ANDROID__
			debug_log(DLOG_GAME, "restore coop callsign mismatch: game=d2 file='%s' saved='%s' current='%s'",
				filename, saved_callsign, Players[Player_num].callsign);
#endif
			PHYSFS_close(fp);
			return 0;
		}
	}

// Read description
	PHYSFS_read(fp, desc, sizeof(char) * DESC_LENGTH, 1);
	desc[DESC_LENGTH] = '\0';

// Skip the current screen shot...
	state_skip_thumbnail(fp, version);

// Read the Between levels flag...
	i = PHYSFSX_readSXE32(fp, swap);
	i = 0;

// Read the mission info...
	PHYSFS_read(fp, mission, sizeof(char) * 9, 1);

	if (mission[8] == 1) { // rebirth savegame_mission_name_abi::pathname
		char *p;
		PHYSFS_read(fp, mission, 128, 1);
		if (mission[127]) {
			con_printf(CON_URGENT, "restore: invalid mission name in '%s'\n", filename);
#ifdef __ANDROID__
			debug_log(DLOG_GAME, "restore invalid mission name: game=d2 file='%s'", filename);
#endif
			PHYSFS_close(fp);
			return 0;
		}
		if ((p = strrchr(mission, '/')))
			memmove(mission, p + 1, strlen(p + 1) + 1);
	}

	if (!load_mission_by_name( mission ))	{
		con_printf(CON_URGENT, "restore: unable to load mission '%s' from '%s'\n", mission, filename);
#ifdef __ANDROID__
		debug_log(DLOG_GAME, "restore unable to load mission: game=d2 file='%s' mission='%s'",
			filename, mission);
#endif
		nm_messagebox( NULL, 1, "Ok", "Error!\nUnable to load mission\n'%s'\n", mission );
		PHYSFS_close(fp);
		return 0;
	}

//Read level info
	current_level = PHYSFSX_readSXE32(fp, swap);
	PHYSFS_seek(fp, PHYSFS_tell(fp) + sizeof(PHYSFS_sint32)); // skip Next_level_num

//Restore GameTime
	tmptime32 = PHYSFSX_readSXE32(fp, swap);
	GameTime64 = (fix64)tmptime32;
	if (input_demo_replay_has_checkpoint())
		GameTime64 += input_demo_replay_checkpoint_start_gt();
#ifdef __ANDROID__
	{
		char desc_log[DESC_LENGTH + 1];
		char mission_log[sizeof(mission)];

		state_android_copy_log_string(desc_log, sizeof(desc_log), desc, DESC_LENGTH);
		state_android_copy_log_string(mission_log, sizeof(mission_log), mission, sizeof(mission));
		debug_log(DLOG_GAME,
			"restore save header: game=d2 file='%s' version=%d swap=%d desc='%s' mission='%s' level=%d gametime=%d current_callsign='%s' game_mode=%d secret_restore=%d",
			filename, version, swap, desc_log, mission_log, current_level,
			(int)tmptime32, Players[Player_num].callsign, Game_mode,
			secret_restore);
	}
#endif

// Start new game....
	if (!(Game_mode & GM_MULTI_COOP))
	{
		Game_mode = GM_NORMAL;
#ifdef NETWORK
		change_playernum_to(0);
#endif
		N_players = 1;
		strcpy( org_callsign, Players[0].callsign );
		if (!secret_restore) {
			InitPlayerObject();				//make sure player's object set up
			init_player_stats_game(0);		//clear all stats
		}
	}
	else // in coop we want to stay the player we are already.
	{
		strcpy( org_callsign, Players[Player_num].callsign );
		if (!secret_restore)
			init_player_stats_game(Player_num);
	}

	if (Game_wind)
		window_set_visible(Game_wind, 0);

//Read player info

	{
		StartNewLevelSub(current_level, 1, secret_restore);

		if (secret_restore) {
			player	dummy_player;
			player_rw *pl_rw;
			MALLOC(pl_rw, player_rw, 1);
			PHYSFS_read(fp, pl_rw, sizeof(player_rw), 1);
			player_rw_swap(pl_rw, swap);
			state_player_rw_to_player(pl_rw, &dummy_player);
			d_free(pl_rw);
			if (secret_restore == 1) {		//	This means he didn't die, so he keeps what he got in the secret level.
				Players[Player_num].level = dummy_player.level;
				Players[Player_num].last_score = dummy_player.last_score;
				Players[Player_num].time_level = dummy_player.time_level;

				Players[Player_num].num_robots_level = dummy_player.num_robots_level;
				Players[Player_num].num_robots_total = dummy_player.num_robots_total;
				Players[Player_num].hostages_rescued_total = dummy_player.hostages_rescued_total;
				Players[Player_num].hostages_total = dummy_player.hostages_total;
				Players[Player_num].hostages_on_board = dummy_player.hostages_on_board;
				Players[Player_num].hostages_level = dummy_player.hostages_level;
				Players[Player_num].homing_object_dist = dummy_player.homing_object_dist;
				Players[Player_num].hours_level = dummy_player.hours_level;
				Players[Player_num].hours_total = dummy_player.hours_total;
				do_cloak_invul_secret_stuff(old_gametime);
			} else {
				Players[Player_num] = dummy_player;
			}
		} else {
			player_rw *pl_rw;
			MALLOC(pl_rw, player_rw, 1);
			PHYSFS_read(fp, pl_rw, sizeof(player_rw), 1);
			player_rw_swap(pl_rw, swap);
			state_player_rw_to_player(pl_rw, &Players[Player_num]);
			d_free(pl_rw);
		}
	}
	strcpy( Players[Player_num].callsign, org_callsign );
	if (Game_mode & GM_MULTI_COOP)
		Players[Player_num].objnum = coop_org_objnum;
#ifdef __ANDROID__
	debug_log(DLOG_GAME,
		"restore player state: game=d2 file='%s' callsign='%s' level=%d lives=%d objnum=%d n_players=%d game_mode=%d secret_restore=%d",
		filename, Players[Player_num].callsign, Players[Player_num].level,
		Players[Player_num].lives, Players[Player_num].objnum, N_players,
		Game_mode, secret_restore);
#endif

// Restore the weapon states
	PHYSFS_read(fp, &Players[Player_num].primary_weapon, sizeof(sbyte), 1);
	PHYSFS_read(fp, &Players[Player_num].secondary_weapon, sizeof(sbyte), 1);

	select_weapon(Players[Player_num].primary_weapon, 0, 0, 0);
	select_weapon(Players[Player_num].secondary_weapon, 1, 0, 0);

// Restore the difficulty level
	Difficulty_level = PHYSFSX_readSXE32(fp, swap);

// Restore the cheats enabled flag
	game_disable_cheats(); // disable cheats first
	cheats.enabled = PHYSFSX_readSXE32(fp, swap);

	Do_appearance_effect = 0;			// Don't do this for middle o' game stuff.

	//Clear out all the objects from the lvl file
	for (segnum=0; segnum <= Highest_segment_index; segnum++)
		Segments[segnum].objects = -1;
	reset_objects(1);
	init_morphs();
	state_clear_stuck_object_state();
	state_clear_controlcen_runtime_state();
	state_clear_afterburner_runtime_state();

	//Read objects, and pop 'em into their respective segments.
	i = PHYSFSX_readSXE32(fp, swap);
	Highest_object_index = i-1;
	//object_read_n_swap(Objects, i, swap, fp);
	for (i=0; i<=Highest_object_index; i++ )
	{
		object_rw *obj_rw;
		MALLOC(obj_rw, object_rw, 1);
		PHYSFS_read(fp, obj_rw, sizeof(object_rw), 1);
		object_rw_swap(obj_rw, swap);
		state_object_rw_to_object(obj_rw, &Objects[i]);
		d_free(obj_rw);
	}

	//Check for rebirth missing signatures
	if (!Objects[0].signature && !Objects[1].signature)
		for (i=0; i<=Highest_object_index; i++)
			Objects[i].signature = i + 1;

	if (!state_restore_segment_object_links()) {
		input_demo_append_replay_probe_message("checkpoint_object_links", NULL,
			"restore=invalid fallback=relink_by_index");
		state_relink_objects_by_index();
	} else {
		input_demo_append_replay_probe_message("checkpoint_object_links", NULL,
			"restore=ok");
	}

	for (i=0; i<=Highest_object_index; i++ )	{
		obj = &Objects[i];

		//look for, and fix, boss with bogus shields
		if (obj->type == OBJ_ROBOT && Robot_info[obj->id].boss_flag) {
			fix save_shields = obj->shields;

			copy_defaults_to_robot(obj);		//calculate starting shields

			//if in valid range, use loaded shield value
			if (save_shields > 0 && save_shields <= obj->shields)
				obj->shields = save_shields;
			else
				obj->shields /= 2;  //give player a break
		}
	}
	special_reset_objects();

	//	1 = Didn't die on secret level.
	//	2 = Died on secret level.
	if (secret_restore && (Current_level_num >= 0)) {
		set_pos_from_return_segment();
		if (secret_restore == 2)
			init_player_stats_new_ship(Player_num);
	}

	//Restore wall info
	Num_walls = PHYSFSX_readSXE32(fp, swap);
	wall_read_n_swap(Walls, Num_walls, swap, fp);

	//Check for rebirth linked_wall value
	for (i=0;i<Num_walls;i++)
		if (Walls[i].linked_wall == 65535) // rebirth dcx::wallnum_t::None
			Walls[i].linked_wall = -1;

	//now that we have the walls, check if any sounds are linked to
	//walls that are now open
	for (i=0;i<Num_walls;i++) {
		if (Walls[i].type == WALL_OPEN)
			digi_kill_sound_linked_to_segment(Walls[i].segnum,Walls[i].sidenum,-1);	//-1 means kill any sound
	}

	//Restore exploding wall info
	if (version >= 10) {
		i = PHYSFSX_readSXE32(fp, swap);
		expl_wall_read_n_swap(expl_wall_list, i, swap, fp);
	}

	//Restore door info
	Num_open_doors = PHYSFSX_readSXE32(fp, swap);
	active_door_read_n_swap(ActiveDoors, Num_open_doors, swap, fp);

	if (version >= 14) {		//Restore cloaking wall info
		Num_cloaking_walls = PHYSFSX_readSXE32(fp, swap);
		cloaking_wall_read_n_swap(CloakingWalls, Num_cloaking_walls, swap, fp);
	}

	//Restore trigger info
	Num_triggers = PHYSFSX_readSXE32(fp, swap);
	trigger_read_n_swap(Triggers, Num_triggers, swap, fp);

	//Restore tmap info (to temp values so we can use compiled-in tmap info to compute static_light
	for (i=0; i<=Highest_segment_index; i++ )	{
		for (j=0; j<6; j++ )	{
			Segments[i].sides[j].wall_num = PHYSFSX_readSXE16(fp, swap);
			TempTmapNum[i][j] = PHYSFSX_readSXE16(fp, swap);
			TempTmapNum2[i][j] = PHYSFSX_readSXE16(fp, swap);
		}
	}

	//Restore the fuelcen info
	Control_center_destroyed = PHYSFSX_readSXE32(fp, swap);
	Countdown_timer = PHYSFSX_readSXE32(fp, swap);
	Num_robot_centers = PHYSFSX_readSXE32(fp, swap);
	matcen_info_read_n_swap(RobotCenters, Num_robot_centers, swap, fp);
	control_center_triggers_read_n_swap(&ControlCenterTriggers, 1, swap, fp);
	Num_fuelcenters = PHYSFSX_readSXE32(fp, swap);
	fuelcen_read_n_swap(Station, Num_fuelcenters, swap, fp);

	// Restore the control cen info
	Control_center_been_hit = PHYSFSX_readSXE32(fp, swap);
	Control_center_player_been_seen = PHYSFSX_readSXE32(fp, swap);
	Control_center_next_fire_time = PHYSFSX_readSXE32(fp, swap);
	Control_center_present = PHYSFSX_readSXE32(fp, swap);
	Dead_controlcen_object_num = PHYSFSX_readSXE32(fp, swap);
	if (Control_center_destroyed)
		Total_countdown_time = Countdown_timer/F0_5; // we do not need to know this, but it should not be 0 either...
		

	// Restore the AI state
	ai_restore_state( fp, version, swap );

	// Restore the automap visited info
	if ( Highest_segment_index+1 > MAX_SEGMENTS_ORIGINAL )
	{
		memset(&Automap_visited, 0, MAX_SEGMENTS);
		PHYSFS_read(fp, Automap_visited, sizeof(ubyte), Highest_segment_index + 1);
	}
	else
		PHYSFS_read(fp, Automap_visited, sizeof(ubyte), MAX_SEGMENTS_ORIGINAL);

	//	Restore hacked up weapon system stuff.
	Auto_fire_fusion_cannon_time = 0;
	Next_laser_fire_time = GameTime64;
	Next_missile_fire_time = GameTime64;
	Last_laser_fired_time = GameTime64;

	state_game_id = 0;

	if ( version >= 7 )	{
		state_game_id = PHYSFSX_readSXE32(fp, swap);
		cheats.rapidfire = PHYSFSX_readSXE32(fp, swap);
		PHYSFS_seek(fp, PHYSFS_tell(fp) + sizeof(PHYSFS_sint32)); // PHYSFSX_readSXE32(fp, swap); // was Lunacy
		PHYSFS_seek(fp, PHYSFS_tell(fp) + sizeof(PHYSFS_sint32)); // PHYSFSX_readSXE32(fp, swap); // was Lunacy, too... and one was Ugly robot stuff a long time ago...
	}

	if (version >= 17) {
		for (i = 0; i < NUM_MARKERS; i++)
			MarkerObject[i] = PHYSFSX_readSXE32(fp, swap);
		PHYSFS_seek(fp, PHYSFS_tell(fp) + (NUM_MARKERS)*(CALLSIGN_LEN+1)); // PHYSFS_read(fp, MarkerOwner, sizeof(MarkerOwner), 1); // skip obsolete MarkerOwner
		PHYSFS_read(fp, MarkerMessage, sizeof(MarkerMessage), 1);
	}
	else {
		int num;

		// skip dummy info

		num = PHYSFSX_readSXE32(fp, swap);           // was NumOfMarkers
		PHYSFS_seek(fp, PHYSFS_tell(fp) + sizeof(PHYSFS_sint32)); // PHYSFSX_readSXE32(fp, swap); // was CurMarker

		PHYSFS_seek(fp, PHYSFS_tell(fp) + num * (sizeof(vms_vector) + 40));

		for (num=0;num<NUM_MARKERS;num++)
			MarkerObject[num] = -1;
	}

	if (version>=11) {
		if (secret_restore != 1)
			Players[Player_num].afterburner_charge = PHYSFSX_readSXE32(fp, swap);
		else {
			PHYSFSX_readSXE32(fp, swap);
		}
	}
	if (version>=12) {
		//read last was super information
		PHYSFS_read(fp, &Primary_last_was_super, sizeof(Primary_last_was_super), 1);
		PHYSFS_read(fp, &Secondary_last_was_super, sizeof(Secondary_last_was_super), 1);
	}

	if (version >= 12) {
		Flash_effect = PHYSFSX_readSXE32(fp, swap);
		tmptime32 = PHYSFSX_readSXE32(fp, swap);
		Time_flash_last_played = (fix64)tmptime32;
		PaletteRedAdd = PHYSFSX_readSXE32(fp, swap);
		PaletteGreenAdd = PHYSFSX_readSXE32(fp, swap);
		PaletteBlueAdd = PHYSFSX_readSXE32(fp, swap);
	} else {
		Flash_effect = 0;
		Time_flash_last_played = 0;
		PaletteRedAdd = 0;
		PaletteGreenAdd = 0;
		PaletteBlueAdd = 0;
	}

	//	Load Light_subtracted
	if (version >= 16) {
		if ( Highest_segment_index+1 > MAX_SEGMENTS_ORIGINAL )
		{
			memset(&Light_subtracted, 0, sizeof(Light_subtracted[0])*MAX_SEGMENTS);
			PHYSFS_read(fp, Light_subtracted, sizeof(Light_subtracted[0]), Highest_segment_index + 1);
		}
		else
			PHYSFS_read(fp, Light_subtracted, sizeof(Light_subtracted[0]), MAX_SEGMENTS_ORIGINAL);
		apply_all_changed_light();
	} else {
		int	i;
		for (i=0; i<=Highest_segment_index; i++)
			Light_subtracted[i] = 0;
	}

	// static_light should now be computed - now actually set tmap info
	for (i=0; i<=Highest_segment_index; i++ )	{
		for (j=0; j<6; j++ )	{
			Segments[i].sides[j].tmap_num=TempTmapNum[i][j];
			Segments[i].sides[j].tmap_num2=TempTmapNum2[i][j];
		}
	}

	if (!secret_restore) {
		if (version >= 20) {
			First_secret_visit = PHYSFSX_readSXE32(fp, swap);
		} else
			First_secret_visit = 1;
	} else
		First_secret_visit = 0;

	if (version >= 22)
	{
		if (secret_restore != 1)
			Omega_charge = PHYSFSX_readSXE32(fp, swap);
		else
			PHYSFSX_readSXE32(fp, swap);
	}

// Read Coop Info
	if (Game_mode & GM_MULTI_COOP)
	{
		player restore_players[MAX_PLAYERS];
		object restore_objects[MAX_PLAYERS];
		int coop_got_nplayers = 0;

		for (i = 0; i < MAX_PLAYERS; i++) 
		{
			player_rw *pl_rw;
			object *obj;

			// prepare arrays for mapping our players below
			coop_player_got[i] = 0;

			// read the stored players
			MALLOC(pl_rw, player_rw, 1);
			PHYSFS_read(fp, pl_rw, sizeof(player_rw), 1);
			player_rw_swap(pl_rw, swap);
			state_player_rw_to_player(pl_rw, &restore_players[i]);
			d_free(pl_rw);
			
			// make all (previous) player objects to ghosts but store them first for later remapping
			obj = &Objects[restore_players[i].objnum];
			if (restore_players[i].connected == CONNECT_PLAYING && obj->type == OBJ_PLAYER)
			{
				memcpy(&restore_objects[i], obj, sizeof(object));
				obj->type = OBJ_GHOST;
				multi_reset_player_object(obj);
			}
		}
		#ifdef __ANDROID__
		for (i = 0; i < N_players; i++)
		{
			COOPLOG("restore_players[%d]: callsign='%s' connected=%d objnum=%d",
				i, restore_players[i].callsign, restore_players[i].connected,
				restore_players[i].objnum);
			COOPLOG("current_players[%d]: callsign='%s' connected=%d objnum=%d",
				i, Players[i].callsign, Players[i].connected,
				Players[i].objnum);
		}
		#endif
#ifdef __ANDROID__
		/* Match current players to saved players using client_id from
		 * the metadata trailer (preferred) or callsign (fallback).
		 * Read metadata early by seeking to end of file, then seek back. */
		{
			coop_save_metadata meta_early;
			int have_meta = 0;
			PHYSFS_sint64 saved_pos = PHYSFS_tell(fp);
			PHYSFS_sint64 meta_start = PHYSFS_fileLength(fp) - (PHYSFS_sint64)sizeof(coop_save_metadata);
			if (meta_start > saved_pos)
				have_meta = coop_read_save_metadata(fp, meta_start, &meta_early);
			PHYSFS_seek(fp, saved_pos);

			for (i = 0; i < MAX_PLAYERS; i++)
			{
				object *obj;
				int sav_objnum;

				if (!(Players[i].connected == CONNECT_PLAYING ||
				      Players[i].connected == CONNECT_WAITING))
					continue;

				j = -1;
				/* Try metadata match (client_id first, callsign second) */
				if (have_meta) {
					int meta_idx = coop_find_player_in_metadata(
						Players[i].callsign,
						Netgame.players[i].client_id,
						&meta_early);
					if (meta_idx >= 0 && meta_idx < meta_early.num_active_players)
						j = meta_early.active_players[meta_idx].original_slot;
				}

				if (j < 0 || j >= MAX_PLAYERS ||
				    restore_players[j].connected != CONNECT_PLAYING) {
					COOPLOG("P%d '%s' not found in save -- spawning fresh", i, Players[i].callsign);
					HUD_init_message(HM_MULTI, "'%s' not in save -- spawning fresh",
						Players[i].callsign);
					continue;
				}

				sav_objnum = Players[i].objnum;
				memcpy(&Players[i], &restore_players[j], sizeof(player));
				Players[i].objnum = sav_objnum;
				coop_player_got[i] = 1;
				coop_got_nplayers++;
				COOPLOG("mapped P%d '%s' -> save slot %d, objnum=%d",
					i, Players[i].callsign, j, sav_objnum);

				obj = &Objects[Players[i].objnum];
				// since a player always uses the same object, we just have to copy the saved object properties to the existing one. i hate you...
				obj->id = i; // assign player object id to player number
				obj->control_type = restore_objects[j].control_type;
				obj->movement_type = restore_objects[j].movement_type;
				obj->render_type = restore_objects[j].render_type;
				obj->flags = restore_objects[j].flags;
				obj->pos = restore_objects[j].pos;
				obj->orient = restore_objects[j].orient;
				obj->size = restore_objects[j].size;
				obj->shields = restore_objects[j].shields;
				obj->lifeleft = restore_objects[j].lifeleft;
				obj->mtype.phys_info = restore_objects[j].mtype.phys_info;
				obj->rtype.pobj_info = restore_objects[j].rtype.pobj_info;
				// make this restored player object an actual player again
				obj->type = OBJ_PLAYER;
				multi_reset_player_object(obj);
				update_object_seg(obj);
				COOPLOG("P%d post-reset: ct=%d mt=%d phys_flags=0x%x",
					i, obj->control_type, obj->movement_type,
					obj->mtype.phys_info.flags);
			}
		}
#else
		for (i = 0; i < MAX_PLAYERS; i++) // copy restored players to the current slots
		{
			for (j = 0; j < MAX_PLAYERS; j++)
			{
				// map stored players to current players depending on their unique (which we made sure) callsign
				if (Players[i].connected == CONNECT_PLAYING && restore_players[j].connected == CONNECT_PLAYING && !strcmp(Players[i].callsign, restore_players[j].callsign))
				{
					object *obj;
					int sav_objnum = Players[i].objnum;
					
					memcpy(&Players[i], &restore_players[j], sizeof(player));
					Players[i].objnum = sav_objnum;
					
					coop_player_got[i] = 1;
					coop_got_nplayers++;

					obj = &Objects[Players[i].objnum];
					obj->id = i;
					obj->control_type = restore_objects[j].control_type;
					obj->movement_type = restore_objects[j].movement_type;
					obj->render_type = restore_objects[j].render_type;
					obj->flags = restore_objects[j].flags;
					obj->pos = restore_objects[j].pos;
					obj->orient = restore_objects[j].orient;
					obj->size = restore_objects[j].size;
					obj->shields = restore_objects[j].shields;
					obj->lifeleft = restore_objects[j].lifeleft;
					obj->mtype.phys_info = restore_objects[j].mtype.phys_info;
					obj->rtype.pobj_info = restore_objects[j].rtype.pobj_info;
					obj->type = OBJ_PLAYER;
					multi_reset_player_object(obj);
					update_object_seg(obj);
				}
			}
		}
#endif
		PHYSFS_read(fp, &Netgame.mission_title, sizeof(char), MISSION_NAME_LEN+1);
		PHYSFS_read(fp, &Netgame.mission_name, sizeof(char), 9);
		Netgame.levelnum = PHYSFSX_readSXE32(fp, swap);
		PHYSFS_read(fp, &Netgame.difficulty, sizeof(ubyte), 1);
		PHYSFS_read(fp, &Netgame.game_status, sizeof(ubyte), 1);
		PHYSFS_read(fp, &Netgame.numplayers, sizeof(ubyte), 1);
		PHYSFS_read(fp, &Netgame.max_numplayers, sizeof(ubyte), 1);
		PHYSFS_read(fp, &Netgame.numconnected, sizeof(ubyte), 1);
		Netgame.level_time = PHYSFSX_readSXE32(fp, swap);
#ifdef __ANDROID__
		/* Player count may differ from saved state (new joiners or
		 * disconnected players). Re-derive from live session so the
		 * networking layer stays consistent. */
		{
			int live_n = 0;
			for (i = 0; i < MAX_PLAYERS; i++)
				if (Players[i].connected == CONNECT_PLAYING ||
				    Players[i].connected == CONNECT_WAITING)
					live_n++;
			if (live_n > Netgame.numplayers)
				Netgame.numplayers = live_n;
			if (live_n > Netgame.max_numplayers)
				Netgame.max_numplayers = live_n;
			Netgame.numconnected = live_n;
		}
#endif
		for (i = 0; i < MAX_PLAYERS; i++)
		{
			Netgame.killed[i] = Players[i].net_killed_total;
			Netgame.player_score[i] = Players[i].score;
			Netgame.player_flags[i] = Players[i].flags;
		}
#ifdef __ANDROID__
		/* Don't disconnect players that weren't in the save -- they
		 * are new joiners who should keep their current state */
		(void)0;
#else
		for (i = 0; i < MAX_PLAYERS; i++) // Disconnect connected players not available in this Savegame
			if (!coop_player_got[i] && Players[i].connected == CONNECT_PLAYING)
				multi_disconnect_player(i);
#endif
		Viewer = ConsoleObject = &Objects[Players[Player_num].objnum]; // make sure Viewer and ConsoleObject are set up (which we skipped by not using InitPlayerObject but we need since objects changed while loading)
		special_reset_objects(); // since we juggeled around with objects to remap coop players rebuild the index of free objects
#ifdef __ANDROID__
		/* android port: coop restore -- if matching failed the console
		 * object is still OBJ_GHOST from Phase 1.  multi_reset_player_object
		 * also clears PF_TURNROLL|PF_LEVELLING|PF_WIGGLE and doesn't set
		 * CT_FLYING.  Fix all of it so controls work. */
		{
			extern int Player_is_dead;
			Player_is_dead = 0;
		}
		ConsoleObject->type = OBJ_PLAYER;
		fly_init(ConsoleObject);
		ConsoleObject->mtype.phys_info.flags |= PF_TURNROLL | PF_LEVELLING | PF_WIGGLE | PF_USES_THRUST;
		HUD_init_message_literal(HM_DEFAULT, "Coop restore: controls reinit");
		COOPLOG("fly_init applied: type=%d ct=%d mt=%d pf=0x%x obj_flags=0x%x dead=%d",
			ConsoleObject->type, ConsoleObject->control_type, ConsoleObject->movement_type,
			ConsoleObject->mtype.phys_info.flags, ConsoleObject->flags, Player_is_dead);
		coop_indicator_diag_trigger();
#endif
	}

#ifdef __ANDROID__
	state_android_restore_player_flight_state();
#endif

	if (version >= STATE_RUNTIME_VERSION)
		state_read_runtime_state(fp, swap, secret_restore, version);
	state_log_checkpoint_ai_restore_state();

#ifdef __ANDROID__
	debug_log(DLOG_GAME,
		"restore save complete: game=d2 file='%s' current_level=%d callsign='%s' highest_object=%d game_mode=%d secret_restore=%d",
		filename, Current_level_num, Players[Player_num].callsign,
		Highest_object_index, Game_mode, secret_restore);
	{
		PHYSFS_sint64 pos_after_base = PHYSFS_tell(fp);
		PHYSFS_sint64 file_len = PHYSFS_fileLength(fp);
		android_save_meta_disk android_meta;
		int have_android_meta = state_read_android_save_meta(fp, file_len, &android_meta);
		coop_save_metadata coop_meta;
		if (coop_read_save_metadata(fp, pos_after_base, &coop_meta)) {
			con_printf(CON_DEBUG, "coop_save: restored metadata (%d active, %d absent)",
				coop_meta.num_active_players, coop_meta.num_absent_players);
			/* Repopulate the absent player list so returning players get inventory back */
			if (Game_mode & GM_MULTI_COOP)
				coop_load_absent_from_metadata(&coop_meta);
			/* Restore guidebot ownership state (v2+) */
			if (coop_meta.version >= 2 && (Game_mode & GM_MULTI_COOP)) {
				Buddy_allowed_to_talk = coop_meta.buddy_allowed_to_talk;
				Escort_owner_player = coop_meta.escort_owner_player;
				if (Escort_owner_player >= 0 && Escort_owner_player < MAX_PLAYERS) {
					if (Escort_owner_player == Player_num)
						HUD_init_message_literal(HM_DEFAULT, "Guide-Bot: you have control");
					else
						HUD_init_message(HM_DEFAULT, "Guide-Bot: %s has control", Players[Escort_owner_player].callsign);
				}
			}
			} else if (!(have_android_meta &&
				file_len - pos_after_base == (PHYSFS_sint64)sizeof(android_meta)) &&
				pos_after_base != file_len)
			con_printf(CON_URGENT, "savegame not completely read, might be corrupt! (cur %d, size %d)",
				(int)pos_after_base, (int)file_len);
	}
#else
	if (PHYSFS_tell(fp) != PHYSFS_fileLength(fp))
		con_printf(CON_URGENT, "savegame not completely read, might be corrupt! (cur %d, size %d)",
			(int)PHYSFS_tell(fp), (int)PHYSFS_fileLength(fp));
#endif

	PHYSFS_close(fp);
	if (input_demo_replay_has_checkpoint()) {
		int64_t collision_delay_last_play_time = 0;

		if (input_demo_replay_get_checkpoint_collision_delay_last_play_time(&collision_delay_last_play_time))
			collide_set_collision_delay_last_play_time((fix64) collision_delay_last_play_time);
		else
			collide_set_collision_delay_last_play_time(0);
	}
	escort_rebuild_runtime_state_after_restore();

	if (Game_wind)
		if (!window_is_visible(Game_wind))
			window_set_visible(Game_wind, 1);
	reset_time();

	return 1;
}

int state_get_game_id(char *filename)
{
	int version;
	PHYSFS_file *fp;
	int swap = 0;	// if file is not endian native, have to swap all shorts and ints
	char id[5], saved_callsign[CALLSIGN_LEN+1];

	#ifndef NDEBUG
	if (GameArg.SysUsePlayersDir && strncmp(filename, "Players/", 8))
		Int3();
	#endif

	if (!(Game_mode & GM_MULTI_COOP))
		return 0;

	fp = PHYSFSX_openReadBuffered(filename);
	if ( !fp ) return 0;

//Read id
	PHYSFS_read(fp, id, sizeof(char) * 4, 1);
	if ( memcmp( id, dgss_id, 4 )) {
		PHYSFS_close(fp);
		return 0;
	}

//Read version
	//Check for swapped file here, as dgss_id is written as a string (i.e. endian independent)
	PHYSFS_read(fp, &version, sizeof(int), 1);
	if (version & 0xffff0000)
	{
		swap = 1;
		version = SWAPINT(version);
	}

	if (version < STATE_COMPATIBLE_VERSION)	{
		PHYSFS_close(fp);
		return 0;
	}

// Read Coop state_game_id to validate the savegame we are about to load matches the others
	state_game_id = PHYSFSX_readSXE32(fp, swap);
	PHYSFS_read(fp, &saved_callsign, sizeof(char)*CALLSIGN_LEN+1, 1);
#ifdef __ANDROID__
	/* Autosaves use COOP_AUTOSAVE_CALLSIGN -- accept both it and the
	 * current player's callsign so autosaves survive session changes */
	if (strcmp(saved_callsign, Players[Player_num].callsign) &&
	    strcmp(saved_callsign, COOP_AUTOSAVE_CALLSIGN))
		return 0;
#else
	if (strcmp(saved_callsign, Players[Player_num].callsign)) // check the callsign of the palyer who saved this state. It MUST match. If we transferred this savegame from pilot A to pilot B, others won't be able to restore us. So bail out here if this is the case.
		return 0;
#endif

	return state_game_id;
}

#ifdef __ANDROID__
int state_get_save_file_callsign(char *filename, char *callsign, int callsign_size)
{
	PHYSFS_file *fp;
	char id[5];
	char mission[128];
	int version;
	int swap = 0;
	player_rw pl_rw;

	if (!filename || !callsign || callsign_size <= 0)
		return 0;
	callsign[0] = '\0';
	fp = PHYSFSX_openReadBuffered(filename);
	if (!fp)
		return 0;

	PHYSFS_read(fp, id, sizeof(char) * 4, 1);
	if (memcmp(id, dgss_id, 4)) {
		PHYSFS_close(fp);
		return 0;
	}
	PHYSFS_read(fp, &version, sizeof(int), 1);
	if (version & 0xffff0000) {
		swap = 1;
		version = SWAPINT(version);
	}
	if (version < STATE_COMPATIBLE_VERSION) {
		PHYSFS_close(fp);
		return 0;
	}

	PHYSFS_seek(fp, PHYSFS_tell(fp) + DESC_LENGTH);
	state_skip_thumbnail(fp, version);
	PHYSFSX_readSXE32(fp, swap);
	PHYSFS_read(fp, mission, sizeof(char) * 9, 1);
	if (mission[8] == 1)
		PHYSFS_read(fp, mission, 128, 1);
	PHYSFSX_readSXE32(fp, swap);
	PHYSFSX_readSXE32(fp, swap);
	PHYSFSX_readSXE32(fp, swap);
	memset(&pl_rw, 0, sizeof(pl_rw));
	if (PHYSFS_read(fp, &pl_rw, sizeof(pl_rw), 1) != 1) {
		PHYSFS_close(fp);
		return 0;
	}
	PHYSFS_close(fp);
	strncpy(callsign, pl_rw.callsign, callsign_size - 1);
	callsign[callsign_size - 1] = '\0';
	return callsign[0] != '\0';
}
#endif
