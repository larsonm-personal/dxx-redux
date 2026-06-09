#ifndef DXX_LEVEL_METADATA_SCAN_H
#define DXX_LEVEL_METADATA_SCAN_H

#ifdef __cplusplus
extern "C" {
#endif

#define LEVEL_METADATA_MAX_SEGMENTS                         9000
#define LEVEL_METADATA_MAX_SIDES                            6
#define LEVEL_METADATA_DEFAULT_ENERGY_CENTER_GROUP_DISTANCE (40 * 65536)

typedef struct level_metadata_scan_view {
	int num_segments;
	int segment_special_fuelcen;
	int segment_special_robotmaker;
	int energy_center_group_distance;
	void *user;
	int (*segment_child)(void *user, int seg, int side);
	int (*reverse_side)(void *user, int seg, int child);
	int (*segment_special)(void *user, int seg);
	int (*segment_center)(void *user, int seg, int xyz[3]);
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
} level_metadata_state;

void level_metadata_state_clear(level_metadata_state *state);
int level_metadata_scan_level(const level_metadata_scan_view *view, level_metadata_state *state);

#ifdef __cplusplus
}
#endif

#endif
