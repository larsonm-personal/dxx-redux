#ifndef DXX_ROUTE_CONFIRMATION_H
#define DXX_ROUTE_CONFIRMATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ROUTE_CONFIRMATION_CANONICAL_SEED 1u
#define ROUTE_CONFIRMATION_FIXED_HZ       60
#define ROUTE_CONFIRMATION_MAX_OBJECTIVES 96

struct object;

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
	int objective_count;
	int current_route_step_index;
	int current_kind;
	int current_activation_kind;
	route_confirmation_rng_boundary rng_start;
	route_confirmation_rng_boundary rng_end;
	int player_radius;
	int guidebot_radius;
	int effective_radius;
	char problem[256];
	route_confirmation_objective_result objectives[ROUTE_CONFIRMATION_MAX_OBJECTIVES];
} route_confirmation_summary;

int route_confirmation_start(void);
void route_confirmation_prepare_frame_time(void);
void route_confirmation_before_frame(void);
void route_confirmation_after_frame(void);
void route_confirmation_stop(void);
int route_confirmation_is_terminal(void);
int route_confirmation_drive_companion(struct object *objp);
int route_confirmation_handle_exit_trigger(int objnum);
const route_confirmation_summary *route_confirmation_get_summary(void);
const char *route_confirmation_status_name(int status);

#ifdef __cplusplus
}
#endif

#endif
