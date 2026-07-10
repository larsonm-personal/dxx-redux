#include "secretarea.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bm.h"
#include "cntrlcen.h"
#include "effects.h"
#include "game.h"
#include "gameseg.h"
#include "gameseq.h"
#include "fvi.h"
#include "hudmsg.h"
#include "level_metadata_scan.h"
#include "object.h"
#include "player.h"
#include "powerup.h"
#include "robot.h"
#include "secret_area_item_names.h"
#include "segment.h"
#include "automap.h"
#include "switch.h"
#include "wall.h"
#ifdef DXX_BUILD_DESCENT_II
#include "escort.h"
#endif
#ifdef NETWORK
#include "multi.h"
#endif

static secret_area_state Secret_area_state;
static level_metadata_state Level_metadata_state;
static int Level_metadata_route_start_objnum = -1;
static int Level_metadata_route_start_seg = -1;
static int Secret_area_reveal_unfound;

typedef struct level_metadata_game_context {
	int start_objnum;
} level_metadata_game_context;

static void secret_area_trace(const char *stage)
{
	if (getenv("DXX_SECRET_AREA_DUMP_TRACE")) {
#ifdef DXX_BUILD_DESCENT_II
		fprintf(stderr, "SECRET-AREA-DUMP TRACE d2_secret_area_%s\n", stage);
#else
		fprintf(stderr, "SECRET-AREA-DUMP TRACE d1_secret_area_%s\n", stage);
#endif
		fflush(stderr);
	}
}

static int secret_area_segment_child(void *user, int seg, int side)
{
	(void) user;
	if (seg < 0 || seg >= Num_segments || side < 0 || side >= MAX_SIDES_PER_SEGMENT)
		return -1;
	return Segments[seg].children[side];
}

static int secret_area_segment_is_explored(void *user, int seg)
{
	(void) user;
	return seg >= 0 && seg < Num_segments && Automap_visited[seg] != 0;
}

static int secret_area_reverse_side(void *user, int seg, int child)
{
	(void) user;
	if (seg < 0 || seg >= Num_segments || child < 0 || child >= Num_segments)
		return -1;
	return find_connect_side(&Segments[seg], &Segments[child]);
}

static int secret_area_side_is_flyable(void *user, int seg, int side)
{
	(void) user;
	if (seg < 0 || seg >= Num_segments || side < 0 || side >= MAX_SIDES_PER_SEGMENT)
		return 0;
	return (WALL_IS_DOORWAY(&Segments[seg], side) & WID_FLY_FLAG) != 0;
}

static int secret_area_side_is_control_center_link(void *user, int seg, int side)
{
	int i;

	(void) user;
	for (i = 0; i < ControlCenterTriggers.num_links; ++i)
		if (ControlCenterTriggers.seg[i] == seg && ControlCenterTriggers.side[i] == side)
			return 1;
	return 0;
}

static int secret_area_wall_num(void *user, int seg, int side)
{
	(void) user;
	if (seg < 0 || seg >= Num_segments || side < 0 || side >= MAX_SIDES_PER_SEGMENT)
		return -1;
	return Segments[seg].sides[side].wall_num;
}

static int secret_area_wall_segment(void *user, int wall_num)
{
	(void) user;
	if (wall_num < 0 || wall_num >= Num_walls)
		return -1;
	return Walls[wall_num].segnum;
}

static int secret_area_wall_side(void *user, int wall_num)
{
	(void) user;
	if (wall_num < 0 || wall_num >= Num_walls)
		return -1;
	return Walls[wall_num].sidenum;
}

static int secret_area_wall_type(void *user, int wall_num)
{
	(void) user;
	if (wall_num < 0 || wall_num >= Num_walls)
		return WALL_NORMAL;
	return Walls[wall_num].type;
}

static int secret_area_wall_flags(void *user, int wall_num)
{
	(void) user;
	if (wall_num < 0 || wall_num >= Num_walls)
		return 0;
	return Walls[wall_num].flags;
}

static int secret_area_wall_keys(void *user, int wall_num)
{
	(void) user;
	if (wall_num < 0 || wall_num >= Num_walls)
		return KEY_NONE;
	return Walls[wall_num].keys;
}

