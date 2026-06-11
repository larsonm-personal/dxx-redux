#include "secretarea.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gameseg.h"
#include "gameseq.h"
#include "fvi.h"
#include "hudmsg.h"
#include "level_metadata_scan.h"
#include "object.h"
#include "player.h"
#include "powerup.h"
#include "secret_area_item_names.h"
#include "segment.h"
#include "switch.h"
#include "wall.h"

static secret_area_state Secret_area_state;
static level_metadata_state Level_metadata_state;
static int Secret_area_reveal_unfound;

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

static int secret_area_reverse_side(void *user, int seg, int child)
{
	(void) user;
	if (seg < 0 || seg >= Num_segments || child < 0 || child >= Num_segments)
		return -1;
	return find_connect_side(&Segments[seg], &Segments[child]);
}

static int secret_area_wall_num(void *user, int seg, int side)
{
	(void) user;
	if (seg < 0 || seg >= Num_segments || side < 0 || side >= MAX_SIDES_PER_SEGMENT)
		return -1;
	return Segments[seg].sides[side].wall_num;
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

static int secret_area_player_start(int *seg, int xyz[3])
{
	int objnum;

	for (objnum = 0; objnum < num_objects; ++objnum) {
		int type = Objects[objnum].type;
		if (type != OBJ_PLAYER && type != OBJ_GHOST)
			continue;
		if (seg)
			*seg = Objects[objnum].segnum;
		if (xyz) {
			xyz[0] = Objects[objnum].pos.x;
			xyz[1] = Objects[objnum].pos.y;
			xyz[2] = Objects[objnum].pos.z;
		}
		return 1;
	}
	return 0;
}

static int secret_area_start_position(void *user, int xyz[3])
{
	(void) user;
	if (!xyz)
		return 0;
	if (secret_area_player_start(NULL, xyz))
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

#ifdef DXX_BUILD_DESCENT_II
static int secret_area_trigger_opens_side(int trigger_num, int seg, int side)
{
	int i;

	if (trigger_num < 0 || trigger_num >= Num_triggers)
		return 0;
	if (Triggers[trigger_num].type != TT_OPEN_DOOR &&
	    Triggers[trigger_num].type != TT_OPEN_WALL &&
	    Triggers[trigger_num].type != TT_ILLUSORY_WALL)
		return 0;
	for (i = 0; i < Triggers[trigger_num].num_links; ++i)
		if (Triggers[trigger_num].seg[i] == seg && Triggers[trigger_num].side[i] == side)
			return 1;
	return 0;
}
#endif

static int secret_area_side_opener_source_wall_at(int seg, int side, int wanted_index, int allow_keyed_target)
{
#ifdef DXX_BUILD_DESCENT_II
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
#else
	(void) seg;
	(void) side;
	(void) wanted_index;
	(void) allow_keyed_target;
#endif
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

static void level_metadata_rescan_current_level(void)
{
	level_metadata_scan_view view;
	int start_segment;

	memset(&view, 0, sizeof(view));
	view.num_segments = Num_segments;
	view.num_walls = Num_walls;
	view.start_segment = secret_area_player_start(&start_segment, NULL) ? start_segment : Player_init[Player_num].segnum;
	view.segment_special_fuelcen = SEGMENT_IS_FUELCEN;
	view.segment_special_robotmaker = SEGMENT_IS_ROBOTMAKER;
	view.segment_special_control_center = SEGMENT_IS_CONTROLCEN;
	view.energy_center_group_distance = secret_area_energy_center_group_distance();
	view.wall_type_blastable = WALL_BLASTABLE;
	view.wall_type_door = WALL_DOOR;
	view.wall_type_illusion = WALL_ILLUSION;
	view.wall_type_open = WALL_OPEN;
	view.wall_key_none = KEY_NONE;
	view.wall_key_blue = KEY_BLUE;
	view.wall_key_red = KEY_RED;
	view.wall_key_gold = KEY_GOLD;
	view.obj_type_none = OBJ_NONE;
	view.obj_type_hostage = OBJ_HOSTAGE;
	view.obj_type_powerup = OBJ_POWERUP;
	view.obj_type_control_center = OBJ_CNTRLCEN;
	view.obj_flag_should_be_dead = OF_SHOULD_BE_DEAD;
	view.powerup_key_blue = POW_KEY_BLUE;
	view.powerup_key_red = POW_KEY_RED;
	view.powerup_key_gold = POW_KEY_GOLD;
	view.segment_child = secret_area_segment_child;
	view.reverse_side = secret_area_reverse_side;
	view.wall_num = secret_area_wall_num;
	view.wall_type = secret_area_wall_type;
	view.wall_keys = secret_area_wall_keys;
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
	view.side_has_exit_trigger = secret_area_side_has_exit_trigger;
	view.triggered_side_opener_count = secret_area_metadata_triggered_side_opener_count;
	view.target_visible_from_segment = secret_area_target_visible_from_segment;
	level_metadata_scan_level(&view, &Level_metadata_state);
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
