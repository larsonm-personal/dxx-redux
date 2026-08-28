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
#include "guidebot_route_certifier.h"
#include "level_metadata_scan.h"
#include "object.h"
#include "player.h"
#include "physfsx.h"
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
#include <stdatomic.h>
#include "android_log.h"
#include "android_native_build_info.h"
#include "android_profile.h"
#include "android_route_metadata.h"
#include <unistd.h>
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
static level_metadata_state Level_metadata_live_candidate_state;
static route_planner_plan_summary Level_metadata_live_candidate_summary;
static guidebot_route_validity_certificate
    Level_metadata_live_candidate_certificate;
static route_snapshot_summary Level_metadata_canonical_snapshot;
static int Level_metadata_canonical_snapshot_valid;
static unsigned long long Level_metadata_canonical_analysis_profile_hash;
static route_snapshot_summary Level_metadata_live_snapshot;
static int Level_metadata_live_snapshot_valid;
static unsigned long long Level_metadata_progression_object_audit_hash;
static int Level_metadata_progression_object_audit_hash_valid;
static unsigned long long Level_metadata_navigation_access_audit_hash;
static int Level_metadata_navigation_access_audit_hash_valid;
static int Level_metadata_route_start_objnum = -1;
static int Level_metadata_route_start_seg = -1;
static int Level_metadata_live_route_target_seg = -1;
static int Secret_area_reveal_unfound;
static int Level_metadata_objective_mode;
static int Level_metadata_expensive_planning_allowed = 1;
static int Level_metadata_defer_guidebot_accessibility;
static int Level_metadata_route_readiness = LEVEL_METADATA_READINESS_CALCULATING;
static unsigned int Level_metadata_route_revision;
#ifdef __ANDROID__
static atomic_int Level_metadata_background_result;
static int Level_metadata_pending_cache_miss_logged;
#define LEVEL_METADATA_MAX_PUBLICATION_ADOPTION_ATTEMPTS 30
#endif
static route_analysis_cache_summary Level_metadata_analysis_cache_summary;
static guidebot_route_shadow_summary Level_metadata_route_shadow_summary;
static level_metadata_state Level_metadata_shadow_route_state;
static route_planner_plan_summary Level_metadata_shadow_plan_summary;
static guidebot_route_certifier_workspace Level_metadata_route_certifier_workspace;
static guidebot_route_certifier_workspace Level_metadata_route_frontier_workspace;
static guidebot_route_certifier_summary Level_metadata_route_certifier_summary;
static guidebot_route_validity_certificate Level_metadata_live_certificate;
static guidebot_route_decision Level_metadata_published_route_decision;
static int Level_metadata_published_route_decision_valid;
static int Level_metadata_live_route_provenance;
static int Level_metadata_live_certifier_enabled = 1;
static unsigned long long Level_metadata_route_shadow_logged_hash;
static level_metadata_live_work_summary Level_metadata_live_work_summary;
static int Level_metadata_live_route_work_pending;

#ifdef __ANDROID__
static unsigned long long level_metadata_live_work_clock_us(void *user)
{
	(void) user;
	return (unsigned long long) android_profile_monotonic_us();
}
#endif

static int level_metadata_gameplay_full_planner_allowed(void)
{
#ifdef __ANDROID__
	return 0;
#else
	return Level_metadata_expensive_planning_allowed;
#endif
}

#ifdef __ANDROID__
static void level_metadata_build_unexplored_candidate(
    const level_metadata_scan_view *view,
    const level_metadata_unexplored_route *unexplored,
    level_metadata_state *state,
    route_planner_plan_summary *summary)
{
	level_metadata_route_step *start;
	level_metadata_route_step *target;

	level_metadata_state_clear(state);
	state->route_status = LEVEL_METADATA_ROUTE_OK;
	state->route_step_count = 2;
	start = &state->route_steps[0];
	start->kind = LEVEL_METADATA_ROUTE_START;
	start->seg = view->start_segment;
	start->side = -1;
	start->wall_num = -1;
	start->trigger_num = -1;
	start->key_index = -1;
	start->key_carrier_objnum = -1;
	start->path_terminal_segment = view->start_segment;
	start->path_segment_count = 1;
	snprintf(start->label, sizeof(start->label), "%s", "Start");
	target = &state->route_steps[1];
	target->kind = LEVEL_METADATA_ROUTE_UNEXPLORED;
	target->seg = unexplored->target_seg;
	target->side = -1;
	target->wall_num = -1;
	target->trigger_num = -1;
	target->key_index = -1;
	target->key_carrier_objnum = -1;
	target->path_terminal_segment = unexplored->waypoint_seg;
	target->path_segment_count = 1;
	if (view->segment_center)
		target->activation_pos_valid = view->segment_center(
		    view->user, target->seg, target->activation_pos);
	snprintf(target->label, sizeof(target->label), "%s", "Unexplored");
	memset(summary, 0, sizeof(*summary));
	summary->endpoint_kind = ROUTE_PLANNER_ENDPOINT_UNEXPLORED;
	summary->route_step_count = state->route_step_count;
	summary->first_pending_step = 1;
	summary->first_pending_path_segment_count = 1;
	summary->first_pending_path_terminal_segment =
	    unexplored->waypoint_seg;
	summary->partial_frontier_segment = -1;
}
#endif

static void level_metadata_certify_fresh_live_plan(void)
{
	const int pending_index =
	    Level_metadata_live_plan_summary.first_pending_step;

	memset(
	    &Level_metadata_live_certificate, 0,
	    sizeof(Level_metadata_live_certificate));
	Level_metadata_live_certificate.status =
	    GUIDEBOT_ROUTE_CERTIFICATE_VALID;
	Level_metadata_live_certificate.source_trigger = -1;
	Level_metadata_live_certificate.source_wall = -1;
	Level_metadata_live_certificate.source_object = -1;
	Level_metadata_live_certificate.frontier_segment = -1;
	if (pending_index >= 0 &&
	    pending_index < Level_metadata_live_route_state.route_step_count) {
		const level_metadata_route_step *pending =
		    &Level_metadata_live_route_state.route_steps[pending_index];

		Level_metadata_live_certificate.source_trigger = pending->trigger_num;
		Level_metadata_live_certificate.source_wall = pending->wall_num;
		Level_metadata_live_certificate.source_object =
		    pending->key_carrier_objnum;
		Level_metadata_live_certificate.frontier_segment =
		    Level_metadata_live_plan_summary
		        .first_pending_path_terminal_segment;
	}
}

static void level_metadata_route_shadow_reset(void)
{
	const int enabled = Level_metadata_route_shadow_summary.enabled;

	memset(
	    &Level_metadata_route_shadow_summary, 0,
	    sizeof(Level_metadata_route_shadow_summary));
	Level_metadata_route_shadow_summary.enabled = enabled;
	guidebot_route_decision_clear(
	    &Level_metadata_route_shadow_summary.primary);
	guidebot_route_decision_clear(
	    &Level_metadata_route_shadow_summary.shadow);
	route_snapshot_clear_replay_fixture();
	Level_metadata_route_shadow_logged_hash = 0;
}

static void level_metadata_publish_live_route_decision(void)
{
	guidebot_route_decision decision;
	int valid = guidebot_route_decision_project(
	    Level_metadata_live_route_state_valid
	        ? &Level_metadata_live_route_state
	        : NULL,
	    Level_metadata_live_plan_summary_valid
	        ? &Level_metadata_live_plan_summary
	        : NULL,
	    Level_metadata_live_snapshot_valid ? &Level_metadata_live_snapshot : NULL,
	    Level_metadata_route_readiness,
	    Level_metadata_live_route_target_seg,
	    &decision);

	if (valid)
		decision.certificate = Level_metadata_live_certificate;
	if (valid != Level_metadata_published_route_decision_valid ||
	    (valid && !guidebot_route_decision_guidance_equal(
	                  &decision, &Level_metadata_published_route_decision)))
		Level_metadata_route_revision++;
	Level_metadata_published_route_decision = decision;
	Level_metadata_published_route_decision_valid = valid;
}

static int level_metadata_read_bytes(
    PHYSFS_file *file, unsigned char *buffer, PHYSFS_uint64 count)
{
	return PHYSFS_readBytes(file, buffer, count) == (PHYSFS_sint64) count;
}

static int level_metadata_skip_bytes(PHYSFS_file *file, PHYSFS_uint64 count)
{
	const PHYSFS_sint64 pos = PHYSFS_tell(file);

	return pos >= 0 && PHYSFS_seek(file, (PHYSFS_uint64) pos + count);
}

static int level_metadata_read_le16(PHYSFS_file *file, int *value)
{
	unsigned char bytes[2];

	if (!level_metadata_read_bytes(file, bytes, sizeof(bytes)))
		return 0;
	*value = bytes[0] | (bytes[1] << 8);
	return 1;
}

static int level_metadata_read_le32(PHYSFS_file *file, int *value)
{
	unsigned char bytes[4];

	if (!level_metadata_read_bytes(file, bytes, sizeof(bytes)))
		return 0;
	*value = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) |
	         (bytes[3] << 24);
	return 1;
}

static void level_metadata_read_raw_level_name(
    const char *level_file, char name[256])
{
	const int compatible_version = 22;
	const int plvl_signature = 0x504c564c;
	PHYSFS_file *file;
	int first_signature;
	int wrapper_version;
	int minedata_offset;
	int gamedata_offset;
	int signature;
	int version;
	int length = 0;
	int newline_terminated;

	name[0] = '\0';
	if (!level_file || !level_file[0] || !(file = PHYSFS_openRead(level_file)))
		return;
	if (!level_metadata_read_le32(file, &first_signature))
		goto done;
	if (first_signature == plvl_signature) {
		if (!level_metadata_read_le32(file, &wrapper_version) ||
		    !level_metadata_read_le32(file, &minedata_offset) ||
		    !level_metadata_read_le32(file, &gamedata_offset) ||
		    !PHYSFS_seek(file, (PHYSFS_uint64) gamedata_offset))
			goto done;
	} else if (!PHYSFS_seek(file, 0))
		goto done;
	if (!level_metadata_read_le16(file, &signature) || signature != 0x6705 ||
	    !level_metadata_read_le16(file, &version) ||
	    version < compatible_version || !level_metadata_skip_bytes(file, 115))
		goto done;
#ifdef DXX_BUILD_DESCENT_II
	if (version >= 29 && !level_metadata_skip_bytes(file, 24))
		goto done;
#endif
	if (version < 14)
		goto done;
	newline_terminated = version >= 31;
	while (length < 255) {
		const int c = PHYSFSX_fgetc(file);
		if (c == EOF || c == 0 ||
		    (newline_terminated && (c == '\n' || c == '\r')))
			break;
		name[length++] = (char) c;
	}
	name[length] = '\0';
done:
	PHYSFS_close(file);
}

static int level_metadata_level_name_is_usable(const char *name)
{
	int saw_text = 0;
	const unsigned char *p;

	if (!name)
		return 0;
	for (p = (const unsigned char *) name; *p; ++p) {
		if (*p < 32 || *p >= 127)
			return 0;
		if (*p != ' ')
			saw_text = 1;
	}
	return saw_text;
}

void level_metadata_choose_level_display_name(
    const char *level_file, const char *current_level_name,
    char *display_name, int display_name_capacity)
{
	char raw_name[256];
	const char *selected;
	const char *stem;
	const char *slash;
	char file_stem[256];
	char *dot;
	int raw_usable;
	int current_usable;

	if (!display_name || display_name_capacity <= 0)
		return;
	level_metadata_read_raw_level_name(level_file, raw_name);
	raw_usable = level_metadata_level_name_is_usable(raw_name);
	current_usable = level_metadata_level_name_is_usable(current_level_name);
	if (raw_usable &&
	    (!current_usable || strlen(raw_name) > strlen(current_level_name)))
		selected = raw_name;
	else if (current_usable)
		selected = current_level_name;
	else if (raw_usable)
		selected = raw_name;
	else {
		stem = level_file ? level_file : "";
		slash = strrchr(stem, '/');
		if (!slash || (strrchr(stem, '\\') && strrchr(stem, '\\') > slash))
			slash = strrchr(stem, '\\');
		if (slash)
			stem = slash + 1;
		snprintf(file_stem, sizeof(file_stem), "%s", stem);
		dot = strrchr(file_stem, '.');
		if (dot)
			*dot = '\0';
		selected = file_stem;
	}
	snprintf(display_name, (size_t) display_name_capacity, "%s", selected);
}

