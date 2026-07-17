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
#include "laser.h"
#include "level_metadata_scan.h"
#include "object.h"
#include "player.h"
#include "polyobj.h"
#include "powerup.h"
#include "robot.h"
#include "route_snapshot_c.h"
#include "route_analysis_cache.h"
#include "secret_area_item_names.h"
#include "segment.h"
#include "automap.h"
#include "switch.h"
#include "wall.h"
#include "weapon.h"
#ifdef __ANDROID__
#include "physfs.h"
#endif
#ifdef DXX_BUILD_DESCENT_II
#include "ai.h"
#include "escort.h"
#endif
#ifdef NETWORK
#include "multi.h"
#endif

static secret_area_state Secret_area_state;
static level_metadata_state Level_metadata_canonical_state;
static route_planner_plan_summary Level_metadata_canonical_plan_summary;
static int Level_metadata_canonical_plan_summary_valid;
static level_metadata_state Level_metadata_live_route_state;
static int Level_metadata_live_route_state_valid;
static route_planner_plan_summary Level_metadata_live_plan_summary;
static int Level_metadata_live_plan_summary_valid;
static route_snapshot_summary Level_metadata_canonical_snapshot;
static int Level_metadata_canonical_snapshot_valid;
static route_snapshot_summary Level_metadata_live_snapshot;
static int Level_metadata_live_snapshot_valid;
static int Level_metadata_route_start_objnum = -1;
static int Level_metadata_route_start_seg = -1;
static int Secret_area_reveal_unfound;
static int Level_metadata_objective_mode;
static route_analysis_cache_summary Level_metadata_analysis_cache_summary;
static unsigned char
    Level_metadata_completed_canonical_steps[LEVEL_METADATA_MAX_ROUTE_STEPS];

#ifndef DXX_ANDROID_VERSION_CODE
#define DXX_ANDROID_VERSION_CODE 0
#endif

static void level_metadata_apply_planned_route(
    level_metadata_state *destination,
    const level_metadata_state *route)
{
	destination->travel_distance = route->travel_distance;
	destination->travel_time_seconds = route->travel_time_seconds;
	destination->route_status = route->route_status;
	snprintf(destination->route_problem, sizeof(destination->route_problem), "%s", route->route_problem);
	snprintf(destination->route_note, sizeof(destination->route_note), "%s", route->route_note);
	destination->route_step_count = route->route_step_count;
	memset(destination->route_steps, 0, sizeof(destination->route_steps));
	memcpy(destination->route_steps, route->route_steps,
	       sizeof(destination->route_steps[0]) * route->route_step_count);
}

typedef struct level_metadata_game_context {
	int start_objnum;
} level_metadata_game_context;

static level_metadata_game_context Level_metadata_game_context;
static level_metadata_scan_view Level_metadata_scan_view;
static int Level_metadata_scan_view_initialized;

#define LEVEL_METADATA_VISIBILITY_CACHE_INITIAL_CAPACITY 4096
#define LEVEL_METADATA_FVI_CONFIRM_SPAN                  (64 * F1_0)

static fix level_metadata_switch_projectile_radius(void)
{
	const weapon_info *weapon = &Weapon_info[LASER_ID_L1];

	if (weapon->render_type == WEAPON_RENDER_BLOB ||
	    weapon->render_type == WEAPON_RENDER_VCLIP)
		return weapon->blob_size;
	if (weapon->render_type == WEAPON_RENDER_POLYMODEL &&
	    weapon->model_num >= 0 && weapon->model_num < N_polygon_models &&
	    weapon->po_len_to_width_ratio > 0)
		return fixdiv(Polygon_models[weapon->model_num].rad,
		              weapon->po_len_to_width_ratio);
	if (weapon->render_type == WEAPON_RENDER_NONE)
		return F1_0;
	return 0;
}

enum level_metadata_visibility_target_kind {
	LEVEL_METADATA_VISIBILITY_TARGET_WALL = 1,
	LEVEL_METADATA_VISIBILITY_TARGET_POSITION = 2
};

typedef struct level_metadata_visibility_key {
	int kind;
	int from_seg;
	int from_pos[3];
	int target_id;
	int target_pos[3];
	int clearance_radius;
} level_metadata_visibility_key;

typedef struct level_metadata_visibility_entry {
	unsigned long long hash;
	level_metadata_visibility_key key;
	unsigned char used;
	unsigned char result;
} level_metadata_visibility_entry;

static level_metadata_visibility_entry *Level_metadata_visibility_entries;
static int Level_metadata_visibility_count;
static level_metadata_visibility_cache_summary Level_metadata_visibility_summary;

typedef struct level_metadata_wall_shot_diagnostics {
	unsigned int requests;
	unsigned int cache_accepts;
	unsigned int cache_rejects;
	unsigned int invalid_inputs;
	unsigned int unoccupiable_poses;
	unsigned int target_wall_hits;
	unsigned int transparent_connected;
	unsigned int transparent_disconnected;
	unsigned int blocked_by_other_wall;
	unsigned int bad_start_points;
	unsigned int other_fates;
} level_metadata_wall_shot_diagnostics;

static level_metadata_wall_shot_diagnostics Level_metadata_wall_shot_diagnostics;

static void level_metadata_trace_wall_shot_diagnostics(void)
{
	if (!getenv("DXX_SECRET_AREA_DUMP_TRACE"))
		return;
	fprintf(stderr,
	        "SECRET-AREA-DUMP TRACE wall_shots requests=%u cache_accepts=%u "
	        "cache_rejects=%u invalid=%u unoccupiable=%u target_hits=%u "
	        "transparent_connected=%u transparent_disconnected=%u "
	        "blocked_other_wall=%u bad_start=%u other=%u\n",
	        Level_metadata_wall_shot_diagnostics.requests,
	        Level_metadata_wall_shot_diagnostics.cache_accepts,
	        Level_metadata_wall_shot_diagnostics.cache_rejects,
	        Level_metadata_wall_shot_diagnostics.invalid_inputs,
	        Level_metadata_wall_shot_diagnostics.unoccupiable_poses,
	        Level_metadata_wall_shot_diagnostics.target_wall_hits,
	        Level_metadata_wall_shot_diagnostics.transparent_connected,
	        Level_metadata_wall_shot_diagnostics.transparent_disconnected,
	        Level_metadata_wall_shot_diagnostics.blocked_by_other_wall,
	        Level_metadata_wall_shot_diagnostics.bad_start_points,
	        Level_metadata_wall_shot_diagnostics.other_fates);
	fflush(stderr);
}

static unsigned long long level_metadata_visibility_hash_int(
    unsigned long long hash,
    int value)
{
	hash ^= (unsigned int) value;
	return hash * 1099511628211ULL;
}

static unsigned long long level_metadata_visibility_hash_key(
    const level_metadata_visibility_key *key)
{
	unsigned long long hash = 1469598103934665603ULL;
	int coordinate;

	hash = level_metadata_visibility_hash_int(hash, key->kind);
	hash = level_metadata_visibility_hash_int(hash, key->from_seg);
	for (coordinate = 0; coordinate < 3; ++coordinate)
		hash = level_metadata_visibility_hash_int(hash, key->from_pos[coordinate]);
	hash = level_metadata_visibility_hash_int(hash, key->target_id);
	for (coordinate = 0; coordinate < 3; ++coordinate)
		hash = level_metadata_visibility_hash_int(hash, key->target_pos[coordinate]);
	hash = level_metadata_visibility_hash_int(hash, key->clearance_radius);
	return hash ? hash : 1;
}

static int level_metadata_visibility_key_equal(
    const level_metadata_visibility_key *left,
    const level_metadata_visibility_key *right)
{
	return memcmp(left, right, sizeof(*left)) == 0;
}

static int level_metadata_visibility_cache_resize(int capacity)
{
	level_metadata_visibility_entry *previous = Level_metadata_visibility_entries;
	int previous_capacity = Level_metadata_visibility_summary.capacity;
	level_metadata_visibility_entry *entries;
	int index;

	entries = (level_metadata_visibility_entry *) calloc(
	    (size_t) capacity, sizeof(*entries));
	if (!entries)
		return 0;
	Level_metadata_visibility_entries = entries;
	Level_metadata_visibility_summary.capacity = capacity;
	Level_metadata_visibility_count = 0;
	for (index = 0; index < previous_capacity; ++index) {
		int slot;
		if (!previous[index].used)
			continue;
		slot = (int) (previous[index].hash & (unsigned long long) (capacity - 1));
		while (entries[slot].used)
			slot = (slot + 1) & (capacity - 1);
		entries[slot] = previous[index];
		Level_metadata_visibility_count++;
	}
	free(previous);
	Level_metadata_visibility_summary.entries =
	    Level_metadata_visibility_count;
	return 1;
}

