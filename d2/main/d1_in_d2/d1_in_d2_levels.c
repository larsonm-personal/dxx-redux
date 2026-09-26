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

/* New file, largely derived from the original Parallax/Interplay Descent code */

/* D1 level decoding, trigger execution and campaign progression
 * Shared world-loading, wall, score-display and window services stay in the engine */

#include "d1_in_d2_levels.h"
#include "d1_in_d2.h"
#include "d1_in_d2_assets.h"
#include "gamemine.h"
#include "piggy.h"
#include "vclip.h"
#include "textures.h"
#include "console.h"
#include "dxxerror.h"
#include "cntrlcen.h"
#include <string.h>
#include "fuelcen.h"
#include "gameseg.h"
#include "gameseq.h"
#include "game.h"
#include "endlevel.h"
#include "newdemo.h"
#include "multi.h"
#include "key.h"
#include "gauges.h"
#include "newmenu.h"
#include "scores.h"
#include "titles.h"
#include "text.h"
#include "mission.h"
#include "menu.h"
#include "d1_in_d2_presentation.h"
#include "byteswap.h"
#ifdef __ANDROID__
#include "coop/coop_endgame.h"
#include "coop/coop_briefing.h"
#include "coop/coop_travel.h"
#endif
#include "rewind_file_compat.h"

/* The serialized D2 trigger has an unused byte and unused flag bits. Keep the
 * native action mask in that byte and source ON separately from TF_DISABLED:
 * native D1 clears ON for one-shots but does not gate execution on it. Keeping
 * these flags in the core record preserves their existing save/rewind layout
 * Original type/link bytes are runtime storage with a versioned save extension */
enum { D1_TRIGGER_RECORD = 128, D1_TRIGGER_ON = 64 };

int d1_in_d2_initialize_level_ambience(void)
{
	int i;
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	for (i = 0; i <= Highest_segment_index; ++i)
		Segment2s[i].s2_flags &= ~(S2F_AMBIENT_LAVA | S2F_AMBIENT_WATER);
	return 1;
}

int d1_in_d2_trigger_source_flags(const trigger *source, short *flags)
{
	if (!(source->flags & D1_TRIGGER_RECORD))
		return 0;
	const unsigned actions = (ubyte)source->pad;
	*flags = (actions & 15) | ((actions & 240) << 2) |
	         ((source->flags & D1_TRIGGER_ON) ? TRIGGER_ON : 0) |
	         ((source->flags & TF_ONE_SHOT) ? TRIGGER_ONE_SHOT : 0);
	return 1;
}

int d1_in_d2_trigger_exit_flags(const trigger *source)
{
	short flags;
	if (!d1_in_d2_trigger_source_flags(source, &flags))
		return -1;
	return flags & (TRIGGER_EXIT | TRIGGER_SECRET_EXIT);
}

int d1_in_d2_trigger_source_link(const trigger *source)
{
	return (source->flags & D1_TRIGGER_RECORD) ? source->d1_saved.link_num : -1;
}

void d1_in_d2_write_trigger_storage(rewind_file *fp)
{
	const int count = d1_in_d2_use_d1_gameplay() ? Num_triggers : 0;
	PHYSFS_write(fp, &count, sizeof(count), 1);
	for (int i = 0; i < count; ++i) {
		PHYSFS_write(fp, &Triggers[i].d1_saved.type, 1, 1);
		PHYSFS_write(fp, &Triggers[i].d1_saved.link_num, 1, 1);
	}
}

int d1_in_d2_read_trigger_storage(rewind_file *fp, int swap, int apply)
{
	d1_trigger_storage storage[MAX_TRIGGERS];
	int count;
	if (PHYSFS_read(fp, &count, sizeof(count), 1) != 1)
		return 0;
	if (swap) count = SWAPINT(count);
	if (count < 0 || count > MAX_TRIGGERS || count != (d1_in_d2_use_d1_gameplay() ? Num_triggers : 0))
		return 0;
	for (int i = 0; i < count; ++i) {
		if (!(Triggers[i].flags & D1_TRIGGER_RECORD) ||
		    PHYSFS_read(fp, &storage[i].type, 1, 1) != 1 ||
		    PHYSFS_read(fp, &storage[i].link_num, 1, 1) != 1)
			return 0;
	}
	if (apply)
		for (int i = 0; i < count; ++i)
			Triggers[i].d1_saved = storage[i];
	return 1;
}