static int secret_area_wall_trigger(void *user, int wall_num)
{
	(void) user;
	if (wall_num < 0 || wall_num >= Num_walls)
		return -1;
	return Walls[wall_num].trigger;
}

static int secret_area_wall_clip_flags(void *user, int wall_num)
{
	int clip_num;

	(void) user;
	if (wall_num < 0 || wall_num >= Num_walls)
		return 0;
	clip_num = Walls[wall_num].clip_num;
	if (clip_num < 0 || clip_num >= Num_wall_anims)
		return 0;
	return WallAnims[clip_num].flags;
}

static int secret_area_wall_is_shootable_trigger(void *user, int wall_num)
{
	int seg;
	int side;
	int tm;
	int ec;

	(void) user;
	if (wall_num < 0 || wall_num >= Num_walls)
		return 0;
	seg = Walls[wall_num].segnum;
	side = Walls[wall_num].sidenum;
	if (seg < 0 || seg >= Num_segments || side < 0 || side >= MAX_SIDES_PER_SEGMENT)
		return 0;
	tm = Segments[seg].sides[side].tmap_num2;
	if (tm == 0)
		return 0;
	tm &= 0x3fff;
	if (tm < 0 || tm >= MAX_TEXTURES)
		return 0;
	ec = TmapInfo[tm].eclip_num;
	if (ec >= 0 &&
	    ec < Num_effects &&
	    ec < MAX_EFFECTS &&
	    Effects[ec].dest_bm_num != -1 &&
	    (Effects[ec].flags & EF_ONE_SHOT) == 0)
		return 1;
#ifdef DXX_BUILD_DESCENT_II
	if (ec == -1 && TmapInfo[tm].destroyed != -1)
		return 1;
#endif
	return 0;
}

static int secret_area_segment_special(void *user, int seg)
{
	(void) user;
	if (seg < 0 || seg >= Num_segments)
		return SEGMENT_IS_NOTHING;
#ifdef DXX_BUILD_DESCENT_II
	return Segment2s[seg].special;
#else
	return Segments[seg].special;
#endif
}

static int secret_area_segment_center(void *user, int seg, int xyz[3])
{
	vms_vector center;

	(void) user;
	if (seg < 0 || seg >= Num_segments || !xyz)
		return 0;
	compute_segment_center(&center, &Segments[seg]);
	xyz[0] = center.x;
	xyz[1] = center.y;
	xyz[2] = center.z;
	return 1;
}

static int secret_area_segment_vertex(void *user, int seg, int index, int xyz[3])
{
	int vertex;

	(void) user;
	if (seg < 0 || seg >= Num_segments || index < 0 || index >= MAX_VERTICES_PER_SEGMENT || !xyz)
		return 0;
	vertex = Segments[seg].verts[index];
	if (vertex < 0 || vertex >= Num_vertices)
		return 0;
	xyz[0] = Vertices[vertex].x;
	xyz[1] = Vertices[vertex].y;
	xyz[2] = Vertices[vertex].z;
	return 1;
}

static int secret_area_object_start(int objnum, int *seg, int xyz[3])
{
	if (objnum < 0 || objnum >= num_objects || Objects[objnum].type == OBJ_NONE)
		return 0;
	if (seg)
		*seg = Objects[objnum].segnum;
	if (xyz) {
		xyz[0] = Objects[objnum].pos.x;
		xyz[1] = Objects[objnum].pos.y;
		xyz[2] = Objects[objnum].pos.z;
	}
	return 1;
}

static int secret_area_player_start(int *seg, int xyz[3])
{
	int objnum;
	int local_objnum = Players[Player_num].objnum;

	if (local_objnum >= 0 && local_objnum < num_objects &&
	    (Objects[local_objnum].type == OBJ_PLAYER || Objects[local_objnum].type == OBJ_GHOST))
		return secret_area_object_start(local_objnum, seg, xyz);

	for (objnum = 0; objnum < num_objects; ++objnum) {
		int type = Objects[objnum].type;
		if (type != OBJ_PLAYER && type != OBJ_GHOST)
			continue;
		return secret_area_object_start(objnum, seg, xyz);
	}
	return 0;
}