static int level_metadata_visibility_cache_lookup(
    const level_metadata_visibility_key *key,
    int *result)
{
	unsigned long long hash;
	int capacity = Level_metadata_visibility_summary.capacity;
	int slot;
	int probe;

	if (!Level_metadata_visibility_entries || capacity <= 0)
		return 0;
	hash = level_metadata_visibility_hash_key(key);
	slot = (int) (hash & (unsigned long long) (capacity - 1));
	for (probe = 0; probe < capacity; ++probe) {
		const level_metadata_visibility_entry *entry =
		    &Level_metadata_visibility_entries[slot];
		if (!entry->used)
			return 0;
		if (entry->hash == hash &&
		    level_metadata_visibility_key_equal(&entry->key, key)) {
			*result = entry->result != 0;
			Level_metadata_visibility_summary.hits++;
			return 1;
		}
		slot = (slot + 1) & (capacity - 1);
	}
	return 0;
}

static void level_metadata_visibility_cache_store(
    const level_metadata_visibility_key *key,
    int result)
{
	unsigned long long hash;
	int capacity = Level_metadata_visibility_summary.capacity;
	int slot;
	int probe;

	if (capacity <= 0) {
		if (!level_metadata_visibility_cache_resize(
		        LEVEL_METADATA_VISIBILITY_CACHE_INITIAL_CAPACITY)) {
			Level_metadata_visibility_summary.bypasses++;
			return;
		}
		capacity = Level_metadata_visibility_summary.capacity;
	} else if ((Level_metadata_visibility_count + 1) * 10 > capacity * 7) {
		if (level_metadata_visibility_cache_resize(capacity * 2))
			capacity = Level_metadata_visibility_summary.capacity;
	}
	hash = level_metadata_visibility_hash_key(key);
	slot = (int) (hash & (unsigned long long) (capacity - 1));
	for (probe = 0; probe < capacity; ++probe) {
		level_metadata_visibility_entry *entry =
		    &Level_metadata_visibility_entries[slot];
		if (!entry->used) {
			entry->used = 1;
			entry->hash = hash;
			entry->key = *key;
			entry->result = result != 0;
			Level_metadata_visibility_count++;
			Level_metadata_visibility_summary.entries =
			    Level_metadata_visibility_count;
			return;
		}
		if (entry->hash == hash &&
		    level_metadata_visibility_key_equal(&entry->key, key)) {
			entry->result = result != 0;
			return;
		}
		slot = (slot + 1) & (capacity - 1);
	}
	Level_metadata_visibility_summary.bypasses++;
}

static unsigned long long level_metadata_visibility_world_hash(void)
{
	unsigned long long hash = 1469598103934665603ULL;
	int segment;
	int side;
	int vertex;

	hash = level_metadata_visibility_hash_int(hash, Current_level_num);
	hash = level_metadata_visibility_hash_int(hash, Num_segments);
	hash = level_metadata_visibility_hash_int(hash, Num_vertices);
	hash = level_metadata_visibility_hash_int(hash, Num_walls);
	for (vertex = 0; vertex < Num_vertices; ++vertex) {
		hash = level_metadata_visibility_hash_int(hash, Vertices[vertex].x);
		hash = level_metadata_visibility_hash_int(hash, Vertices[vertex].y);
		hash = level_metadata_visibility_hash_int(hash, Vertices[vertex].z);
	}
	for (segment = 0; segment < Num_segments; ++segment) {
		for (side = 0; side < MAX_SIDES_PER_SEGMENT; ++side) {
			hash = level_metadata_visibility_hash_int(
			    hash, Segments[segment].children[side]);
			hash = level_metadata_visibility_hash_int(
			    hash, Segments[segment].sides[side].wall_num);
			hash = level_metadata_visibility_hash_int(
			    hash, Segments[segment].sides[side].tmap_num);
			hash = level_metadata_visibility_hash_int(
			    hash, Segments[segment].sides[side].tmap_num2);
			hash = level_metadata_visibility_hash_int(
			    hash, WALL_IS_DOORWAY(&Segments[segment], side));
		}
	}
	return hash ? hash : 1;
}

static void level_metadata_visibility_cache_sync(void)
{
	unsigned long long world_hash =
	    level_metadata_visibility_world_hash();
	int capacity = Level_metadata_visibility_summary.capacity;
	int resets = Level_metadata_visibility_summary.resets;

	if (Level_metadata_visibility_summary.world_hash == world_hash &&
	    Level_metadata_visibility_summary.resets != 0)
		return;
	if (Level_metadata_visibility_entries)
		memset(
		    Level_metadata_visibility_entries, 0,
		    (size_t) Level_metadata_visibility_summary.capacity *
		        sizeof(*Level_metadata_visibility_entries));
	memset(
	    &Level_metadata_visibility_summary, 0,
	    sizeof(Level_metadata_visibility_summary));
	Level_metadata_visibility_summary.world_hash = world_hash;
	Level_metadata_visibility_summary.capacity = capacity;
	Level_metadata_visibility_summary.resets = resets + 1;
	Level_metadata_visibility_count = 0;
}

static unsigned int level_metadata_next_generation(unsigned int current)
{
	current++;
	return current ? current : 1;
}

static void level_metadata_seed_snapshot_generations(
    route_snapshot_summary *snapshot)
{
	snapshot->topology_generation = 1;
	snapshot->start_generation = 1;
	snapshot->progression_generation = 1;
	snapshot->navigation_generation = 1;
	snapshot->trigger_generation = 1;
	snapshot->object_generation = 1;
	snapshot->automap_generation = 1;
}

static void level_metadata_advance_snapshot_generations(
    route_snapshot_summary *snapshot,
    const route_snapshot_summary *previous)
{
	snapshot->topology_generation = previous->topology_generation;
	snapshot->start_generation = previous->start_generation;
	snapshot->progression_generation = previous->progression_generation;
	snapshot->navigation_generation = previous->navigation_generation;
	snapshot->trigger_generation = previous->trigger_generation;
	snapshot->object_generation = previous->object_generation;
	snapshot->automap_generation = previous->automap_generation;
	if (snapshot->topology_hash != previous->topology_hash)
		snapshot->topology_generation = level_metadata_next_generation(snapshot->topology_generation);
	if (snapshot->start_hash != previous->start_hash)
		snapshot->start_generation = level_metadata_next_generation(snapshot->start_generation);
	if (snapshot->progression_hash != previous->progression_hash)
		snapshot->progression_generation = level_metadata_next_generation(snapshot->progression_generation);
	if (snapshot->navigation_hash != previous->navigation_hash)
		snapshot->navigation_generation = level_metadata_next_generation(snapshot->navigation_generation);
	if (snapshot->trigger_hash != previous->trigger_hash)
		snapshot->trigger_generation = level_metadata_next_generation(snapshot->trigger_generation);
	if (snapshot->object_hash != previous->object_hash)
		snapshot->object_generation = level_metadata_next_generation(snapshot->object_generation);
	if (snapshot->automap_hash != previous->automap_hash)
		snapshot->automap_generation = level_metadata_next_generation(snapshot->automap_generation);
}

typedef struct level_metadata_opener_entry {
	short source_wall;
	short next;
} level_metadata_opener_entry;

#define LEVEL_METADATA_MAX_OPENER_ENTRIES            (MAX_WALLS * MAX_WALLS_PER_LINK)
#define LEVEL_METADATA_MIN_NARROW_COMPONENT_SEGMENTS 3

static vms_vector Level_metadata_segment_centers[LEVEL_METADATA_MAX_SEGMENTS];
static int Level_metadata_side_clearance[LEVEL_METADATA_MAX_SEGMENTS][MAX_SIDES_PER_SEGMENT];
static short Level_metadata_opener_first[LEVEL_METADATA_MAX_SEGMENTS][MAX_SIDES_PER_SEGMENT];
static level_metadata_opener_entry Level_metadata_opener_entries[LEVEL_METADATA_MAX_OPENER_ENTRIES];
static int Level_metadata_opener_entry_count;
static int Level_metadata_topology_num_segments;
static int Level_metadata_topology_num_walls;
static int Level_metadata_topology_num_triggers;
static int Level_metadata_topology_valid;
static int Level_metadata_opener_index_valid;

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

static int secret_area_safe_wall_count(void)
{
	return Num_walls < MAX_WALLS ? Num_walls : MAX_WALLS;
}

static int secret_area_wall_index_valid(int wall_num)
{
	return wall_num >= 0 && wall_num < secret_area_safe_wall_count();
}

static int secret_area_bounded_trigger_link_count(int trigger_num)
{
	int count;

	if (trigger_num < 0 || trigger_num >= Num_triggers)
		return 0;
	count = Triggers[trigger_num].num_links;
	if (count < 0)
		return 0;
	return count < MAX_WALLS_PER_LINK ? count : MAX_WALLS_PER_LINK;
}

