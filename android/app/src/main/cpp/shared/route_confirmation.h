#ifndef DXX_ROUTE_CONFIRMATION_H
#define DXX_ROUTE_CONFIRMATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ROUTE_CONFIRMATION_CANONICAL_SEED             1u
#define ROUTE_CONFIRMATION_FIXED_HZ                   60
#define ROUTE_CONFIRMATION_MAX_OBJECTIVES             96
#define ROUTE_CONFIRMATION_DEFAULT_TIME_LIMIT_SECONDS 180u

struct object;
struct vms_vector;

enum route_confirmation_status {
	ROUTE_CONFIRMATION_IDLE = 0,
	ROUTE_CONFIRMATION_RUNNING = 1,
	ROUTE_CONFIRMATION_CONFIRMED = 2,
	ROUTE_CONFIRMATION_PARTIAL = 3,
	ROUTE_CONFIRMATION_FAILED = 4,
	ROUTE_CONFIRMATION_TIMEOUT = 5,
	ROUTE_CONFIRMATION_UNSUPPORTED = 6
};

typedef struct route_confirmation_objective_result {
	int route_step_index;
	int kind;
	int activation_kind;
	/* level_metadata_route_recovery_kind; recovery never completes its parent */
	int is_switch_restorer;
	int restored_wall_num;
	int trigger_num;
	int64_t completed_ticks;
	unsigned int completed_frame;
	char label[64];
} route_confirmation_objective_result;

typedef struct route_confirmation_rng_stream {
	unsigned int state;
	unsigned int call_count;
} route_confirmation_rng_stream;

typedef struct route_confirmation_rng_boundary {
	route_confirmation_rng_stream simulation;
	route_confirmation_rng_stream effects;
} route_confirmation_rng_boundary;

typedef struct route_confirmation_summary {
	int status;
	unsigned int seed;
	int fixed_hz;
	unsigned int frame_count;
	int64_t elapsed_ticks;
	int reactor_countdown_observed;
	int64_t reactor_destroyed_ticks;
	int reactor_countdown_ticks;
	int objective_count;
	int current_route_step_index;
	int current_kind;
	int current_activation_kind;
	route_confirmation_rng_boundary rng_start;
	route_confirmation_rng_boundary rng_end;
	int player_radius;
	int guidebot_radius;
	int effective_radius;
	int starts_from_current_state;
	/* -1 verifies the whole level; 0/1/2 verify only blue/red/gold pickup */
	int requested_key;
	int requested_exit_trigger;
	int transitioned_level;
	int transitioned_key_flags;
	int start_segment;
	int start_key_flags;
	int start_position[3];
	char problem[256];
	route_confirmation_objective_result objectives[ROUTE_CONFIRMATION_MAX_OBJECTIVES];
} route_confirmation_summary;

int route_confirmation_start(void);
int route_confirmation_start_from_current_state(void);
int route_confirmation_configure_key_goal(const char *key);
int route_confirmation_commit_player_position(void);
int route_confirmation_configure_exit_goal(const char *trigger);
int route_confirmation_handle_requested_exit_trigger(int objnum, int trigger);
int route_confirmation_run_exit_transition(void);
int route_confirmation_set_time_limit_seconds(unsigned int seconds);
/* Test-only speed variation; null selects the canonical 160 percent */
int route_confirmation_configure_speed(const char *percent);
int route_confirmation_speed_percent(void);
void route_confirmation_scale_path_velocity(const struct object *objp, struct vms_vector *velocity);
void route_confirmation_prepare_frame_time(void);
void route_confirmation_before_frame(void);
void route_confirmation_after_frame(void);
void route_confirmation_stop(void);
int route_confirmation_is_terminal(void);
int route_confirmation_drive_companion(struct object *objp);
int route_confirmation_handle_exit_trigger(int objnum);
int route_confirmation_handle_final_boss_endlevel(void);
const route_confirmation_summary *route_confirmation_get_summary(void);
const char *route_confirmation_status_name(int status);

#ifdef __cplusplus
}
#endif

#endif
