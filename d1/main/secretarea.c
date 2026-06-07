#include "secretarea.h"

#include <string.h>

#include "gameseg.h"
#include "gameseq.h"
#include "object.h"
#include "player.h"
#include "powerup.h"
#include "segment.h"
#include "switch.h"
#include "wall.h"

static secret_area_state Secret_area_state;

static int secret_area_segment_child(void *user, int seg, int side)
{
	(void)user;
	if (seg < 0 || seg >= Num_segments || side < 0 || side >= MAX_SIDES_PER_SEGMENT)
		return -1;
	return Segments[seg].children[side];
}

static int secret_area_reverse_side(void *user, int seg, int child)
{
	(void)user;
	if (seg < 0 || seg >= Num_segments || child < 0 || child >= Num_segments)
		return -1;
	return find_connect_side(&Segments[seg], &Segments[child]);
}

static int secret_area_wall_num(void *user, int seg, int side)
{
	(void)user;
	if (seg < 0 || seg >= Num_segments || side < 0 || side >= MAX_SIDES_PER_SEGMENT)
		return -1;
	return Segments[seg].sides[side].wall_num;
}

static int secret_area_wall_type(void *user, int wall_num)
{
	(void)user;
	if (wall_num < 0 || wall_num >= Num_walls)
		return WALL_NORMAL;
	return Walls[wall_num].type;
}

static int secret_area_wall_flags(void *user, int wall_num)
{
	(void)user;
	if (wall_num < 0 || wall_num >= Num_walls)
		return 0;
	return Walls[wall_num].flags;
}

static int secret_area_wall_keys(void *user, int wall_num)
{
	(void)user;
	if (wall_num < 0 || wall_num >= Num_walls)
		return KEY_NONE;
	return Walls[wall_num].keys;
}

static int secret_area_wall_clip_flags(void *user, int wall_num)
{
	int clip_num;

	(void)user;
	if (wall_num < 0 || wall_num >= Num_walls)
		return 0;
	clip_num = Walls[wall_num].clip_num;
	if (clip_num < 0 || clip_num >= Num_wall_anims)
		return 0;
	return WallAnims[clip_num].flags;
}

static int secret_area_segment_special(void *user, int seg)
{
	(void)user;
	if (seg < 0 || seg >= Num_segments)
		return SEGMENT_IS_NOTHING;
	return Segments[seg].special;
}

static int secret_area_segment_center(void *user, int seg, int xyz[3])
{
	vms_vector center;

	(void)user;
	if (seg < 0 || seg >= Num_segments || !xyz)
		return 0;
	compute_segment_center(&center, &Segments[seg]);
	xyz[0] = center.x;
	xyz[1] = center.y;
	xyz[2] = center.z;
	return 1;
}

static int secret_area_object_count(void *user)
{
	(void)user;
	return num_objects;
}

static int secret_area_object_segment(void *user, int objnum)
{
	(void)user;
	if (objnum < 0 || objnum >= num_objects)
		return -1;
	return Objects[objnum].segnum;
}

static int secret_area_object_type(void *user, int objnum)
{
	(void)user;
	if (objnum < 0 || objnum >= num_objects)
		return OBJ_NONE;
	return Objects[objnum].type;
}

static int secret_area_object_id(void *user, int objnum)
{
	(void)user;
	if (objnum < 0 || objnum >= num_objects)
		return -1;
	return Objects[objnum].id;
}

static int secret_area_object_flags(void *user, int objnum)
{
	(void)user;
	if (objnum < 0 || objnum >= num_objects)
		return 0;
	return Objects[objnum].flags;
}

static int secret_area_object_contains_type(void *user, int objnum)
{
	(void)user;
	if (objnum < 0 || objnum >= num_objects)
		return OBJ_NONE;
	return Objects[objnum].contains_type;
}

static int secret_area_object_contains_id(void *user, int objnum)
{
	(void)user;
	if (objnum < 0 || objnum >= num_objects)
		return -1;
	return Objects[objnum].contains_id;
}

static int secret_area_side_has_exit_trigger(void *user, int seg, int side)
{
	int wall_num;
	int trigger_num;
	int flags;

	(void)user;
	wall_num = secret_area_wall_num(NULL, seg, side);
	if (wall_num < 0 || wall_num >= Num_walls)
		return 0;
	trigger_num = Walls[wall_num].trigger;
	if (trigger_num < 0 || trigger_num >= Num_triggers)
		return 0;
	flags = Triggers[trigger_num].flags;
	return (flags & (TRIGGER_EXIT | TRIGGER_SECRET_EXIT)) != 0;
}

void secret_area_rescan_current_level(void)
{
	secret_area_scan_view view;

	memset(&view, 0, sizeof(view));
	view.num_segments = Num_segments;
	view.num_walls = Num_walls;
	view.start_segment = Player_init[Player_num].segnum;
	view.max_generated = SECRET_AREA_MAX_GENERATED;
	view.wall_type_blastable = WALL_BLASTABLE;
	view.wall_type_door = WALL_DOOR;
	view.wall_type_illusion = WALL_ILLUSION;
	view.wall_type_open = WALL_OPEN;
	view.wall_flag_door_locked = WALL_DOOR_LOCKED;
	view.wall_flag_illusion_off = WALL_ILLUSION_OFF;
	view.wall_key_none = KEY_NONE;
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
	view.side_has_exit_trigger = secret_area_side_has_exit_trigger;
	secret_area_scan_level(&view, &Secret_area_state);
}

const secret_area_state *secret_area_get_state(void)
{
	return &Secret_area_state;
}

int secret_area_note_segment_entered(int segnum)
{
	return secret_area_mark_segment_entered(&Secret_area_state, segnum);
}
