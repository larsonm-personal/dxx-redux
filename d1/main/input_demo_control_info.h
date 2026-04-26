#ifndef D1_INPUT_DEMO_CONTROL_INFO_H
#define D1_INPUT_DEMO_CONTROL_INFO_H

#include <string.h>

#include "kconfig.h"
#include "input_demo_controls.h"

static inline int input_demo_control_info_game(void)
{
	return INPUT_DEMO_GAME_D1;
}

static inline void input_demo_control_state_from_control_info(input_demo_control_state *state,
	input_demo_control_pulse *pulse, const control_info *controls)
{
	input_demo_control_state_clear(state);
	input_demo_control_pulse_clear(pulse);
	state->pitch_time = controls->pitch_time;
	state->heading_time = controls->heading_time;
	state->bank_time = controls->bank_time;
	state->forward_thrust_time = controls->forward_thrust_time;
	state->sideways_thrust_time = controls->sideways_thrust_time;
	state->vertical_thrust_time = controls->vertical_thrust_time;
	state->fire_primary_state = controls->fire_primary_state;
	state->fire_secondary_state = controls->fire_secondary_state;
	state->rear_view_state = controls->rear_view_state;
	state->automap_state = controls->automap_state;
	pulse->fire_primary_count = controls->fire_primary_count;
	pulse->fire_secondary_count = controls->fire_secondary_count;
	pulse->fire_flare_count = controls->fire_flare_count;
	pulse->drop_bomb_count = controls->drop_bomb_count;
	pulse->cycle_primary_count = controls->cycle_primary_count;
	pulse->cycle_secondary_count = controls->cycle_secondary_count;
	pulse->select_weapon_count = controls->select_weapon_count;
	pulse->rear_view_count = controls->rear_view_count;
	pulse->automap_count = controls->automap_count;
}

static inline void input_demo_control_info_from_state(control_info *controls,
	const input_demo_control_state *state, const input_demo_control_pulse *pulse)
{
	memset(controls, 0, sizeof(*controls));
	controls->pitch_time = (fix)state->pitch_time;
	controls->heading_time = (fix)state->heading_time;
	controls->bank_time = (fix)state->bank_time;
	controls->forward_thrust_time = (fix)state->forward_thrust_time;
	controls->sideways_thrust_time = (fix)state->sideways_thrust_time;
	controls->vertical_thrust_time = (fix)state->vertical_thrust_time;
	controls->fire_primary_state = state->fire_primary_state;
	controls->fire_secondary_state = state->fire_secondary_state;
	controls->rear_view_state = state->rear_view_state;
	controls->automap_state = state->automap_state;
	controls->fire_primary_count = pulse->fire_primary_count;
	controls->fire_secondary_count = pulse->fire_secondary_count;
	controls->fire_flare_count = pulse->fire_flare_count;
	controls->drop_bomb_count = pulse->drop_bomb_count;
	controls->cycle_primary_count = pulse->cycle_primary_count;
	controls->cycle_secondary_count = pulse->cycle_secondary_count;
	controls->select_weapon_count = pulse->select_weapon_count;
	controls->rear_view_count = pulse->rear_view_count;
	controls->automap_count = pulse->automap_count;
}

#endif