static int secret_area_side_is_flyable(void *user, int seg, int side)
{
	int wall_num;

	(void) user;
	if (seg < 0 || seg >= Num_segments || side < 0 || side >= MAX_SIDES_PER_SEGMENT)
		return 0;
	wall_num = Segments[seg].sides[side].wall_num;
	if (wall_num >= 0 && !secret_area_wall_index_valid(wall_num))
		return Segments[seg].children[side] >= 0;
	return (WALL_IS_DOORWAY(&Segments[seg], side) & WID_FLY_FLAG) != 0;
}

static int secret_area_player_radius(void);

static int secret_area_compute_segment_clearance_radius(int seg, int radius)
{
	object probe;

	if (seg < 0 || seg >= Num_segments || radius <= 0)
		return 0;
	memset(&probe, 0, sizeof(probe));
	probe.pos = Level_metadata_segment_centers[seg];
	probe.segnum = seg;
	probe.size = radius;
	return object_intersects_wall(&probe) ? 1 : radius;
}

static int secret_area_position_occupiable(
    int seg, const vms_vector *position, int radius)
{
	object probe;
	vms_vector point;

	if (!position || seg < 0 || seg >= Num_segments || radius <= 0)
		return 0;
	point = *position;
	if (find_point_seg(&point, seg) != seg)
		return 0;
	memset(&probe, 0, sizeof(probe));
	probe.pos = *position;
	probe.segnum = seg;
	probe.size = radius;
	return !object_intersects_wall(&probe);
}

static int secret_area_side_clearance_radius(void *user, int seg, int side)
{
	(void) user;
	if (seg < 0 || seg >= Num_segments ||
	    seg >= LEVEL_METADATA_MAX_SEGMENTS || side < 0 ||
	    side >= MAX_SIDES_PER_SEGMENT)
		return 0;
	return Level_metadata_side_clearance[seg][side];
}

static int secret_area_player_radius(void)
{
	int objnum;
	int local_objnum = Players[Player_num].objnum;

	if (local_objnum >= 0 && local_objnum < num_objects &&
	    (Objects[local_objnum].type == OBJ_PLAYER ||
	     Objects[local_objnum].type == OBJ_GHOST) &&
	    Objects[local_objnum].size > 0)
		return Objects[local_objnum].size;
	for (objnum = 0; objnum < num_objects; ++objnum)
		if ((Objects[objnum].type == OBJ_PLAYER ||
		     Objects[objnum].type == OBJ_GHOST) &&
		    Objects[objnum].size > 0)
			return Objects[objnum].size;
	return 0;
}

static int secret_area_side_is_hard_blocked(void *user, int seg, int side)
{
#ifdef DXX_BUILD_DESCENT_II
	level_metadata_game_context *context = (level_metadata_game_context *) user;
	int objnum;
	int wall_num;
	object *objp;

	if (!context ||
	    seg < 0 || seg >= Num_segments ||
	    side < 0 || side >= MAX_SIDES_PER_SEGMENT)
		return 0;
	objnum = context->start_objnum;
	if (objnum < 0 || objnum > Highest_object_index)
		return 0;
	objp = &Objects[objnum];
	if (objp->type != OBJ_ROBOT || !Robot_info[objp->id].companion)
		return 0;
	wall_num = Segments[seg].sides[side].wall_num;
	if (!secret_area_wall_index_valid(wall_num) ||
	    !(Walls[wall_num].flags & WALL_BUDDY_PROOF))
		return 0;
	return !((WALL_IS_DOORWAY(&Segments[seg], side) & WID_FLY_FLAG) ||
	         ai_door_is_openable(objp, &Segments[seg], side));
#else
	(void) user;
	(void) seg;
	(void) side;
	return 0;
#endif
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
	return secret_area_wall_index_valid(Segments[seg].sides[side].wall_num) ? Segments[seg].sides[side].wall_num : -1;
}

static int secret_area_wall_segment(void *user, int wall_num)
{
	(void) user;
	if (!secret_area_wall_index_valid(wall_num))
		return -1;
	return Walls[wall_num].segnum;
}

static int secret_area_wall_side(void *user, int wall_num)
{
	(void) user;
	if (!secret_area_wall_index_valid(wall_num))
		return -1;
	return Walls[wall_num].sidenum;
}

static int secret_area_wall_type(void *user, int wall_num)
{
	(void) user;
	if (!secret_area_wall_index_valid(wall_num))
		return WALL_NORMAL;
	return Walls[wall_num].type;
}

static int secret_area_wall_flags(void *user, int wall_num)
{
	(void) user;
	if (!secret_area_wall_index_valid(wall_num))
		return 0;
	return Walls[wall_num].flags;
}

static int secret_area_wall_is_opening(void *user, int wall_num)
{
	(void) user;
	if (!secret_area_wall_index_valid(wall_num))
		return 0;
#ifdef DXX_BUILD_DESCENT_II
	return Walls[wall_num].state == WALL_DOOR_OPENING ||
	       Walls[wall_num].state == WALL_DOOR_CLOAKING;
#else
	return Walls[wall_num].state == WALL_DOOR_OPENING;
#endif
}

static int secret_area_wall_keys(void *user, int wall_num)
{
	(void) user;
	if (!secret_area_wall_index_valid(wall_num))
		return KEY_NONE;
	return Walls[wall_num].keys;
}

static int secret_area_wall_trigger(void *user, int wall_num)
{
	(void) user;
	if (!secret_area_wall_index_valid(wall_num))
		return -1;
	return Walls[wall_num].trigger;
}