int d1_in_d2_decode_trigger(trigger *out, const v29_trigger *source, int native_d1)
{
	if (source->num_links < 0 || source->num_links > MAX_WALLS_PER_LINK ||
	    (native_d1 && ((unsigned short)source->flags & ~1023u)))
		return 0;
	for (int i = 0; i < source->num_links; ++i)
		if (source->seg[i] < 0 || source->side[i] < 0 || source->side[i] >= 6)
			return 0;
	trigger result = {0};
	result.value = source->value;
	result.time = source->time;
	result.num_links = source->num_links;
	memcpy(result.seg, source->seg, sizeof(result.seg));
	memcpy(result.side, source->side, sizeof(result.side));
	const unsigned flags = (unsigned short)source->flags;
	if (flags & TRIGGER_ONE_SHOT)
		result.flags = TF_ONE_SHOT;
	if (native_d1) {
		result.d1_saved.type = source->type;
		result.d1_saved.link_num = source->link_num;
		result.flags |= D1_TRIGGER_RECORD | ((flags & TRIGGER_ON) ? D1_TRIGGER_ON : 0);
		result.pad = (sbyte)((flags & 15) | ((flags >> 2) & 240));
		/* Keep a representative type for existing format/metadata consumers
		 * Native execution and runtime exit queries use every source action */
		if (flags & TRIGGER_SECRET_EXIT) result.type = TT_SECRET_EXIT;
		else if (flags & TRIGGER_EXIT) result.type = TT_EXIT;
		else if (flags & TRIGGER_CONTROL_DOORS) result.type = TT_OPEN_DOOR;
		else if (flags & TRIGGER_MATCEN) result.type = TT_MATCEN;
		else if (flags & TRIGGER_ILLUSION_OFF) result.type = TT_ILLUSION_OFF;
		else if (flags & TRIGGER_ILLUSION_ON) result.type = TT_ILLUSION_ON;
	} else {
		/* Historical D2 v29/v30 conversion, including its single-action order */
		if (flags & TRIGGER_CONTROL_DOORS) result.type = TT_OPEN_DOOR;
		else if (flags & (TRIGGER_SHIELD_DAMAGE | TRIGGER_ENERGY_DRAIN)) return 0;
		else if (flags & TRIGGER_EXIT) result.type = TT_EXIT;
		else if (flags & TRIGGER_MATCEN) result.type = TT_MATCEN;
		else if (flags & TRIGGER_ILLUSION_OFF) result.type = TT_ILLUSION_OFF;
		else if (flags & TRIGGER_SECRET_EXIT) result.type = TT_SECRET_EXIT;
		else if (flags & TRIGGER_ILLUSION_ON) result.type = TT_ILLUSION_ON;
		else if (flags & TRIGGER_UNLOCK_DOORS) result.type = TT_UNLOCK_DOOR;
		else if (flags & TRIGGER_OPEN_WALL) result.type = TT_OPEN_WALL;
		else if (flags & TRIGGER_CLOSE_WALL) result.type = TT_CLOSE_WALL;
		else if (flags & TRIGGER_ILLUSORY_WALL) result.type = TT_ILLUSORY_WALL;
		else return 0;
	}
	*out = result;
	return 1;
}

int d1_in_d2_decode_level_trigger(trigger *out, const v29_trigger *source)
{
	v29_trigger repaired = *source;
	/* D1 ignores flags outside its native action mask */
	repaired.flags &= 1023;
	const int linked_actions = TRIGGER_CONTROL_DOORS | TRIGGER_MATCEN | TRIGGER_ILLUSION_ON | TRIGGER_ILLUSION_OFF;
	if (repaired.num_links < 0 || repaired.num_links > MAX_WALLS_PER_LINK) {
		if (!(repaired.flags & linked_actions))
			repaired.num_links = 0;
		else {
			/* Some editors leave high-byte garbage in the count. The old D2
			 * level adapter narrowed it to a byte; retain only a bounded count */
			const int count = (ubyte)repaired.num_links;
			if (repaired.num_links < 0 || count == 0 || count > MAX_WALLS_PER_LINK)
				return 0;
			repaired.num_links = count;
		}
	}
	int count = 0;
	for (int i = 0; i < repaired.num_links; ++i) {
		if (repaired.seg[i] < 0 || repaired.seg[i] > Highest_segment_index ||
		    repaired.side[i] < 0 || repaired.side[i] >= 6)
			continue;
		repaired.seg[count] = repaired.seg[i];
		repaired.side[count++] = repaired.side[i];
	}
	repaired.num_links = count;
	if (repaired.flags != source->flags || repaired.num_links != source->num_links)
		Warning("Repaired D1 level trigger flags=%d->%d links=%d->%d", source->flags, repaired.flags, source->num_links, repaired.num_links);
	return d1_in_d2_decode_trigger(out, &repaired, 1);
}