static void level_metadata_apply_planned_route(
    level_metadata_state *destination,
    const level_metadata_state *route)
{
	destination->travel_distance = route->travel_distance;
	destination->travel_time_seconds = route->travel_time_seconds;
	destination->route_status = route->route_status;
	snprintf(destination->route_problem, sizeof(destination->route_problem), "%s", route->route_problem);
	snprintf(destination->route_note, sizeof(destination->route_note), "%s", route->route_note);
	destination->unnecessary_key_mask = route->unnecessary_key_mask;
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
static level_metadata_progress_callback Level_metadata_progress_callback;
static void *Level_metadata_progress_user;
static level_metadata_cancel_callback Level_metadata_cancel_callback;
static void *Level_metadata_cancel_user;

void level_metadata_set_progress_callback(
    level_metadata_progress_callback callback, void *user)
{
	Level_metadata_progress_callback = callback;
	Level_metadata_progress_user = callback ? user : NULL;
}

void level_metadata_set_cancel_callback(
    level_metadata_cancel_callback callback, void *user)
{
	Level_metadata_cancel_callback = callback;
	Level_metadata_cancel_user = callback ? user : NULL;
}

static void level_metadata_report_progress(
    const char *stage, int completed, int total)
{
	if (Level_metadata_progress_callback)
		Level_metadata_progress_callback(
		    Level_metadata_progress_user, stage, completed, total);
}

#define LEVEL_METADATA_VISIBILITY_CACHE_INITIAL_CAPACITY 4096
#define LEVEL_METADATA_VISIBILITY_CACHE_MAX_CAPACITY     262144
#define LEVEL_METADATA_OCCUPIABILITY_CACHE_CAPACITY      65536
#define LEVEL_METADATA_FVI_CONFIRM_SPAN                  (64 * F1_0)
#define LEVEL_METADATA_ANALYSIS_FVI_LIMIT                1000000U

static unsigned int Level_metadata_analysis_fvi_count;
static unsigned int Level_metadata_analysis_fvi_limit =
    LEVEL_METADATA_ANALYSIS_FVI_LIMIT;
static int Level_metadata_persistent_cache_enabled = 1;
static int Level_metadata_analysis_budget_exhausted;
static int Level_metadata_analysis_cancelled;

static void level_metadata_analysis_budget_reset(void)
{
	Level_metadata_analysis_fvi_count = 0;
	Level_metadata_analysis_budget_exhausted = 0;
	Level_metadata_analysis_cancelled = 0;
}

static int level_metadata_analysis_consume_fvi(void)
{
	if (Level_metadata_cancel_callback &&
	    Level_metadata_cancel_callback(Level_metadata_cancel_user)) {
		Level_metadata_analysis_cancelled = 1;
		return 0;
	}
	if (Level_metadata_analysis_fvi_count >=
	    Level_metadata_analysis_fvi_limit) {
		Level_metadata_analysis_budget_exhausted = 1;
		return 0;
	}
	Level_metadata_analysis_fvi_count++;
	return 1;
}

void level_metadata_set_analysis_fvi_limit(unsigned int limit)
{
	Level_metadata_analysis_fvi_limit =
	    limit ? limit : LEVEL_METADATA_ANALYSIS_FVI_LIMIT;
}

unsigned int level_metadata_get_analysis_fvi_count(void)
{
	return Level_metadata_analysis_fvi_count;
}

void level_metadata_set_persistent_cache_enabled(int enabled)
{
	Level_metadata_persistent_cache_enabled = enabled != 0;
}

void level_metadata_set_defer_guidebot_accessibility(int defer)
{
	Level_metadata_defer_guidebot_accessibility = defer != 0;
}

static fix Level_metadata_switch_projectile_radius_override;

int level_metadata_get_switch_projectile_radius(void)
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

void level_metadata_set_switch_projectile_radius_override(int radius)
{
	Level_metadata_switch_projectile_radius_override = radius;
}

static fix level_metadata_switch_projectile_radius(void)
{
	return Level_metadata_switch_projectile_radius_override > 0
	           ? Level_metadata_switch_projectile_radius_override
	           : level_metadata_get_switch_projectile_radius();
}

enum level_metadata_visibility_target_kind {
	LEVEL_METADATA_VISIBILITY_TARGET_WALL = 1,
	LEVEL_METADATA_VISIBILITY_TARGET_POSITION = 2,
	LEVEL_METADATA_VISIBILITY_TARGET_WALL_STRICT = 3,
	LEVEL_METADATA_VISIBILITY_POSITION_OCCUPIABLE = 4,
	LEVEL_METADATA_VISIBILITY_TARGET_WALL_POTENTIAL = 5
};

#define LEVEL_METADATA_SWITCH_SHOT_MODEL_VERSION 2

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

#define LEVEL_METADATA_VISIBILITY_CHUNK_MAGIC 0x56495343u
#define LEVEL_METADATA_VISIBILITY_CHUNK_SIZE  256

typedef struct level_metadata_visibility_chunk {
	unsigned int magic;
	unsigned int checksum;
	route_analysis_cache_key key;
	unsigned int sequence;
	unsigned int count;
	level_metadata_visibility_entry entries[LEVEL_METADATA_VISIBILITY_CHUNK_SIZE];
} level_metadata_visibility_chunk;

static level_metadata_visibility_entry *Level_metadata_visibility_entries;
static level_metadata_visibility_entry *Level_metadata_occupiability_entries;
static int Level_metadata_visibility_count;
static level_metadata_visibility_cache_summary Level_metadata_visibility_summary;
static route_analysis_cache_key Level_metadata_visibility_checkpoint_key;
static int Level_metadata_visibility_checkpoint_key_valid;
static int Level_metadata_visibility_checkpoint_loading;
static unsigned int Level_metadata_visibility_checkpoint_sequence;
static level_metadata_visibility_chunk Level_metadata_visibility_pending_chunk;

static unsigned long long level_metadata_visibility_allocated_bytes(
    int visibility_capacity,
    int occupiability_allocated)
{
	unsigned long long entries = visibility_capacity > 0
	                                 ? (unsigned int) visibility_capacity
	                                 : 0;

	if (occupiability_allocated)
		entries += LEVEL_METADATA_OCCUPIABILITY_CACHE_CAPACITY;
	return entries * sizeof(level_metadata_visibility_entry);
}

static void level_metadata_visibility_note_memory(
    unsigned long long temporary_peak_bytes)
{
	Level_metadata_visibility_summary.allocated_bytes =
	    level_metadata_visibility_allocated_bytes(
	        Level_metadata_visibility_summary.capacity,
	        Level_metadata_occupiability_entries != NULL);
	if (temporary_peak_bytes <
	    Level_metadata_visibility_summary.allocated_bytes)
		temporary_peak_bytes =
		    Level_metadata_visibility_summary.allocated_bytes;
	if (temporary_peak_bytes >
	    Level_metadata_visibility_summary.peak_allocated_bytes)
		Level_metadata_visibility_summary.peak_allocated_bytes =
		    temporary_peak_bytes;
}

#ifdef __ANDROID__
static void level_metadata_record_live_reuse_timing(
    unsigned long long elapsed_us)
{
	unsigned int index =
	    Level_metadata_analysis_cache_summary.live_reuse_sample_next;

	if (index >= ROUTE_ANALYSIS_TIMING_SAMPLE_CAPACITY)
		index = 0;
	Level_metadata_analysis_cache_summary.live_reuse_samples[index] =
	    elapsed_us;
	Level_metadata_analysis_cache_summary.live_reuse_sample_next =
	    (index + 1) % ROUTE_ANALYSIS_TIMING_SAMPLE_CAPACITY;
	if (Level_metadata_analysis_cache_summary.live_reuse_sample_count <
	    ROUTE_ANALYSIS_TIMING_SAMPLE_CAPACITY)
		Level_metadata_analysis_cache_summary.live_reuse_sample_count++;
}
#endif

static void level_metadata_visibility_checkpoint_store(
    const level_metadata_visibility_entry *entry);

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
	unsigned long long temporary_peak_bytes;
	int index;

	if (capacity > LEVEL_METADATA_VISIBILITY_CACHE_MAX_CAPACITY)
		return 0;
	entries = (level_metadata_visibility_entry *) calloc(
	    (size_t) capacity, sizeof(*entries));
	if (!entries)
		return 0;
	temporary_peak_bytes = level_metadata_visibility_allocated_bytes(
	    previous_capacity + capacity,
	    Level_metadata_occupiability_entries != NULL);
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
	level_metadata_visibility_note_memory(temporary_peak_bytes);
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
		/* A full open-addressed table turns every absent lookup into a scan of
		 * the entire memory bound. Stop admitting entries before that cliff. */
		if (capacity >= LEVEL_METADATA_VISIBILITY_CACHE_MAX_CAPACITY ||
		    !level_metadata_visibility_cache_resize(capacity * 2)) {
			Level_metadata_visibility_summary.bypasses++;
			return;
		}
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
			level_metadata_visibility_checkpoint_store(entry);
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
	unsigned long long allocated_bytes =
	    Level_metadata_visibility_summary.allocated_bytes;
	unsigned long long peak_allocated_bytes =
	    Level_metadata_visibility_summary.peak_allocated_bytes;

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
	Level_metadata_visibility_summary.allocated_bytes = allocated_bytes;
	Level_metadata_visibility_summary.peak_allocated_bytes =
	    peak_allocated_bytes;
	Level_metadata_visibility_count = 0;
	if (Level_metadata_occupiability_entries)
		memset(
		    Level_metadata_occupiability_entries, 0,
		    LEVEL_METADATA_OCCUPIABILITY_CACHE_CAPACITY *
		        sizeof(*Level_metadata_occupiability_entries));
	Level_metadata_visibility_checkpoint_key_valid = 0;
	Level_metadata_visibility_checkpoint_sequence = 0;
	memset(
	    &Level_metadata_visibility_pending_chunk, 0,
	    sizeof(Level_metadata_visibility_pending_chunk));
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
	snapshot->actor_generation = 1;
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
	snapshot->actor_generation = previous->actor_generation;
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
	if (snapshot->actor_hash != previous->actor_hash)
		snapshot->actor_generation = level_metadata_next_generation(snapshot->actor_generation);
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

static int secret_area_position_occupiable_cached(
    int seg, const vms_vector *position, int radius)
{
	level_metadata_visibility_key key;
	level_metadata_visibility_entry *entry;
	unsigned long long hash;
	int result;

	if (!position || seg < 0 || seg >= Num_segments || radius <= 0)
		return 0;
	memset(&key, 0, sizeof(key));
	key.kind = LEVEL_METADATA_VISIBILITY_POSITION_OCCUPIABLE;
	key.from_seg = seg;
	key.from_pos[0] = position->x;
	key.from_pos[1] = position->y;
	key.from_pos[2] = position->z;
	key.clearance_radius = radius;
	/* Pose tests are cheaper and much more repetitive than collision rays. A
	 * collision here only loses a memoized result; it cannot change the answer. */
	if (!Level_metadata_occupiability_entries) {
		Level_metadata_occupiability_entries =
		    (level_metadata_visibility_entry *) calloc(
		        LEVEL_METADATA_OCCUPIABILITY_CACHE_CAPACITY,
		        sizeof(*Level_metadata_occupiability_entries));
		if (Level_metadata_occupiability_entries)
			level_metadata_visibility_note_memory(0);
	}
	if (!Level_metadata_occupiability_entries)
		return secret_area_position_occupiable(seg, position, radius);
	hash = level_metadata_visibility_hash_key(&key);
	entry = &Level_metadata_occupiability_entries[hash & (LEVEL_METADATA_OCCUPIABILITY_CACHE_CAPACITY - 1)];
	if (entry->used && entry->hash == hash &&
	    level_metadata_visibility_key_equal(&entry->key, &key))
		return entry->result != 0;
	result = secret_area_position_occupiable(seg, position, radius);
	entry->used = 1;
	entry->hash = hash;
	entry->key = key;
	entry->result = result != 0;
	return result;
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
	int model_num = Player_ship ? Player_ship->model_num : -1;

	if (model_num >= 0 && model_num < N_polygon_models &&
	    Polygon_models[model_num].rad > 0)
		return Polygon_models[model_num].rad;

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

static int secret_area_navigator_radius(int start_objnum)
{
	int objnum;

	if (start_objnum >= 0 && start_objnum < num_objects &&
	    Objects[start_objnum].size > 0)
		return Objects[start_objnum].size;
#ifdef DXX_BUILD_DESCENT_II
	for (objnum = 0; objnum < num_objects; ++objnum)
		if (Objects[objnum].type == OBJ_ROBOT &&
		    Objects[objnum].id >= 0 && Objects[objnum].id < N_robot_types &&
		    Robot_info[Objects[objnum].id].companion &&
		    Objects[objnum].size > 0)
			return Objects[objnum].size;
#else
	(void) objnum;
#endif
	return secret_area_player_radius();
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
	if (!control_center_triggers_are_valid(&ControlCenterTriggers, Highest_segment_index))
		return 0;
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

static int secret_area_object_is_fleeing(void *user, int objnum)
{
	(void) user;
	if (objnum < 0 || objnum >= num_objects)
		return 0;
	return Objects[objnum].type == OBJ_ROBOT &&
	       Objects[objnum].ctype.ai_info.behavior == AIB_RUN_FROM;
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
		if (!level_metadata_analysis_consume_fvi())
			return 0;
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
	if (!level_metadata_analysis_consume_fvi() ||
	    find_vector_intersection(&query, &hit_data) != HIT_NONE)
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

typedef struct level_metadata_route_shot_context {
	int target_wall;
	int allow_transparency;
} level_metadata_route_shot_context;

static int level_metadata_route_shot_wall_is_passable(
    void *user, int seg, int side)
{
	level_metadata_route_shot_context *context =
	    (level_metadata_route_shot_context *) user;
	int wall_num;

	if (!context || seg < 0 || seg >= Num_segments || side < 0 ||
	    side >= MAX_SIDES_PER_SEGMENT)
		return 0;
	wall_num = Segments[seg].sides[side].wall_num;
	if (!secret_area_wall_index_valid(wall_num) ||
	    wall_num == context->target_wall)
		return 0;
	if (!context->allow_transparency &&
	    (WALL_IS_DOORWAY(&Segments[seg], side) & WID_RENDPAST_FLAG))
		return 0;
	/*
	 * A firing pose remains valid when the player can clear an intervening
	 * door with one shot and fire through it with the next.  Match the engine's
	 * wall-hit rules: an unlocked, keyless door opens when a player weapon hits
	 * it.
	 */
	if (Walls[wall_num].type == WALL_DOOR &&
	    Walls[wall_num].keys == KEY_NONE &&
	    !(Walls[wall_num].flags & WALL_DOOR_LOCKED)) {
		return 1;
	}
	return 0;
}

static int level_metadata_wall_shootable_from_position_impl(
    int seg,
    const int from_pos[3],
    int wall_num,
    int allow_transparency)
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
	level_metadata_route_shot_context route_shot_context;

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
	key.kind = allow_transparency == 2
	               ? LEVEL_METADATA_VISIBILITY_TARGET_WALL_POTENTIAL
	           : allow_transparency
	               ? LEVEL_METADATA_VISIBILITY_TARGET_WALL
	               : LEVEL_METADATA_VISIBILITY_TARGET_WALL_STRICT;
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
	if (!secret_area_position_occupiable_cached(
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
	query.flags = FQ_GET_SEGLIST;
	if (allow_transparency == 2)
		query.flags |= FQ_TRANSWALL;
	else if (allow_transparency)
		query.flags |= FQ_TRANSPOINT;
	route_shot_context.target_wall = wall_num;
	route_shot_context.allow_transparency = allow_transparency;
	query.flags |= FQ_PASSABLE_WALL_CALLBACK;
	query.wall_is_passable = level_metadata_route_shot_wall_is_passable;
	query.wall_is_passable_user = &route_shot_context;
	Level_metadata_visibility_summary.misses++;
	if (!level_metadata_analysis_consume_fvi())
		return 0;
	fate = find_vector_intersection(&query, &hit_data);
	hit_target_wall =
	    fate == HIT_WALL && hit_data.hit_type == HIT_WALL &&
	    hit_data.hit_side_seg == wall_seg && hit_data.hit_side == wall_side;
	if (hit_target_wall)
		Level_metadata_wall_shot_diagnostics.target_wall_hits++;
	else if (fate == HIT_WALL) {
		if (getenv("DXX_SECRET_AREA_DUMP_TRACE")) {
			const int blocker_seg = hit_data.hit_side_seg;
			const int blocker_side = hit_data.hit_side;
			const int blocker_wall =
			    blocker_seg >= 0 && blocker_seg < Num_segments &&
			            blocker_side >= 0 && blocker_side < MAX_SIDES_PER_SEGMENT
			        ? Segments[blocker_seg].sides[blocker_side].wall_num
			        : -1;

			fprintf(stderr,
			        "SECRET-AREA-DUMP TRACE blocked_wall_ray from_seg=%d "
			        "target_wall=%d target_trigger=%d blocker_seg=%d "
			        "blocker_side=%d blocker_wall=%d",
			        seg, wall_num, Walls[wall_num].trigger, blocker_seg,
			        blocker_side, blocker_wall);
			if (blocker_wall >= 0 && blocker_wall < Num_walls)
				fprintf(stderr,
				        " blocker_type=%d blocker_state=%d blocker_flags=%d "
				        "blocker_trigger=%d keys=%d "
				        "clip=%d clip_flags=%d linked_wall=%d tmap1=%d "
				        "tmap2=%d doorway_flags=%d child=%d",
				        Walls[blocker_wall].type, Walls[blocker_wall].state,
				        Walls[blocker_wall].flags, Walls[blocker_wall].trigger,
				        Walls[blocker_wall].keys, Walls[blocker_wall].clip_num,
				        Walls[blocker_wall].clip_num >= 0 &&
				                Walls[blocker_wall].clip_num < Num_wall_anims
				            ? WallAnims[Walls[blocker_wall].clip_num].flags
				            : 0,
				        Walls[blocker_wall].linked_wall,
				        Segments[blocker_seg].sides[blocker_side].tmap_num,
				        Segments[blocker_seg].sides[blocker_side].tmap_num2,
				        WALL_IS_DOORWAY(&Segments[blocker_seg], blocker_side),
				        Segments[blocker_seg].children[blocker_side]);
			fprintf(stderr, "\n");
			fflush(stderr);
		}
		Level_metadata_wall_shot_diagnostics.blocked_by_other_wall++;
	} else if (fate == HIT_BAD_P0)
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
	fate = hit_target_wall || (allow_transparency && fate == HIT_NONE);
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

int level_metadata_wall_shootable_from_position(
    int seg, const int from_pos[3], int wall_num)
{
	return level_metadata_wall_shootable_from_position_impl(
	    seg, from_pos, wall_num, 1);
}

int level_metadata_wall_potentially_shootable_from_position(
    int seg, const int from_pos[3], int wall_num)
{
	return level_metadata_wall_shootable_from_position_impl(
	    seg, from_pos, wall_num, 2);
}

static int secret_area_wall_shootable_from_position(
    void *user, int seg, const int from_pos[3], int wall_num)
{
	(void) user;
	return level_metadata_wall_shootable_from_position(
	    seg, from_pos, wall_num);
}

static int secret_area_wall_potentially_shootable_from_position(
    void *user, int seg, const int from_pos[3], int wall_num)
{
	(void) user;
	return level_metadata_wall_potentially_shootable_from_position(
	    seg, from_pos, wall_num);
}

static int secret_area_wall_shot_incidence_cosine(
    void *user, const int from_pos[3], int wall_num)
{
	vms_vector direction;
	vms_vector from;
	vms_vector target;
	fix first;
	fix second;
	int wall_seg;
	int wall_side;

	(void) user;
	if (!from_pos || !secret_area_wall_index_valid(wall_num))
		return F1_0;
	wall_seg = Walls[wall_num].segnum;
	wall_side = Walls[wall_num].sidenum;
	if (wall_seg < 0 || wall_seg >= Num_segments || wall_side < 0 ||
	    wall_side >= MAX_SIDES_PER_SEGMENT)
		return F1_0;
	from.x = from_pos[0];
	from.y = from_pos[1];
	from.z = from_pos[2];
	compute_center_point_on_side(&target, &Segments[wall_seg], wall_side);
	if (!vm_vec_normalized_dir(&direction, &target, &from))
		return F1_0;
	first = vm_vec_dot(
	    &direction, &Segments[wall_seg].sides[wall_side].normals[0]);
	second = vm_vec_dot(
	    &direction, &Segments[wall_seg].sides[wall_side].normals[1]);
	if (first < 0)
		first = -first;
	if (second < 0)
		second = -second;
	return first > second ? first : second;
}

static int secret_area_wall_shootable_without_transparency_from_position(
    void *user, int seg, const int from_pos[3], int wall_num)
{
	(void) user;
	return level_metadata_wall_shootable_from_position_impl(
	    seg, from_pos, wall_num, 0);
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
#ifdef DXX_BUILD_DESCENT_II
	view->wall_flag_buddy_proof = WALL_BUDDY_PROOF;
#endif
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
	view->object_is_fleeing = secret_area_object_is_fleeing;
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
	view->wall_potentially_shootable_from_position =
	    secret_area_wall_potentially_shootable_from_position;
	view->wall_shootable_without_transparency_from_position =
	    secret_area_wall_shootable_without_transparency_from_position;
	view->wall_shot_incidence_cosine =
	    secret_area_wall_shot_incidence_cosine;
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
	view->navigator_radius = secret_area_navigator_radius(start_objnum);
	view->defer_guidebot_accessibility =
	    !Level_metadata_expensive_planning_allowed ||
	    Level_metadata_defer_guidebot_accessibility;
	view->progress_user = Level_metadata_progress_user;
	view->progress = Level_metadata_progress_callback;
	view->cancel_user = Level_metadata_cancel_user;
	view->cancelled = Level_metadata_cancel_callback;
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

static unsigned long long level_metadata_analysis_profile_hash(
    const level_metadata_scan_view *view)
{
	unsigned long long analysis_profile_hash = 1469598103934665603ULL;

	analysis_profile_hash = level_metadata_visibility_hash_int(
	    analysis_profile_hash, view->navigator_radius);
	analysis_profile_hash = level_metadata_visibility_hash_int(
	    analysis_profile_hash, level_metadata_switch_projectile_radius());
	analysis_profile_hash = level_metadata_visibility_hash_int(
	    analysis_profile_hash, LEVEL_METADATA_SWITCH_SHOT_MODEL_VERSION);
	return analysis_profile_hash ? analysis_profile_hash : 1;
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
	       Level_metadata_canonical_analysis_profile_hash &&
	       route_analysis_cache_make_key(
	           ROUTE_ANALYSIS_CACHE_GENERATION, game,
	           Level_metadata_canonical_analysis_profile_hash,
	           &Level_metadata_canonical_snapshot, key);
}

static unsigned int level_metadata_visibility_chunk_checksum(
    const level_metadata_visibility_chunk *chunk)
{
	level_metadata_visibility_chunk copy = *chunk;
	const unsigned char *bytes = (const unsigned char *) &copy;
	unsigned int hash = 2166136261u;
	size_t index;

	copy.checksum = 0;
	for (index = 0; index < sizeof(copy); ++index) {
		hash ^= bytes[index];
		hash *= 16777619u;
	}
	return hash;
}

static int level_metadata_visibility_chunk_filename(
    const route_analysis_cache_key *key,
    unsigned int sequence,
    char *filename,
    size_t capacity)
{
	char route_filename[192];
	int written;

	if (!route_analysis_cache_filename(
	        key, route_filename, sizeof(route_filename)))
		return 0;
	written = snprintf(
	    filename, capacity, "%s.samples-%06u", route_filename, sequence);
	return written > 0 && (size_t) written < capacity;
}

static void level_metadata_visibility_checkpoint_flush(void)
{
#ifdef __ANDROID__
	char generation_dir[64];
	char filename[224];
	char temporary_filename[256];
	PHYSFS_File *file;
	int write_ok;

	if (!Level_metadata_persistent_cache_enabled ||
	    !Level_metadata_visibility_checkpoint_key_valid ||
	    !Level_metadata_visibility_pending_chunk.count)
		return;
	Level_metadata_visibility_pending_chunk.magic =
	    LEVEL_METADATA_VISIBILITY_CHUNK_MAGIC;
	Level_metadata_visibility_pending_chunk.key =
	    Level_metadata_visibility_checkpoint_key;
	Level_metadata_visibility_pending_chunk.sequence =
	    Level_metadata_visibility_checkpoint_sequence;
	Level_metadata_visibility_pending_chunk.checksum =
	    level_metadata_visibility_chunk_checksum(
	        &Level_metadata_visibility_pending_chunk);
	if (!level_metadata_visibility_chunk_filename(
	        &Level_metadata_visibility_checkpoint_key,
	        Level_metadata_visibility_checkpoint_sequence,
	        filename, sizeof(filename)) ||
	    snprintf(
	        temporary_filename, sizeof(temporary_filename), "%s.tmp-%ld",
	        filename, (long) getpid()) >= (int) sizeof(temporary_filename))
		return;
	PHYSFS_mkdir("route-cache");
	snprintf(
	    generation_dir, sizeof(generation_dir), "route-cache/g%u",
	    Level_metadata_visibility_checkpoint_key.generation);
	if (!PHYSFS_mkdir(generation_dir) && !PHYSFS_exists(generation_dir))
		return;
	file = PHYSFS_openWrite(temporary_filename);
	write_ok = file &&
	           PHYSFS_writeBytes(
	               file, &Level_metadata_visibility_pending_chunk,
	               sizeof(Level_metadata_visibility_pending_chunk)) ==
	               (PHYSFS_sint64) sizeof(Level_metadata_visibility_pending_chunk);
	if (file)
		write_ok = PHYSFS_close(file) && write_ok;
	if (write_ok)
		write_ok = PHYSFSX_rename(temporary_filename, filename);
	if (!write_ok) {
		PHYSFS_delete(temporary_filename);
		return;
	}
	Level_metadata_visibility_checkpoint_sequence++;
	memset(
	    &Level_metadata_visibility_pending_chunk, 0,
	    sizeof(Level_metadata_visibility_pending_chunk));
#endif
}

static void level_metadata_visibility_checkpoint_discard_completed(void)
{
#ifdef __ANDROID__
	unsigned int sequence;

	if (!Level_metadata_visibility_checkpoint_key_valid ||
	    !Level_metadata_analysis_cache_summary.filename[0] ||
	    !PHYSFS_exists(Level_metadata_analysis_cache_summary.filename))
		return;
	for (sequence = 0;
	     sequence < Level_metadata_visibility_checkpoint_sequence;
	     ++sequence) {
		char filename[224];

		if (!level_metadata_visibility_chunk_filename(
		        &Level_metadata_visibility_checkpoint_key,
		        sequence, filename, sizeof(filename)))
			break;
		PHYSFS_delete(filename);
	}
	Level_metadata_visibility_checkpoint_key_valid = 0;
	Level_metadata_visibility_checkpoint_sequence = 0;
	memset(
	    &Level_metadata_visibility_pending_chunk, 0,
	    sizeof(Level_metadata_visibility_pending_chunk));
#endif
}

static void level_metadata_visibility_checkpoint_store(
    const level_metadata_visibility_entry *entry)
{
	unsigned int count;

	if (!Level_metadata_persistent_cache_enabled || !entry ||
	    !Level_metadata_visibility_checkpoint_key_valid ||
	    Level_metadata_visibility_checkpoint_loading)
		return;
	count = Level_metadata_visibility_pending_chunk.count;
	if (count >= LEVEL_METADATA_VISIBILITY_CHUNK_SIZE) {
		level_metadata_visibility_checkpoint_flush();
		count = Level_metadata_visibility_pending_chunk.count;
	}
	Level_metadata_visibility_pending_chunk.entries[count] = *entry;
	Level_metadata_visibility_pending_chunk.count = count + 1;
	if (Level_metadata_visibility_pending_chunk.count >=
	    LEVEL_METADATA_VISIBILITY_CHUNK_SIZE)
		level_metadata_visibility_checkpoint_flush();
}

static void level_metadata_visibility_checkpoint_load(void)
{
#ifdef __ANDROID__
	route_analysis_cache_key key;
	unsigned int sequence;

	if (!Level_metadata_persistent_cache_enabled ||
	    !level_metadata_analysis_cache_key(&key))
		return;
	Level_metadata_visibility_checkpoint_key = key;
	Level_metadata_visibility_checkpoint_key_valid = 1;
	Level_metadata_visibility_checkpoint_loading = 1;
	for (sequence = 0;; ++sequence) {
		char filename[224];
		PHYSFS_File *file;
		level_metadata_visibility_chunk chunk;
		unsigned int index;
		int read_ok;

		if (!level_metadata_visibility_chunk_filename(
		        &key, sequence, filename, sizeof(filename)))
			break;
		file = PHYSFS_openRead(filename);
		if (!file)
			break;
		read_ok =
		    PHYSFS_fileLength(file) == (PHYSFS_sint64) sizeof(chunk) &&
		    PHYSFS_readBytes(file, &chunk, sizeof(chunk)) ==
		        (PHYSFS_sint64) sizeof(chunk);
		read_ok = PHYSFS_close(file) && read_ok;
		if (!read_ok ||
		    chunk.magic != LEVEL_METADATA_VISIBILITY_CHUNK_MAGIC ||
		    chunk.sequence != sequence ||
		    chunk.count > LEVEL_METADATA_VISIBILITY_CHUNK_SIZE ||
		    memcmp(&chunk.key, &key, sizeof(key)) != 0 ||
		    chunk.checksum != level_metadata_visibility_chunk_checksum(&chunk))
			break;
		for (index = 0; index < chunk.count; ++index)
			if (chunk.entries[index].used &&
			    chunk.entries[index].key.kind !=
			        LEVEL_METADATA_VISIBILITY_POSITION_OCCUPIABLE)
				level_metadata_visibility_cache_store(
				    &chunk.entries[index].key,
				    chunk.entries[index].result);
	}
	Level_metadata_visibility_checkpoint_loading = 0;
	Level_metadata_visibility_checkpoint_sequence = sequence;
#endif
}

static int level_metadata_analysis_cache_load(
    level_metadata_state *state,
    route_planner_plan_summary *summary)
{
#if defined(__ANDROID__)
	route_analysis_cache_key key;
	PHYSFS_File *file;
	void *record;
	PHYSFS_sint64 length;
	int valid;

	if (!Level_metadata_persistent_cache_enabled ||
	    !level_metadata_analysis_cache_key(&key) ||
	    !route_analysis_cache_filename(
	        &key, Level_metadata_analysis_cache_summary.filename,
	        sizeof(Level_metadata_analysis_cache_summary.filename)))
		return 0;
	Level_metadata_analysis_cache_summary.generation = key.generation;
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
		if (Level_metadata_expensive_planning_allowed &&
		    state->route_status != LEVEL_METADATA_ROUTE_OK) {
			/* A partial route is useful to the live game, but the isolated
			 * analyzer must continue from the visibility checkpoints. */
			level_metadata_state_clear(state);
			memset(summary, 0, sizeof(*summary));
			summary->first_pending_step = -1;
			summary->first_pending_path_terminal_segment = -1;
			summary->partial_frontier_segment = -1;
			return 0;
		}
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

static int level_metadata_analysis_cache_save(
    const level_metadata_state *state,
    const route_planner_plan_summary *summary)
{
#if defined(__ANDROID__)
	route_analysis_cache_key key;
	char generation_dir[64];
	char temporary_filename[224];
	PHYSFS_File *file;
	void *record;
	size_t size = route_analysis_cache_record_size();
	int write_ok = 0;

	if (!Level_metadata_persistent_cache_enabled ||
	    !level_metadata_analysis_cache_key(&key) ||
	    !route_analysis_cache_filename(
	        &key, Level_metadata_analysis_cache_summary.filename,
	        sizeof(Level_metadata_analysis_cache_summary.filename)))
		return 0;
	record = malloc(size);
	if (!record ||
	    !route_analysis_cache_encode(&key, state, summary, record, size)) {
		free(record);
		Level_metadata_analysis_cache_summary.io_errors++;
		return 0;
	}
	PHYSFS_mkdir("route-cache");
	snprintf(generation_dir, sizeof(generation_dir),
	         "route-cache/g%u", key.generation);
	if (!PHYSFS_mkdir(generation_dir) && !PHYSFS_exists(generation_dir)) {
		free(record);
		Level_metadata_analysis_cache_summary.io_errors++;
		return 0;
	}
	if (snprintf(temporary_filename, sizeof(temporary_filename), "%s.tmp-%ld",
	             Level_metadata_analysis_cache_summary.filename,
	             (long) getpid()) >= (int) sizeof(temporary_filename)) {
		free(record);
		Level_metadata_analysis_cache_summary.io_errors++;
		return 0;
	}
	file = PHYSFS_openWrite(temporary_filename);
	if (file) {
		write_ok = PHYSFS_writeBytes(
		               file, record, (PHYSFS_uint64) size) ==
		           (PHYSFS_sint64) size;
		write_ok = PHYSFS_close(file) && write_ok;
	}
	if (write_ok)
		write_ok = PHYSFSX_rename(
		    temporary_filename,
		    Level_metadata_analysis_cache_summary.filename);
	if (!write_ok) {
		PHYSFS_delete(temporary_filename);
		Level_metadata_analysis_cache_summary.io_errors++;
	} else
		Level_metadata_analysis_cache_summary.writes++;
	free(record);
	return write_ok;
#else
	(void) state;
	(void) summary;
	return 0;
#endif
}

#ifdef __ANDROID__
static void level_metadata_log_unresolved_link_state(
    const level_metadata_scan_view *view,
    int step_index,
    const level_metadata_route_step *step,
    const char *role,
    int wall_num,
    int seg,
    int side)
{
	int child = -1;
	int doorway = 0;
	int flyable = 0;
	int opening = 0;
	int passable = 0;
	int type = -1;
	int flags = 0;
	int state = -1;

	if (seg >= 0 && seg < Num_segments && side >= 0 &&
	    side < MAX_SIDES_PER_SEGMENT) {
		child = Segments[seg].children[side];
		doorway = WALL_IS_DOORWAY(&Segments[seg], side);
		flyable = view->side_is_flyable &&
		          view->side_is_flyable(view->user, seg, side);
	}
	if (wall_num >= 0 && wall_num < Num_walls) {
		type = Walls[wall_num].type;
		flags = Walls[wall_num].flags;
		state = Walls[wall_num].state;
		opening = view->wall_is_opening &&
		          view->wall_is_opening(view->user, wall_num);
		passable = type == view->wall_type_open ||
		           (flags & view->wall_flag_door_opened) != 0 || flyable;
	}
	debug_log(
	    DLOG_GUIDEBOT,
	    "route_unresolved_link step=%d trigger=%d role=%s wall=%d seg=%d "
	    "side=%d child=%d type=%d state=%d flags=0x%x doorway=0x%x "
	    "flyable=%d opening=%d completion_passable=%d\n",
	    step_index, step->trigger_num, role, wall_num, seg, side, child, type,
	    state, flags, doorway, flyable, opening, passable);
}

static void level_metadata_log_unresolved_completion_evidence(
    const level_metadata_scan_view *view)
{
	static unsigned long long previous_signature;
	static int previous_level = 0;
	unsigned long long signature = 1469598103934665603ULL;
	int step_index;
	int completed_count = 0;

	for (step_index = 0;
	     step_index < Level_metadata_canonical_state.route_step_count;
	     ++step_index) {
		const level_metadata_route_step *step =
		    &Level_metadata_canonical_state.route_steps[step_index];
		int link;

		if (step->kind != LEVEL_METADATA_ROUTE_TRIGGER ||
		    step->activation_kind !=
		        LEVEL_METADATA_ROUTE_ACTIVATION_UNRESOLVED_TRIGGER ||
		    level_metadata_route_step_required_by_world_state(view, step))
			continue;
		++completed_count;
		signature = (signature ^ (unsigned int) step_index) * 1099511628211ULL;
		signature = (signature ^ (unsigned int) step->trigger_num) * 1099511628211ULL;
		for (link = -1; link < step->opened_link_count; ++link) {
			const int wall_num = link < 0 ? step->wall_num
			                              : step->opened_link_wall[link];

			if (wall_num < 0 || wall_num >= Num_walls)
				continue;
			signature = (signature ^ (unsigned int) wall_num) * 1099511628211ULL;
			signature = (signature ^ (unsigned int) Walls[wall_num].type) *
			            1099511628211ULL;
			signature = (signature ^ (unsigned int) Walls[wall_num].state) *
			            1099511628211ULL;
			signature = (signature ^ (unsigned int) Walls[wall_num].flags) *
			            1099511628211ULL;
		}
	}
	if (!completed_count)
		return;
	if (previous_level == Current_level_num && previous_signature == signature)
		return;
	previous_level = Current_level_num;
	previous_signature = signature;

	for (step_index = 0;
	     step_index < Level_metadata_canonical_state.route_step_count;
	     ++step_index) {
		const level_metadata_route_step *step =
		    &Level_metadata_canonical_state.route_steps[step_index];
		int link;
		int source_wall;
		int trigger_flags = 0;

		if (step->kind != LEVEL_METADATA_ROUTE_TRIGGER ||
		    step->activation_kind !=
		        LEVEL_METADATA_ROUTE_ACTIVATION_UNRESOLVED_TRIGGER ||
		    level_metadata_route_step_required_by_world_state(view, step))
			continue;
		if (view->trigger_flags && step->trigger_num >= 0 &&
		    step->trigger_num < view->num_triggers)
			trigger_flags =
			    view->trigger_flags(view->user, step->trigger_num);
		debug_log(
		    DLOG_GUIDEBOT,
		    "route_unresolved_complete step=%d trigger=%d trigger_flags=0x%x "
		    "locator_wall=%d links=%d\n",
		    step_index, step->trigger_num, trigger_flags, step->wall_num,
		    step->opened_link_count);
		if (step->wall_num >= 0 && step->wall_num < Num_walls)
			level_metadata_log_unresolved_link_state(
			    view, step_index, step, "locator", step->wall_num,
			    Walls[step->wall_num].segnum,
			    Walls[step->wall_num].sidenum);
		for (link = 0; link < step->opened_link_count; ++link)
			level_metadata_log_unresolved_link_state(
			    view, step_index, step, "effect",
			    step->opened_link_wall[link], step->opened_link_seg[link],
			    step->opened_link_side[link]);
		for (source_wall = 0; source_wall < Num_walls; ++source_wall) {
			int seg;
			int side;

			if (!view->wall_trigger ||
			    view->wall_trigger(view->user, source_wall) != step->trigger_num)
				continue;
			seg = Walls[source_wall].segnum;
			side = Walls[source_wall].sidenum;
			debug_log(
			    DLOG_GUIDEBOT,
			    "route_unresolved_source step=%d trigger=%d wall=%d seg=%d "
			    "side=%d type=%d state=%d flags=0x%x tmap2=%d shootable=%d\n",
			    step_index, step->trigger_num, source_wall, seg, side,
			    Walls[source_wall].type, Walls[source_wall].state,
			    Walls[source_wall].flags,
			    seg >= 0 && seg < Num_segments && side >= 0 &&
			            side < MAX_SIDES_PER_SEGMENT
			        ? Segments[seg].sides[side].tmap_num2
			        : 0,
			    view->wall_is_shootable_trigger &&
			        view->wall_is_shootable_trigger(view->user, source_wall));
		}
	}
}

static void level_metadata_log_connectivity_wall_changes(
    const guidebot_route_certifier_summary *summary)
{
	static unsigned char previous_type[MAX_WALLS];
	static unsigned char previous_state[MAX_WALLS];
	static unsigned short previous_flags[MAX_WALLS];
	static unsigned int previous_visited;
	static int previous_level;
	static int previous_wall_count = -1;
	const int comparable = previous_level == Current_level_num &&
	                       previous_wall_count == Num_walls;
	int wall_num;

	if (comparable && previous_visited != summary->visited_segments)
		for (wall_num = 0; wall_num < Num_walls && wall_num < MAX_WALLS;
		     ++wall_num) {
			int controlling_trigger = -1;
			int seg;
			int side;

			if (previous_type[wall_num] == Walls[wall_num].type &&
			    previous_state[wall_num] == Walls[wall_num].state &&
			    previous_flags[wall_num] == Walls[wall_num].flags)
				continue;
			seg = Walls[wall_num].segnum;
			side = Walls[wall_num].sidenum;
#ifdef DXX_BUILD_DESCENT_II
			controlling_trigger = Walls[wall_num].controlling_trigger;
#endif
			debug_log(
			    DLOG_GUIDEBOT,
			    "route_connectivity_wall_change visited=%u->%u wall=%d seg=%d "
			    "side=%d type=%u->%d state=%u->%d flags=0x%x->0x%x keys=%d "
			    "doorway=0x%x trigger=%d controlling_trigger=%d\n",
			    previous_visited, summary->visited_segments, wall_num, seg, side,
			    previous_type[wall_num], Walls[wall_num].type,
			    previous_state[wall_num], Walls[wall_num].state,
			    previous_flags[wall_num], Walls[wall_num].flags,
			    Walls[wall_num].keys,
			    seg >= 0 && seg < Num_segments && side >= 0 &&
			            side < MAX_SIDES_PER_SEGMENT
			        ? WALL_IS_DOORWAY(&Segments[seg], side)
			        : 0,
			    Walls[wall_num].trigger, controlling_trigger);
		}
	for (wall_num = 0; wall_num < Num_walls && wall_num < MAX_WALLS;
	     ++wall_num) {
		previous_type[wall_num] = Walls[wall_num].type;
		previous_state[wall_num] = Walls[wall_num].state;
		previous_flags[wall_num] = Walls[wall_num].flags;
	}
	previous_level = Current_level_num;
	previous_wall_count = Num_walls;
	previous_visited = summary->visited_segments;
}
#endif

static int level_metadata_try_reuse_canonical_route(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    route_planner_plan_summary *summary,
    guidebot_route_validity_certificate *certificate,
    const guidebot_route_certifier_budget *budget)
{
	int valid;

	if (!view || !state || !summary ||
	    !Level_metadata_canonical_plan_summary_valid ||
	    !Level_metadata_canonical_snapshot_valid ||
	    !Level_metadata_live_snapshot_valid)
		return 0;
	Level_metadata_analysis_cache_summary.live_certifier_attempts++;
	valid = guidebot_route_certify_current_state_budgeted(
	    view, &Level_metadata_canonical_state,
	    &Level_metadata_canonical_plan_summary,
	    &Level_metadata_route_certifier_workspace, state, summary,
	    certificate,
	    &Level_metadata_route_certifier_summary, budget);
#ifdef __ANDROID__
	debug_log(
	    DLOG_GUIDEBOT,
	    "route_certifier valid=%d prepared_first=%d selected=%d segment=%d "
	    "fallback=%d blocking=%d blocking_segment=%d blocking_reason=%d "
	    "start=%d keys=0x%x "
	    "required=0x%llx evaluated=%u rejected=%u visited=%u "
	    "firing_evaluated=%u firing_reranked=%d firing_cache_hit=%d "
	    "firing_approximate=%d firing_steep=%d firing_original=%d "
	    "firing_near=%d,%d,%d,%d,%d,%d,%d,%d\n",
	    valid, Level_metadata_canonical_plan_summary.first_pending_step,
	    Level_metadata_route_certifier_summary.selected_step,
	    Level_metadata_route_certifier_summary.selected_segment,
	    Level_metadata_route_certifier_summary.used_prepared_fallback,
	    Level_metadata_route_certifier_summary.blocking_step,
	    Level_metadata_route_certifier_summary.blocking_segment,
	    Level_metadata_route_certifier_summary.blocking_reason,
	    view->start_segment, view->initial_key_mask,
	    Level_metadata_route_certifier_summary.required_steps_low,
	    Level_metadata_route_certifier_summary.evaluated_actions,
	    Level_metadata_route_certifier_summary.rejected_actions,
	    Level_metadata_route_certifier_summary.visited_segments,
	    Level_metadata_route_certifier_summary.evaluated_firing_positions,
	    Level_metadata_route_certifier_summary.reranked_firing_position,
	    Level_metadata_route_certifier_summary.firing_cache_hit,
	    Level_metadata_route_certifier_summary.approximate_firing_position,
	    Level_metadata_route_certifier_summary.steep_firing_position,
	    Level_metadata_route_certifier_workspace.firing_search_original_segment,
	    Level_metadata_route_certifier_workspace
	        .firing_search_detailed_segments[0],
	    Level_metadata_route_certifier_workspace
	        .firing_search_detailed_segments[1],
	    Level_metadata_route_certifier_workspace
	        .firing_search_detailed_segments[2],
	    Level_metadata_route_certifier_workspace
	        .firing_search_detailed_segments[3],
	    Level_metadata_route_certifier_workspace
	        .firing_search_detailed_segments[4],
	    Level_metadata_route_certifier_workspace
	        .firing_search_detailed_segments[5],
	    Level_metadata_route_certifier_workspace
	        .firing_search_detailed_segments[6],
	    Level_metadata_route_certifier_workspace
	        .firing_search_detailed_segments[7]);
	level_metadata_log_connectivity_wall_changes(
	    &Level_metadata_route_certifier_summary);
	level_metadata_log_unresolved_completion_evidence(view);
#endif
	if (valid == GUIDEBOT_ROUTE_CERTIFIER_VALID) {
		Level_metadata_analysis_cache_summary.live_certifier_successes++;
		if (Level_metadata_route_certifier_summary.used_prepared_fallback)
			Level_metadata_analysis_cache_summary
			    .live_certifier_prepared_fallbacks++;
	} else if (valid == GUIDEBOT_ROUTE_CERTIFIER_INVALID)
		Level_metadata_analysis_cache_summary.live_certifier_failures++;
	if (Level_metadata_route_certifier_summary.visited_segments >
	    Level_metadata_analysis_cache_summary
	        .live_certifier_max_visited_segments)
		Level_metadata_analysis_cache_summary
		    .live_certifier_max_visited_segments =
		    Level_metadata_route_certifier_summary.visited_segments;
	if (Level_metadata_route_certifier_summary.evaluated_edges >
	    Level_metadata_analysis_cache_summary
	        .live_certifier_max_evaluated_edges)
		Level_metadata_analysis_cache_summary
		    .live_certifier_max_evaluated_edges =
		    Level_metadata_route_certifier_summary.evaluated_edges;
	if (Level_metadata_route_certifier_summary.evaluated_actions >
	    Level_metadata_analysis_cache_summary
	        .live_certifier_max_evaluated_actions)
		Level_metadata_analysis_cache_summary
		    .live_certifier_max_evaluated_actions =
		    Level_metadata_route_certifier_summary.evaluated_actions;
	return valid;
}

static void level_metadata_run_route_shadow(
    const level_metadata_scan_view *view,
    int endpoint_kind,
    int route_target_seg)
{
	guidebot_route_decision primary;
	guidebot_route_decision shadow;
	char problem[128];
	int shadow_valid;
	unsigned int hints = 0;
	unsigned int shadow_fvi_count;
	unsigned long long mismatch_hash;
	const unsigned int saved_fvi_count = Level_metadata_analysis_fvi_count;
	const int saved_budget_exhausted =
	    Level_metadata_analysis_budget_exhausted;
	const int saved_cancelled = Level_metadata_analysis_cancelled;
#ifdef __ANDROID__
	const long long started_us = android_profile_monotonic_us();
	unsigned long long elapsed_us;
#else
	const unsigned long long elapsed_us = 0;
#endif

	if (!Level_metadata_route_shadow_summary.enabled || !view ||
	    endpoint_kind != ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL ||
	    !Level_metadata_live_snapshot_valid)
		return;
#ifdef __ANDROID__
	Level_metadata_live_work_summary.blocked_full_plan_calls++;
	debug_log(
	    DLOG_GUIDEBOT,
	    "route_work_deferred reason=shadow_full_plan stage=shadow "
	    "elapsed_us=0 incumbent=%d",
	    Level_metadata_live_route_state_valid);
	return;
#endif
	level_metadata_state_clear(&Level_metadata_shadow_route_state);
	memset(
	    &Level_metadata_shadow_plan_summary, 0,
	    sizeof(Level_metadata_shadow_plan_summary));
	Level_metadata_shadow_plan_summary.first_pending_step = -1;
	Level_metadata_shadow_plan_summary.first_pending_path_terminal_segment = -1;
	Level_metadata_shadow_plan_summary.partial_frontier_segment = -1;
	problem[0] = '\0';
	level_metadata_analysis_budget_reset();
	shadow_valid = route_planner_plan_view(
	    view,
	    endpoint_kind,
	    route_target_seg,
	    &Level_metadata_shadow_route_state,
	    NULL,
	    &Level_metadata_shadow_plan_summary,
	    problem,
	    sizeof(problem));
	if (Level_metadata_analysis_cancelled ||
	    (Level_metadata_analysis_budget_exhausted && !shadow_valid))
		shadow_valid = 0;
	shadow_fvi_count = Level_metadata_analysis_fvi_count;
#ifdef __ANDROID__
	elapsed_us = (unsigned long long) (android_profile_monotonic_us() -
	                                   started_us);
#endif
	Level_metadata_analysis_fvi_count = saved_fvi_count;
	Level_metadata_analysis_budget_exhausted = saved_budget_exhausted;
	Level_metadata_analysis_cancelled = saved_cancelled;
	guidebot_route_decision_project(
	    Level_metadata_live_route_state_valid
	        ? &Level_metadata_live_route_state
	        : NULL,
	    Level_metadata_live_plan_summary_valid
	        ? &Level_metadata_live_plan_summary
	        : NULL,
	    &Level_metadata_live_snapshot,
	    Level_metadata_route_readiness,
	    route_target_seg,
	    &primary);
	guidebot_route_decision_project(
	    shadow_valid ? &Level_metadata_shadow_route_state : NULL,
	    shadow_valid ? &Level_metadata_shadow_plan_summary : NULL,
	    &Level_metadata_live_snapshot,
	    shadow_valid ? Level_metadata_route_readiness
	                 : LEVEL_METADATA_READINESS_FAILED,
	    route_target_seg,
	    &shadow);
	if (Level_metadata_route_readiness ==
	    LEVEL_METADATA_READINESS_CALCULATING)
		hints |= GUIDEBOT_ROUTE_SHADOW_HINT_CACHE_READINESS;
	if (Level_metadata_canonical_snapshot_valid &&
	    Level_metadata_canonical_snapshot.navigation_hash !=
	        Level_metadata_live_snapshot.navigation_hash)
		hints |= GUIDEBOT_ROUTE_SHADOW_HINT_REVERSIBLE_WALL_STATE;
	guidebot_route_shadow_record(
	    &Level_metadata_route_shadow_summary,
	    Level_metadata_live_route_state_valid &&
	        Level_metadata_live_plan_summary_valid,
	    &primary,
	    shadow_valid,
	    &shadow,
	    shadow_fvi_count,
	    elapsed_us,
	    &Level_metadata_live_snapshot,
#ifdef DXX_BUILD_DESCENT_II
	    ROUTE_ANALYSIS_CACHE_GAME_D2,
#else
	    ROUTE_ANALYSIS_CACHE_GAME_D1,
#endif
	    Current_level_num,
	    hints);
	if (Level_metadata_route_shadow_summary.mismatch_kind !=
	    GUIDEBOT_ROUTE_SHADOW_MATCH)
		route_snapshot_capture_replay_fixture(view);
	mismatch_hash = primary.decision_hash ^
	                (shadow.decision_hash << 1) ^
	                (unsigned long long) (unsigned int) primary.path_terminal_segment << 17 ^
	                (unsigned long long) (unsigned int) shadow.path_terminal_segment << 33 ^
	                (unsigned long long)
	                    Level_metadata_route_shadow_summary.fixture.reason_kind;
#ifdef __ANDROID__
	if (Level_metadata_route_shadow_summary.mismatch_kind !=
	        GUIDEBOT_ROUTE_SHADOW_MATCH &&
	    mismatch_hash != Level_metadata_route_shadow_logged_hash) {
		Level_metadata_route_shadow_logged_hash = mismatch_hash;
		debug_log(
		    DLOG_GUIDEBOT,
		    "route_shadow_mismatch reason=%s level=%d primary=%llu shadow=%llu "
		    "primary_terminal=%d shadow_terminal=%d topology=%llu state=%llu "
		    "fvi=%u us=%llu\n",
		    guidebot_route_shadow_reason_name(
		        Level_metadata_route_shadow_summary.fixture.reason_kind),
		    Current_level_num,
		    primary.decision_hash,
		    shadow.decision_hash,
		    primary.path_terminal_segment,
		    shadow.path_terminal_segment,
		    Level_metadata_live_snapshot.topology_hash,
		    Level_metadata_live_snapshot.state_hash,
		    shadow_fvi_count,
		    elapsed_us);
	}
#else
	(void) mismatch_hash;
#endif
}

static void level_metadata_rescan_current_level_internal(
    int start_objnum,
    int route_target_seg,
    int route_only,
    level_metadata_unexplored_route *unexplored_result)
{
#ifdef __ANDROID__
	const long long rescan_started_us = android_profile_monotonic_us();
#endif
	const int continuing_live_work =
	    route_only && Level_metadata_live_route_work_pending;
	const int lightweight_unexplored_work =
	    route_only && unexplored_result &&
	    (Level_metadata_live_snapshot_valid ||
	     Level_metadata_canonical_snapshot_valid);
	level_metadata_scan_view *view = level_metadata_refresh_scan_view(start_objnum);
#ifdef __ANDROID__
	const long long refresh_finished_us = android_profile_monotonic_us();
	long long snapshot_finished_us = refresh_finished_us;
	long long visibility_finished_us = refresh_finished_us;
	long long summary_finished_us = refresh_finished_us;
	long long planning_finished_us = refresh_finished_us;
#endif

	Level_metadata_route_start_objnum = start_objnum;
	Level_metadata_route_start_seg = view->start_segment;
	Level_metadata_live_route_target_seg = route_target_seg;
	if (!route_only) {
		level_metadata_route_shadow_reset();
		memset(
		    &Level_metadata_live_work_summary, 0,
		    sizeof(Level_metadata_live_work_summary));
		memset(
		    &Level_metadata_route_certifier_workspace, 0,
		    sizeof(Level_metadata_route_certifier_workspace));
		memset(
		    &Level_metadata_route_frontier_workspace, 0,
		    sizeof(Level_metadata_route_frontier_workspace));
		level_metadata_invalidate_live_route_work();
		level_metadata_report_progress("level_topology", 0, 1);
		Level_metadata_canonical_analysis_profile_hash =
		    level_metadata_analysis_profile_hash(view);
		Level_metadata_live_route_state_valid = 0;
		Level_metadata_live_plan_summary_valid = 0;
		Level_metadata_live_route_provenance =
		    LEVEL_METADATA_ROUTE_PROVENANCE_NONE;
		Level_metadata_live_snapshot_valid = 0;
		Level_metadata_published_route_decision_valid = 0;
		guidebot_route_decision_clear(
		    &Level_metadata_published_route_decision);
		Level_metadata_canonical_snapshot_valid = route_snapshot_build_summary(
		    view,
		    &Level_metadata_canonical_snapshot,
		    NULL,
		    0);
#ifdef __ANDROID__
		snapshot_finished_us = android_profile_monotonic_us();
#endif
		level_metadata_visibility_cache_sync();
#ifdef __ANDROID__
		visibility_finished_us = android_profile_monotonic_us();
#endif
		if (Level_metadata_canonical_snapshot_valid)
			level_metadata_seed_snapshot_generations(
			    &Level_metadata_canonical_snapshot);
		level_metadata_report_progress("level_topology", 1, 1);
	} else if (!continuing_live_work && !lightweight_unexplored_work) {
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
	if (!continuing_live_work && !lightweight_unexplored_work) {
		Level_metadata_progression_object_audit_hash_valid =
		    route_snapshot_build_domain_hash(
		        view,
		        ROUTE_SNAPSHOT_DOMAIN_PROGRESSION_OBJECTS,
		        &Level_metadata_progression_object_audit_hash,
		        NULL);
		Level_metadata_navigation_access_audit_hash_valid =
		    route_snapshot_build_domain_hash(
		        view,
		        ROUTE_SNAPSHOT_DOMAIN_NAVIGATION_ACCESS,
		        &Level_metadata_navigation_access_audit_hash,
		        NULL);
	}
	if (!route_only) {
		level_metadata_state shared_route;
		char problem[128];
		int route_cache_saved = 0;

		memset(&Level_metadata_wall_shot_diagnostics, 0,
		       sizeof(Level_metadata_wall_shot_diagnostics));
		level_metadata_report_progress("level_summary", 0, 1);
		level_metadata_scan_level_summary(view, &Level_metadata_canonical_state);
#ifdef __ANDROID__
		summary_finished_us = android_profile_monotonic_us();
#endif
		level_metadata_report_progress("level_summary", 1, 1);
		level_metadata_state_clear(&shared_route);
		memset(&Level_metadata_canonical_plan_summary, 0,
		       sizeof(Level_metadata_canonical_plan_summary));
		Level_metadata_canonical_plan_summary.first_pending_step = -1;
		Level_metadata_canonical_plan_summary.first_pending_path_terminal_segment = -1;
		Level_metadata_canonical_plan_summary.partial_frontier_segment = -1;
		level_metadata_report_progress("route_planning", 0, 1);
		level_metadata_analysis_budget_reset();
		Level_metadata_canonical_plan_summary_valid =
		    level_metadata_analysis_cache_load(
		        &shared_route, &Level_metadata_canonical_plan_summary);
		if (!Level_metadata_canonical_plan_summary_valid &&
		    Level_metadata_expensive_planning_allowed) {
			level_metadata_visibility_checkpoint_load();
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
				route_cache_saved = level_metadata_analysis_cache_save(
				    &shared_route,
				    &Level_metadata_canonical_plan_summary);
		}
		if (Level_metadata_analysis_cancelled ||
		    (Level_metadata_analysis_budget_exhausted &&
		     !Level_metadata_canonical_plan_summary_valid)) {
			Level_metadata_canonical_plan_summary_valid = 0;
			snprintf(
			    problem, sizeof(problem), "%s",
			    Level_metadata_analysis_cancelled
			        ? "metadata analysis cancelled"
			        : "metadata analysis exceeded its collision-work budget");
		}
		if (Level_metadata_canonical_plan_summary_valid) {
			level_metadata_apply_planned_route(
			    &Level_metadata_canonical_state, &shared_route);
			Level_metadata_expensive_planning_allowed = 1;
			Level_metadata_route_readiness =
			    shared_route.route_status == LEVEL_METADATA_ROUTE_OK ? LEVEL_METADATA_READINESS_COMPLETE : Level_metadata_canonical_plan_summary.first_pending_step >= 0     ? LEVEL_METADATA_READINESS_NEXT_READY
			                                                                                           : Level_metadata_canonical_plan_summary.partial_frontier_segment >= 0 ? LEVEL_METADATA_READINESS_PARTIAL
			                                                                                                                                                                 : LEVEL_METADATA_READINESS_FAILED;
		} else {
			Level_metadata_canonical_state.travel_distance = 0.0;
			Level_metadata_canonical_state.travel_time_seconds = 0;
			Level_metadata_canonical_state.route_status = LEVEL_METADATA_ROUTE_FAILED;
			Level_metadata_canonical_state.route_step_count = 0;
			memset(Level_metadata_canonical_state.route_steps, 0,
			       sizeof(Level_metadata_canonical_state.route_steps));
			snprintf(Level_metadata_canonical_state.route_problem,
			         sizeof(Level_metadata_canonical_state.route_problem),
			         "%s",
			         Level_metadata_expensive_planning_allowed ? (problem[0] ? problem : "shared route planning failed") : "route metadata still calculating");
			Level_metadata_canonical_state.route_note[0] = '\0';
			Level_metadata_route_readiness =
			    Level_metadata_expensive_planning_allowed ? LEVEL_METADATA_READINESS_FAILED : LEVEL_METADATA_READINESS_CALCULATING;
		}
		if (Level_metadata_canonical_plan_summary_valid)
			Level_metadata_route_revision++;
		level_metadata_report_progress("route_planning", 1, 1);
		level_metadata_visibility_checkpoint_flush();
		if (route_cache_saved &&
		    Level_metadata_canonical_plan_summary_valid &&
		    shared_route.route_status == LEVEL_METADATA_ROUTE_OK)
			level_metadata_visibility_checkpoint_discard_completed();
		level_metadata_trace_wall_shot_diagnostics();
#ifdef __ANDROID__
		planning_finished_us = android_profile_monotonic_us();
		debug_log(
		    DLOG_PROFILING,
		    "route_metadata_phases level=%d mode=%s refresh_us=%lld "
		    "snapshot_us=%lld visibility_sync_us=%lld summary_us=%lld "
		    "planning_us=%lld",
		    Current_level_num,
		    Level_metadata_expensive_planning_allowed ? "analyze" : "prepare",
		    refresh_finished_us - rescan_started_us,
		    snapshot_finished_us - refresh_finished_us,
		    visibility_finished_us - snapshot_finished_us,
		    summary_finished_us - visibility_finished_us,
		    planning_finished_us - summary_finished_us);
#endif
	}
	if (route_only) {
		char problem[128];
		int endpoint_kind = ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL;
		const int incumbent_valid =
		    Level_metadata_live_route_state_valid &&
		    Level_metadata_live_plan_summary_valid;
		int candidate_provenance = LEVEL_METADATA_ROUTE_PROVENANCE_NONE;
		int candidate_valid = 0;
		int certifier_pending = 0;
		int deferred = 0;
#ifdef __ANDROID__
		guidebot_route_certifier_budget certifier_budget;
		long long live_stage_started_us;
		unsigned long long live_stage_elapsed_us;
		unsigned long long refresh_elapsed_us;
#endif
		problem[0] = '\0';

		if (unexplored_result)
			endpoint_kind = ROUTE_PLANNER_ENDPOINT_UNEXPLORED;
		else if (route_target_seg >= 0)
			endpoint_kind = ROUTE_PLANNER_ENDPOINT_SEGMENT;

		level_metadata_state_clear(&Level_metadata_live_candidate_state);
		memset(&Level_metadata_live_candidate_summary, 0,
		       sizeof(Level_metadata_live_candidate_summary));
		Level_metadata_live_candidate_summary.first_pending_step = -1;
		Level_metadata_live_candidate_summary
		    .first_pending_path_terminal_segment = -1;
		Level_metadata_live_candidate_summary.partial_frontier_segment = -1;
		level_metadata_analysis_budget_reset();
		memset(
		    &Level_metadata_live_candidate_certificate, 0,
		    sizeof(Level_metadata_live_candidate_certificate));
		Level_metadata_live_candidate_certificate.status =
		    GUIDEBOT_ROUTE_CERTIFICATE_UNCHECKED;
		Level_metadata_live_candidate_certificate.source_trigger = -1;
		Level_metadata_live_candidate_certificate.source_wall = -1;
		Level_metadata_live_candidate_certificate.source_object = -1;
		Level_metadata_live_candidate_certificate.frontier_segment = -1;
		if (!Level_metadata_live_route_work_pending)
			guidebot_route_certifier_reset_job(
			    &Level_metadata_route_certifier_workspace);
#ifdef __ANDROID__
		live_stage_started_us = android_profile_monotonic_us();
		certifier_budget.clock_us = level_metadata_live_work_clock_us;
		certifier_budget.clock_user = NULL;
		certifier_budget.deadline_us =
		    (unsigned long long) live_stage_started_us + 2000ULL;
		certifier_budget.work_limit = 512;
		if (endpoint_kind == ROUTE_PLANNER_ENDPOINT_UNEXPLORED) {
			const int endpoint_result =
			    guidebot_route_find_unexplored_budgeted(
			        view, &Level_metadata_route_certifier_workspace,
			        unexplored_result, &certifier_budget);

			candidate_valid =
			    endpoint_result == GUIDEBOT_ROUTE_CERTIFIER_VALID;
			certifier_pending =
			    endpoint_result == GUIDEBOT_ROUTE_CERTIFIER_PENDING;
			Level_metadata_live_route_work_pending = certifier_pending;
			if (candidate_valid) {
				level_metadata_build_unexplored_candidate(
				    view, unexplored_result,
				    &Level_metadata_live_candidate_state,
				    &Level_metadata_live_candidate_summary);
				if (!Level_metadata_live_snapshot_valid &&
				    Level_metadata_canonical_snapshot_valid) {
					Level_metadata_live_snapshot =
					    Level_metadata_canonical_snapshot;
					Level_metadata_live_snapshot_valid = 1;
				}
				if (Level_metadata_live_snapshot_valid)
					route_snapshot_build_domain_hash(
					    view, ROUTE_SNAPSHOT_DOMAIN_AUTOMAP,
					    &Level_metadata_live_snapshot.automap_hash,
					    NULL);
				Level_metadata_live_candidate_certificate.status =
				    GUIDEBOT_ROUTE_CERTIFICATE_VALID;
				Level_metadata_live_candidate_certificate.frontier_segment =
				    unexplored_result->waypoint_seg;
				candidate_provenance =
				    LEVEL_METADATA_ROUTE_PROVENANCE_CERTIFIER;
			}
		}
#endif
		if (endpoint_kind == ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL &&
		    Level_metadata_live_certifier_enabled) {
			int certifier_result;

			Level_metadata_analysis_cache_summary.live_reuse_attempts++;
#ifdef __ANDROID__
			live_stage_started_us = android_profile_monotonic_us();
			certifier_budget.deadline_us =
			    (unsigned long long) live_stage_started_us + 2000ULL;
#endif
			certifier_result = level_metadata_try_reuse_canonical_route(
			    view, &Level_metadata_live_candidate_state,
			    &Level_metadata_live_candidate_summary,
			    &Level_metadata_live_candidate_certificate,
#ifdef __ANDROID__
			    &certifier_budget
#else
			    NULL
#endif
			);
			candidate_valid =
			    certifier_result == GUIDEBOT_ROUTE_CERTIFIER_VALID;
			certifier_pending =
			    certifier_result == GUIDEBOT_ROUTE_CERTIFIER_PENDING;
			Level_metadata_live_route_work_pending = certifier_pending;
			if (candidate_valid)
				candidate_provenance =
				    Level_metadata_route_certifier_summary
				            .used_prepared_fallback
				        ? LEVEL_METADATA_ROUTE_PROVENANCE_PREPARED_FALLBACK
				        : LEVEL_METADATA_ROUTE_PROVENANCE_CERTIFIER;
#ifdef __ANDROID__
			live_stage_elapsed_us = (unsigned long long) (android_profile_monotonic_us() - live_stage_started_us);
			Level_metadata_analysis_cache_summary.live_reuse_total_us +=
			    live_stage_elapsed_us;
			level_metadata_record_live_reuse_timing(live_stage_elapsed_us);
			if (live_stage_elapsed_us >
			    Level_metadata_analysis_cache_summary.live_reuse_max_us)
				Level_metadata_analysis_cache_summary.live_reuse_max_us =
				    live_stage_elapsed_us;
#endif
		}
		if (candidate_valid)
			Level_metadata_analysis_cache_summary.live_reuses++;
		else if (!certifier_pending &&
		         Level_metadata_expensive_planning_allowed &&
		         level_metadata_gameplay_full_planner_allowed()) {
			if (endpoint_kind == ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL)
				Level_metadata_analysis_cache_summary.live_fallbacks++;
			Level_metadata_live_work_summary.full_plan_calls++;
#ifdef __ANDROID__
			live_stage_started_us = android_profile_monotonic_us();
#endif
			candidate_valid = route_planner_plan_view(
			    view,
			    endpoint_kind,
			    route_target_seg,
			    &Level_metadata_live_candidate_state,
			    unexplored_result,
			    &Level_metadata_live_candidate_summary,
			    problem,
			    sizeof(problem));
			if (candidate_valid) {
				Level_metadata_live_candidate_certificate.status =
				    GUIDEBOT_ROUTE_CERTIFICATE_VALID;
				candidate_provenance =
				    LEVEL_METADATA_ROUTE_PROVENANCE_FULL_PLANNER;
			}
#ifdef __ANDROID__
			if (candidate_valid &&
			    Level_metadata_live_candidate_summary.first_pending_step >= 0 &&
			    Level_metadata_live_candidate_summary.first_pending_step <
			        Level_metadata_live_candidate_state.route_step_count) {
				const level_metadata_route_step *pending =
				    &Level_metadata_live_candidate_state.route_steps
				         [Level_metadata_live_candidate_summary.first_pending_step];

				debug_log(
				    DLOG_GUIDEBOT,
				    "route_full_planner step=%d kind=%d activation=%d seg=%d "
				    "terminal=%d wall=%d trigger=%d key=%d label=%s\n",
				    Level_metadata_live_candidate_summary.first_pending_step,
				    pending->kind, pending->activation_kind, pending->seg,
				    Level_metadata_live_candidate_summary
				        .first_pending_path_terminal_segment,
				    pending->wall_num, pending->trigger_num, pending->key_index,
				    pending->label);
			}
#endif
#ifdef __ANDROID__
			if (endpoint_kind == ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL) {
				live_stage_elapsed_us = (unsigned long long) (android_profile_monotonic_us() - live_stage_started_us);
				Level_metadata_analysis_cache_summary.live_fallback_total_us +=
				    live_stage_elapsed_us;
				if (live_stage_elapsed_us >
				    Level_metadata_analysis_cache_summary.live_fallback_max_us)
					Level_metadata_analysis_cache_summary.live_fallback_max_us =
					    live_stage_elapsed_us;
			}
#endif
		} else if (!candidate_valid &&
		           (certifier_pending ||
		            Level_metadata_expensive_planning_allowed)) {
			deferred = 1;
			if (!certifier_pending)
				Level_metadata_live_work_summary.blocked_full_plan_calls++;
			Level_metadata_live_work_summary.deferred_refreshes++;
			if (incumbent_valid)
				Level_metadata_live_work_summary.retained_incumbents++;
			else
				Level_metadata_live_work_summary
				    .deferred_without_incumbent++;
#ifdef __ANDROID__
			debug_log(
			    DLOG_GUIDEBOT,
			    "route_work_deferred reason=%s "
			    "stage=live_refresh elapsed_us=%llu pending=0x%x "
			    "incumbent=%d endpoint=%d",
			    certifier_pending ? "budget" : "full_plan_forbidden",
			    (unsigned long long) (android_profile_monotonic_us() -
			                          rescan_started_us),
			    Level_metadata_live_work_summary.pending_event_mask,
			    incumbent_valid, endpoint_kind);
#endif
		}
		if (Level_metadata_analysis_cancelled ||
		    (Level_metadata_analysis_budget_exhausted &&
		     !candidate_valid)) {
			candidate_valid = 0;
			snprintf(
			    problem, sizeof(problem), "%s",
			    Level_metadata_analysis_cancelled
			        ? "metadata analysis cancelled"
			        : "metadata analysis exceeded its collision-work budget");
		}
		if (candidate_valid) {
			Level_metadata_live_route_state =
			    Level_metadata_live_candidate_state;
			Level_metadata_live_plan_summary =
			    Level_metadata_live_candidate_summary;
			Level_metadata_live_certificate =
			    Level_metadata_live_candidate_certificate;
			Level_metadata_live_route_state_valid = 1;
			Level_metadata_live_plan_summary_valid = 1;
			Level_metadata_live_route_provenance = candidate_provenance;
			if (candidate_provenance ==
			    LEVEL_METADATA_ROUTE_PROVENANCE_FULL_PLANNER)
				level_metadata_certify_fresh_live_plan();
			level_metadata_publish_live_route_decision();
		} else if (deferred && incumbent_valid) {
			Level_metadata_live_route_state_valid = 1;
			Level_metadata_live_plan_summary_valid = 1;
		} else if (!deferred || !incumbent_valid) {
			level_metadata_state_clear(&Level_metadata_live_route_state);
			Level_metadata_live_route_state.route_status =
			    deferred ? LEVEL_METADATA_ROUTE_PARTIAL
			             : LEVEL_METADATA_ROUTE_FAILED;
			snprintf(Level_metadata_live_route_state.route_problem,
			         sizeof(Level_metadata_live_route_state.route_problem),
			         "shared route planner: %s",
			         deferred     ? "live route calculation deferred"
			         : problem[0] ? problem
			                      : "unknown failure");
			memset(&Level_metadata_live_plan_summary, 0,
			       sizeof(Level_metadata_live_plan_summary));
			Level_metadata_live_plan_summary.first_pending_step = -1;
			Level_metadata_live_plan_summary
			    .first_pending_path_terminal_segment = -1;
			Level_metadata_live_plan_summary.partial_frontier_segment = -1;
			Level_metadata_live_route_state_valid = deferred;
			Level_metadata_live_plan_summary_valid = deferred;
			Level_metadata_live_route_provenance =
			    LEVEL_METADATA_ROUTE_PROVENANCE_NONE;
			memset(&Level_metadata_live_certificate, 0,
			       sizeof(Level_metadata_live_certificate));
			Level_metadata_live_certificate.status =
			    GUIDEBOT_ROUTE_CERTIFICATE_UNCHECKED;
			level_metadata_publish_live_route_decision();
		}
		if (!deferred)
			level_metadata_run_route_shadow(
			    view, endpoint_kind, route_target_seg);
#ifdef __ANDROID__
		refresh_elapsed_us =
		    (unsigned long long) (android_profile_monotonic_us() -
		                          rescan_started_us);
		live_stage_elapsed_us =
		    (unsigned long long) (android_profile_monotonic_us() -
		                          live_stage_started_us);
		Level_metadata_live_work_summary.last_tick_us =
		    live_stage_elapsed_us;
		Level_metadata_live_work_summary.last_refresh_us =
		    refresh_elapsed_us;
		Level_metadata_live_work_summary.certifier_ticks++;
		if (certifier_pending)
			Level_metadata_live_work_summary.certifier_deferred_ticks++;
		else
			Level_metadata_live_work_summary.certifier_completed_ticks++;
		if (live_stage_elapsed_us > 4000ULL) {
			Level_metadata_live_work_summary.certifier_overruns++;
			debug_log(
			    DLOG_GUIDEBOT,
			    "route_work_overrun stage=live_refresh elapsed_us=%llu "
			    "budget_us=2000 pending=%d",
			    live_stage_elapsed_us, certifier_pending);
		}
		if (live_stage_elapsed_us >
		    Level_metadata_live_work_summary.max_tick_us)
			Level_metadata_live_work_summary.max_tick_us =
			    live_stage_elapsed_us;
		if (refresh_elapsed_us >
		    Level_metadata_live_work_summary.max_refresh_us)
			Level_metadata_live_work_summary.max_refresh_us =
			    refresh_elapsed_us;
		if (refresh_elapsed_us > 4000ULL) {
			Level_metadata_live_work_summary.refresh_overruns++;
			debug_log(
			    DLOG_GUIDEBOT,
			    "route_work_overrun stage=refresh elapsed_us=%llu "
			    "budget_us=4000 pending=%d",
			    refresh_elapsed_us, certifier_pending);
		}
#endif
	}
}

int level_metadata_try_load_pending_cache(void)
{
	level_metadata_state shared_route;
	route_planner_plan_summary summary;
	int improved;

	if ((Level_metadata_canonical_plan_summary_valid &&
	     Level_metadata_route_readiness == LEVEL_METADATA_READINESS_COMPLETE) ||
	    !Level_metadata_canonical_snapshot_valid)
		return Level_metadata_canonical_plan_summary_valid;
#ifdef __ANDROID__
	if (atomic_load(&Level_metadata_background_result) < 0) {
		Level_metadata_route_readiness = LEVEL_METADATA_READINESS_FAILED;
		Level_metadata_canonical_state.route_status = LEVEL_METADATA_ROUTE_FAILED;
		snprintf(Level_metadata_canonical_state.route_problem,
		         sizeof(Level_metadata_canonical_state.route_problem), "%s",
		         "background route analysis failed");
		return 0;
	}
#endif
	level_metadata_state_clear(&shared_route);
	memset(&summary, 0, sizeof(summary));
	summary.first_pending_step = -1;
	summary.first_pending_path_terminal_segment = -1;
	summary.partial_frontier_segment = -1;
	if (!level_metadata_analysis_cache_load(&shared_route, &summary)) {
#ifdef __ANDROID__
		if (atomic_load(&Level_metadata_background_result) > 0) {
			Level_metadata_analysis_cache_summary
			    .publication_adoption_attempts++;
			if (Level_metadata_analysis_cache_summary
			        .publication_adoption_attempts >=
			    LEVEL_METADATA_MAX_PUBLICATION_ADOPTION_ATTEMPTS) {
				Level_metadata_analysis_cache_summary
				    .publication_adoption_failures++;
				Level_metadata_route_readiness =
				    LEVEL_METADATA_READINESS_FAILED;
				Level_metadata_canonical_state.route_status =
				    LEVEL_METADATA_ROUTE_FAILED;
				snprintf(
				    Level_metadata_canonical_state.route_problem,
				    sizeof(Level_metadata_canonical_state.route_problem), "%s",
				    "published route metadata could not be adopted");
				debug_log(
				    DLOG_PROFILING,
				    "route_metadata cache adoption failed attempts=%u file=%s misses=%u rejections=%u io_errors=%u physfs=%s",
				    Level_metadata_analysis_cache_summary
				        .publication_adoption_attempts,
				    Level_metadata_analysis_cache_summary.filename,
				    Level_metadata_analysis_cache_summary.misses,
				    Level_metadata_analysis_cache_summary.rejections,
				    Level_metadata_analysis_cache_summary.io_errors,
				    PHYSFS_getLastError());
				return 0;
			}
		}
		if (atomic_load(&Level_metadata_background_result) > 0 &&
		    !Level_metadata_pending_cache_miss_logged) {
			debug_log(
			    DLOG_PROFILING,
			    "route_metadata cache adoption miss file=%s misses=%u rejections=%u io_errors=%u physfs=%s",
			    Level_metadata_analysis_cache_summary.filename,
			    Level_metadata_analysis_cache_summary.misses,
			    Level_metadata_analysis_cache_summary.rejections,
			    Level_metadata_analysis_cache_summary.io_errors,
			    PHYSFS_getLastError());
			Level_metadata_pending_cache_miss_logged = 1;
		}
#endif
		return 0;
	}
	improved =
	    !Level_metadata_canonical_plan_summary_valid ||
	    shared_route.route_status == LEVEL_METADATA_ROUTE_OK ||
	    shared_route.route_step_count >
	        Level_metadata_canonical_state.route_step_count;
	if (!improved)
		return 0;
	level_metadata_apply_planned_route(
	    &Level_metadata_canonical_state, &shared_route);
	Level_metadata_canonical_plan_summary = summary;
	Level_metadata_canonical_plan_summary_valid = 1;
	Level_metadata_route_readiness =
	    shared_route.route_status == LEVEL_METADATA_ROUTE_OK ? LEVEL_METADATA_READINESS_COMPLETE : summary.first_pending_step >= 0     ? LEVEL_METADATA_READINESS_NEXT_READY
	                                                                                           : summary.partial_frontier_segment >= 0 ? LEVEL_METADATA_READINESS_PARTIAL
	                                                                                                                                   : LEVEL_METADATA_READINESS_FAILED;
	Level_metadata_expensive_planning_allowed = 1;
	Level_metadata_route_revision++;
#ifdef __ANDROID__
	Level_metadata_pending_cache_miss_logged = 0;
	debug_log(DLOG_PROFILING,
	          "route_metadata cache adopted file=%s readiness=%s steps=%d",
	          Level_metadata_analysis_cache_summary.filename,
	          level_metadata_route_readiness_name(Level_metadata_route_readiness),
	          shared_route.route_step_count);
#endif
	return 1;
}

void level_metadata_note_background_result(int success)
{
#ifdef __ANDROID__
	atomic_store(&Level_metadata_background_result, success ? 1 : -1);
	debug_log(DLOG_GAME, "Route metadata background result=%s",
	          success ? "cache_published" : "failed");
#else
	(void) success;
#endif
}

int level_metadata_get_route_readiness(void)
{
	return Level_metadata_route_readiness;
}

const char *level_metadata_route_readiness_name(int readiness)
{
	switch (readiness) {
		case LEVEL_METADATA_READINESS_CALCULATING: return "calculating";
		case LEVEL_METADATA_READINESS_NEXT_READY: return "next_ready";
		case LEVEL_METADATA_READINESS_COMPLETE: return "complete";
		case LEVEL_METADATA_READINESS_PARTIAL: return "partial";
		case LEVEL_METADATA_READINESS_FAILED: return "failed";
		default: return "unknown";
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
	if (result) {
		memset(result, 0, sizeof(*result));
		result->target_seg = -1;
		result->waypoint_seg = -1;
	}
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

unsigned int level_metadata_get_visibility_checkpoint_sequence(void)
{
	return Level_metadata_visibility_checkpoint_sequence;
}

unsigned int level_metadata_get_route_revision(void)
{
	return Level_metadata_route_revision;
}

int level_metadata_get_route_analysis_cache_summary(
    route_analysis_cache_summary *summary)
{
	unsigned long long samples[ROUTE_ANALYSIS_TIMING_SAMPLE_CAPACITY];
	unsigned int count;
	unsigned int index;

	if (!summary)
		return 0;
	*summary = Level_metadata_analysis_cache_summary;
	count = summary->live_reuse_sample_count;
	for (index = 0; index < count; ++index) {
		unsigned int insert = index;

		samples[index] = summary->live_reuse_samples[index];
		while (insert > 0 && samples[insert - 1] > samples[insert]) {
			unsigned long long temporary = samples[insert - 1];
			samples[insert - 1] = samples[insert];
			samples[insert] = temporary;
			insert--;
		}
	}
	if (count > 0) {
		unsigned int p95_rank = (count * 95 + 99) / 100;

		if (count & 1)
			summary->live_reuse_median_us = samples[count / 2];
		else {
			unsigned long long lower = samples[count / 2 - 1];
			unsigned long long upper = samples[count / 2];
			summary->live_reuse_median_us = lower + (upper - lower) / 2;
		}
		summary->live_reuse_p95_us = samples[p95_rank - 1];
	}
#ifdef __ANDROID__
	return 1;
#else
	return 0;
#endif
}

int level_metadata_get_live_work_summary(
    level_metadata_live_work_summary *summary)
{
	if (!summary)
		return 0;
	*summary = Level_metadata_live_work_summary;
	summary->pending = Level_metadata_live_route_work_pending;
	summary->reachability_cursor =
	    Level_metadata_route_certifier_workspace.reach_head;
	summary->firing_candidate_cursor =
	    Level_metadata_route_certifier_workspace.firing_search_segment;
	summary->firing_candidate_pass =
	    Level_metadata_route_certifier_workspace.firing_search_pass;
	summary->unexplored_candidate_cursor =
	    Level_metadata_route_certifier_workspace.unexplored_scan_segment;
	return 1;
}

int level_metadata_live_route_work_pending(void)
{
	return Level_metadata_live_route_work_pending;
}

void level_metadata_invalidate_live_route_work(void)
{
	Level_metadata_live_route_work_pending = 0;
	Level_metadata_live_work_summary.pending_event_mask = 0;
	guidebot_route_certifier_reset_job(
	    &Level_metadata_route_certifier_workspace);
}

void level_metadata_set_live_work_pending_event_mask(
    unsigned int event_mask)
{
	Level_metadata_live_work_summary.pending_event_mask = event_mask;
}

static void secret_area_scan_current_level(int allow_expensive_planning)
{
	secret_area_scan_view view;
	int start_segment;
#ifdef __ANDROID__
	const long long started_us = android_profile_monotonic_us();
	long long topology_finished_us;
	long long secret_scan_finished_us;
#endif

	secret_area_trace("start");
	Level_metadata_expensive_planning_allowed = allow_expensive_planning;
	Secret_area_reveal_unfound = 0;
	Level_metadata_objective_mode = LEVEL_METADATA_OBJECTIVES_OFF;
	Level_metadata_topology_valid = 0;
	secret_area_ensure_level_topology();
#ifdef __ANDROID__
	topology_finished_us = android_profile_monotonic_us();
#endif
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
	level_metadata_report_progress("secret_areas", 0, 1);
	secret_area_scan_level(&view, &Secret_area_state);
	level_metadata_report_progress("secret_areas", 1, 1);
#ifdef __ANDROID__
	secret_scan_finished_us = android_profile_monotonic_us();
#endif
	level_metadata_rescan_current_level();
#ifdef __ANDROID__
	debug_log(
	    DLOG_PROFILING,
	    "route_metadata level=%d mode=%s readiness=%s elapsed_us=%lld "
	    "topology_us=%lld secret_scan_us=%lld metadata_us=%lld "
	    "cache_hits=%u cache_misses=%u fvi=%u visibility_entries=%d",
	    Current_level_num,
	    allow_expensive_planning ? "analyze" : "prepare",
	    level_metadata_route_readiness_name(Level_metadata_route_readiness),
	    android_profile_monotonic_us() - started_us,
	    topology_finished_us - started_us,
	    secret_scan_finished_us - topology_finished_us,
	    android_profile_monotonic_us() - secret_scan_finished_us,
	    Level_metadata_analysis_cache_summary.hits,
	    Level_metadata_analysis_cache_summary.misses,
	    Level_metadata_analysis_fvi_count,
	    Level_metadata_visibility_summary.entries);
#endif
	secret_area_trace("done");
}

void secret_area_rescan_current_level(void)
{
	secret_area_scan_current_level(1);
}

void secret_area_prepare_current_level(void)
{
#ifdef __ANDROID__
	android_route_metadata_invalidate_pending();
	atomic_store(&Level_metadata_background_result, 0);
	Level_metadata_pending_cache_miss_logged = 0;
	Level_metadata_analysis_cache_summary.publication_adoption_attempts = 0;
	Level_metadata_analysis_cache_summary.publication_adoption_failures = 0;
#endif
	secret_area_scan_current_level(0);
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

int level_metadata_get_live_route_decision(guidebot_route_decision *decision)
{
	if (!decision || !Level_metadata_published_route_decision_valid)
		return 0;
	*decision = Level_metadata_published_route_decision;
	return 1;
}

int level_metadata_get_live_route_provenance(void)
{
	return Level_metadata_live_route_provenance;
}

const char *level_metadata_route_provenance_name(int provenance)
{
	switch (provenance) {
		case LEVEL_METADATA_ROUTE_PROVENANCE_CERTIFIER:
			return "certifier";
		case LEVEL_METADATA_ROUTE_PROVENANCE_PREPARED_FALLBACK:
			return "prepared_fallback";
		case LEVEL_METADATA_ROUTE_PROVENANCE_FULL_PLANNER:
			return "full_planner";
		default:
			return "none";
	}
}

void level_metadata_set_live_certifier_enabled(int enabled)
{
	Level_metadata_live_certifier_enabled = enabled != 0;
}

int level_metadata_get_live_certifier_enabled(void)
{
	return Level_metadata_live_certifier_enabled;
}

void level_metadata_set_route_shadow_enabled(int enabled)
{
	level_metadata_route_shadow_reset();
	Level_metadata_route_shadow_summary.enabled = enabled != 0;
}

int level_metadata_get_route_shadow_summary(
    guidebot_route_shadow_summary *summary)
{
	if (!summary)
		return 0;
	*summary = Level_metadata_route_shadow_summary;
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

int level_metadata_route_audit_domain(
    int start_objnum, int domain, unsigned int *work_units)
{
	const route_snapshot_summary *baseline =
	    Level_metadata_live_snapshot_valid
	        ? &Level_metadata_live_snapshot
	    : Level_metadata_canonical_snapshot_valid
	        ? &Level_metadata_canonical_snapshot
	        : NULL;
	level_metadata_scan_view *view;
	unsigned long long current_hash;
	unsigned long long baseline_hash;

	if (work_units)
		*work_units = 0;
	if (!baseline)
		return -1;
	switch (domain) {
		case ROUTE_SNAPSHOT_DOMAIN_START:
			baseline_hash = baseline->start_hash;
			break;
		case ROUTE_SNAPSHOT_DOMAIN_PROGRESSION:
			baseline_hash = baseline->progression_hash;
			break;
		case ROUTE_SNAPSHOT_DOMAIN_NAVIGATION:
			if (!Level_metadata_navigation_access_audit_hash_valid)
				return -1;
			baseline_hash =
			    Level_metadata_navigation_access_audit_hash;
			domain = ROUTE_SNAPSHOT_DOMAIN_NAVIGATION_ACCESS;
			break;
		case ROUTE_SNAPSHOT_DOMAIN_TRIGGERS:
			baseline_hash = baseline->trigger_hash;
			break;
		case ROUTE_SNAPSHOT_DOMAIN_OBJECTS:
			baseline_hash = baseline->object_hash;
			break;
		case ROUTE_SNAPSHOT_DOMAIN_AUTOMAP:
			baseline_hash = baseline->automap_hash;
			break;
		case ROUTE_SNAPSHOT_DOMAIN_PROGRESSION_OBJECTS:
			if (!Level_metadata_progression_object_audit_hash_valid)
				return -1;
			baseline_hash =
			    Level_metadata_progression_object_audit_hash;
			break;
		default: return -1;
	}
	view = level_metadata_refresh_scan_view(start_objnum);
	if (!route_snapshot_build_domain_hash(
	        view, domain, &current_hash, work_units))
		return -1;
	return current_hash != baseline_hash;
}

int level_metadata_validate_live_route_certificate(
    int start_objnum, unsigned int *work_units)
{
	level_metadata_scan_view *view;
	const level_metadata_route_step *step;
	int index;

	if (work_units)
		*work_units = 0;
	if (!Level_metadata_live_route_state_valid ||
	    !Level_metadata_live_plan_summary_valid ||
	    Level_metadata_live_certificate.status !=
	        GUIDEBOT_ROUTE_CERTIFICATE_VALID)
		return -1;
	index = Level_metadata_live_plan_summary.first_pending_step;
	if (index < 0 ||
	    index >= Level_metadata_live_route_state.route_step_count)
		return 1;
	view = level_metadata_refresh_scan_view(start_objnum);
	step = &Level_metadata_live_route_state.route_steps[index];
	if (work_units)
		*work_units = step->opened_link_count > 0
		                  ? (unsigned int) step->opened_link_count
		                  : 1;
	if (!level_metadata_route_step_required_by_world_state(view, step))
		return 0;
	if (step->kind == LEVEL_METADATA_ROUTE_TRIGGER &&
	    (step->trigger_num < 0 || step->trigger_num >= view->num_triggers ||
	     (view->trigger_flags &&
	      (view->trigger_flags(view->user, step->trigger_num) &
	       view->trigger_flag_disabled) != 0)))
		return 0;
	if (Level_metadata_live_certificate.source_object >= 0 &&
	    view->object_count && view->object_flags &&
	    (Level_metadata_live_certificate.source_object >=
	         view->object_count(view->user) ||
	     (view->object_flags(
	          view->user, Level_metadata_live_certificate.source_object) &
	      view->obj_flag_should_be_dead) != 0))
		return 0;
	return 1;
}

int level_metadata_prepare_guidebot_path_view(int start_objnum)
{
	return level_metadata_refresh_scan_view(start_objnum) != NULL;
}

int level_metadata_get_exit_route_step_current(
    int start_objnum,
    level_metadata_route_step *step,
    int *step_index,
    int *target_segment)
{
	if (!Level_metadata_live_route_state_valid ||
	    !level_metadata_refresh_scan_view(start_objnum))
		return 0;
	return guidebot_route_select_exit_step_current_state(
	    &Level_metadata_scan_view, &Level_metadata_live_route_state, step,
	    step_index, target_segment);
}

int level_metadata_guidebot_side_passable_current(int segment, int side)
{
	if (!Level_metadata_scan_view_initialized)
		return 0;
	return guidebot_route_side_passable_current(
	    &Level_metadata_scan_view, segment, side);
}

int level_metadata_guidebot_route_frontier_current(
    int start_segment,
    int goal_segment,
    int max_depth,
    int avoid_from,
    int avoid_to,
    int avoid_from2,
    int avoid_to2)
{
	if (!Level_metadata_scan_view_initialized)
		return -1;
	return guidebot_route_best_physical_frontier(
	    &Level_metadata_scan_view, start_segment, goal_segment, max_depth,
	    avoid_from, avoid_to, avoid_from2, avoid_to2,
	    &Level_metadata_route_frontier_workspace);
}

int level_metadata_guidebot_route_deferred_countdown_frontier_current(
    int start_segment,
    int goal_segment,
    int max_depth,
    int avoid_from,
    int avoid_to,
    int avoid_from2,
    int avoid_to2)
{
	if (!Level_metadata_scan_view_initialized)
		return -1;
	return guidebot_route_best_deferred_countdown_frontier(
	    &Level_metadata_scan_view, start_segment, goal_segment, max_depth,
	    avoid_from, avoid_to, avoid_from2, avoid_to2,
	    &Level_metadata_route_frontier_workspace);
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

static PHYSFS_sint64 secret_area_runtime_write(
    rewind_file *fp, const void *data, PHYSFS_uint32 size, PHYSFS_uint32 count)
{
#if REWIND_FILE_USES_WRAPPER
	return rewind_file_write(fp, data, size, count);
#else
	return PHYSFS_write(fp, data, size, count);
#endif
}

static PHYSFS_sint64 secret_area_runtime_read(
    rewind_file *fp, void *data, PHYSFS_uint32 size, PHYSFS_uint32 count)
{
#if REWIND_FILE_USES_WRAPPER
	return rewind_file_read(fp, data, size, count);
#else
	return PHYSFS_read(fp, data, size, count);
#endif
}

static int secret_area_runtime_read_sxe32(rewind_file *fp, int swap)
{
#if REWIND_FILE_USES_WRAPPER
	return rewind_file_read_sxe32(fp, swap);
#else
	return PHYSFSX_readSXE32(fp, swap);
#endif
}

void secret_area_write_runtime_state(rewind_file *fp)
{
	unsigned char empty_found[SECRET_AREA_MAX_GENERATED] = { 0 };
	int total = secret_area_total(&Secret_area_state);

	secret_area_runtime_write(fp, &total, sizeof(total), 1);
	secret_area_runtime_write(fp,
	                          total > 0 ? Secret_area_state.found : empty_found,
	                          sizeof(empty_found[0]), SECRET_AREA_MAX_GENERATED);
}

void secret_area_read_runtime_state(rewind_file *fp, int swap)
{
	unsigned char found[SECRET_AREA_MAX_GENERATED] = { 0 };
	int saved_total = secret_area_runtime_read_sxe32(fp, swap);

	secret_area_runtime_read(fp, found, sizeof(found[0]),
	                         SECRET_AREA_MAX_GENERATED);
	secret_area_restore_saved_found(saved_total, found,
	                                SECRET_AREA_MAX_GENERATED, Automap_visited,
	                                Highest_segment_index + 1);
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
