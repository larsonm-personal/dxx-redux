#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <string>
#include <vector>

#include "input_demo_direct_command_policy.h"
#include "input_demo_rng_trace.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "maths.h"
#ifdef __cplusplus
}
#endif

static int report_failure(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

static int report_failure_string(const std::string &message)
{
	return report_failure(message.c_str());
}

static bool read_text_file(const char *path, std::string *text)
{
	FILE *f = fopen(path, "rb");
	char buffer[256];
	size_t bytes;

	if (!f)
		return false;
	text->clear();
	while ((bytes = fread(buffer, 1, sizeof(buffer), f)) != 0)
		text->append(buffer, bytes);
	fclose(f);
	return true;
}

static const char *input_demo_test_game_name(void)
{
#if defined(INPUT_DEMO_TEST_D2)
	return "d2";
#else
	return "d1";
#endif
}

static int input_demo_test_game_id(void)
{
#if defined(INPUT_DEMO_TEST_D2)
	return INPUT_DEMO_GAME_D2;
#else
	return INPUT_DEMO_GAME_D1;
#endif
}

static const char *input_demo_test_rng_mode(void)
{
	return d_rand_get_replay_mode() == D_RAND_REPLAY_MODE_LCG_STATE ? "lcg_state" : "libc_reseed";
}

static void fill_test_checkpoint_data(unsigned char *data, size_t data_size)
{
	size_t i;

	memset(data, 0, data_size);
	if (data_size >= 8) {
		data[0] = 'D';
		data[1] = 'G';
		data[2] = 'S';
		data[3] = 'S';
		data[4] = 24;
	}
	for (i = 8; i != data_size; ++i)
		data[i] = (i % 32) == 0 ? (unsigned char) i : 0;
}

static void fill_test_player_cfg(input_demo_player_cfg *player_cfg)
{
#if defined(INPUT_DEMO_TEST_D2)
	static const uint8_t primary_order[] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 255 };
	static const uint8_t secondary_order[] = { 9, 8, 4, 3, 1, 5, 0, 255, 7, 6, 2 };
#else
	static const uint8_t primary_order[] = { 4, 3, 2, 1, 0, 255, 16 };
	static const uint8_t secondary_order[] = { 4, 3, 1, 0, 255, 2 };
#endif
	size_t i;

	input_demo_player_cfg_clear(player_cfg);
	player_cfg->auto_leveling = 1;
	player_cfg->persistent_debris = 1;
#if defined(INPUT_DEMO_TEST_D2)
	player_cfg->has_headlight_active_default = 1;
	player_cfg->headlight_active_default = 0;
#endif
	player_cfg->no_fire_autoselect = 1;
	player_cfg->cycle_autoselect_only = 1;
	player_cfg->select_after_fire = 0;
	player_cfg->classic_autoselect_weapon = 1;
	player_cfg->original_homing = 1;
	player_cfg->primary_order_count = (uint8_t) (sizeof(primary_order) / sizeof(primary_order[0]));
	player_cfg->secondary_order_count = (uint8_t) (sizeof(secondary_order) / sizeof(secondary_order[0]));
	for (i = 0; i != sizeof(primary_order) / sizeof(primary_order[0]); ++i)
		player_cfg->primary_order[i] = primary_order[i];
	for (i = 0; i != sizeof(secondary_order) / sizeof(secondary_order[0]); ++i)
		player_cfg->secondary_order[i] = secondary_order[i];
}

static void fill_test_checkpoint_escort_state(input_demo_checkpoint_escort_state *escort_state)
{
	input_demo_checkpoint_escort_state_clear(escort_state);
	escort_state->valid = 1;
	escort_state->buddy_allowed_to_talk = 0;
	escort_state->buddy_last_seen_player = 1077412;
	escort_state->buddy_last_player_path_created = 899154;
	escort_state->escort_kill_object = 42;
	escort_state->escort_last_path_created = 1077412;
	escort_state->escort_goal_object = 11;
	escort_state->escort_special_goal = -1;
	escort_state->escort_goal_index = 31;
	escort_state->buddy_messages_suppressed = 1;
	escort_state->buddy_sorry_time = 262144;
	escort_state->looking_for_marker = -1;
	escort_state->last_buddy_key = 7;
	escort_state->last_buddy_message_time = 65536;
	escort_state->last_come_back_message_time = 899154;
	escort_state->buddy_last_missile_time = 524288;
	escort_state->escort_owner_player = -1;
}

static void fill_test_frame_state(input_demo_result *result, int frame_index)
{
	input_demo_result_clear(result);
	result->has_game_time64 = 1;
	result->game_time64 = 3276 + (int64_t) frame_index * 3276;
	result->player0.present = 1;
	result->player0.energy = 67 - frame_index;
	result->player0.shields = 42 + frame_index;
	result->player0.score = 12500 + frame_index;
	result->player0.lives = 3;
	result->player0.laser_level = 1 + frame_index;
	result->player0.primary_weapon = 0;
	result->player0.secondary_weapon = frame_index;
	result->player0.primary_ammo[1] = (uint16_t) (200 + frame_index);
	result->player0.secondary_ammo[0] = (uint16_t) (4 + frame_index);
	result->position.present = 1;
	result->position.segment = 142 + frame_index;
	result->position.x = 12345678 + frame_index;
	result->position.y = -8765432 - frame_index;
	result->position.z = 3456789 + frame_index;
	result->position.has_forward = 1;
	result->position.fx = 65536;
	result->level_summary.present = 1;
	result->level_summary.robots_alive = 23 - frame_index;
	result->level_summary.robots_killed = 8 + frame_index;
	result->level_summary.hostages_remaining = 2;
	result->level_summary.powerups_remaining = 15 - frame_index;
	result->level_summary.control_center_destroyed = frame_index == 2 ? 1 : 0;
}