static int secret_area_metadata_start(void *user, int *seg, int xyz[3])
{
	const level_metadata_game_context *context = (const level_metadata_game_context *) user;

	if (context && secret_area_object_start(context->start_objnum, seg, xyz))
		return 1;
	return secret_area_player_start(seg, xyz);
}

static int secret_area_current_key_mask(void)
{
	int key_player = Player_num;
	int flags;
	int key_mask = 0;
#if defined(NETWORK) && defined(DXX_BUILD_DESCENT_II)
	if ((Game_mode & GM_MULTI_COOP) &&
	    Escort_owner_player >= 0 && Escort_owner_player < MAX_PLAYERS &&
	    Players[Escort_owner_player].connected == CONNECT_PLAYING)
		key_player = Escort_owner_player;
#endif
	flags = Players[key_player].flags;
	if (flags & PLAYER_FLAGS_BLUE_KEY)
		key_mask |= LEVEL_METADATA_KEY_MASK_BLUE;
	if (flags & PLAYER_FLAGS_RED_KEY)
		key_mask |= LEVEL_METADATA_KEY_MASK_RED;
	if (flags & PLAYER_FLAGS_GOLD_KEY)
		key_mask |= LEVEL_METADATA_KEY_MASK_GOLD;
	return key_mask;
}

static int secret_area_start_position(void *user, int xyz[3])
{
	if (!xyz)
		return 0;
	if (secret_area_metadata_start(user, NULL, xyz))
		return 1;
	xyz[0] = Player_init[Player_num].pos.x;
	xyz[1] = Player_init[Player_num].pos.y;
	xyz[2] = Player_init[Player_num].pos.z;
	return 1;
}

static int secret_area_energy_center_group_distance(void)
{
	const char *value = getenv("DXX_ENERGY_CENTER_GROUP_DISTANCE");
	int distance;

	if (!value || !*value)
		return LEVEL_METADATA_DEFAULT_ENERGY_CENTER_GROUP_DISTANCE;
	distance = atoi(value);
	return distance > 0 ? distance : LEVEL_METADATA_DEFAULT_ENERGY_CENTER_GROUP_DISTANCE;
}

static int secret_area_object_count(void *user)
{
	(void) user;
	return num_objects;
}

static int secret_area_object_segment(void *user, int objnum)
{
	(void) user;
	if (objnum < 0 || objnum >= num_objects)
		return -1;
	return Objects[objnum].segnum;
}

static int secret_area_object_type(void *user, int objnum)
{
	(void) user;
	if (objnum < 0 || objnum >= num_objects)
		return OBJ_NONE;
	return Objects[objnum].type;
}

static int secret_area_object_id(void *user, int objnum)
{
	(void) user;
	if (objnum < 0 || objnum >= num_objects)
		return -1;
	return Objects[objnum].id;
}

static int secret_area_object_flags(void *user, int objnum)
{
	(void) user;
	if (objnum < 0 || objnum >= num_objects)
		return 0;
	return Objects[objnum].flags;
}

static int secret_area_object_contains_type(void *user, int objnum)
{
	(void) user;
	if (objnum < 0 || objnum >= num_objects)
		return OBJ_NONE;
	return Objects[objnum].contains_type;
}

static int secret_area_object_contains_id(void *user, int objnum)
{
	(void) user;
	if (objnum < 0 || objnum >= num_objects)
		return -1;
	return Objects[objnum].contains_id;
}

static int secret_area_object_contains_count(void *user, int objnum)
{
	(void) user;
	if (objnum < 0 || objnum >= num_objects)
		return 0;
	return Objects[objnum].contains_count;
}

static int secret_area_object_position(void *user, int objnum, int xyz[3])
{
	(void) user;
	if (objnum < 0 || objnum >= num_objects || !xyz)
		return 0;
	xyz[0] = Objects[objnum].pos.x;
	xyz[1] = Objects[objnum].pos.y;
	xyz[2] = Objects[objnum].pos.z;
	return 1;
}

static int secret_area_object_is_boss(void *user, int objnum)
{
	(void) user;
	if (objnum < 0 || objnum >= num_objects)
		return 0;
	if (Objects[objnum].type != OBJ_ROBOT)
		return 0;
	if (Objects[objnum].id < 0 || Objects[objnum].id >= N_robot_types)
		return 0;
	return Robot_info[Objects[objnum].id].boss_flag != 0;
}

