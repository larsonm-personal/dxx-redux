#ifndef DXX_SECRET_AREA_SCAN_H
#define DXX_SECRET_AREA_SCAN_H

#ifdef __cplusplus
extern "C" {
#endif

#define SECRET_AREA_MAX_SEGMENTS  9000
#define SECRET_AREA_MAX_GENERATED 30
#define SECRET_AREA_MAX_ENTRANCES 16
#define SECRET_AREA_MAX_SIDES     6
#define SECRET_AREA_MAX_ITEMS     64
#define SECRET_AREA_ITEM_NAME_LEN 32

enum secret_area_disabled_reason {
	SECRET_AREA_DISABLED_NONE = 0,
	SECRET_AREA_DISABLED_INVALID_VIEW = 1,
	SECRET_AREA_DISABLED_TOO_MANY_CANDIDATES = 2
};

typedef struct secret_area_scan_view {
	int num_segments;
	int num_walls;
	int start_segment;
	int max_generated;
	int wall_type_blastable;
	int wall_type_door;
	int wall_type_illusion;
	int wall_type_open;
	int wall_flag_door_locked;
	int wall_flag_illusion_off;
	int wall_key_none;
	int wall_clip_hidden;
	int obj_type_none;
	int obj_type_robot;
	int obj_type_hostage;
	int obj_type_powerup;
	int obj_type_control_center;
	int obj_flag_should_be_dead;
	int powerup_key_blue;
	int powerup_key_red;
	int powerup_key_gold;
	int segment_special_control_center;
	int segment_special_robotmaker;
	void *user;
	int (*segment_child)(void *user, int seg, int side);
	int (*reverse_side)(void *user, int seg, int child);
	int (*wall_num)(void *user, int seg, int side);
	int (*wall_type)(void *user, int wall_num);
	int (*wall_flags)(void *user, int wall_num);
	int (*wall_keys)(void *user, int wall_num);
	int (*wall_clip_flags)(void *user, int wall_num);
	int (*segment_special)(void *user, int seg);
	int (*segment_center)(void *user, int seg, int xyz[3]);
	int (*object_count)(void *user);
	int (*object_segment)(void *user, int objnum);
	int (*object_type)(void *user, int objnum);
	int (*object_id)(void *user, int objnum);
	int (*object_flags)(void *user, int objnum);
	int (*object_contains_type)(void *user, int objnum);
	int (*object_contains_id)(void *user, int objnum);
	int (*object_contains_count)(void *user, int objnum);
	const char *(*powerup_name)(void *user, int id);
	int (*side_has_exit_trigger)(void *user, int seg, int side);
	int (*triggered_side_opener_count)(void *user, int seg, int side);
	int (*triggered_side_opener_segment)(void *user, int seg, int side, int index);
	int (*triggered_side_opener_side)(void *user, int seg, int side, int index);
	int (*triggered_side_opener_wall_num)(void *user, int seg, int side, int index);
	int (*triggered_side_opener_is_marginal)(void *user, int seg, int side, int index);
} secret_area_scan_view;

typedef struct secret_area_entrance {
	int seg;
	int side;
	int secret_seg;
	int wall_num;
} secret_area_entrance;

typedef struct secret_area_item {
	int id;
	int count;
	int direct_count;
	int contained_count;
	char name[SECRET_AREA_ITEM_NAME_LEN];
} secret_area_item;

typedef struct secret_area_entry {
	int display_index;
	int entry_distance;
	int entry_seg;
	int entry_side;
	int lowest_segment;
	int label_pos[3];
	int segment_count;
	int segments[SECRET_AREA_MAX_SEGMENTS];
	int entrance_count;
	secret_area_entrance entrances[SECRET_AREA_MAX_ENTRANCES];
	int robot_count;
	int robotmaker_count;
	int item_count;
	secret_area_item items[SECRET_AREA_MAX_ITEMS];
} secret_area_entry;

typedef struct secret_area_state {
	int enabled;
	int disabled_reason;
	int raw_candidate_count;
	int final_candidate_count;
	int found_count;
	int segment_to_secret[SECRET_AREA_MAX_SEGMENTS];
	secret_area_entry secrets[SECRET_AREA_MAX_GENERATED];
	unsigned char found[SECRET_AREA_MAX_GENERATED];
} secret_area_state;

void secret_area_state_clear(secret_area_state *state);
int secret_area_scan_level(const secret_area_scan_view *view, secret_area_state *state);
int secret_area_mark_segment_entered(secret_area_state *state, int seg);
void secret_area_restore_found(secret_area_state *state, int saved_total, const unsigned char *found, int found_capacity);
void secret_area_restore_found_from_visited(secret_area_state *state, const unsigned char *visited, int visited_count);
int secret_area_total(const secret_area_state *state);
int secret_area_found_count(const secret_area_state *state);
const char *secret_area_disabled_reason_name(int reason);

#ifdef __cplusplus
}
#endif

#endif