static int expect_test_player_cfg(const input_demo_player_cfg *player_cfg)
{
	if (!player_cfg)
		return report_failure("replay player_cfg output missing");
	if (player_cfg->auto_leveling != 1 || player_cfg->persistent_debris != 1 ||
	    player_cfg->no_fire_autoselect != 1 || player_cfg->cycle_autoselect_only != 1 ||
	    player_cfg->select_after_fire != 0 || player_cfg->classic_autoselect_weapon != 1 ||
	    player_cfg->original_homing != 1)
		return report_failure("replay player_cfg scalar mismatch");
#if defined(INPUT_DEMO_TEST_D2)
	if (!player_cfg->has_headlight_active_default || player_cfg->headlight_active_default != 0 ||
	    player_cfg->primary_order_count != 11 || player_cfg->secondary_order_count != 11 ||
	    player_cfg->primary_order[0] != 9 || player_cfg->primary_order[10] != 255 ||
	    player_cfg->secondary_order[0] != 9 || player_cfg->secondary_order[10] != 2)
		return report_failure("replay D2 player_cfg mismatch");
#else
	if (player_cfg->has_headlight_active_default ||
	    player_cfg->primary_order_count != 7 || player_cfg->secondary_order_count != 6 ||
	    player_cfg->primary_order[0] != 4 || player_cfg->primary_order[6] != 16 ||
	    player_cfg->secondary_order[0] != 4 || player_cfg->secondary_order[5] != 2)
		return report_failure("replay D1 player_cfg mismatch");
#endif
	return 0;
}

static int expect_test_checkpoint_escort_state(const input_demo_checkpoint_escort_state *escort_state)
{
	if (!escort_state || !escort_state->valid)
		return report_failure("replay checkpoint escort state missing");
	if (escort_state->buddy_allowed_to_talk != 0 ||
	    escort_state->buddy_last_seen_player != 1077412 ||
	    escort_state->buddy_last_player_path_created != 899154 ||
	    escort_state->escort_kill_object != 42 ||
	    escort_state->escort_last_path_created != 1077412 ||
	    escort_state->escort_goal_object != 11 ||
	    escort_state->escort_special_goal != -1 ||
	    escort_state->escort_goal_index != 31 ||
	    escort_state->buddy_messages_suppressed != 1 ||
	    escort_state->buddy_sorry_time != 262144 ||
	    escort_state->looking_for_marker != -1 ||
	    escort_state->last_buddy_key != 7 ||
	    escort_state->last_buddy_message_time != 65536 ||
	    escort_state->last_come_back_message_time != 899154 ||
	    escort_state->buddy_last_missile_time != 524288 ||
	    escort_state->escort_owner_player != -1)
		return report_failure("replay checkpoint escort state mismatch");
	return 0;
}

static void fill_test_checkpoint_thief_state(input_demo_checkpoint_thief_state *thief_state)
{
	input_demo_checkpoint_thief_state_clear(thief_state);
	thief_state->valid = 1;
	thief_state->stolen_item_index = 5;
	thief_state->re_init_thief_time = 1703936;
	thief_state->last_thief_hit_time = 1507328;
}

static int expect_test_checkpoint_thief_state(const input_demo_checkpoint_thief_state *thief_state)
{
	if (!thief_state || !thief_state->valid)
		return report_failure("replay checkpoint thief state missing");
	if (thief_state->stolen_item_index != 5 ||
	    thief_state->re_init_thief_time != 1703936 ||
	    thief_state->last_thief_hit_time != 1507328)
		return report_failure("replay checkpoint thief state mismatch");
	return 0;
}

static void fill_test_checkpoint_collision_delay(int *has_last_play_time,
                                                 int64_t *last_play_time)
{
	if (has_last_play_time)
		*has_last_play_time = 1;
	if (last_play_time)
		*last_play_time = 196608;
}

static int expect_test_checkpoint_collision_delay(int has_last_play_time,
                                                  int64_t last_play_time)
{
	if (!has_last_play_time)
		return report_failure("replay checkpoint collision delay state missing");
	if (last_play_time != 196608)
		return report_failure("replay checkpoint collision delay state mismatch");
	return 0;
}

static int make_test_dir(const char *path)
{
#if defined(_WIN32)
	if (_mkdir(path) == 0 || errno == EEXIST)
		return 1;
#else
	if (mkdir(path, 0777) == 0 || errno == EEXIST)
		return 1;
#endif
	return 0;
}

static void remove_test_dir(const char *path)
{
#if defined(_WIN32)
	_rmdir(path);
#else
	rmdir(path);
#endif
}

static int write_test_fixture(const char *path)
{
	input_demo_recorder_settings settings;
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	input_demo_result frame_state;
	char error[256] = "";

	input_demo_recorder_settings_clear(&settings);
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 1;
	settings.difficulty = 2;
	settings.rng_mode = input_demo_test_rng_mode();
	settings.has_player_cfg = 1;
	settings.record_per_frame_state = 1;
	fill_test_player_cfg(&settings.player_cfg);
	if (!input_demo_recorder_start(&settings, error, sizeof(error)))
		return report_failure_string(std::string("recorder start failed: ") + error);
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	state.forward_thrust_time = 44;
	fill_test_frame_state(&frame_state, 0);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0, &frame_state, NULL, error, sizeof(error)))
		return report_failure_string(std::string("capture frame 0 failed: ") + error);
	if (!input_demo_recorder_stage_direct_command_death_abort(error, sizeof(error)))
		return report_failure_string(std::string("stage death abort command failed: ") + error);
	input_demo_control_pulse_clear(&pulse);
	pulse.fire_primary_count = 1;
	fill_test_frame_state(&frame_state, 1);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0, &frame_state, NULL, error, sizeof(error)))
		return report_failure_string(std::string("capture frame 1 failed: ") + error);
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	fill_test_frame_state(&frame_state, 2);
	if (!input_demo_recorder_capture_frame(4000, &state, &pulse, 102, 1, 3, &frame_state, NULL, error, sizeof(error)))
		return report_failure_string(std::string("capture frame 2 failed: ") + error);
	if (!input_demo_recorder_flush(path, error, sizeof(error)))
		return report_failure_string(std::string("recorder flush failed: ") + error);
	return 0;
}