#ifdef DXX_BUILD_DESCENT_II
static int secret_area_object_is_companion(void *user, int objnum)
{
	(void) user;
	if (objnum < 0 || objnum >= num_objects)
		return 0;
	if (Objects[objnum].type != OBJ_ROBOT)
		return 0;
	if (Objects[objnum].id < 0 || Objects[objnum].id >= N_robot_types)
		return 0;
	return Robot_info[Objects[objnum].id].companion != 0;
}
#endif

static const char *secret_area_powerup_name(void *user, int id)
{
	(void) user;
#ifdef DXX_BUILD_DESCENT_II
	return secret_area_fallback_powerup_name(1, id);
#else
	return secret_area_fallback_powerup_name(0, id);
#endif
}

static int secret_area_side_has_exit_trigger(void *user, int seg, int side)
{
	int wall_num;
	int trigger_num;

	(void) user;
	wall_num = secret_area_wall_num(NULL, seg, side);
	if (wall_num < 0 || wall_num >= Num_walls)
		return 0;
	trigger_num = Walls[wall_num].trigger;
	if (trigger_num < 0 || trigger_num >= Num_triggers)
		return 0;
#ifdef DXX_BUILD_DESCENT_II
	return Triggers[trigger_num].type == TT_EXIT || Triggers[trigger_num].type == TT_SECRET_EXIT;
#else
	return (Triggers[trigger_num].flags & (TRIGGER_EXIT | TRIGGER_SECRET_EXIT)) != 0;
#endif
}

static int secret_area_target_visible_from_segment(void *user, int seg, const int from_pos[3], int target_seg, const int target_pos[3])
{
	fvi_info hit_data;
	fvi_query query;
	vms_vector from;
	vms_vector target;

	(void) user;
	if (seg < 0 || seg >= Num_segments || target_seg < 0 || target_seg >= Num_segments || !from_pos || !target_pos)
		return 0;
	from.x = from_pos[0];
	from.y = from_pos[1];
	from.z = from_pos[2];
	target.x = target_pos[0];
	target.y = target_pos[1];
	target.z = target_pos[2];
	memset(&query, 0, sizeof(query));
	memset(&hit_data, 0, sizeof(hit_data));
	query.p0 = &from;
	query.p1 = &target;
	query.startseg = seg;
	query.rad = 0;
	query.thisobjnum = -1;
	query.flags = FQ_TRANSWALL;
	return find_vector_intersection(&query, &hit_data) == HIT_NONE;
}

static int secret_area_trigger_opens_side(int trigger_num, int seg, int side)
{
	int i;

	if (trigger_num < 0 || trigger_num >= Num_triggers)
		return 0;
#ifdef DXX_BUILD_DESCENT_II
	if (Triggers[trigger_num].type != TT_OPEN_DOOR &&
	    Triggers[trigger_num].type != TT_OPEN_WALL &&
	    Triggers[trigger_num].type != TT_ILLUSORY_WALL)
		return 0;
#else
	if ((Triggers[trigger_num].flags &
	     (TRIGGER_CONTROL_DOORS | TRIGGER_ILLUSION_OFF)) == 0)
		return 0;
#endif
	for (i = 0; i < Triggers[trigger_num].num_links; ++i)
		if (Triggers[trigger_num].seg[i] == seg && Triggers[trigger_num].side[i] == side)
			return 1;
	return 0;
}

static int secret_area_side_opener_source_wall_at(int seg, int side, int wanted_index, int allow_keyed_target)
{
	int trigger_num;
	int wall_num;
	int found = 0;

	if (seg < 0 || seg >= Num_segments || side < 0 || side >= MAX_SIDES_PER_SEGMENT)
		return -1;
	wall_num = Segments[seg].sides[side].wall_num;
	if (wall_num < 0 || wall_num >= Num_walls)
		return -1;
	if (!allow_keyed_target && Walls[wall_num].keys != KEY_NONE)
		return -1;
	for (trigger_num = 0; trigger_num < Num_triggers; ++trigger_num) {
		int source_wall;

		if (!secret_area_trigger_opens_side(trigger_num, seg, side))
			continue;
		for (source_wall = 0; source_wall < Num_walls; ++source_wall) {
			if (Walls[source_wall].trigger != trigger_num)
				continue;
			if (found == wanted_index)
				return source_wall;
			found++;
		}
	}
	return -1;
}

