#ifndef DXX_LEVEL_METADATA_SCAN_H
#define DXX_LEVEL_METADATA_SCAN_H

#ifdef __cplusplus
extern "C" {
#endif

#define LEVEL_METADATA_MAX_SEGMENTS                         9000
#define LEVEL_METADATA_MAX_SIDES                            6
#define LEVEL_METADATA_MAX_TARGETS                          512
#define LEVEL_METADATA_DEFAULT_ENERGY_CENTER_GROUP_DISTANCE (40 * 65536)
#define LEVEL_METADATA_FIX_SCALE                            65536.0
#define LEVEL_METADATA_SHIP_SPEED_UNITS_PER_SECOND          50.0

/*
 * Baseline for normalized volume. This must be generated from Descent 1 level 1
 * with level_metadata_scan.c's segment-volume algorithm. It is intentionally
 * centralized so later tuning changes have one fixture-backed value to update.
 */
#define LEVEL_METADATA_D1_LEVEL1_VOLUME_BASELINE 4572902.81488615

typedef struct level_metadata_scan_view {
	int num_segments;
	int num_walls;
	int start_segment;
	int segment_special_fuelcen;
	int segment_special_robotmaker;
	int segment_special_control_center;
	int energy_center_group_distance;
	int wall_type_blastable;
	int wall_type_door;
	int wall_type_illusion;
	int wall_type_open;
	int wall_key_none;
	int wall_key_blue;
	int wall_key_red;
	int wall_key_gold;
	int obj_type_none;
	int obj_type_hostage;
	int obj_type_powerup;
	int obj_type_control_center;
	int obj_flag_should_be_dead;
	int powerup_key_blue;
	int powerup_key_red;
	int powerup_key_gold;
	void *user;
	int (*segment_child)(void *user, int seg, int side);
	int (*reverse_side)(void *user, int seg, int child);
	int (*wall_num)(void *user, int seg, int side);
	int (*wall_type)(void *user, int wall_num);
	int (*wall_keys)(void *user, int wall_num);
	int (*segment_special)(void *user, int seg);
	int (*segment_center)(void *user, int seg, int xyz[3]);
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
	int (*side_has_exit_trigger)(void *user, int seg, int side);
	int (*triggered_side_opener_count)(void *user, int seg, int side);
} level_metadata_scan_view;

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
	int travel_targets_reached;
	int travel_targets_total;
	int travel_key_detours;
	int travel_status;
	char travel_problem[128];
} level_metadata_state;

void level_metadata_state_clear(level_metadata_state *state);
int level_metadata_scan_level(const level_metadata_scan_view *view, level_metadata_state *state);
const char *level_metadata_travel_status_name(int status);

#ifdef __cplusplus
}
#endif

#endif