static int write_checkpoint_test_fixture(const char *path)
{
	unsigned char checkpoint_data[256];
	input_demo_recorder_settings settings;
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	input_demo_result result;
	input_demo_result frame_state;
	char error[256] = "";

	fill_test_checkpoint_data(checkpoint_data, sizeof(checkpoint_data));

	input_demo_recorder_settings_clear(&settings);
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 1;
	settings.difficulty = 2;
	settings.rng_mode = input_demo_test_rng_mode();
	settings.has_player_cfg = 1;
	settings.record_per_frame_state = 1;
	fill_test_player_cfg(&settings.player_cfg);
	settings.checkpoint_save_name = "inputdemo_start.dgss";
	settings.checkpoint_data = checkpoint_data;
	settings.checkpoint_size = sizeof(checkpoint_data);
	settings.has_checkpoint_start_gt = 1;
	settings.checkpoint_start_gt = 124125;
	fill_test_checkpoint_collision_delay(&settings.has_checkpoint_collision_delay_last_play_time,
	                                     &settings.checkpoint_collision_delay_last_play_time);
	fill_test_checkpoint_escort_state(&settings.checkpoint_escort_state);
	fill_test_checkpoint_thief_state(&settings.checkpoint_thief_state);
	if (!input_demo_recorder_start(&settings, error, sizeof(error)))
		return report_failure_string(std::string("checkpoint recorder start failed: ") + error);
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	state.forward_thrust_time = 44;
	fill_test_frame_state(&frame_state, 0);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0, &frame_state, NULL, error, sizeof(error)))
		return report_failure_string(std::string("checkpoint capture frame failed: ") + error);
	input_demo_result_clear(&result);
	snprintf(result.game, sizeof(result.game), "%s", input_demo_test_game_name());
	snprintf(result.mission, sizeof(result.mission), "%s", input_demo_test_game_name());
	result.level = 1;
	result.difficulty = 2;
	result.frame_count = 1;
	result.has_game_time64 = 1;
	result.game_time64 = 124130;
	if (!input_demo_recorder_flush_with_result(path, &result, error, sizeof(error)))
		return report_failure_string(std::string("checkpoint recorder flush failed: ") + error);
	return 0;
}