static int secret_area_side_opener_segment_at(int seg, int side, int wanted_index)
{
	int source_wall = secret_area_side_opener_source_wall_at(seg, side, wanted_index, 0);

	if (source_wall < 0 || source_wall >= Num_walls)
		return -1;
	return Walls[source_wall].segnum;
}

static int secret_area_triggered_side_opener_count(void *user, int seg, int side)
{
	int count = 0;

	(void) user;
	while (secret_area_side_opener_segment_at(seg, side, count) >= 0)
		count++;
	return count;
}

static int secret_area_metadata_triggered_side_opener_count(void *user, int seg, int side)
{
	int count = 0;

	(void) user;
	/* Metadata travel treats trigger-opened keyed walls as progress doors. */
	while (secret_area_side_opener_source_wall_at(seg, side, count, 1) >= 0)
		count++;
	return count;
}

static int secret_area_triggered_side_opener_segment(void *user, int seg, int side, int index)
{
	(void) user;
	return secret_area_side_opener_segment_at(seg, side, index);
}

static int secret_area_triggered_side_opener_side(void *user, int seg, int side, int index)
{
	int source_wall;

	(void) user;
	source_wall = secret_area_side_opener_source_wall_at(seg, side, index, 0);
	if (source_wall < 0 || source_wall >= Num_walls)
		return -1;
	return Walls[source_wall].sidenum;
}

static int secret_area_triggered_side_opener_wall_num(void *user, int seg, int side, int index)
{
	(void) user;
	return secret_area_side_opener_source_wall_at(seg, side, index, 0);
}

static int secret_area_metadata_triggered_side_opener_wall_num(void *user, int seg, int side, int index)
{
	(void) user;
	return secret_area_side_opener_source_wall_at(seg, side, index, 1);
}

static int secret_area_trigger_type(void *user, int trigger_num)
{
	(void) user;
#ifdef DXX_BUILD_DESCENT_II
	if (trigger_num < 0 || trigger_num >= Num_triggers)
		return -1;
	return Triggers[trigger_num].type;
#else
	int flags;

	if (trigger_num < 0 || trigger_num >= Num_triggers)
		return -1;
	flags = Triggers[trigger_num].flags;
	if (flags & TRIGGER_CONTROL_DOORS)
		return TRIGGER_CONTROL_DOORS;
	if (flags & TRIGGER_ILLUSION_OFF)
		return TRIGGER_ILLUSION_OFF;
	if (flags & TRIGGER_EXIT)
		return TRIGGER_EXIT;
	if (flags & TRIGGER_SECRET_EXIT)
		return TRIGGER_SECRET_EXIT;
	return -1;
#endif
}

static int secret_area_trigger_flags(void *user, int trigger_num)
{
	(void) user;
	if (trigger_num < 0 || trigger_num >= Num_triggers)
		return 0;
	return Triggers[trigger_num].flags;
}

static int secret_area_trigger_link_count(void *user, int trigger_num)
{
	(void) user;
	if (trigger_num < 0 || trigger_num >= Num_triggers)
		return 0;
	return Triggers[trigger_num].num_links;
}

static int secret_area_trigger_link_segment(void *user, int trigger_num, int link_index)
{
	(void) user;
	if (trigger_num < 0 || trigger_num >= Num_triggers ||
	    link_index < 0 || link_index >= Triggers[trigger_num].num_links)
		return -1;
	return Triggers[trigger_num].seg[link_index];
}

static int secret_area_trigger_link_side(void *user, int trigger_num, int link_index)
{
	(void) user;
	if (trigger_num < 0 || trigger_num >= Num_triggers ||
	    link_index < 0 || link_index >= Triggers[trigger_num].num_links)
		return -1;
	return Triggers[trigger_num].side[link_index];
}

