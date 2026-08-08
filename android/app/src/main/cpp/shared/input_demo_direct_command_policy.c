#include "input_demo_direct_command_policy.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int input_demo_direct_command_copy_error(const char *message,
                                                char *error, size_t error_size)
{
	if (error && error_size) {
		snprintf(error, error_size, "%s", message ? message : "direct command policy failed");
		error[error_size - 1] = '\0';
	}
	return 0;
}

static int input_demo_direct_command_fail(const input_demo_direct_command_policy *policy,
                                          const char *message, char *error, size_t error_size)
{
	input_demo_direct_command_copy_error(message, error, error_size);
	if (policy && policy->unload_replay_on_failure)
		input_demo_replay_unload();
	return 0;
}

static int input_demo_direct_command_callback_error(const char *operation,
                                                    int validate_only, const char *callback_error, char *error, size_t error_size)
{
	char message[512];

	if (callback_error && callback_error[0])
		snprintf(message, sizeof(message), "%s direct command %s failed: %s",
		         operation, validate_only ? "validation" : "application", callback_error);
	else
		snprintf(message, sizeof(message), "%s direct command %s failed",
		         operation, validate_only ? "validation" : "application");
	message[sizeof(message) - 1] = '\0';
	return input_demo_direct_command_copy_error(message, error, error_size);
}

static int input_demo_direct_command_is_game_specific(int kind)
{
	switch (kind) {
		case INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_GOAL:
		case INPUT_DEMO_REPLAY_DIRECT_COMMAND_DROP_MARKER:
		case INPUT_DEMO_REPLAY_DIRECT_COMMAND_DROP_CURRENT_WEAPON:
		case INPUT_DEMO_REPLAY_DIRECT_COMMAND_DROP_SECONDARY_WEAPON:
		case INPUT_DEMO_REPLAY_DIRECT_COMMAND_DROP_FLAG:
		case INPUT_DEMO_REPLAY_DIRECT_COMMAND_ESCORT_RELEASE_CONTROL:
		case INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_SPAWN:
		case INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_FIND_SECRET:
		case INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_FIND_UNEXPLORED:
		case INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_WARP_TO_ME:
		case INPUT_DEMO_REPLAY_DIRECT_COMMAND_SELECT_WEAPON_EXACT:
			return 1;
		default:
			return 0;
	}
}

static int input_demo_direct_command_dispatch(
    const input_demo_direct_command_policy *policy,
    input_demo_direct_command_phase phase,
    const input_demo_replay_direct_command_event *event,
    int validate_only, char *error, size_t error_size)
{
	char callback_error[256] = "";

	if (phase == INPUT_DEMO_DIRECT_COMMAND_PHASE_DEAD) {
		if (event->kind != INPUT_DEMO_REPLAY_DIRECT_COMMAND_DEATH_ABORT)
			return 1;
		if (!policy || !policy->apply_death_abort)
			return input_demo_direct_command_copy_error(
			    "death abort direct command is unsupported", error, error_size);
		if (!policy->apply_death_abort(policy->context, validate_only,
		                               callback_error, sizeof(callback_error)))
			return input_demo_direct_command_callback_error("death abort", validate_only,
			                                                callback_error, error, error_size);
		return 1;
	}

	if (event->kind == INPUT_DEMO_REPLAY_DIRECT_COMMAND_DEATH_ABORT)
		return 1;
	if (event->kind == INPUT_DEMO_REPLAY_DIRECT_COMMAND_CHANGE_DIFFICULTY) {
		if (!policy || !policy->change_difficulty)
			return input_demo_direct_command_copy_error(
			    "change difficulty direct command is unsupported", error, error_size);
		if (!policy->change_difficulty(policy->context, event->value0, validate_only,
		                               callback_error, sizeof(callback_error)))
			return input_demo_direct_command_callback_error("change difficulty", validate_only,
			                                                callback_error, error, error_size);
		return 1;
	}
	if (input_demo_direct_command_is_game_specific(event->kind)) {
		if (!policy || !policy->apply_game_specific)
			return input_demo_direct_command_copy_error(
			    "game-specific direct command is unsupported", error, error_size);
		if (!policy->apply_game_specific(policy->context, event, validate_only,
		                                 callback_error, sizeof(callback_error)))
			return input_demo_direct_command_callback_error("game-specific", validate_only,
			                                                callback_error, error, error_size);
		return 1;
	}
	return input_demo_direct_command_copy_error(
	    "unknown typed direct command is unsupported", error, error_size);
}

int input_demo_direct_command_apply_current_frame(
    const input_demo_direct_command_policy *policy,
    input_demo_direct_command_phase phase,
    char *error, size_t error_size)
{
	input_demo_replay_direct_command_event *events = NULL;
	uint32_t direct_command_count = 0;
	uint32_t direct_command_index;
	char policy_error[512] = "";
	char replay_error[256] = "";

	if (error && error_size)
		error[0] = '\0';
	if (!input_demo_replay_is_loaded())
		return 1;
	if (phase != INPUT_DEMO_DIRECT_COMMAND_PHASE_DEAD &&
	    phase != INPUT_DEMO_DIRECT_COMMAND_PHASE_GAMEPLAY)
		return input_demo_direct_command_fail(policy,
		                                      "invalid direct command replay phase", error, error_size);
	if (!input_demo_replay_get_current_frame_direct_command_count(
	        &direct_command_count, replay_error, sizeof(replay_error)))
		return input_demo_direct_command_fail(policy, replay_error, error, error_size);
	if (!direct_command_count)
		return 1;
#if SIZE_MAX <= UINT32_MAX
	if (direct_command_count > SIZE_MAX / sizeof(*events))
		return input_demo_direct_command_fail(policy,
		                                      "direct command event count is too large", error, error_size);
#endif
	events = (input_demo_replay_direct_command_event *) calloc(
	    direct_command_count, sizeof(*events));
	if (!events)
		return input_demo_direct_command_fail(policy,
		                                      "could not allocate direct command event batch", error, error_size);

	for (direct_command_index = 0; direct_command_index != direct_command_count;
	     ++direct_command_index) {
		input_demo_replay_direct_command_event_clear(&events[direct_command_index]);
		if (!input_demo_replay_get_current_frame_direct_command_event(
		        direct_command_index, &events[direct_command_index],
		        replay_error, sizeof(replay_error))) {
			free(events);
			return input_demo_direct_command_fail(policy, replay_error, error, error_size);
		}
	}

	for (direct_command_index = 0; direct_command_index != direct_command_count;
	     ++direct_command_index) {
		if (!input_demo_direct_command_dispatch(policy, phase,
		                                        &events[direct_command_index], 1, policy_error, sizeof(policy_error))) {
			free(events);
			return input_demo_direct_command_fail(policy, policy_error, error, error_size);
		}
	}
	for (direct_command_index = 0; direct_command_index != direct_command_count;
	     ++direct_command_index) {
		if (!input_demo_direct_command_dispatch(policy, phase,
		                                        &events[direct_command_index], 0, policy_error, sizeof(policy_error))) {
			free(events);
			return input_demo_direct_command_fail(policy, policy_error, error, error_size);
		}
	}

	free(events);
	return 1;
}