static int expect_replay_loader(void)
{
	const char *dir = "test_input_demo_replay_fixture";
	const std::string demo_path = std::string(dir) + "/replay.dximdemo";
	const std::string trace_path = demo_path + INPUT_DEMO_RNG_TRACE_SUFFIX;
	const std::string actual_result_path = demo_path + ".actual.json";
	input_demo_replay_frame frame;
	input_demo_replay_direct_command_event direct_command_event;
	input_demo_player_cfg player_cfg;
	input_demo_result actual_result;
	uint32_t direct_command_count = 0;
	char error[256] = "";

	if (!make_test_dir(dir))
		return report_failure("could not create replay test directory");
	if (write_test_fixture(demo_path.c_str()))
		return 1;
	if (!input_demo_replay_load(demo_path.c_str(), error, sizeof(error)))
		return report_failure_string(std::string("replay load failed: ") + error);
	if (!input_demo_replay_is_loaded())
		return report_failure("replay should be loaded");
	if (input_demo_replay_game() != input_demo_test_game_id())
		return report_failure("replay game id mismatch");
	if (!input_demo_replay_mission() || std::string(input_demo_replay_mission()) != input_demo_test_game_name())
		return report_failure("replay mission mismatch");
	if (input_demo_replay_level() != 1)
		return report_failure("replay level mismatch");
	if (input_demo_replay_difficulty() != 2)
		return report_failure("replay difficulty mismatch");
	if (!input_demo_replay_start_mode() || std::string(input_demo_replay_start_mode()) != "new_level")
		return report_failure("replay start_mode mismatch");
	if (!input_demo_replay_rng_mode() || std::string(input_demo_replay_rng_mode()) != input_demo_test_rng_mode())
		return report_failure("replay rng_mode mismatch");
	if (!input_demo_replay_has_player_cfg())
		return report_failure("replay should expose player_cfg metadata");
	if (!input_demo_replay_get_player_cfg(&player_cfg))
		return report_failure("replay player_cfg getter failed");
	if (expect_test_player_cfg(&player_cfg))
		return 1;
	if (input_demo_replay_frame_count() != 3)
		return report_failure("replay frame count mismatch");
	if (input_demo_replay_next_frame_index() != 0)
		return report_failure("replay cursor should start at frame 0");
	if (input_demo_replay_is_finished())
		return report_failure("replay should not start finished");
	{
		input_demo_checkpoint_escort_state escort_state;
		input_demo_checkpoint_thief_state thief_state;
		int64_t collision_delay_last_play_time = 0;

		input_demo_checkpoint_escort_state_clear(&escort_state);
		if (input_demo_replay_get_checkpoint_escort_state(&escort_state))
			return report_failure("new_level replay should not expose checkpoint escort state");
		input_demo_checkpoint_thief_state_clear(&thief_state);
		if (input_demo_replay_get_checkpoint_thief_state(&thief_state))
			return report_failure("new_level replay should not expose checkpoint thief state");
		if (input_demo_replay_get_checkpoint_collision_delay_last_play_time(&collision_delay_last_play_time))
			return report_failure("new_level replay should not expose checkpoint collision delay state");
	}
	if (!input_demo_replay_actual_result_path() || std::string(input_demo_replay_actual_result_path()) != actual_result_path)
		return report_failure("replay actual result path mismatch");

	if (!input_demo_replay_get_current_frame(&frame, error, sizeof(error)))
		return report_failure_string(std::string("replay current frame 0 failed: ") + error);
	if (frame.frame != 0 || frame.frame_time != 3276 || frame.state.forward_thrust_time != 44 ||
	    frame.pulse.fire_primary_count != 0 || frame.rng_state != 100 || frame.has_rng_call_count ||
	    !frame.has_state || frame.state_result.player0.score != 12500 || frame.state_result.position.segment != 142)
		return report_failure("replay frame 0 mismatch");
	if (!input_demo_replay_get_current_frame_direct_command_count(&direct_command_count, error, sizeof(error)))
		return report_failure_string(std::string("replay frame 0 direct command count failed: ") + error);
	if (direct_command_count != 0)
		return report_failure("replay frame 0 direct command count mismatch");
	if (!input_demo_replay_advance_frame(error, sizeof(error)))
		return report_failure_string(std::string("replay advance 0 failed: ") + error);

	if (!input_demo_replay_get_current_frame(&frame, error, sizeof(error)))
		return report_failure_string(std::string("replay current frame 1 failed: ") + error);
	if (frame.frame != 1 || frame.frame_time != 3276 || frame.state.forward_thrust_time != 44 ||
	    frame.pulse.fire_primary_count != 1 || frame.rng_state != 100 || frame.has_rng_call_count ||
	    !frame.has_state || frame.state_result.player0.score != 12501 || frame.state_result.position.segment != 143)
		return report_failure("replay frame 1 mismatch");
	if (!input_demo_replay_get_current_frame_direct_command_count(&direct_command_count, error, sizeof(error)))
		return report_failure_string(std::string("replay frame 1 direct command count failed: ") + error);
	if (direct_command_count != 1)
		return report_failure("replay frame 1 direct command count mismatch");
	input_demo_replay_direct_command_event_clear(&direct_command_event);
	if (!input_demo_replay_get_current_frame_direct_command_event(0, &direct_command_event, error, sizeof(error)))
		return report_failure_string(std::string("replay frame 1 direct command event failed: ") + error);
	if (direct_command_event.kind != INPUT_DEMO_REPLAY_DIRECT_COMMAND_DEATH_ABORT)
		return report_failure("replay frame 1 death abort direct command mismatch");
	if (!input_demo_replay_advance_frame(error, sizeof(error)))
		return report_failure_string(std::string("replay advance 1 failed: ") + error);

	if (!input_demo_replay_get_current_frame(&frame, error, sizeof(error)))
		return report_failure_string(std::string("replay current frame 2 failed: ") + error);
	if (frame.frame != 2 || frame.frame_time != 4000 || frame.state.forward_thrust_time != 0 ||
	    frame.pulse.fire_primary_count != 0 || frame.rng_state != 102 || !frame.has_rng_call_count ||
	    frame.rng_call_count != 3 || !frame.has_state || frame.state_result.player0.score != 12502 ||
	    frame.state_result.level_summary.control_center_destroyed != 1)
		return report_failure("replay frame 2 mismatch");
	if (!input_demo_replay_advance_frame(error, sizeof(error)))
		return report_failure_string(std::string("replay advance 2 failed: ") + error);

	if (!input_demo_replay_is_finished())
		return report_failure("replay should be finished after advancing all frames");
	if (input_demo_replay_get_current_frame(&frame, error, sizeof(error)))
		return report_failure("replay unexpectedly returned a frame after end of stream");
	input_demo_result_clear(&actual_result);
	snprintf(actual_result.game, sizeof(actual_result.game), "%s", input_demo_test_game_name());
	snprintf(actual_result.mission, sizeof(actual_result.mission), "%s", input_demo_test_game_name());
	actual_result.level = 1;
	actual_result.difficulty = 2;
	actual_result.frame_count = 3;
	if (!input_demo_replay_compare_result(&actual_result, error, sizeof(error)))
		return report_failure_string(std::string("replay embedded result compare failed: ") + error);
	input_demo_replay_unload();
	remove(demo_path.c_str());
	remove(trace_path.c_str());
	remove(actual_result_path.c_str());
	remove_test_dir(dir);
	return 0;
}