static void level_metadata_copy_route(const level_metadata_state *route_state)
{
	if (!route_state)
		return;
	Level_metadata_state.route_status = route_state->route_status;
	snprintf(Level_metadata_state.route_problem,
	         sizeof(Level_metadata_state.route_problem),
	         "%s",
	         route_state->route_problem);
	Level_metadata_state.route_step_count = route_state->route_step_count;
	memcpy(Level_metadata_state.route_steps,
	       route_state->route_steps,
	       sizeof(Level_metadata_state.route_steps));
}

static void level_metadata_rescan_current_level_internal(
    int start_objnum,
    int route_target_seg,
    int route_only,
    level_metadata_unexplored_route *unexplored_result)
{
	level_metadata_game_context context;
	level_metadata_scan_view view;
	level_metadata_state route_state;
	int start_segment;

	context.start_objnum = start_objnum;
	memset(&view, 0, sizeof(view));
	view.user = &context;
	view.num_segments = Num_segments;
	view.num_walls = Num_walls;
	view.start_segment = secret_area_metadata_start(&context, &start_segment, NULL) ? start_segment : Player_init[Player_num].segnum;
	Level_metadata_route_start_objnum = start_objnum;
	Level_metadata_route_start_seg = view.start_segment;
	view.initial_key_mask = secret_area_current_key_mask();
	view.initial_control_center_destroyed = Control_center_destroyed != 0;
	view.segment_special_fuelcen = SEGMENT_IS_FUELCEN;
	view.segment_special_robotmaker = SEGMENT_IS_ROBOTMAKER;
	view.segment_special_control_center = SEGMENT_IS_CONTROLCEN;
	view.energy_center_group_distance = secret_area_energy_center_group_distance();
	view.wall_type_blastable = WALL_BLASTABLE;
	view.wall_type_door = WALL_DOOR;
	view.wall_type_illusion = WALL_ILLUSION;
	view.wall_type_open = WALL_OPEN;
	view.wall_flag_door_locked = WALL_DOOR_LOCKED;
	view.wall_flag_door_opened = WALL_DOOR_OPENED;
	view.wall_key_none = KEY_NONE;
	view.wall_key_blue = KEY_BLUE;
	view.wall_key_red = KEY_RED;
	view.wall_key_gold = KEY_GOLD;
	view.wall_clip_hidden = WCF_HIDDEN;
	view.obj_type_none = OBJ_NONE;
	view.obj_type_robot = OBJ_ROBOT;
	view.obj_type_hostage = OBJ_HOSTAGE;
	view.obj_type_powerup = OBJ_POWERUP;
	view.obj_type_control_center = OBJ_CNTRLCEN;
	view.obj_flag_should_be_dead = OF_SHOULD_BE_DEAD;
	view.powerup_key_blue = POW_KEY_BLUE;
	view.powerup_key_red = POW_KEY_RED;
	view.powerup_key_gold = POW_KEY_GOLD;
#ifdef DXX_BUILD_DESCENT_II
	view.trigger_type_open_door = TT_OPEN_DOOR;
	view.trigger_type_exit = TT_EXIT;
	view.trigger_type_secret_exit = TT_SECRET_EXIT;
	view.trigger_type_illusion_off = TT_ILLUSION_OFF;
	view.trigger_type_unlock_door = TT_UNLOCK_DOOR;
	view.trigger_type_open_wall = TT_OPEN_WALL;
	view.trigger_type_illusory_wall = TT_ILLUSORY_WALL;
	view.trigger_flag_disabled = TF_DISABLED;
#else
	view.trigger_type_open_door = TRIGGER_CONTROL_DOORS;
	view.trigger_type_exit = TRIGGER_EXIT;
	view.trigger_type_secret_exit = TRIGGER_SECRET_EXIT;
	view.trigger_type_illusion_off = TRIGGER_ILLUSION_OFF;
	view.trigger_type_unlock_door = -2;
	view.trigger_type_open_wall = -3;
	view.trigger_type_illusory_wall = -4;
#endif
	view.segment_child = secret_area_segment_child;
	view.segment_is_explored = secret_area_segment_is_explored;
	view.reverse_side = secret_area_reverse_side;
	view.side_is_flyable = secret_area_side_is_flyable;
	view.side_is_control_center_link = secret_area_side_is_control_center_link;
	view.wall_num = secret_area_wall_num;
	view.wall_segment = secret_area_wall_segment;
	view.wall_side = secret_area_wall_side;
	view.wall_type = secret_area_wall_type;
	view.wall_flags = secret_area_wall_flags;
	view.wall_keys = secret_area_wall_keys;
	view.wall_clip_flags = secret_area_wall_clip_flags;
	view.wall_trigger = secret_area_wall_trigger;
	view.segment_special = secret_area_segment_special;
	view.segment_center = secret_area_segment_center;
	view.segment_vertex = secret_area_segment_vertex;
	view.start_position = secret_area_start_position;
	view.object_count = secret_area_object_count;
	view.object_segment = secret_area_object_segment;
	view.object_type = secret_area_object_type;
	view.object_id = secret_area_object_id;
	view.object_flags = secret_area_object_flags;
	view.object_contains_type = secret_area_object_contains_type;
	view.object_contains_id = secret_area_object_contains_id;
	view.object_contains_count = secret_area_object_contains_count;
	view.object_position = secret_area_object_position;
	view.object_is_boss = secret_area_object_is_boss;
#ifdef DXX_BUILD_DESCENT_II
	view.object_is_companion = secret_area_object_is_companion;
#endif
	view.side_has_exit_trigger = secret_area_side_has_exit_trigger;
	view.triggered_side_opener_count = secret_area_metadata_triggered_side_opener_count;
	view.triggered_side_opener_wall_num = secret_area_metadata_triggered_side_opener_wall_num;
	view.trigger_type = secret_area_trigger_type;
	view.trigger_flags = secret_area_trigger_flags;
	view.trigger_link_count = secret_area_trigger_link_count;
	view.trigger_link_segment = secret_area_trigger_link_segment;
	view.trigger_link_side = secret_area_trigger_link_side;
	view.target_visible_from_segment = secret_area_target_visible_from_segment;
	view.wall_is_shootable_trigger = secret_area_wall_is_shootable_trigger;
	if (!route_only)
		level_metadata_scan_level(&view, &Level_metadata_state);
	if (route_only) {
		if (unexplored_result)
			level_metadata_scan_unexplored_route(&view, &route_state, unexplored_result);
		else if (route_target_seg >= 0)
			level_metadata_scan_route_to_segment(&view, route_target_seg, &route_state);
		else {
			level_metadata_state_clear(&route_state);
			level_metadata_scan_end_route(&view, &route_state);
		}
		level_metadata_copy_route(&route_state);
	}
}