int d1_in_d2_bind_trigger_links(int trigger_num)
{
	const trigger *trig = &Triggers[trigger_num];
	short flags;
	if (!d1_in_d2_trigger_source_flags(trig, &flags))
		return -1;
	if (trig->num_links < 0 || trig->num_links > MAX_WALLS_PER_LINK)
		return 0;
	for (int i = 0; i < trig->num_links; ++i) {
		if (trig->seg[i] < 0 || trig->seg[i] > Highest_segment_index ||
		    trig->side[i] < 0 || trig->side[i] >= 6)
			return 0;
		const int wall = Segments[trig->seg[i]].sides[trig->side[i]].wall_num;
		if (flags & (TRIGGER_CONTROL_DOORS | TRIGGER_ILLUSION_ON | TRIGGER_ILLUSION_OFF)) {
			if (wall >= Num_walls)
				return 0;
			if (wall >= 0)
				Walls[wall].controlling_trigger = trigger_num;
		}
	}
	return 1;
}

/* Native d1/main/switch.c action order, using shared world primitives */
int d1_in_d2_activate_trigger(int trigger_num, int player_num)
{
	short flags;
	trigger *trig = &Triggers[trigger_num];
	if (!d1_in_d2_trigger_source_flags(trig, &flags))
		return -1;
	if (trig->num_links < 0 || trig->num_links > MAX_WALLS_PER_LINK)
		return 1;
	for (int i = 0; i < trig->num_links; ++i)
		if (trig->seg[i] < 0 || trig->seg[i] > Highest_segment_index ||
		    trig->side[i] < 0 || trig->side[i] >= 6)
			return 1;
	if (flags & (TRIGGER_ILLUSION_ON | TRIGGER_ILLUSION_OFF)) {
		/* Wall primitives expect a connected pair. Reject a malformed source
		 * before applying any damage or partially changing linked walls */
		for (int i = 0; i < trig->num_links; ++i) {
			segment *seg = &Segments[trig->seg[i]];
			const int child = seg->children[trig->side[i]];
			if (child < 0 || child > Highest_segment_index)
				return 1;
			const int side = find_connect_side(seg, &Segments[child]);
			const int wall = seg->sides[trig->side[i]].wall_num;
			if (side < 0 || side >= 6 || wall < 0 || wall >= Num_walls)
				return 1;
			const int paired = Segments[child].sides[side].wall_num;
			if (paired < 0 || paired >= Num_walls)
				return 1;
		}
	}
	if (player_num == Player_num) {
		if (flags & TRIGGER_SHIELD_DAMAGE)
			Players[Player_num].shields -= trig->value;
		if (flags & TRIGGER_EXIT)
			start_endlevel_sequence();
		if (flags & TRIGGER_SECRET_EXIT) {
			if (Newdemo_state == ND_STATE_RECORDING && !newdemo_stop_quick_recording())
				Newdemo_state = ND_STATE_PAUSED;
#ifdef NETWORK
			if (Game_mode & GM_MULTI) multi_send_endlevel_start(1);
			if (Game_mode & GM_NETWORK) multi_do_protocol_frame(1, 1);
#endif
			d1_in_d2_finish_level(1);
			Control_center_destroyed = 0;
			return 1;
		}
		if (flags & TRIGGER_ENERGY_DRAIN) {
			Players[Player_num].energy -= trig->value;
			if (Game_mode & GM_MULTI) multi_send_ship_status();
		}
	}
	if (flags & TRIGGER_CONTROL_DOORS)
		for (int i = 0; i < trig->num_links; ++i)
			wall_toggle(trig->seg[i], trig->side[i]);
	if ((flags & TRIGGER_MATCEN) && (!(Game_mode & GM_MULTI) || (Game_mode & GM_MULTI_ROBOTS)))
		for (int i = 0; i < trig->num_links; ++i)
			trigger_matcen(trig->seg[i]);
	if (flags & TRIGGER_ILLUSION_ON)
		for (int i = 0; i < trig->num_links; ++i)
			wall_illusion_on(&Segments[trig->seg[i]], trig->side[i]);
	if (flags & TRIGGER_ILLUSION_OFF)
		for (int i = 0; i < trig->num_links; ++i)
			wall_illusion_off(&Segments[trig->seg[i]], trig->side[i]);
	return 0;
}