static int expect_checkpoint_replay_loader(void)
{
	const char *dir = "test_input_demo_replay_checkpoint_fixture";
	const std::string demo_path = std::string(dir) + "/checkpoint_replay.dximdemo";
	const std::string trace_path = demo_path + INPUT_DEMO_RNG_TRACE_SUFFIX;
	const std::string actual_result_path = demo_path + ".actual.json";
	input_demo_replay_frame frame;
	input_demo_player_cfg player_cfg;
	input_demo_result actual_result;
	char error[256] = "";

	if (!make_test_dir(dir))
		return report_failure("could not create checkpoint replay test directory");
	if (write_checkpoint_test_fixture(demo_path.c_str()))
		return 1;
	{
		unsigned char expected_checkpoint[256];
		std::string demo_text;

		fill_test_checkpoint_data(expected_checkpoint, sizeof(expected_checkpoint));
		if (!read_text_file(demo_path.c_str(), &demo_text))
			return report_failure("could not read checkpoint replay fixture");
		if (demo_text.find("\"compression\":\"zlib\"") == std::string::npos)
			return report_failure_string(std::string("checkpoint replay fixture was not compressed: ") + demo_text);
	}
	if (!input_demo_replay_load(demo_path.c_str(), error, sizeof(error)))
		return report_failure_string(std::string("checkpoint replay load failed: ") + error);
	if (!input_demo_replay_is_loaded())
		return report_failure("checkpoint replay should be loaded");
	if (!input_demo_replay_start_mode() || std::string(input_demo_replay_start_mode()) != "save_checkpoint")
		return report_failure("checkpoint replay start_mode mismatch");
	if (!input_demo_replay_has_player_cfg() || !input_demo_replay_get_player_cfg(&player_cfg))
		return report_failure("checkpoint replay player_cfg missing");
	if (expect_test_player_cfg(&player_cfg))
		return 1;
	if (!input_demo_replay_has_checkpoint())
		return report_failure("checkpoint replay should retain checkpoint payload");
	if (!input_demo_replay_checkpoint_save_name() ||
	    std::string(input_demo_replay_checkpoint_save_name()) != "inputdemo_start.dgss")
		return report_failure("checkpoint replay save_name mismatch");
	if (input_demo_replay_checkpoint_size() != 256)
		return report_failure("checkpoint replay size mismatch");
	{
		unsigned char expected_checkpoint[256];

		fill_test_checkpoint_data(expected_checkpoint, sizeof(expected_checkpoint));
		if (!input_demo_replay_checkpoint_data() ||
		    memcmp(input_demo_replay_checkpoint_data(), expected_checkpoint, sizeof(expected_checkpoint)) != 0)
			return report_failure("checkpoint replay bytes mismatch");
	}
	if (input_demo_replay_checkpoint_start_gt() != 124125)
		return report_failure("checkpoint replay timing metadata mismatch");
	{
		input_demo_checkpoint_escort_state escort_state;
		input_demo_checkpoint_thief_state thief_state;
		int have_collision_delay_last_play_time = 0;
		int64_t collision_delay_last_play_time = 0;

		input_demo_checkpoint_escort_state_clear(&escort_state);
		if (!input_demo_replay_get_checkpoint_escort_state(&escort_state))
			return report_failure("checkpoint replay escort state missing");
		if (expect_test_checkpoint_escort_state(&escort_state))
			return 1;
		input_demo_checkpoint_thief_state_clear(&thief_state);
		if (!input_demo_replay_get_checkpoint_thief_state(&thief_state))
			return report_failure("checkpoint replay thief state missing");
		if (expect_test_checkpoint_thief_state(&thief_state))
			return 1;
		have_collision_delay_last_play_time =
		    input_demo_replay_get_checkpoint_collision_delay_last_play_time(&collision_delay_last_play_time);
		if (expect_test_checkpoint_collision_delay(have_collision_delay_last_play_time,
		                                           collision_delay_last_play_time))
			return 1;
	}
	if (!input_demo_replay_actual_result_path() || std::string(input_demo_replay_actual_result_path()) != actual_result_path)
		return report_failure("checkpoint replay actual result path mismatch");
	if (!input_demo_replay_get_current_frame(&frame, error, sizeof(error)))
		return report_failure_string(std::string("checkpoint replay current frame failed: ") + error);
	if (frame.frame != 0 || frame.frame_time != 3276 || frame.state.forward_thrust_time != 44 || frame.rng_state != 100 ||
	    !frame.has_state || frame.state_result.player0.score != 12500)
		return report_failure("checkpoint replay frame mismatch");
	input_demo_result_clear(&actual_result);
	snprintf(actual_result.game, sizeof(actual_result.game), "%s", input_demo_test_game_name());
	snprintf(actual_result.mission, sizeof(actual_result.mission), "%s", input_demo_test_game_name());
	actual_result.level = 1;
	actual_result.difficulty = 2;
	actual_result.frame_count = 1;
	actual_result.has_game_time64 = 1;
	actual_result.game_time64 = 124130;
	if (!input_demo_replay_compare_result(&actual_result, error, sizeof(error)))
		return report_failure_string(std::string("checkpoint replay result compare failed: ") + error);
	input_demo_replay_unload();
	remove(demo_path.c_str());
	remove(trace_path.c_str());
	remove(actual_result_path.c_str());
	remove_test_dir(dir);
	return 0;
}

enum direct_command_policy_fixture_kind {
	DIRECT_COMMAND_POLICY_FIXTURE_ZERO,
	DIRECT_COMMAND_POLICY_FIXTURE_ONE,
	DIRECT_COMMAND_POLICY_FIXTURE_MULTIPLE,
	DIRECT_COMMAND_POLICY_FIXTURE_UNKNOWN_LATE,
	DIRECT_COMMAND_POLICY_FIXTURE_MALFORMED_LATE,
	DIRECT_COMMAND_POLICY_FIXTURE_UNSUPPORTED
};

typedef struct direct_command_policy_test_context {
	std::vector<std::string> calls;
	int mutation_count;
	int reject_game_kind;
} direct_command_policy_test_context;

static int direct_command_policy_fixture_error(const char *operation, const char *error)
{
	input_demo_recorder_cancel();
	return report_failure_string(std::string(operation) + ": " + (error ? error : ""));
}

