/*
 *
 * D1-in-D2 input-demo compatibility helpers.
 *
 */

#ifndef _D1_IN_D2_INPUT_DEMO_H
#define _D1_IN_D2_INPUT_DEMO_H

typedef struct player player;

/* Native D1 finishes the exit-trigger frame, including flythrough setup */
int d1_in_d2_input_demo_continue_level_exit(void);

int d1_in_d2_input_demo_result_robots_killed(player *current_player,
	int d2_baseline_valid, int d2_baseline);

#endif /* _D1_IN_D2_INPUT_DEMO_H */