int d1_in_d2_cross_trigger(int trigger_num, segment *seg, int side, int objnum, int shot)
{
	short flags;
	if (!d1_in_d2_trigger_source_flags(&Triggers[trigger_num], &flags))
		return -1;
	if (objnum != Players[Player_num].objnum || Newdemo_state == ND_STATE_PLAYBACK)
		return 1;
	if (check_trigger_sub(trigger_num, Player_num, shot))
		return 1;
	if (flags & TRIGGER_ONE_SHOT) {
		Triggers[trigger_num].flags &= ~D1_TRIGGER_ON;
		const int child = seg->children[side];
		if (child < 0 || child > Highest_segment_index)
			return 0;
		segment *other = &Segments[child];
		const int other_side = find_connect_side(seg, other);
		if (other_side < 0 || other_side >= 6)
			return 0;
		const int wall = other->sides[other_side].wall_num;
		if (wall < 0 || wall >= Num_walls)
			return 0;
		const int paired = Walls[wall].trigger;
		if (paired >= 0 && paired < Num_triggers && (Triggers[paired].flags & D1_TRIGGER_RECORD))
			Triggers[paired].flags &= ~D1_TRIGGER_ON;
	}
	return 0;
}

/* Native d1/main/gameseq.c progression. The engine still owns level loading,
 * shared death teardown and resource publication; no D2 secret world is saved */
static int advance_native_level(int secret)
{
	Control_center_destroyed = 0;
#ifdef EDITOR
	if (Current_level_num == 0)
		return 1;
#endif
	key_flush();
#ifdef NETWORK
	if ((Game_mode & GM_MULTI) && multi_endlevel(&secret))
		return Current_level_num == Last_level;
#endif
	key_flush();
	if (Current_level_num == Last_level) {
		if (Newdemo_state == ND_STATE_RECORDING || Newdemo_state == ND_STATE_PAUSED)
			newdemo_stop_recording(0);
#ifdef __ANDROID__
		coop_endgame_begin();
#endif
		do_end_briefing_screens(Ending_text_filename);
		return 1;
	}
	int destination = Current_level_num + 1;
	if (Current_level_num < 0) {
		if (secret || Current_level_num < Last_secret_level) {
			con_printf(CON_URGENT, "Invalid D1 secret return from level %d\n", Current_level_num);
			return 0;
		}
		destination = Secret_level_table[-Current_level_num - 1] + 1;
	} else if (secret) {
		int i;
		for (i = 0; i < -Last_secret_level; ++i)
			if (Secret_level_table[i] == Current_level_num)
				break;
		if (i == -Last_secret_level) {
			con_printf(CON_URGENT, "No D1 secret destination for level %d\n", Current_level_num);
			return 0;
		}
		destination = -i - 1;
	}
	Next_level_num = destination;
	StartNewLevel(destination);
	key_flush();
	return 0;
}

static int score_and_advance(int secret)
{
	/* Native D1 shows the campaign ending before final single-player/co-op
	 * bonuses, but scores ordinary levels before starting the destination */
	if (Current_level_num == Last_level &&
	    !((Game_mode & GM_MULTI) && !(Game_mode & GM_MULTI_COOP))) {
		const int finished = advance_native_level(secret);
		DoEndLevelScoreGlitz(0);
		return finished;
	}
#ifdef NETWORK
	if (Game_mode & GM_MULTI)
		multi_endlevel_score();
	else
#endif
		DoEndLevelScoreGlitz(0);
	return advance_native_level(secret);
}