static int write_direct_command_policy_fixture(const char *path,
	direct_command_policy_fixture_kind fixture_kind)
{
	input_demo_recorder_settings settings;
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	char error[256] = "";

	input_demo_recorder_settings_clear(&settings);
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 1;
	settings.difficulty = 2;
	settings.rng_mode = input_demo_test_rng_mode();
	if (!input_demo_recorder_start(&settings, error, sizeof(error)))
		return direct_command_policy_fixture_error("direct command policy recorder start failed", error);

	if (fixture_kind == DIRECT_COMMAND_POLICY_FIXTURE_ZERO) {
		if (!input_demo_recorder_stage_frame_event_json(
			"{\"kind\":\"score\",\"delta\":1}", error, sizeof(error)))
			return direct_command_policy_fixture_error("stage zero-command durable event failed", error);
	} else if (fixture_kind == DIRECT_COMMAND_POLICY_FIXTURE_ONE) {
		if (!input_demo_recorder_stage_direct_command_change_difficulty(3, error, sizeof(error)))
			return direct_command_policy_fixture_error("stage one difficulty command failed", error);
	} else if (fixture_kind == DIRECT_COMMAND_POLICY_FIXTURE_MULTIPLE) {
		if (!input_demo_recorder_stage_direct_command_death_abort(error, sizeof(error)))
			return direct_command_policy_fixture_error("stage death abort command failed", error);
		if (!input_demo_recorder_stage_frame_event_json(
			"{\"kind\":\"score\",\"delta\":2}", error, sizeof(error)))
			return direct_command_policy_fixture_error("stage interleaved durable event failed", error);
		if (!input_demo_recorder_stage_direct_command_change_difficulty(3, error, sizeof(error)))
			return direct_command_policy_fixture_error("stage multiple difficulty command failed", error);
		if (!input_demo_recorder_stage_direct_command_guidebot_goal(17, 1, error, sizeof(error)))
			return direct_command_policy_fixture_error("stage guidebot goal command failed", error);
		if (!input_demo_recorder_stage_direct_command_drop_marker(1, "marker text", error, sizeof(error)))
			return direct_command_policy_fixture_error("stage marker command failed", error);
		if (!input_demo_recorder_stage_direct_command_drop_current_weapon(error, sizeof(error)))
			return direct_command_policy_fixture_error("stage current weapon command failed", error);
		if (!input_demo_recorder_stage_direct_command_drop_secondary_weapon(error, sizeof(error)))
			return direct_command_policy_fixture_error("stage secondary weapon command failed", error);
		if (!input_demo_recorder_stage_direct_command_drop_flag(error, sizeof(error)))
			return direct_command_policy_fixture_error("stage flag command failed", error);
		if (!input_demo_recorder_stage_direct_command_escort_release_control(error, sizeof(error)))
			return direct_command_policy_fixture_error("stage escort release command failed", error);
		if (!input_demo_recorder_stage_direct_command_guidebot_spawn(error, sizeof(error)))
			return direct_command_policy_fixture_error("stage guidebot spawn command failed", error);
		if (!input_demo_recorder_stage_direct_command_guidebot_find_secret(error, sizeof(error)))
			return direct_command_policy_fixture_error("stage find secret command failed", error);
		if (!input_demo_recorder_stage_direct_command_guidebot_find_unexplored(error, sizeof(error)))
			return direct_command_policy_fixture_error("stage find unexplored command failed", error);
		if (!input_demo_recorder_stage_direct_command_guidebot_warp_to_me(error, sizeof(error)))
			return direct_command_policy_fixture_error("stage guidebot warp command failed", error);
	} else if (fixture_kind == DIRECT_COMMAND_POLICY_FIXTURE_UNKNOWN_LATE) {
		if (!input_demo_recorder_stage_direct_command_change_difficulty(3, error, sizeof(error)))
			return direct_command_policy_fixture_error("stage command before unknown failed", error);
		if (!input_demo_recorder_stage_frame_event_json(
			"{\"kind\":\"direct_command\",\"command\":\"future_command\"}",
			error, sizeof(error)))
			return direct_command_policy_fixture_error("stage unknown command failed", error);
	} else if (fixture_kind == DIRECT_COMMAND_POLICY_FIXTURE_MALFORMED_LATE) {
		if (!input_demo_recorder_stage_direct_command_change_difficulty(3, error, sizeof(error)))
			return direct_command_policy_fixture_error("stage command before malformed failed", error);
		if (!input_demo_recorder_stage_frame_event_json(
			"{\"kind\":\"direct_command\",\"command\":\"change_difficulty\"}",
			error, sizeof(error)))
			return direct_command_policy_fixture_error("stage malformed command failed", error);
	} else {
		if (!input_demo_recorder_stage_direct_command_guidebot_spawn(error, sizeof(error)))
			return direct_command_policy_fixture_error("stage unsupported command failed", error);
	}

	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0,
		NULL, NULL, error, sizeof(error)))
		return direct_command_policy_fixture_error("direct command policy capture failed", error);
	if (!input_demo_recorder_flush(path, error, sizeof(error)))
		return direct_command_policy_fixture_error("direct command policy flush failed", error);
	return 0;
}

static std::string direct_command_policy_game_call(
	const input_demo_replay_direct_command_event *event, int validate_only)
{
	char call[160];

	snprintf(call, sizeof(call), "%s game kind=%d value0=%d value1=%d text=%s",
		validate_only ? "validate" : "apply", event->kind,
		event->value0, event->value1, event->text);
	return call;
}

static int direct_command_policy_death_callback(void *context, int validate_only,
	char *error, size_t error_size)
{
	direct_command_policy_test_context *test_context =
		static_cast<direct_command_policy_test_context *>(context);

	(void) error;
	(void) error_size;
	test_context->calls.push_back(validate_only ? "validate death" : "apply death");
	if (!validate_only)
		test_context->mutation_count++;
	return 1;
}

static int direct_command_policy_difficulty_callback(void *context, int difficulty,
	int validate_only, char *error, size_t error_size)
{
	direct_command_policy_test_context *test_context =
		static_cast<direct_command_policy_test_context *>(context);
	char call[80];

	(void) error;
	(void) error_size;
	snprintf(call, sizeof(call), "%s difficulty=%d",
		validate_only ? "validate" : "apply", difficulty);
	test_context->calls.push_back(call);
	if (!validate_only)
		test_context->mutation_count++;
	return 1;
}

static int direct_command_policy_game_callback(void *context,
	const input_demo_replay_direct_command_event *event,
	int validate_only, char *error, size_t error_size)
{
	direct_command_policy_test_context *test_context =
		static_cast<direct_command_policy_test_context *>(context);

	test_context->calls.push_back(direct_command_policy_game_call(event, validate_only));
	if (validate_only && event->kind == test_context->reject_game_kind) {
		if (error && error_size)
			snprintf(error, error_size, "test adapter rejected kind %d", event->kind);
		return 0;
	}
	if (!validate_only)
		test_context->mutation_count++;
	return 1;
}

static void direct_command_policy_init(input_demo_direct_command_policy *policy,
	direct_command_policy_test_context *context)
{
	memset(policy, 0, sizeof(*policy));
	policy->context = context;
	policy->apply_death_abort = direct_command_policy_death_callback;
	policy->change_difficulty = direct_command_policy_difficulty_callback;
	policy->apply_game_specific = direct_command_policy_game_callback;
}