void level_metadata_rescan_current_level(void)
{
	level_metadata_rescan_current_level_internal(-1, -1, 0, NULL);
}

void level_metadata_rescan_current_level_from_object(int objnum)
{
	level_metadata_rescan_current_level_internal(objnum, -1, 0, NULL);
}

void level_metadata_rescan_route_from_object(int objnum)
{
	level_metadata_rescan_current_level_internal(objnum, -1, 1, NULL);
}

void level_metadata_rescan_route_to_segment_from_object(int objnum, int target_seg)
{
	level_metadata_rescan_current_level_internal(objnum, target_seg, 1, NULL);
}

int level_metadata_rescan_unexplored_route_from_object(
    int objnum,
    level_metadata_unexplored_route *result)
{
	level_metadata_rescan_current_level_internal(objnum, -1, 1, result);
	return result && result->target_seg >= 0;
}

int level_metadata_get_route_start_objnum(void)
{
	return Level_metadata_route_start_objnum;
}

int level_metadata_get_route_start_seg(void)
{
	return Level_metadata_route_start_seg;
}

void secret_area_rescan_current_level(void)
{
	secret_area_scan_view view;
	int start_segment;

	secret_area_trace("start");
	Secret_area_reveal_unfound = 0;
	memset(&view, 0, sizeof(view));
	view.num_segments = Num_segments;
	view.num_walls = Num_walls;
	view.start_segment = secret_area_player_start(&start_segment, NULL) ? start_segment : Player_init[Player_num].segnum;
	view.max_generated = SECRET_AREA_MAX_GENERATED;
	view.wall_type_blastable = WALL_BLASTABLE;
	view.wall_type_door = WALL_DOOR;
	view.wall_type_illusion = WALL_ILLUSION;
	view.wall_type_open = WALL_OPEN;
	view.wall_flag_door_locked = WALL_DOOR_LOCKED;
	view.wall_flag_illusion_off = WALL_ILLUSION_OFF;
	view.wall_key_none = KEY_NONE;
	view.wall_key_blue = KEY_BLUE;
	view.wall_key_red = KEY_RED;
	view.wall_key_gold = KEY_GOLD;
	view.wall_clip_hidden = WCF_HIDDEN;
	view.obj_type_none = OBJ_NONE;
	view.obj_type_robot = OBJ_ROBOT;
	view.obj_type_hostage = OBJ_HOSTAGE;
	view.obj_type_powerup = OBJ_POWERUP;
	view.obj_type_control_center = OBJ_CNTRLCEN;
	view.obj_flag_should_be_dead = OF_SHOULD_BE_DEAD;
	view.powerup_key_blue = POW_KEY_BLUE;
	view.powerup_key_red = POW_KEY_RED;
	view.powerup_key_gold = POW_KEY_GOLD;
	view.segment_special_control_center = SEGMENT_IS_CONTROLCEN;
	view.segment_special_robotmaker = SEGMENT_IS_ROBOTMAKER;
	view.segment_child = secret_area_segment_child;
	view.reverse_side = secret_area_reverse_side;
	view.wall_num = secret_area_wall_num;
	view.wall_type = secret_area_wall_type;
	view.wall_flags = secret_area_wall_flags;
	view.wall_keys = secret_area_wall_keys;
	view.wall_clip_flags = secret_area_wall_clip_flags;
	view.segment_special = secret_area_segment_special;
	view.segment_center = secret_area_segment_center;
	view.object_count = secret_area_object_count;
	view.object_segment = secret_area_object_segment;
	view.object_type = secret_area_object_type;
	view.object_id = secret_area_object_id;
	view.object_flags = secret_area_object_flags;
	view.object_contains_type = secret_area_object_contains_type;
	view.object_contains_id = secret_area_object_contains_id;
	view.object_contains_count = secret_area_object_contains_count;
	view.powerup_name = secret_area_powerup_name;
	view.side_has_exit_trigger = secret_area_side_has_exit_trigger;
	view.triggered_side_opener_count = secret_area_triggered_side_opener_count;
	view.triggered_side_opener_segment = secret_area_triggered_side_opener_segment;
	view.triggered_side_opener_side = secret_area_triggered_side_opener_side;
	view.triggered_side_opener_wall_num = secret_area_triggered_side_opener_wall_num;
	secret_area_scan_level(&view, &Secret_area_state);
	level_metadata_rescan_current_level();
	secret_area_trace("done");
}