static int secret_area_wall_clip_flags(void *user, int wall_num)
{
	int clip_num;

	(void) user;
	if (!secret_area_wall_index_valid(wall_num))
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
	if (!secret_area_wall_index_valid(wall_num))
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
	if (Level_metadata_topology_valid && seg < Level_metadata_topology_num_segments)
		center = Level_metadata_segment_centers[seg];
	else
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

static int secret_area_side_center(void *user, int seg, int side, int xyz[3])
{
	vms_vector center;

	(void) user;
	if (seg < 0 || seg >= Num_segments ||
	    side < 0 || side >= MAX_SIDES_PER_SEGMENT || !xyz)
		return 0;
	compute_center_point_on_side(&center, &Segments[seg], side);
	xyz[0] = center.x;
	xyz[1] = center.y;
	xyz[2] = center.z;
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
	if (!secret_area_wall_index_valid(wall_num))
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

static int level_metadata_fvi_segment_chain_valid(
    const fvi_info *hit_data,
    int start_seg,
    int target_seg)
{
	int index;

	if (!hit_data || hit_data->n_segs <= 0 ||
	    hit_data->seglist[0] != start_seg ||
	    (target_seg >= 0 &&
	     hit_data->seglist[hit_data->n_segs - 1] != target_seg))
		return 0;
	for (index = 0; index + 1 < hit_data->n_segs; ++index) {
		int side;

		if (hit_data->seglist[index] < 0 ||
		    hit_data->seglist[index] >= Num_segments ||
		    hit_data->seglist[index + 1] < 0 ||
		    hit_data->seglist[index + 1] >= Num_segments)
			return 0;
		for (side = 0; side < MAX_SIDES_PER_SEGMENT; ++side)
			if (Segments[hit_data->seglist[index]].children[side] ==
			    hit_data->seglist[index + 1])
				break;
		if (side == MAX_SIDES_PER_SEGMENT)
			return 0;
	}
	return 1;
}

static int level_metadata_fvi_segmented_visibility(
    const vms_vector *from,
    int start_seg,
    const vms_vector *target,
    int target_seg,
    int target_wall_seg,
    int target_wall_side,
    fix radius)
{
	fix distance;
	int chunks;
	int chunk;
	int current_seg = start_seg;
	vms_vector current;

	if (!from || !target || start_seg < 0 || start_seg >= Num_segments ||
	    target_seg < 0 || target_seg >= Num_segments)
		return 0;
	current = *from;
	{
		vms_vector distance_target = *target;
		distance = vm_vec_dist_quick(&current, &distance_target);
	}
	chunks = distance / LEVEL_METADATA_FVI_CONFIRM_SPAN + 1;
	for (chunk = 1; chunk <= chunks; ++chunk) {
		fvi_info hit_data;
		fvi_query query;
		vms_vector endpoint;
		int endpoint_seg;
		int fate;

		endpoint.x = from->x +
		             (fix) (((long long) target->x - from->x) * chunk /
		                    chunks);
		endpoint.y = from->y +
		             (fix) (((long long) target->y - from->y) * chunk /
		                    chunks);
		endpoint.z = from->z +
		             (fix) (((long long) target->z - from->z) * chunk /
		                    chunks);
		memset(&query, 0, sizeof(query));
		memset(&hit_data, 0, sizeof(hit_data));
		query.p0 = &current;
		query.p1 = &endpoint;
		query.startseg = current_seg;
		query.rad = radius;
		query.thisobjnum = -1;
		query.flags =
		    (target_wall_seg >= 0 ? FQ_TRANSPOINT : FQ_TRANSWALL) |
		    FQ_GET_SEGLIST;
		fate = find_vector_intersection(&query, &hit_data);
		if (fate == HIT_NONE)
			endpoint_seg = hit_data.hit_seg;
		else if (
		    chunk == chunks && fate == HIT_WALL &&
		    hit_data.hit_side_seg == target_wall_seg &&
		    hit_data.hit_side == target_wall_side)
			endpoint_seg = target_wall_seg;
		else
			return 0;
		if (endpoint_seg < 0 || endpoint_seg >= Num_segments ||
		    !level_metadata_fvi_segment_chain_valid(
		        &hit_data, current_seg, endpoint_seg))
			return 0;
		if (chunk == chunks)
			return endpoint_seg == target_seg;
		current = endpoint;
		current_seg = endpoint_seg;
	}
	return 0;
}

static int level_metadata_fvi_visibility_credible(
    const fvi_info *hit_data,
    const vms_vector *from,
    int start_seg,
    const vms_vector *target,
    int target_seg,
    int target_wall_seg,
    int target_wall_side,
    fix radius)
{
	if (level_metadata_fvi_segment_chain_valid(
	        hit_data, start_seg, target_seg) ||
	    level_metadata_fvi_segmented_visibility(
	        from, start_seg, target, target_seg, target_wall_seg,
	        target_wall_side, radius))
		return 1;
	return 0;
}

static int secret_area_target_visible_from_position_uncached(
    int seg,
    const int from_pos[3],
    int target_seg,
    const int target_pos[3])
{
	fvi_info hit_data;
	fvi_query query;
	vms_vector from;
	vms_vector target;

	if (seg < 0 || seg >= Num_segments || target_seg < -1 ||
	    target_seg >= Num_segments || !from_pos || !target_pos)
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
	query.flags = FQ_TRANSPOINT | FQ_GET_SEGLIST;
	if (find_vector_intersection(&query, &hit_data) != HIT_NONE)
		return 0;
	return level_metadata_fvi_visibility_credible(
	    &hit_data, &from, seg, &target, target_seg, -1, -1, 0);
}

int level_metadata_target_visible_from_position(
    int seg,
    const int from_pos[3],
    int target_seg,
    const int target_pos[3])
{
	level_metadata_visibility_key key;
	int result;

	if (seg < 0 || seg >= Num_segments || target_seg < -1 ||
	    target_seg >= Num_segments || !from_pos || !target_pos)
		return 0;
	memset(&key, 0, sizeof(key));
	key.kind = LEVEL_METADATA_VISIBILITY_TARGET_POSITION;
	key.from_seg = seg;
	memcpy(key.from_pos, from_pos, sizeof(key.from_pos));
	key.target_id = target_seg;
	memcpy(key.target_pos, target_pos, sizeof(key.target_pos));
	if (level_metadata_visibility_cache_lookup(&key, &result))
		return result;
	Level_metadata_visibility_summary.misses++;
	result = secret_area_target_visible_from_position_uncached(
	    seg, from_pos, target_seg, target_pos);
	level_metadata_visibility_cache_store(&key, result);
	return result;
}

static int secret_area_target_visible_from_segment(
    void *user,
    int seg,
    const int from_pos[3],
    int target_seg,
    const int target_pos[3])
{
	(void) user;
	return level_metadata_target_visible_from_position(
	    seg, from_pos, target_seg, target_pos);
}

int level_metadata_wall_shootable_from_position(
    int seg, const int from_pos[3], int wall_num)
{
	level_metadata_visibility_key key;
	fvi_info hit_data;
	fvi_query query;
	vms_vector from;
	vms_vector target;
	int wall_seg;
	int wall_side;
	int navigator_radius;
	fix projectile_radius;
	int fate;
	int hit_target_wall;

	Level_metadata_wall_shot_diagnostics.requests++;
	if (seg < 0 || seg >= Num_segments || !from_pos ||
	    !secret_area_wall_index_valid(wall_num)) {
		Level_metadata_wall_shot_diagnostics.invalid_inputs++;
		return 0;
	}
	wall_seg = Walls[wall_num].segnum;
	wall_side = Walls[wall_num].sidenum;
	if (wall_seg < 0 || wall_seg >= Num_segments || wall_side < 0 || wall_side >= MAX_SIDES_PER_SEGMENT) {
		Level_metadata_wall_shot_diagnostics.invalid_inputs++;
		return 0;
	}
	navigator_radius = secret_area_player_radius();
	if (navigator_radius <= 0) {
		Level_metadata_wall_shot_diagnostics.invalid_inputs++;
		return 0;
	}
	memset(&key, 0, sizeof(key));
	key.kind = LEVEL_METADATA_VISIBILITY_TARGET_WALL;
	key.from_seg = seg;
	memcpy(key.from_pos, from_pos, sizeof(key.from_pos));
	key.target_id = wall_num;
	key.clearance_radius = navigator_radius;
	if (level_metadata_visibility_cache_lookup(&key, &fate)) {
		if (fate)
			Level_metadata_wall_shot_diagnostics.cache_accepts++;
		else
			Level_metadata_wall_shot_diagnostics.cache_rejects++;
		return fate;
	}
	from.x = from_pos[0];
	from.y = from_pos[1];
	from.z = from_pos[2];
	if (!secret_area_position_occupiable(
	        seg, &from, navigator_radius)) {
		Level_metadata_wall_shot_diagnostics.unoccupiable_poses++;
		level_metadata_visibility_cache_store(&key, 0);
		return 0;
	}
	compute_center_point_on_side(&target, &Segments[wall_seg], wall_side);
	memset(&query, 0, sizeof(query));
	memset(&hit_data, 0, sizeof(hit_data));
	query.p0 = &from;
	query.p1 = &target;
	query.startseg = seg;
	projectile_radius = level_metadata_switch_projectile_radius();
	query.rad = projectile_radius;
	query.thisobjnum = -1;
	query.flags = FQ_TRANSWALL | FQ_GET_SEGLIST;
	Level_metadata_visibility_summary.misses++;
	fate = find_vector_intersection(&query, &hit_data);
	hit_target_wall =
	    fate == HIT_WALL && hit_data.hit_type == HIT_WALL &&
	    hit_data.hit_side_seg == wall_seg && hit_data.hit_side == wall_side;
	if (hit_target_wall)
		Level_metadata_wall_shot_diagnostics.target_wall_hits++;
	else if (fate == HIT_WALL)
		Level_metadata_wall_shot_diagnostics.blocked_by_other_wall++;
	else if (fate == HIT_BAD_P0)
		Level_metadata_wall_shot_diagnostics.bad_start_points++;
	else if (fate != HIT_NONE)
		Level_metadata_wall_shot_diagnostics.other_fates++;
	/*
	 * An intended-wall collision is already direct physical proof that FVI
	 * traversed from the firing pose to the switch without an earlier blocker.
	 * Do not invalidate that proof using FVI's advisory segment list, which the
	 * engine itself documents as occasionally incorrect.  A transparent/no-hit
	 * trace still needs an independently credible connected traversal.
	 */
	fate = hit_target_wall || fate == HIT_NONE;
	if (fate && !hit_target_wall &&
	    !level_metadata_fvi_visibility_credible(
	        &hit_data, &from, seg, &target, wall_seg, wall_seg, wall_side,
	        projectile_radius)) {
		int index;

		if (getenv("DXX_SECRET_AREA_DUMP_TRACE")) {
			fprintf(stderr,
			        "SECRET-AREA-DUMP TRACE rejected_disconnected_wall_ray "
			        "from_seg=%d wall=%d target_seg=%d hit_type=%d seglist=",
			        seg, wall_num, wall_seg, hit_data.hit_type);
			for (index = 0; index < hit_data.n_segs; ++index)
				fprintf(stderr, "%s%d", index ? "," : "", hit_data.seglist[index]);
			fprintf(stderr, "\n");
			fflush(stderr);
		}
		fate = 0;
		Level_metadata_wall_shot_diagnostics.transparent_disconnected++;
	} else if (fate && !hit_target_wall) {
		Level_metadata_wall_shot_diagnostics.transparent_connected++;
	}
	level_metadata_visibility_cache_store(&key, fate);
	return fate;
}

static int secret_area_wall_shootable_from_position(
    void *user, int seg, const int from_pos[3], int wall_num)
{
	(void) user;
	return level_metadata_wall_shootable_from_position(
	    seg, from_pos, wall_num);
}

static int secret_area_trigger_opens_links(int trigger_num)
{
	if (trigger_num < 0 || trigger_num >= Num_triggers)
		return 0;
#ifdef DXX_BUILD_DESCENT_II
	return Triggers[trigger_num].type == TT_OPEN_DOOR ||
	       Triggers[trigger_num].type == TT_ILLUSION_OFF ||
	       Triggers[trigger_num].type == TT_UNLOCK_DOOR ||
	       Triggers[trigger_num].type == TT_OPEN_WALL ||
	       Triggers[trigger_num].type == TT_ILLUSORY_WALL;
#else
	return (Triggers[trigger_num].flags &
	        (TRIGGER_CONTROL_DOORS | TRIGGER_ILLUSION_OFF)) != 0;
#endif
}

static int secret_area_trigger_opens_side(int trigger_num, int seg, int side)
{
	int i;

	if (!secret_area_trigger_opens_links(trigger_num))
		return 0;
	for (i = 0; i < secret_area_bounded_trigger_link_count(trigger_num); ++i)
		if (Triggers[trigger_num].seg[i] == seg && Triggers[trigger_num].side[i] == side)
			return 1;
	return 0;
}

static void secret_area_rebuild_level_topology(void)
{
	unsigned char clearance_seen[LEVEL_METADATA_MAX_SEGMENTS];
	int clearance_queue[LEVEL_METADATA_MAX_SEGMENTS];
	int segment_clearance[LEVEL_METADATA_MAX_SEGMENTS];
	short opener_last[LEVEL_METADATA_MAX_SEGMENTS][MAX_SIDES_PER_SEGMENT];
	int player_radius = secret_area_player_radius();
	int seg;
	int side;
	int trigger_num;

	Level_metadata_topology_valid = 0;
	Level_metadata_opener_index_valid = 1;
	Level_metadata_opener_entry_count = 0;
	memset(Level_metadata_opener_first, 0xff, sizeof(Level_metadata_opener_first));
	memset(opener_last, 0xff, sizeof(opener_last));
	memset(Level_metadata_side_clearance, 0,
	       sizeof(Level_metadata_side_clearance));
	memset(segment_clearance, 0, sizeof(segment_clearance));
	memset(clearance_seen, 0, sizeof(clearance_seen));
	for (seg = 0; seg < Num_segments && seg < LEVEL_METADATA_MAX_SEGMENTS; ++seg)
		compute_segment_center(&Level_metadata_segment_centers[seg], &Segments[seg]);
	for (seg = 0; seg < Num_segments && seg < LEVEL_METADATA_MAX_SEGMENTS; ++seg)
		segment_clearance[seg] =
		    secret_area_compute_segment_clearance_radius(seg, player_radius);
	/* Isolated bad centers occur in otherwise navigable skewed geometry. */
	for (seg = 0; seg < Num_segments && seg < LEVEL_METADATA_MAX_SEGMENTS; ++seg) {
		int head = 0;
		int tail = 0;
		if (clearance_seen[seg] || segment_clearance[seg] <= 0 ||
		    segment_clearance[seg] >= player_radius)
			continue;
		clearance_seen[seg] = 1;
		clearance_queue[tail++] = seg;
		while (head < tail) {
			int component_seg = clearance_queue[head++];
			for (side = 0; side < MAX_SIDES_PER_SEGMENT; ++side) {
				int child = Segments[component_seg].children[side];
				if (child < 0 || child >= Num_segments ||
				    child >= LEVEL_METADATA_MAX_SEGMENTS || clearance_seen[child] ||
				    segment_clearance[child] <= 0 ||
				    segment_clearance[child] >= player_radius)
					continue;
				clearance_seen[child] = 1;
				clearance_queue[tail++] = child;
			}
		}
		if (tail < LEVEL_METADATA_MIN_NARROW_COMPONENT_SEGMENTS)
			for (head = 0; head < tail; ++head)
				segment_clearance[clearance_queue[head]] = player_radius;
	}
	for (seg = 0; seg < Num_segments && seg < LEVEL_METADATA_MAX_SEGMENTS; ++seg) {
		for (side = 0; side < MAX_SIDES_PER_SEGMENT; ++side)
			if (Segments[seg].children[side] >= 0 &&
			    Segments[seg].children[side] < LEVEL_METADATA_MAX_SEGMENTS)
				Level_metadata_side_clearance[seg][side] =
				    segment_clearance[Segments[seg].children[side]];
	}
	for (trigger_num = 0; trigger_num < Num_triggers; ++trigger_num) {
		int link;

		if (!secret_area_trigger_opens_links(trigger_num))
			continue;
		for (link = 0; link < secret_area_bounded_trigger_link_count(trigger_num); ++link) {
			int prior_link;
			int source_wall;

			seg = Triggers[trigger_num].seg[link];
			side = Triggers[trigger_num].side[link];
			if (seg < 0 || seg >= Num_segments || seg >= LEVEL_METADATA_MAX_SEGMENTS ||
			    side < 0 || side >= MAX_SIDES_PER_SEGMENT)
				continue;
			for (prior_link = 0; prior_link < link; ++prior_link)
				if (Triggers[trigger_num].seg[prior_link] == seg &&
				    Triggers[trigger_num].side[prior_link] == side)
					break;
			if (prior_link < link)
				continue;
			for (source_wall = 0; source_wall < secret_area_safe_wall_count(); ++source_wall) {
				int entry;

				if (Walls[source_wall].trigger != trigger_num)
					continue;
				if (Level_metadata_opener_entry_count >= LEVEL_METADATA_MAX_OPENER_ENTRIES) {
					Level_metadata_opener_index_valid = 0;
					continue;
				}
				entry = Level_metadata_opener_entry_count++;
				Level_metadata_opener_entries[entry].source_wall = (short) source_wall;
				Level_metadata_opener_entries[entry].next = -1;
				if (Level_metadata_opener_first[seg][side] < 0)
					Level_metadata_opener_first[seg][side] = (short) entry;
				else
					Level_metadata_opener_entries[opener_last[seg][side]].next = (short) entry;
				opener_last[seg][side] = (short) entry;
			}
		}
	}
	Level_metadata_topology_num_segments = Num_segments;
	Level_metadata_topology_num_walls = Num_walls;
	Level_metadata_topology_num_triggers = Num_triggers;
	Level_metadata_topology_valid = Num_segments <= LEVEL_METADATA_MAX_SEGMENTS;
	if (!Level_metadata_topology_valid)
		Level_metadata_opener_index_valid = 0;
}

static void secret_area_ensure_level_topology(void)
{
	if (!Level_metadata_topology_valid ||
	    Level_metadata_topology_num_segments != Num_segments ||
	    Level_metadata_topology_num_walls != Num_walls ||
	    Level_metadata_topology_num_triggers != Num_triggers)
		secret_area_rebuild_level_topology();
}

static int secret_area_side_opener_source_wall_at(int seg, int side, int wanted_index, int allow_keyed_target)
{
	int trigger_num;
	int wall_num;
	int found = 0;

	if (seg < 0 || seg >= Num_segments || side < 0 || side >= MAX_SIDES_PER_SEGMENT)
		return -1;
	wall_num = Segments[seg].sides[side].wall_num;
	if (!secret_area_wall_index_valid(wall_num))
		return -1;
	if (!allow_keyed_target && Walls[wall_num].keys != KEY_NONE)
		return -1;
	secret_area_ensure_level_topology();
	if (Level_metadata_topology_valid && Level_metadata_opener_index_valid) {
		int entry = Level_metadata_opener_first[seg][side];

		while (entry >= 0) {
			if (found == wanted_index)
				return Level_metadata_opener_entries[entry].source_wall;
			found++;
			entry = Level_metadata_opener_entries[entry].next;
		}
		return -1;
	}
	found = 0;
	for (trigger_num = 0; trigger_num < Num_triggers; ++trigger_num) {
		int source_wall;

		if (!secret_area_trigger_opens_side(trigger_num, seg, side))
			continue;
		for (source_wall = 0; source_wall < secret_area_safe_wall_count(); ++source_wall) {
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

	if (!secret_area_wall_index_valid(source_wall))
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
	if (!secret_area_wall_index_valid(source_wall))
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
	return secret_area_bounded_trigger_link_count(trigger_num);
}

static int secret_area_trigger_link_segment(void *user, int trigger_num, int link_index)
{
	(void) user;
	if (trigger_num < 0 || trigger_num >= Num_triggers ||
	    link_index < 0 ||
	    link_index >= secret_area_bounded_trigger_link_count(trigger_num))
		return -1;
	return Triggers[trigger_num].seg[link_index];
}

static int secret_area_trigger_link_side(void *user, int trigger_num, int link_index)
{
	(void) user;
	if (trigger_num < 0 || trigger_num >= Num_triggers ||
	    link_index < 0 ||
	    link_index >= secret_area_bounded_trigger_link_count(trigger_num))
		return -1;
	return Triggers[trigger_num].side[link_index];
}

static void level_metadata_initialize_scan_view(void)
{
	level_metadata_scan_view *view = &Level_metadata_scan_view;

	if (Level_metadata_scan_view_initialized)
		return;
	memset(view, 0, sizeof(*view));
	view->user = &Level_metadata_game_context;
	view->segment_special_fuelcen = SEGMENT_IS_FUELCEN;
	view->segment_special_robotmaker = SEGMENT_IS_ROBOTMAKER;
	view->segment_special_control_center = SEGMENT_IS_CONTROLCEN;
	view->wall_type_blastable = WALL_BLASTABLE;
	view->wall_type_door = WALL_DOOR;
	view->wall_type_illusion = WALL_ILLUSION;
	view->wall_type_open = WALL_OPEN;
	view->wall_flag_door_locked = WALL_DOOR_LOCKED;
	view->wall_flag_door_opened = WALL_DOOR_OPENED;
	view->wall_key_none = KEY_NONE;
	view->wall_key_blue = KEY_BLUE;
	view->wall_key_red = KEY_RED;
	view->wall_key_gold = KEY_GOLD;
	view->wall_clip_hidden = WCF_HIDDEN;
	view->obj_type_robot = OBJ_ROBOT;
	view->obj_type_powerup = OBJ_POWERUP;
	view->obj_type_control_center = OBJ_CNTRLCEN;
	view->obj_flag_should_be_dead = OF_SHOULD_BE_DEAD;
	view->powerup_key_blue = POW_KEY_BLUE;
	view->powerup_key_red = POW_KEY_RED;
	view->powerup_key_gold = POW_KEY_GOLD;
#ifdef DXX_BUILD_DESCENT_II
	view->trigger_type_open_door = TT_OPEN_DOOR;
	view->trigger_type_exit = TT_EXIT;
	view->trigger_type_secret_exit = TT_SECRET_EXIT;
	view->trigger_type_illusion_off = TT_ILLUSION_OFF;
	view->trigger_type_unlock_door = TT_UNLOCK_DOOR;
	view->trigger_type_open_wall = TT_OPEN_WALL;
	view->trigger_type_illusory_wall = TT_ILLUSORY_WALL;
	view->trigger_flag_disabled = TF_DISABLED;
#else
	view->trigger_type_open_door = TRIGGER_CONTROL_DOORS;
	view->trigger_type_exit = TRIGGER_EXIT;
	view->trigger_type_secret_exit = TRIGGER_SECRET_EXIT;
	view->trigger_type_illusion_off = TRIGGER_ILLUSION_OFF;
	view->trigger_type_unlock_door = -2;
	view->trigger_type_open_wall = -3;
	view->trigger_type_illusory_wall = -4;
#endif
	view->segment_child = secret_area_segment_child;
	view->segment_is_explored = secret_area_segment_is_explored;
	view->reverse_side = secret_area_reverse_side;
	view->side_is_flyable = secret_area_side_is_flyable;
	view->side_clearance_radius = secret_area_side_clearance_radius;
	view->side_is_hard_blocked = secret_area_side_is_hard_blocked;
	view->side_is_control_center_link = secret_area_side_is_control_center_link;
	view->wall_num = secret_area_wall_num;
	view->wall_segment = secret_area_wall_segment;
	view->wall_side = secret_area_wall_side;
	view->wall_type = secret_area_wall_type;
	view->wall_flags = secret_area_wall_flags;
	view->wall_is_opening = secret_area_wall_is_opening;
	view->wall_keys = secret_area_wall_keys;
	view->wall_clip_flags = secret_area_wall_clip_flags;
	view->wall_trigger = secret_area_wall_trigger;
	view->segment_special = secret_area_segment_special;
	view->segment_center = secret_area_segment_center;
	view->side_center = secret_area_side_center;
	view->segment_vertex = secret_area_segment_vertex;
	view->start_position = secret_area_start_position;
	view->object_count = secret_area_object_count;
	view->object_segment = secret_area_object_segment;
	view->object_type = secret_area_object_type;
	view->object_id = secret_area_object_id;
	view->object_flags = secret_area_object_flags;
	view->object_contains_type = secret_area_object_contains_type;
	view->object_contains_id = secret_area_object_contains_id;
	view->object_contains_count = secret_area_object_contains_count;
	view->object_position = secret_area_object_position;
	view->object_is_boss = secret_area_object_is_boss;
#ifdef DXX_BUILD_DESCENT_II
	view->object_is_companion = secret_area_object_is_companion;
#endif
	view->side_has_exit_trigger = secret_area_side_has_exit_trigger;
	view->triggered_side_opener_count = secret_area_metadata_triggered_side_opener_count;
	view->triggered_side_opener_wall_num = secret_area_metadata_triggered_side_opener_wall_num;
	view->trigger_type = secret_area_trigger_type;
	view->trigger_flags = secret_area_trigger_flags;
	view->trigger_link_count = secret_area_trigger_link_count;
	view->trigger_link_segment = secret_area_trigger_link_segment;
	view->trigger_link_side = secret_area_trigger_link_side;
	view->target_visible_from_segment = secret_area_target_visible_from_segment;
	view->wall_shootable_from_position =
	    secret_area_wall_shootable_from_position;
	view->wall_is_shootable_trigger = secret_area_wall_is_shootable_trigger;
	Level_metadata_scan_view_initialized = 1;
}

static level_metadata_scan_view *level_metadata_refresh_scan_view(int start_objnum)
{
	level_metadata_scan_view *view = &Level_metadata_scan_view;
	int start_segment;

	level_metadata_initialize_scan_view();
	secret_area_ensure_level_topology();
	Level_metadata_game_context.start_objnum = start_objnum;
	view->num_segments = Num_segments;
	view->num_walls = secret_area_safe_wall_count();
	view->num_triggers = Num_triggers;
	view->start_segment = secret_area_metadata_start(&Level_metadata_game_context, &start_segment, NULL) ? start_segment : Player_init[Player_num].segnum;
	view->initial_key_mask = secret_area_current_key_mask();
	view->initial_control_center_destroyed = Control_center_destroyed != 0;
	view->navigator_radius = secret_area_player_radius();
	if (getenv("DXX_SECRET_AREA_DUMP_TRACE")) {
		int narrow_side_count = 0;
		int seg;
		int side;

		for (seg = 0; seg < Num_segments; ++seg)
			for (side = 0; side < MAX_SIDES_PER_SEGMENT; ++side)
				if (Segments[seg].children[side] >= 0 &&
				    Level_metadata_side_clearance[seg][side] > 0 &&
				    Level_metadata_side_clearance[seg][side] <
				        view->navigator_radius)
					++narrow_side_count;
		fprintf(stderr,
		        "SECRET-AREA-DUMP TRACE navigator_radius=%d narrow_sides=%d "
		        "base_laser_render=%d base_laser_radius=%d\n",
		        view->navigator_radius, narrow_side_count,
		        Weapon_info[LASER_ID_L1].render_type,
		        level_metadata_switch_projectile_radius());
	}
	view->energy_center_group_distance = secret_area_energy_center_group_distance();
	return view;
}

static int level_metadata_analysis_cache_key(
    route_analysis_cache_key *key)
{
#ifdef DXX_BUILD_DESCENT_II
	const unsigned int game = ROUTE_ANALYSIS_CACHE_GAME_D2;
#else
	const unsigned int game = ROUTE_ANALYSIS_CACHE_GAME_D1;
#endif

	return Level_metadata_canonical_snapshot_valid &&
	       route_analysis_cache_make_key(
	           DXX_ANDROID_VERSION_CODE, game,
	           &Level_metadata_canonical_snapshot, key);
}

static int level_metadata_analysis_cache_load(
    level_metadata_state *state,
    route_planner_plan_summary *summary)
{
#if defined(__ANDROID__) && DXX_ANDROID_VERSION_CODE > 0
	route_analysis_cache_key key;
	PHYSFS_File *file;
	void *record;
	PHYSFS_sint64 length;
	int valid;

	if (!level_metadata_analysis_cache_key(&key) ||
	    !route_analysis_cache_filename(
	        &key, Level_metadata_analysis_cache_summary.filename,
	        sizeof(Level_metadata_analysis_cache_summary.filename)))
		return 0;
	Level_metadata_analysis_cache_summary.build_number =
	    DXX_ANDROID_VERSION_CODE;
	Level_metadata_analysis_cache_summary.topology_hash = key.topology_hash;
	file = PHYSFS_openRead(Level_metadata_analysis_cache_summary.filename);
	if (!file) {
		Level_metadata_analysis_cache_summary.misses++;
		return 0;
	}
	length = PHYSFS_fileLength(file);
	if (length != (PHYSFS_sint64) route_analysis_cache_record_size()) {
		PHYSFS_close(file);
		Level_metadata_analysis_cache_summary.misses++;
		Level_metadata_analysis_cache_summary.rejections++;
		return 0;
	}
	record = malloc((size_t) length);
	if (!record) {
		PHYSFS_close(file);
		Level_metadata_analysis_cache_summary.misses++;
		Level_metadata_analysis_cache_summary.io_errors++;
		return 0;
	}
	valid = PHYSFS_readBytes(file, record, length) == length &&
	        PHYSFS_close(file) &&
	        route_analysis_cache_decode(
	            &key, record, (size_t) length, state, summary);
	free(record);
	if (valid) {
		Level_metadata_analysis_cache_summary.hits++;
		return 1;
	}
	Level_metadata_analysis_cache_summary.misses++;
	Level_metadata_analysis_cache_summary.rejections++;
#else
	(void) state;
	(void) summary;
#endif
	return 0;
}

static void level_metadata_analysis_cache_save(
    const level_metadata_state *state,
    const route_planner_plan_summary *summary)
{
#if defined(__ANDROID__) && DXX_ANDROID_VERSION_CODE > 0
	route_analysis_cache_key key;
	char version_dir[64];
	PHYSFS_File *file;
	void *record;
	size_t size = route_analysis_cache_record_size();
	int write_ok = 0;

	if (!level_metadata_analysis_cache_key(&key) ||
	    !route_analysis_cache_filename(
	        &key, Level_metadata_analysis_cache_summary.filename,
	        sizeof(Level_metadata_analysis_cache_summary.filename)))
		return;
	record = malloc(size);
	if (!record ||
	    !route_analysis_cache_encode(&key, state, summary, record, size)) {
		free(record);
		Level_metadata_analysis_cache_summary.io_errors++;
		return;
	}
	PHYSFS_mkdir("route-cache");
	snprintf(version_dir, sizeof(version_dir), "route-cache/%u", key.build_number);
	if (!PHYSFS_mkdir(version_dir) && !PHYSFS_exists(version_dir)) {
		free(record);
		Level_metadata_analysis_cache_summary.io_errors++;
		return;
	}
	file = PHYSFS_openWrite(Level_metadata_analysis_cache_summary.filename);
	if (file) {
		write_ok = PHYSFS_writeBytes(
		               file, record, (PHYSFS_uint64) size) ==
		           (PHYSFS_sint64) size;
		write_ok = PHYSFS_close(file) && write_ok;
	}
	if (!write_ok) {
		PHYSFS_delete(Level_metadata_analysis_cache_summary.filename);
		Level_metadata_analysis_cache_summary.io_errors++;
	} else
		Level_metadata_analysis_cache_summary.writes++;
	free(record);
#else
	(void) state;
	(void) summary;
#endif
}

static int level_metadata_cached_wall_completed(
    const level_metadata_scan_view *view,
    int wall)
{
	int segment;
	int side;
	int type;
	int flags;

	if (!view || wall < 0 || wall >= view->num_walls ||
	    !view->wall_segment || !view->wall_side || !view->wall_type ||
	    !view->wall_flags)
		return 0;
	segment = view->wall_segment(view->user, wall);
	side = view->wall_side(view->user, wall);
	type = view->wall_type(view->user, wall);
	flags = view->wall_flags(view->user, wall);
	return type == view->wall_type_open ||
	       (flags & view->wall_flag_door_opened) != 0 ||
	       (view->side_is_flyable && segment >= 0 && side >= 0 &&
	        view->side_is_flyable(view->user, segment, side));
}

static int level_metadata_cached_step_completed(
    const level_metadata_scan_view *view,
    const level_metadata_route_step *step,
    int step_index)
{
	int link;
	if (step_index >= 0 && step_index < LEVEL_METADATA_MAX_ROUTE_STEPS &&
	    Level_metadata_completed_canonical_steps[step_index])
		return 1;

	switch (step->kind) {
		case LEVEL_METADATA_ROUTE_START:
			return 1;
		case LEVEL_METADATA_ROUTE_KEY:
			return step->key_index >= 0 && step->key_index < 3 &&
			       (view->initial_key_mask & (1 << step->key_index)) != 0;
		case LEVEL_METADATA_ROUTE_REACTOR:
		case LEVEL_METADATA_ROUTE_BOSS:
			return view->initial_control_center_destroyed != 0;
		case LEVEL_METADATA_ROUTE_TRIGGER:
			if (step->opened_link_count <= 0)
				return 0;
			for (link = 0; link < step->opened_link_count; ++link)
				if (level_metadata_cached_wall_completed(
				        view, step->opened_link_wall[link]))
					return 1;
			return 0;
		case LEVEL_METADATA_ROUTE_HIDDEN_DOOR:
		case LEVEL_METADATA_ROUTE_BLASTABLE_WALL:
			return level_metadata_cached_wall_completed(view, step->wall_num);
		default:
			return 0;
	}
}

static int level_metadata_try_reuse_canonical_route(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    route_planner_plan_summary *summary)
{
	int step;
	int firing_pos[3];
	int replace_firing_pos = 0;
	const level_metadata_route_step *pending = NULL;

	if (!view || !state || !summary ||
	    !Level_metadata_canonical_plan_summary_valid ||
	    !Level_metadata_canonical_snapshot_valid ||
	    !Level_metadata_live_snapshot_valid)
		return 0;
	for (step = 0; step < Level_metadata_canonical_state.route_step_count;
	     ++step) {
		pending = &Level_metadata_canonical_state.route_steps[step];
		if (!level_metadata_cached_step_completed(view, pending, step))
			break;
		pending = NULL;
	}
	if (!pending || pending->kind == LEVEL_METADATA_ROUTE_EXIT) {
		if (!pending && !view->initial_control_center_destroyed)
			return 0;
	} else if (pending->activation_kind ==
	           LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH) {
		if (!pending->activation_pos_valid || pending->wall_num < 0 ||
		    !view->wall_shootable_from_position)
			return 0;
		memcpy(firing_pos, pending->activation_pos, sizeof(firing_pos));
		if (pending->aim_pos_valid &&
		    memcmp(firing_pos, pending->aim_pos, sizeof(firing_pos)) == 0) {
			if (!view->segment_center ||
			    !view->segment_center(view->user, pending->seg, firing_pos))
				return 0;
			replace_firing_pos = 1;
		}
		if (!view->wall_shootable_from_position(
		        view->user, pending->seg, firing_pos, pending->wall_num))
			return 0;
	}
	*state = Level_metadata_canonical_state;
	if (pending && replace_firing_pos)
		memcpy(
		    state->route_steps[step].activation_pos, firing_pos,
		    sizeof(firing_pos));
	*summary = Level_metadata_canonical_plan_summary;
	summary->first_pending_step = pending ? step : -1;
	summary->first_pending_path_segment_count = pending ? 1 : 0;
	summary->first_pending_path_terminal_segment = pending ? pending->seg : -1;
	summary->partial_frontier_segment = -1;
	return 1;
}

static void level_metadata_rescan_current_level_internal(
    int start_objnum,
    int route_target_seg,
    int route_only,
    level_metadata_unexplored_route *unexplored_result)
{
	level_metadata_scan_view *view = level_metadata_refresh_scan_view(start_objnum);

	Level_metadata_route_start_objnum = start_objnum;
	Level_metadata_route_start_seg = view->start_segment;
	if (!route_only) {
		Level_metadata_live_route_state_valid = 0;
		Level_metadata_live_plan_summary_valid = 0;
		Level_metadata_live_snapshot_valid = 0;
		memset(
		    Level_metadata_completed_canonical_steps, 0,
		    sizeof(Level_metadata_completed_canonical_steps));
		Level_metadata_canonical_snapshot_valid = route_snapshot_build_summary(
		    view,
		    &Level_metadata_canonical_snapshot,
		    NULL,
		    0);
		level_metadata_visibility_cache_sync();
		if (Level_metadata_canonical_snapshot_valid)
			level_metadata_seed_snapshot_generations(
			    &Level_metadata_canonical_snapshot);
	} else {
		route_snapshot_summary previous_snapshot;
		int previous_valid = Level_metadata_live_snapshot_valid ||
		                     Level_metadata_canonical_snapshot_valid;
		if (Level_metadata_live_snapshot_valid)
			previous_snapshot = Level_metadata_live_snapshot;
		else if (Level_metadata_canonical_snapshot_valid)
			previous_snapshot = Level_metadata_canonical_snapshot;
		Level_metadata_live_snapshot_valid = route_snapshot_build_summary(
		    view,
		    &Level_metadata_live_snapshot,
		    NULL,
		    0);
		level_metadata_visibility_cache_sync();
		if (Level_metadata_live_snapshot_valid) {
			if (previous_valid)
				level_metadata_advance_snapshot_generations(
				    &Level_metadata_live_snapshot,
				    &previous_snapshot);
			else
				level_metadata_seed_snapshot_generations(
				    &Level_metadata_live_snapshot);
		}
	}
	if (!route_only) {
		level_metadata_state shared_route;
		char problem[128];

		memset(&Level_metadata_wall_shot_diagnostics, 0,
		       sizeof(Level_metadata_wall_shot_diagnostics));
		level_metadata_scan_level_summary(view, &Level_metadata_canonical_state);
		level_metadata_state_clear(&shared_route);
		memset(&Level_metadata_canonical_plan_summary, 0,
		       sizeof(Level_metadata_canonical_plan_summary));
		Level_metadata_canonical_plan_summary.first_pending_step = -1;
		Level_metadata_canonical_plan_summary.first_pending_path_terminal_segment = -1;
		Level_metadata_canonical_plan_summary.partial_frontier_segment = -1;
		Level_metadata_canonical_plan_summary_valid =
		    level_metadata_analysis_cache_load(
		        &shared_route, &Level_metadata_canonical_plan_summary);
		if (!Level_metadata_canonical_plan_summary_valid) {
			Level_metadata_canonical_plan_summary_valid = route_planner_plan_view(
			    view,
			    ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL,
			    -1,
			    &shared_route,
			    NULL,
			    &Level_metadata_canonical_plan_summary,
			    problem,
			    sizeof(problem));
			if (Level_metadata_canonical_plan_summary_valid)
				level_metadata_analysis_cache_save(
				    &shared_route,
				    &Level_metadata_canonical_plan_summary);
		}
		if (Level_metadata_canonical_plan_summary_valid) {
			level_metadata_apply_planned_route(
			    &Level_metadata_canonical_state, &shared_route);
		} else {
			Level_metadata_canonical_state.travel_distance = 0.0;
			Level_metadata_canonical_state.travel_time_seconds = 0;
			Level_metadata_canonical_state.route_status = LEVEL_METADATA_ROUTE_FAILED;
			Level_metadata_canonical_state.route_step_count = 0;
			memset(Level_metadata_canonical_state.route_steps, 0,
			       sizeof(Level_metadata_canonical_state.route_steps));
			snprintf(Level_metadata_canonical_state.route_problem,
			         sizeof(Level_metadata_canonical_state.route_problem),
			         "shared route planner: %s",
			         problem[0] ? problem : "unknown failure");
			Level_metadata_canonical_state.route_note[0] = '\0';
		}
		level_metadata_trace_wall_shot_diagnostics();
	}
	if (route_only) {
		char problem[128];
		int endpoint_kind = ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL;

		if (unexplored_result)
			endpoint_kind = ROUTE_PLANNER_ENDPOINT_UNEXPLORED;
		else if (route_target_seg >= 0)
			endpoint_kind = ROUTE_PLANNER_ENDPOINT_SEGMENT;

		level_metadata_state_clear(&Level_metadata_live_route_state);
		memset(&Level_metadata_live_plan_summary, 0,
		       sizeof(Level_metadata_live_plan_summary));
		Level_metadata_live_plan_summary.first_pending_step = -1;
		Level_metadata_live_plan_summary.first_pending_path_terminal_segment = -1;
		Level_metadata_live_plan_summary.partial_frontier_segment = -1;
		Level_metadata_live_plan_summary_valid =
		    endpoint_kind == ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL &&
		    level_metadata_try_reuse_canonical_route(
		        view, &Level_metadata_live_route_state,
		        &Level_metadata_live_plan_summary);
		if (Level_metadata_live_plan_summary_valid)
			Level_metadata_analysis_cache_summary.live_reuses++;
		else {
			if (endpoint_kind == ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL)
				Level_metadata_analysis_cache_summary.live_fallbacks++;
			Level_metadata_live_plan_summary_valid = route_planner_plan_view(
			    view,
			    endpoint_kind,
			    route_target_seg,
			    &Level_metadata_live_route_state,
			    unexplored_result,
			    &Level_metadata_live_plan_summary,
			    problem,
			    sizeof(problem));
		}
		if (!Level_metadata_live_plan_summary_valid) {
			Level_metadata_live_route_state.route_status = LEVEL_METADATA_ROUTE_FAILED;
			snprintf(Level_metadata_live_route_state.route_problem,
			         sizeof(Level_metadata_live_route_state.route_problem),
			         "shared route planner: %s",
			         problem[0] ? problem : "unknown failure");
		}
		Level_metadata_live_route_state_valid = 1;
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

int level_metadata_get_visibility_cache_summary(
    level_metadata_visibility_cache_summary *summary)
{
	if (!summary)
		return 0;
	*summary = Level_metadata_visibility_summary;
	return 1;
}

int level_metadata_get_route_analysis_cache_summary(
    route_analysis_cache_summary *summary)
{
	if (!summary)
		return 0;
	*summary = Level_metadata_analysis_cache_summary;
	return DXX_ANDROID_VERSION_CODE > 0;
}

void level_metadata_mark_route_objective_completed(
    int kind,
    int trigger,
    int wall,
    int key_index)
{
	int index;

	for (index = 0;
	     index < Level_metadata_canonical_state.route_step_count &&
	     index < LEVEL_METADATA_MAX_ROUTE_STEPS;
	     ++index) {
		const level_metadata_route_step *step =
		    &Level_metadata_canonical_state.route_steps[index];
		if (Level_metadata_completed_canonical_steps[index] ||
		    step->kind != kind)
			continue;
		if ((kind == LEVEL_METADATA_ROUTE_TRIGGER &&
		     step->trigger_num != trigger) ||
		    ((kind == LEVEL_METADATA_ROUTE_HIDDEN_DOOR ||
		      kind == LEVEL_METADATA_ROUTE_BLASTABLE_WALL) &&
		     step->wall_num != wall) ||
		    (kind == LEVEL_METADATA_ROUTE_KEY &&
		     step->key_index != key_index))
			continue;
		Level_metadata_completed_canonical_steps[index] = 1;
		return;
	}
}

void secret_area_rescan_current_level(void)
{
	secret_area_scan_view view;
	int start_segment;

	secret_area_trace("start");
	Secret_area_reveal_unfound = 0;
	Level_metadata_objective_mode = LEVEL_METADATA_OBJECTIVES_OFF;
	Level_metadata_topology_valid = 0;
	secret_area_ensure_level_topology();
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
	return &Level_metadata_canonical_state;
}

const level_metadata_state *level_metadata_get_canonical_state(void)
{
	return &Level_metadata_canonical_state;
}

int level_metadata_get_canonical_route_plan_summary(
    route_planner_plan_summary *summary)
{
	if (!summary || !Level_metadata_canonical_plan_summary_valid)
		return 0;
	*summary = Level_metadata_canonical_plan_summary;
	return 1;
}

const level_metadata_state *level_metadata_get_live_route_state(void)
{
	return Level_metadata_live_route_state_valid ? &Level_metadata_live_route_state : NULL;
}

int level_metadata_get_live_route_plan_summary(
    route_planner_plan_summary *summary)
{
	if (!summary || !Level_metadata_live_plan_summary_valid)
		return 0;
	*summary = Level_metadata_live_plan_summary;
	return 1;
}

int level_metadata_get_canonical_route_snapshot(
    route_snapshot_summary *summary)
{
	if (!summary || !Level_metadata_canonical_snapshot_valid)
		return 0;
	*summary = Level_metadata_canonical_snapshot;
	return 1;
}

int level_metadata_get_live_route_snapshot(route_snapshot_summary *summary)
{
	if (!summary || !Level_metadata_live_snapshot_valid)
		return 0;
	*summary = Level_metadata_live_snapshot;
	return 1;
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

int level_metadata_get_objective_mode(void)
{
	return Level_metadata_objective_mode;
}

const char *level_metadata_objective_mode_name(int mode)
{
	switch (mode) {
		case LEVEL_METADATA_OBJECTIVES_ALL: return "all";
		case LEVEL_METADATA_OBJECTIVES_REMAINING: return "remaining";
		case LEVEL_METADATA_OBJECTIVES_NEXT: return "next";
		default: return "off";
	}
}

void level_metadata_set_objective_mode(int mode)
{
	if (mode < LEVEL_METADATA_OBJECTIVES_OFF ||
	    mode >= LEVEL_METADATA_OBJECTIVES_MODE_COUNT)
		mode = LEVEL_METADATA_OBJECTIVES_OFF;
	Level_metadata_objective_mode = mode;
}

void level_metadata_cycle_objective_mode(void)
{
	level_metadata_set_objective_mode(
	    (Level_metadata_objective_mode + 1) %
	    LEVEL_METADATA_OBJECTIVES_MODE_COUNT);
}