static void direct_command_policy_context_clear(direct_command_policy_test_context *context)
{
	context->calls.clear();
	context->mutation_count = 0;
	context->reject_game_kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_NONE;
}

static int expect_direct_command_policy_calls(
	const direct_command_policy_test_context *context,
	const std::vector<std::string> &expected, const char *label)
{
	size_t i;

	if (context->calls.size() != expected.size())
		return report_failure_string(std::string(label) + " callback count mismatch: expected " +
			std::to_string(expected.size()) + ", got " + std::to_string(context->calls.size()));
	for (i = 0; i != expected.size(); ++i)
		if (context->calls[i] != expected[i])
			return report_failure_string(std::string(label) + " callback mismatch at " +
				std::to_string(i) + ": expected '" + expected[i] + "', got '" +
				context->calls[i] + "'");
	return 0;
}

static void append_direct_command_policy_game_calls(std::vector<std::string> *calls,
	int validate_only)
{
	input_demo_replay_direct_command_event event;
	static const int no_payload_kinds[] = {
		INPUT_DEMO_REPLAY_DIRECT_COMMAND_DROP_CURRENT_WEAPON,
		INPUT_DEMO_REPLAY_DIRECT_COMMAND_DROP_SECONDARY_WEAPON,
		INPUT_DEMO_REPLAY_DIRECT_COMMAND_DROP_FLAG,
		INPUT_DEMO_REPLAY_DIRECT_COMMAND_ESCORT_RELEASE_CONTROL,
		INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_SPAWN,
		INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_FIND_SECRET,
		INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_FIND_UNEXPLORED,
		INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_WARP_TO_ME
	};
	size_t i;

	input_demo_replay_direct_command_event_clear(&event);
	event.kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_GOAL;
	event.value0 = 17;
	event.value1 = 1;
	calls->push_back(direct_command_policy_game_call(&event, validate_only));
	input_demo_replay_direct_command_event_clear(&event);
	event.kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_DROP_MARKER;
	event.value0 = 1;
	snprintf(event.text, sizeof(event.text), "%s", "marker text");
	calls->push_back(direct_command_policy_game_call(&event, validate_only));
	for (i = 0; i != sizeof(no_payload_kinds) / sizeof(no_payload_kinds[0]); ++i) {
		input_demo_replay_direct_command_event_clear(&event);
		event.kind = no_payload_kinds[i];
		calls->push_back(direct_command_policy_game_call(&event, validate_only));
	}
}