const secret_area_state *secret_area_get_state(void)
{
	return &Secret_area_state;
}

const level_metadata_state *level_metadata_get_state(void)
{
	return &Level_metadata_state;
}

int secret_area_note_segment_entered(int segnum)
{
	int display_index = secret_area_mark_segment_entered(&Secret_area_state, segnum);
	if (display_index > 0)
		HUD_init_message(HM_DEFAULT, "found secret %d (total: %d/%d)", display_index, secret_area_found_count(&Secret_area_state), secret_area_total(&Secret_area_state));
	return display_index;
}

void secret_area_restore_saved_found(int saved_total, const unsigned char *found, int found_capacity, const unsigned char *visited, int visited_count)
{
	if (saved_total == secret_area_total(&Secret_area_state))
		secret_area_restore_found(&Secret_area_state, saved_total, found, found_capacity);
	else
		secret_area_restore_found_from_visited(&Secret_area_state, visited, visited_count);
}

void secret_area_restore_found_from_automap(const unsigned char *visited, int visited_count)
{
	secret_area_restore_found_from_visited(&Secret_area_state, visited, visited_count);
}

int secret_area_get_reveal_unfound(void)
{
	return Secret_area_reveal_unfound;
}

void secret_area_set_reveal_unfound(int reveal)
{
	Secret_area_reveal_unfound = reveal ? 1 : 0;
}