int d1_in_d2_finish_level(int secret)
{
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	if (Game_wind)
		window_set_visible(Game_wind, 0);
#ifdef __ANDROID__
	coop_flyout_run(NULL);
#endif
	Players[Player_num].hostages_rescued_total += Players[Player_num].hostages_on_board;
	if (!(Game_mode & GM_MULTI) && secret) {
		newmenu_item item = {0};
		item.type = NM_TYPE_TEXT;
		item.text = " ";
		newmenu_do2(NULL, TXT_SECRET_EXIT, 1, &item, NULL, NULL, 0,
		            d1_in_d2_menu_resource(D1_MENU_MAIN, NULL));
	}
#ifdef NETWORK
	if (Game_mode & GM_NETWORK)
		Players[Player_num].connected = secret ? CONNECT_FOUND_SECRET : CONNECT_WAITING;
#endif
	last_drawn_cockpit = -1;
	const int was_deathmatch_end = Current_level_num == Last_level &&
	                             (Game_mode & GM_MULTI) && !(Game_mode & GM_MULTI_COOP);
	if (score_and_advance(secret)) {
		if (!was_deathmatch_end && PLAYING_BUILTIN_MISSION)
			scores_maybe_add_player(0);
		if (Game_wind)
			window_close(Game_wind);
	}
	if (Game_wind)
		window_set_visible(Game_wind, 1);
	reset_time();
	return 1;
}

int d1_in_d2_finish_death(void)
{
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	if (Control_center_destroyed) {
		Players[Player_num].hostages_on_board = 0;
		Players[Player_num].energy = 0;
		Players[Player_num].shields = 0;
		Players[Player_num].connected = CONNECT_DIED_IN_MINE;
#ifdef __ANDROID__
		coop_flyout_run(NULL);
#endif
		do_screen_message(TXT_DIED_IN_MINE);
		const int finished = score_and_advance(0);
		init_player_stats_new_ship(Player_num);
		last_drawn_cockpit = -1;
		if (finished) {
			if (PLAYING_BUILTIN_MISSION)
				scores_maybe_add_player(0);
			if (Game_wind)
				window_close(Game_wind);
		}
	} else {
		init_player_stats_new_ship(Player_num);
		StartLevel(1);
	}
	return 1;
}

int d1_in_d2_level_bonuses(int level_points, int *skill, int *shields, int *energy)
{
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	*skill = Difficulty_level > 1 ? level_points * (Difficulty_level - 1) / 2 : 0;
	*skill -= *skill % 100;
	*shields = f2i(Players[Player_num].shields) * 10 * (Difficulty_level + 1);
	*energy = f2i(Players[Player_num].energy) * 5 * (Difficulty_level + 1);
	return 1;
}

int d1_in_d2_decode_level_textures(short *primary, short *overlay, int new_file_format)
{
	short base = *primary, decal = *overlay;
	const int native_textures = d1_in_d2_native_texture_count();
	if (native_textures > 0 && base >= 0) {
		/* Match native D1 gamesave.c:convert_tmap for old authored levels
		 * with excess texture indices. Optional D2 assets are not D1 slots */
		base %= native_textures;
		decal = ((decal & TMAP_NUM_MASK) % native_textures) | (decal & ~TMAP_NUM_MASK);
	} else if (!d1_in_d2_has_native_assets()) {
		const int have_pig = PHYSFSX_exists(D1_PIGFILE, 1);
		base = d1_in_d2_legacy_texture(base, have_pig, new_file_format);
		if (decal)
			decal = d1_in_d2_legacy_texture(decal, have_pig, new_file_format);
	}
	if (base < 0 || base >= NumTextures || (decal & TMAP_NUM_MASK) >= NumTextures) {
		con_printf(CON_URGENT, "Invalid D1 texture references: primary=%d overlay=%d textures=%d\n",
		           *primary, *overlay & TMAP_NUM_MASK, NumTextures);
		return 0;
	}
	*primary = base;
	*overlay = decal;
	return 1;
}