static int expect_direct_command_policy(void)
{
	const char *dir = "test_input_demo_direct_command_policy_fixture";
	const std::string zero_path = std::string(dir) + "/zero.dximdemo";
	const std::string one_path = std::string(dir) + "/one.dximdemo";
	const std::string multiple_path = std::string(dir) + "/multiple.dximdemo";
	const std::string unknown_path = std::string(dir) + "/unknown.dximdemo";
	const std::string malformed_path = std::string(dir) + "/malformed.dximdemo";
	const std::string unsupported_path = std::string(dir) + "/unsupported.dximdemo";
	const std::string paths[] = {
		zero_path, one_path, multiple_path, unknown_path, malformed_path, unsupported_path
	};
	direct_command_policy_test_context context;
	input_demo_direct_command_policy policy;
	std::vector<std::string> expected;
	char error[256] = "";
	size_t i;

	if (!make_test_dir(dir))
		return report_failure("could not create direct command policy test directory");
	if (write_direct_command_policy_fixture(zero_path.c_str(), DIRECT_COMMAND_POLICY_FIXTURE_ZERO) ||
	    write_direct_command_policy_fixture(one_path.c_str(), DIRECT_COMMAND_POLICY_FIXTURE_ONE) ||
	    write_direct_command_policy_fixture(multiple_path.c_str(), DIRECT_COMMAND_POLICY_FIXTURE_MULTIPLE) ||
	    write_direct_command_policy_fixture(unknown_path.c_str(), DIRECT_COMMAND_POLICY_FIXTURE_UNKNOWN_LATE) ||
	    write_direct_command_policy_fixture(malformed_path.c_str(), DIRECT_COMMAND_POLICY_FIXTURE_MALFORMED_LATE) ||
	    write_direct_command_policy_fixture(unsupported_path.c_str(), DIRECT_COMMAND_POLICY_FIXTURE_UNSUPPORTED))
		return 1;
	direct_command_policy_context_clear(&context);
	direct_command_policy_init(&policy, &context);

	if (!input_demo_direct_command_apply_current_frame(&policy,
		INPUT_DEMO_DIRECT_COMMAND_PHASE_GAMEPLAY, error, sizeof(error)))
		return report_failure_string(std::string("absent replay policy failed: ") + error);
	if (!input_demo_replay_load(zero_path.c_str(), error, sizeof(error)))
		return report_failure_string(std::string("zero-command replay load failed: ") + error);
	if (!input_demo_direct_command_apply_current_frame(&policy,
		INPUT_DEMO_DIRECT_COMMAND_PHASE_GAMEPLAY, error, sizeof(error)))
		return report_failure_string(std::string("zero-command policy failed: ") + error);
	if (!context.calls.empty() || context.mutation_count)
		return report_failure("zero-command policy invoked a callback");
	input_demo_replay_unload();

	direct_command_policy_context_clear(&context);
	if (!input_demo_replay_load(one_path.c_str(), error, sizeof(error)))
		return report_failure_string(std::string("one-command replay load failed: ") + error);
	if (!input_demo_direct_command_apply_current_frame(&policy,
		INPUT_DEMO_DIRECT_COMMAND_PHASE_GAMEPLAY, error, sizeof(error)))
		return report_failure_string(std::string("one-command policy failed: ") + error);
	expected.clear();
	expected.push_back("validate difficulty=3");
	expected.push_back("apply difficulty=3");
	if (expect_direct_command_policy_calls(&context, expected, "one-command") ||
	    context.mutation_count != 1)
		return report_failure("one-command policy mutation mismatch");
	input_demo_replay_unload();

	direct_command_policy_context_clear(&context);
	if (!input_demo_replay_load(multiple_path.c_str(), error, sizeof(error)))
		return report_failure_string(std::string("dead-phase replay load failed: ") + error);
	if (!input_demo_direct_command_apply_current_frame(&policy,
		INPUT_DEMO_DIRECT_COMMAND_PHASE_DEAD, error, sizeof(error)))
		return report_failure_string(std::string("dead-phase policy failed: ") + error);
	expected.clear();
	expected.push_back("validate death");
	expected.push_back("apply death");
	if (expect_direct_command_policy_calls(&context, expected, "dead-phase") ||
	    context.mutation_count != 1)
		return report_failure("dead-phase policy mutation mismatch");
	input_demo_replay_unload();

	direct_command_policy_context_clear(&context);
	if (!input_demo_replay_load(multiple_path.c_str(), error, sizeof(error)))
		return report_failure_string(std::string("gameplay-phase replay load failed: ") + error);
	if (!input_demo_direct_command_apply_current_frame(&policy,
		INPUT_DEMO_DIRECT_COMMAND_PHASE_GAMEPLAY, error, sizeof(error)))
		return report_failure_string(std::string("gameplay-phase policy failed: ") + error);
	expected.clear();
	expected.push_back("validate difficulty=3");
	append_direct_command_policy_game_calls(&expected, 1);
	expected.push_back("apply difficulty=3");
	append_direct_command_policy_game_calls(&expected, 0);
	if (expect_direct_command_policy_calls(&context, expected, "gameplay-phase") ||
	    context.mutation_count != 11)
		return report_failure("gameplay-phase policy mutation mismatch");
	input_demo_replay_unload();

	direct_command_policy_context_clear(&context);
	context.reject_game_kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_WARP_TO_ME;
	if (!input_demo_replay_load(multiple_path.c_str(), error, sizeof(error)))
		return report_failure_string(std::string("adapter rejection replay load failed: ") + error);
	if (input_demo_direct_command_apply_current_frame(&policy,
		INPUT_DEMO_DIRECT_COMMAND_PHASE_GAMEPLAY, error, sizeof(error)))
		return report_failure("adapter rejection unexpectedly succeeded");
	if (context.mutation_count != 0)
		return report_failure("adapter validation failure partially applied commands");
	for (i = 0; i != context.calls.size(); ++i)
		if (context.calls[i].find("apply ") == 0)
			return report_failure("adapter validation failure reached application pass");
	input_demo_replay_unload();

	direct_command_policy_context_clear(&context);
	if (!input_demo_replay_load(malformed_path.c_str(), error, sizeof(error)))
		return report_failure_string(std::string("malformed replay load failed: ") + error);
	if (input_demo_direct_command_apply_current_frame(&policy,
		INPUT_DEMO_DIRECT_COMMAND_PHASE_GAMEPLAY, error, sizeof(error)))
		return report_failure("malformed late command unexpectedly succeeded");
	if (!context.calls.empty() || context.mutation_count)
		return report_failure("malformed late command partially dispatched commands");
	if (!input_demo_replay_is_loaded())
		return report_failure("non-unloading malformed policy unloaded replay");
	input_demo_replay_unload();

	direct_command_policy_context_clear(&context);
	if (!input_demo_replay_load(unknown_path.c_str(), error, sizeof(error)))
		return report_failure_string(std::string("unknown replay load failed: ") + error);
	if (input_demo_direct_command_apply_current_frame(&policy,
		INPUT_DEMO_DIRECT_COMMAND_PHASE_GAMEPLAY, error, sizeof(error)))
		return report_failure("unknown late command unexpectedly succeeded");
	if (!context.calls.empty() || context.mutation_count)
		return report_failure("unknown late command partially dispatched commands");
	if (!input_demo_replay_is_loaded())
		return report_failure("D1-style failure policy unloaded replay");
	input_demo_replay_unload();
	policy.unload_replay_on_failure = 1;
	if (!input_demo_replay_load(unknown_path.c_str(), error, sizeof(error)))
		return report_failure_string(std::string("unloading replay load failed: ") + error);
	if (input_demo_direct_command_apply_current_frame(&policy,
		INPUT_DEMO_DIRECT_COMMAND_PHASE_GAMEPLAY, error, sizeof(error)))
		return report_failure("unloading unknown command unexpectedly succeeded");
	if (input_demo_replay_is_loaded())
		return report_failure("D2-style failure policy did not unload replay");
	policy.unload_replay_on_failure = 0;

	direct_command_policy_context_clear(&context);
	policy.apply_game_specific = NULL;
	if (!input_demo_replay_load(unsupported_path.c_str(), error, sizeof(error)))
		return report_failure_string(std::string("unsupported replay load failed: ") + error);
	if (input_demo_direct_command_apply_current_frame(&policy,
		INPUT_DEMO_DIRECT_COMMAND_PHASE_GAMEPLAY, error, sizeof(error)))
		return report_failure("unsupported D1 command unexpectedly succeeded");
	if (!context.calls.empty() || context.mutation_count)
		return report_failure("unsupported D1 command invoked a callback");
	if (std::string(error).find("unsupported") == std::string::npos)
		return report_failure_string(std::string("unsupported D1 command error mismatch: ") + error);
	input_demo_replay_unload();

	for (i = 0; i != sizeof(paths) / sizeof(paths[0]); ++i) {
		remove(paths[i].c_str());
		remove((paths[i] + INPUT_DEMO_RNG_TRACE_SUFFIX).c_str());
		remove((paths[i] + ".actual.json").c_str());
	}
	remove_test_dir(dir);
	return 0;
}

int main(void)
{
	if (expect_replay_loader())
		return 1;
	if (expect_checkpoint_replay_loader())
		return 1;
	if (expect_direct_command_policy())
		return 1;
	puts("PASS");
	return 0;
}
