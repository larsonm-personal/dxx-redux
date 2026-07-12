#ifndef DXX_LEVEL_METADATA_SCAN_H
#define DXX_LEVEL_METADATA_SCAN_H

#ifdef __cplusplus
extern "C" {
#endif

#define LEVEL_METADATA_MAX_SEGMENTS                         9000
#define LEVEL_METADATA_MAX_WALLS                            254
#define LEVEL_METADATA_MAX_TRIGGERS                         100
#define LEVEL_METADATA_MAX_OBJECTS                          1000
#define LEVEL_METADATA_MAX_SIDES                            6
#define LEVEL_METADATA_MAX_TARGETS                          512
#define LEVEL_METADATA_MAX_ROUTE_STEPS                      96
#define LEVEL_METADATA_MAX_ROUTE_LINKS                      10
#define LEVEL_METADATA_ROUTE_LABEL_LEN                      64
#define LEVEL_METADATA_ROUTE_TRIGGER_TYPE_LEN               24
#define LEVEL_METADATA_KEY_MASK_BLUE                        (1 << 0)
#define LEVEL_METADATA_KEY_MASK_RED                         (1 << 1)
#define LEVEL_METADATA_KEY_MASK_GOLD                        (1 << 2)
#define LEVEL_METADATA_DEFAULT_ENERGY_CENTER_GROUP_DISTANCE (40 * 65536)
#define LEVEL_METADATA_FIX_SCALE                            65536.0
#define LEVEL_METADATA_SHIP_SPEED_UNITS_PER_SECOND          50.0

/*
 * Baseline for normalized volume. This must be generated from Descent 1 level 1
 * with level_metadata_scan.c's segment-volume algorithm. It is intentionally
 * centralized so later tuning changes have one fixture-backed value to update.
 */
#define LEVEL_METADATA_D1_LEVEL1_VOLUME_BASELINE 4572902.81488615

enum level_metadata_route_step_kind {
	LEVEL_METADATA_ROUTE_START = 0,
	LEVEL_METADATA_ROUTE_KEY = 1,
	LEVEL_METADATA_ROUTE_TRIGGER = 2,
	LEVEL_METADATA_ROUTE_REACTOR = 3,
	LEVEL_METADATA_ROUTE_BOSS = 4,
	LEVEL_METADATA_ROUTE_EXIT = 5,
	LEVEL_METADATA_ROUTE_HIDDEN_DOOR = 6,
	LEVEL_METADATA_ROUTE_HOSTAGE = 7,
	LEVEL_METADATA_ROUTE_UNEXPLORED = 8
};

enum level_metadata_route_activation_kind {
	LEVEL_METADATA_ROUTE_ACTIVATION_NONE = 0,
	LEVEL_METADATA_ROUTE_ACTIVATION_PICKUP_KEY = 1,
	LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH = 2,
	LEVEL_METADATA_ROUTE_ACTIVATION_FLY_THROUGH_TRIGGER = 3,
	LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER = 4,
	LEVEL_METADATA_ROUTE_ACTIVATION_OPEN_HIDDEN_DOOR = 5,
	LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_REACTOR = 6,
	LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BOSS = 7,
	LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT = 8
};

enum level_metadata_route_edge_cost {
	LEVEL_METADATA_ROUTE_EDGE_BLOCKED = -1,
	LEVEL_METADATA_ROUTE_EDGE_PASSABLE = 0,
	LEVEL_METADATA_ROUTE_EDGE_PROGRESS = 1
};

#ifdef _MSC_VER
/* Engine headers can leave MSVC packing at 1 byte; this is a shared ABI. */
#pragma pack(push, 8)
#endif

typedef struct level_metadata_route_step {
	int kind;
	int seg;
	int side;
	int wall_num;
	int trigger_num;
	int trigger_type;
	int key_index;
	int activation_kind;
	int activation_pos_valid;
	int activation_pos[3];
	int aim_pos_valid;
	int aim_pos[3];
	int label_pos_valid;
	int label_pos[3];
	double distance_from_previous;
	char label[LEVEL_METADATA_ROUTE_LABEL_LEN];
	char trigger_type_name[LEVEL_METADATA_ROUTE_TRIGGER_TYPE_LEN];
	int opened_link_count;
	int opened_link_seg[LEVEL_METADATA_MAX_ROUTE_LINKS];
	int opened_link_side[LEVEL_METADATA_MAX_ROUTE_LINKS];
	int opened_link_wall[LEVEL_METADATA_MAX_ROUTE_LINKS];
} level_metadata_route_step;

typedef struct level_metadata_scan_view {
	int num_segments;
	int num_walls;
	int num_triggers;
	int start_segment;
	int initial_key_mask;
	int initial_control_center_destroyed;
	int segment_special_fuelcen;
	int segment_special_robotmaker;
	int segment_special_control_center;
	int energy_center_group_distance;
	int wall_type_blastable;
	int wall_type_door;
	int wall_type_illusion;
	int wall_type_open;
	int wall_flag_door_locked;
	int wall_flag_door_opened;
	int wall_clip_hidden;
	int wall_key_none;
	int wall_key_blue;
	int wall_key_red;
	int wall_key_gold;
	int obj_type_robot;
	int obj_type_powerup;
	int obj_type_control_center;
	int obj_flag_should_be_dead;
	int powerup_key_blue;
	int powerup_key_red;
	int powerup_key_gold;
	int trigger_type_open_door;
	int trigger_type_exit;
	int trigger_type_secret_exit;
	int trigger_type_illusion_off;
	int trigger_type_unlock_door;
	int trigger_type_open_wall;
	int trigger_type_illusory_wall;
	int trigger_flag_disabled;
	void *user;
	int (*segment_child)(void *user, int seg, int side);
	int (*segment_is_explored)(void *user, int seg);
	int (*reverse_side)(void *user, int seg, int child);
	int (*side_is_flyable)(void *user, int seg, int side);
	int (*side_is_hard_blocked)(void *user, int seg, int side);
	int (*side_is_control_center_link)(void *user, int seg, int side);
	int (*wall_num)(void *user, int seg, int side);
	int (*wall_segment)(void *user, int wall_num);
	int (*wall_side)(void *user, int wall_num);
	int (*wall_type)(void *user, int wall_num);
	int (*wall_flags)(void *user, int wall_num);
	int (*wall_keys)(void *user, int wall_num);
	int (*wall_clip_flags)(void *user, int wall_num);
	int (*wall_trigger)(void *user, int wall_num);
	int (*segment_special)(void *user, int seg);
	int (*segment_center)(void *user, int seg, int xyz[3]);
	int (*side_center)(void *user, int seg, int side, int xyz[3]);
	int (*segment_vertex)(void *user, int seg, int index, int xyz[3]);
	int (*start_position)(void *user, int xyz[3]);
	int (*object_count)(void *user);
	int (*object_segment)(void *user, int objnum);
	int (*object_type)(void *user, int objnum);
	int (*object_id)(void *user, int objnum);
	int (*object_flags)(void *user, int objnum);
	int (*object_contains_type)(void *user, int objnum);
	int (*object_contains_id)(void *user, int objnum);
	int (*object_contains_count)(void *user, int objnum);
	int (*object_position)(void *user, int objnum, int xyz[3]);
	int (*object_is_boss)(void *user, int objnum);
	int (*object_is_companion)(void *user, int objnum);
	int (*side_has_exit_trigger)(void *user, int seg, int side);
	int (*triggered_side_opener_count)(void *user, int seg, int side);
	int (*triggered_side_opener_wall_num)(void *user, int seg, int side, int index);
	int (*trigger_type)(void *user, int trigger_num);
	int (*trigger_flags)(void *user, int trigger_num);
	int (*trigger_link_count)(void *user, int trigger_num);
	int (*trigger_link_segment)(void *user, int trigger_num, int link_index);
	int (*trigger_link_side)(void *user, int trigger_num, int link_index);
	int (*target_visible_from_segment)(void *user, int seg, const int from_pos[3], int target_seg, const int target_pos[3]);
	int (*wall_visible_from_segment)(void *user, int seg, const int from_pos[3], int wall_num);
	int (*wall_is_shootable_trigger)(void *user, int wall_num);
} level_metadata_scan_view;

typedef struct level_metadata_unexplored_route {
	int component_size;
	int target_seg;
	int waypoint_seg;
	int direct_reachable;
} level_metadata_unexplored_route;

typedef struct level_metadata_route_search_node {
	int reachable;
	double distance;
	int progress_weight;
	int parent_seg;
	int parent_side;
} level_metadata_route_search_node;

typedef struct level_metadata_route_progress_shadow {
	int current_seg;
	int current_pos[3];
	int key_mask;
	int key_in_progress;
	int avoided_key_mask;
	int control_center_destroyed;
	unsigned char fired_triggers[LEVEL_METADATA_MAX_TRIGGERS];
	unsigned char trigger_in_progress[LEVEL_METADATA_MAX_TRIGGERS];
	unsigned char avoided_triggers[LEVEL_METADATA_MAX_TRIGGERS];
	unsigned char opened_hidden_walls[LEVEL_METADATA_MAX_WALLS];
} level_metadata_route_progress_shadow;

typedef struct level_metadata_route_target_shadow {
	int seg;
	int pos[3];
} level_metadata_route_target_shadow;

typedef struct level_metadata_route_target_inventory_shadow {
	int key_count[3];
	level_metadata_route_target_shadow keys[3][LEVEL_METADATA_MAX_TARGETS];
	int reactor_found;
	level_metadata_route_target_shadow reactor;
	int boss_found;
	level_metadata_route_target_shadow boss;
	int exit_count;
	level_metadata_route_target_shadow exits[LEVEL_METADATA_MAX_TARGETS];
} level_metadata_route_target_inventory_shadow;

typedef struct level_metadata_route_target_selection_shadow {
	int selected_index;
	double distance;
	int progress_weight;
} level_metadata_route_target_selection_shadow;

typedef struct level_metadata_route_trigger_source_shadow {
	int target_seg;
	int target_side;
	int target_wall;
	int source_wall;
	int source_seg;
	int source_side;
	int trigger_num;
	int trigger_type;
	int source_pos[3];
} level_metadata_route_trigger_source_shadow;

typedef struct level_metadata_route_trigger_firing_path_shadow {
	int found;
	level_metadata_route_trigger_source_shadow source;
	double distance;
	int progress_weight;
	int terminal_seg;
	int terminal_pos[3];
	int terminal_pos_valid;
} level_metadata_route_trigger_firing_path_shadow;

typedef struct level_metadata_visibility_cache_summary {
	unsigned long long world_hash;
	unsigned long long hits;
	unsigned long long misses;
	int entries;
	int capacity;
	int resets;
	int bypasses;
} level_metadata_visibility_cache_summary;

typedef struct level_metadata_state {
	int energy_center_segment_count;
	int energy_center_raw_count;
	int energy_center_count;
	int energy_center_group_distance;
	int energy_center_nearest_raw_distance;
	int matcen_segment_count;
	int matcen_raw_count;
	int matcen_count;
	double mine_volume;
	double mine_volume_normalized;
	double travel_distance;
	int travel_time_seconds;
	int guidebot_count;
	int guidebot_placed;
	int guidebot_accessible;
	char guidebot_placement_note[128];
	char guidebot_note[128];
	int route_status;
	char route_problem[128];
	char route_note[128];
	int route_step_count;
	level_metadata_route_step route_steps[LEVEL_METADATA_MAX_ROUTE_STEPS];
} level_metadata_state;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

void level_metadata_state_clear(level_metadata_state *state);
int level_metadata_scan_level(const level_metadata_scan_view *view, level_metadata_state *state);
int level_metadata_scan_end_route(const level_metadata_scan_view *view, level_metadata_state *state);
int level_metadata_scan_route_to_segment(const level_metadata_scan_view *view, int target_seg, level_metadata_state *state);
int level_metadata_scan_unexplored_route(
    const level_metadata_scan_view *view,
    level_metadata_state *state,
    level_metadata_unexplored_route *result);
int level_metadata_scan_route_edge_cost(const level_metadata_scan_view *view, int seg, int side);
int level_metadata_scan_route_search_shadow(
    const level_metadata_scan_view *view,
    int optimistic,
    level_metadata_route_search_node *nodes,
    int capacity);
int level_metadata_scan_route_search_state_shadow(
    const level_metadata_scan_view *view,
    const level_metadata_route_progress_shadow *progress,
    int optimistic,
    level_metadata_route_search_node *nodes,
    int capacity);
int level_metadata_scan_route_targets_shadow(
    const level_metadata_scan_view *view,
    level_metadata_route_target_inventory_shadow *inventory);
int level_metadata_scan_route_select_targets_shadow(
    const level_metadata_scan_view *view,
    const level_metadata_route_progress_shadow *progress,
    const level_metadata_route_target_shadow *targets,
    int count,
    level_metadata_route_target_selection_shadow *selection);
int level_metadata_scan_route_select_key_shadow(
    const level_metadata_scan_view *view,
    const level_metadata_route_progress_shadow *progress,
    int key_index,
    level_metadata_route_target_selection_shadow *selection);
int level_metadata_scan_route_trigger_sources_shadow(
    const level_metadata_scan_view *view,
    const level_metadata_route_progress_shadow *progress,
    int seg,
    int side,
    level_metadata_route_trigger_source_shadow *sources,
    int capacity,
    int *count);
int level_metadata_scan_route_trigger_firing_path_shadow(
    const level_metadata_scan_view *view,
    const level_metadata_route_progress_shadow *progress,
    int seg,
    int side,
    level_metadata_route_trigger_firing_path_shadow *result);
const char *level_metadata_route_status_name(int status);
const char *level_metadata_route_step_kind_name(int kind);
const char *level_metadata_route_activation_kind_name(int kind);

#ifdef __cplusplus
}
#endif

#endif