void d1_in_d2_fixup_level_object(object *obj, int level_version)
{
	if (level_version <= 1 && (obj->render_type == RT_WEAPON_VCLIP || obj->render_type == RT_HOSTAGE ||
	                           obj->render_type == RT_POWERUP || obj->render_type == RT_FIREBALL)) {
		const int clip = obj->rtype.vclip_info.vclip_num;
		/* Native D1 also replaces unavailable level vclips with clip zero */
		if (clip < 0 || clip >= Num_vclips || clip >= VCLIP_MAXNUM ||
		    Vclip[clip].num_frames <= 0 || Vclip[clip].frame_time <= 0) {
			Warning("Invalid D1 level object vclip %d, using clip 0", clip);
			obj->rtype.vclip_info.vclip_num = 0;
		}
	}
	if (level_version <= 1 && obj->type == OBJ_CNTRLCEN) {
		/* D1 has one reactor definition, including its original model */
		obj->id = 0;
		obj->rtype.pobj_info.model_num = Reactors[0].model_num;
	}
}

/* Converts descent 1 texture numbers to descent 2 texture numbers.
 * Textures from d1 which are unique to d1 have extra spaces around "return".
 * If we can load the original d1 pig, we make sure this function is bijective.
 * This function was updated using the file config/convtabl.ini from devil 2.2.
 */
short d1_in_d2_legacy_texture(short d1_tmap_num, int d1_pig_present, int new_file_format) {
	switch (d1_tmap_num) {
	case 0: case 2: case 4: case 5:
		// all refer to grey rock001 (exception to bijectivity rule)
		return  d1_pig_present ? 137 : 43; // (devil:95)
	case   1: return 0;
	case   3: return 1; // rock021
	case   6:  return  270; // blue rock002
	case   7:  return  271; // yellow rock265
	case   8: return 2; // rock004
	case   9:  return  d1_pig_present ? 138 : 62; // purple (devil:179)
	case  10:  return  272; // red rock006
	case  11:  return  d1_pig_present ? 139 : 117;
	case  12:  return  d1_pig_present ? 140 : 12; //devil:43
	case  13: return 3; // rock014
	case  14: return 4; // rock019
	case  15: return 5; // rock020
	case  16: return 6;
	case  17:  return  d1_pig_present ? 141 : 52;
	case  18:  return  129;
	case  19: return 7;
	case  20:  return  d1_pig_present ? 142 : 22;
	case  21:  return  d1_pig_present ? 143 : 9;
	case  22: return 8;
	case  23: return 9;
	case  24: return 10;
	case  25:  return  d1_pig_present ? 144 : 12; //devil:35
	case  26: return 11;
	case  27: return 12;
	case  28:  return  d1_pig_present ? 145 : 11; //devil:43
	//range handled by default case, returns 13..21 (- 16)
	case  38:  return  163; //devil:27
	case  39:  return  147; //31
	case  40: return 22;
	case  41:  return  266;
	case  42: return 23;
	case  43: return 24;
	case  44:  return  136; //devil:135
	case  45: return 25;
	case  46: return 26;
	case  47: return 27;
	case  48: return 28;
	case  49:  return  d1_pig_present ? 146 : 43; //devil:60
	case  50:  return  131; //devil:138
	case  51: return 29;
	case  52: return 30;
	case  53: return 31;
	case  54: return 32;
	case  55:  return  165; //devil:193
	case  56: return 33;
	case  57:  return  132; //devil:119
	// range handled by default case, returns 34..63 (- 24)
	case  88:  return  197; //devil:15
	// range handled by default case, returns 64..106 (- 25)
	case 132:  return  167;
        // range handled by default case, returns 107..114 (- 26)
	case 141:  return  d1_pig_present ? 148 : 110; //devil:106
	case 142: return 115;
	case 143: return 116;
	case 144: return 117;
	case 145: return 118;
	case 146: return 119;
	case 147:  return  d1_pig_present ? 149 : 93;
	case 148: return 120;
	case 149: return 121;
	case 150: return 122;
	case 151: return 123;
	case 152: return 124;
	case 153: return 125; // rock263
	case 154:  return  d1_pig_present ? 150 : 27;
	case 155:  return  126; // rock269
	case 156: return 200; // metl002
	case 157: return 201; // metl003
	case 158:  return  186; //devil:227
	case 159:  return  190; //devil:246
	case 160:  return  d1_pig_present ? 151 : 206;
	case 161:  return  d1_pig_present ? 152 : 114; //devil:206
	case 162: return 202;
	case 163: return 203;
	case 164: return 204;
	case 165: return 205;
	case 166: return 206;
	case 167:  return  d1_pig_present ? 153 : 206;
	case 168:  return  d1_pig_present ? 154 : 206;
	case 169:  return  d1_pig_present ? 155 : 206;
	case 170:  return  d1_pig_present ? 156 : 227;//206;
	case 171:  return  d1_pig_present ? 157 : 206;//227;
	case 172: return 207;
	case 173: return 208;
	case 174:  return  d1_pig_present ? 158 : 202;
	case 175:  return  d1_pig_present ? 159 : 206;
	// range handled by default case, returns 209..217 (+ 33)
	case 185:  return  d1_pig_present ? 160 : 217;
	// range handled by default case, returns 218..224 (+ 32)
	case 193:  return  d1_pig_present ? 161 : 206;
	case 194:  return  d1_pig_present ? 162 : 203;//206;
	case 195:  return  d1_pig_present ? 166 : 234;
	case 196: return 225;
	case 197: return 226;
	case 198:  return  d1_pig_present ? 193 : 225;
	case 199:  return  d1_pig_present ? 168 : 206; //devil:204
	case 200:  return  d1_pig_present ? 169 : 206; //devil:204
	case 201: return 227;
	case 202:  return  d1_pig_present ? 170 : 206; //devil:227
	// range handled by default case, returns 228..234 (+ 25)
	case 210:  return  d1_pig_present ? 171 : 234; //devil:242
	case 211:  return  d1_pig_present ? 172 : 206; //devil:240
	// range handled by default case, returns 235..242 (+ 23)
	case 220:  return  d1_pig_present ? 173 : 242; //devil:240
	case 221: return 243;
	case 222: return 244;
	case 223:  return  d1_pig_present ? 174 : 313;
	case 224: return 245;
	case 225: return 246;
	case 226:  return  164;//247; matching names but not matching textures
	case 227:  return  179; //devil:181
	case 228:  return  196;//248; matching names but not matching textures
	case 229:  return  d1_pig_present ? 175 : 15; //devil:66
	case 230:  return  d1_pig_present ? 176 : 15; //devil:66
	// range handled by default case, returns 249..257 (+ 18)
	case 240:  return  d1_pig_present ? 177 : 6; //devil:132
	case 241:  return  130; //devil:131
	case 242:  return  d1_pig_present ? 178 : 78; //devil:15
	case 243:  return  d1_pig_present ? 180 : 33; //devil:38
	case 244: return 258;
	case 245: return 259;
	case 246:  return  d1_pig_present ? 181 : 321; // grate metl127
	case 247: return 260;
	case 248: return 261;
	case 249: return 262;
	case 250:  return  340; //  white doorframe metl126
	case 251:  return  412; //    red doorframe metl133
	case 252:  return  410; //   blue doorframe metl134
	case 253:  return  411; // yellow doorframe metl135
	case 254: return 263; // metl136
	case 255: return 264; // metl139
	case 256: return 265; // metl140
	case 257:  return  d1_pig_present ? 182 : 249;//246; brig001
	case 258:  return  d1_pig_present ? 183 : 251;//246; brig002
	case 259:  return  d1_pig_present ? 184 : 252;//246; brig003
	case 260:  return  d1_pig_present ? 185 : 256;//246; brig004
	case 261: return 273; // exit01
	case 262: return 274; // exit02
	case 263:  return  d1_pig_present ? 187 : 281; // ceil001
	case 264: return 275; // ceil002
	case 265: return 276; // ceil003
	case 266:  return  d1_pig_present ? 188 : 279; //devil:291
	// range handled by default case, returns 277..291 (+ 10)
	case 282: return 293;
	case 283:  return  d1_pig_present ? 189 : 295;
	case 284: return 295;
	case 285: return 296;
	case 286: return 298;
	// range handled by default case, returns 300..310 (+ 13)
	case 298:  return  d1_pig_present ? 191 : 364; // devil:374 misc010
	// range handled by default case, returns 311..326 (+ 12)
	case 315:  return  d1_pig_present ? 192 : 361; // bad producer misc044
	// range handled by default case,  returns  327..337 (+ 11)
	case 327: return 352; // arw01
	case 328: return 353; // misc17
	case 329: return 354; // fan01
	case 330:  return  380; // mntr04
	case 331:  return  379;//373; matching names but not matching textures
	case 332:  return  355;//344; matching names but not matching textures
 	case 333:  return  409; // lava misc11 //devil:404
	case 334: return 356; // ctrl04
	case 335: return 357; // ctrl01
	case 336: return 358; // ctrl02
	case 337: return 359; // ctrl03
	case 338: return 360; // misc14
	case 339: return 361; // producer misc16
	case 340: return 362; // misc049
	case 341: return 364; // misc060
	case 342: return 363; // blown01
	case 343: return 366; // misc061
	case 344: return 365;
	case 345: return 368;
	case 346: return 376;
	case 347: return 370;
	case 348: return 367;
	case 349:  return  372;
	case 350: return 369;
	case 351:  return  374;//429; matching names but not matching textures
	case 352:  return  375;//387; matching names but not matching textures
	case 353:  return  371;
	case 354:  return  377;//425; matching names but not matching textures
	case 355:  return  408;
	case 356: return 378; // lava02
	case 357:  return  383;//384; matching names but not matching textures
	case 358:  return  384;//385; matching names but not matching textures
	case 359:  return  385;//386; matching names but not matching textures
	case 360: return 386;
	case 361: return 387;
	case 362:  return  d1_pig_present ? 194 : 388; // mntr04b (devil: -1)
	case 363: return 388;
	case 364: return 391;
	case 365: return 392;
	case 366: return 393;
	case 367: return 394;
	case 368: return 395;
	case 369: return 396;
	case 370:  return  d1_pig_present ? 195 : 392; // mntr04d (devil: -1)
	// range 371..584 handled by default case (wall01 and door frames)
	default:
		// ranges:
		if (d1_tmap_num >= 29 && d1_tmap_num <= 37)
			return d1_tmap_num - 16;
		if (d1_tmap_num >= 58 && d1_tmap_num <= 87)
			return d1_tmap_num - 24;
		if (d1_tmap_num >= 89 && d1_tmap_num <= 131)
			return d1_tmap_num - 25;
		if (d1_tmap_num >= 133 && d1_tmap_num <= 140)
			return d1_tmap_num - 26;
		if (d1_tmap_num >= 176 && d1_tmap_num <= 184)
			return d1_tmap_num + 33;
		if (d1_tmap_num >= 186 && d1_tmap_num <= 192)
			return d1_tmap_num + 32;
		if (d1_tmap_num >= 203 && d1_tmap_num <= 209)
			return d1_tmap_num + 25;
		if (d1_tmap_num >= 212 && d1_tmap_num <= 219)
			return d1_tmap_num + 23;
		if (d1_tmap_num >= 231 && d1_tmap_num <= 239)
			return d1_tmap_num + 18;
		if (d1_tmap_num >= 267 && d1_tmap_num <= 281)
			return d1_tmap_num + 10;
		if (d1_tmap_num >= 287 && d1_tmap_num <= 297)
			return d1_tmap_num + 13;
		if (d1_tmap_num >= 299 && d1_tmap_num <= 314)
			return d1_tmap_num + 12;
		if (d1_tmap_num >= 316 && d1_tmap_num <= 326)
			 return  d1_tmap_num + 11; // matching names but not matching textures
		// wall01 and door frames:
		if (d1_tmap_num > 370 && d1_tmap_num < 584) {
			if (new_file_format) return d1_tmap_num + 64;
			// d1 shareware needs special treatment:
			if (d1_tmap_num < 410) return d1_tmap_num + 68;
			if (d1_tmap_num < 417) return d1_tmap_num + 73;
			if (d1_tmap_num < 446) return d1_tmap_num + 91;
			if (d1_tmap_num < 453) return d1_tmap_num + 104;
			if (d1_tmap_num < 462) return d1_tmap_num + 111;
			if (d1_tmap_num < 486) return d1_tmap_num + 117;
			if (d1_tmap_num < 494) return d1_tmap_num + 141;
			if (d1_tmap_num < 584) return d1_tmap_num + 147;
		}
		{ // handle rare case where orientation != 0
			short tmap_num = d1_tmap_num &  TMAP_NUM_MASK;
			short orient = d1_tmap_num & ~TMAP_NUM_MASK;
			if (orient != 0) {
				return orient | d1_in_d2_legacy_texture(tmap_num, d1_pig_present, new_file_format);
			} else {
				con_printf(CON_URGENT, "Can't convert unknown Descent 1 texture #%d\n", tmap_num);
				return d1_tmap_num;
			}
		}
	}
}
