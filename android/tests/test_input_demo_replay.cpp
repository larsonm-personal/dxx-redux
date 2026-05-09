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
		player_cfg->select_after_fire != 0 || player_cfg->classic_autoselect_weapon != 1)
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
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0, &frame_state, error, sizeof(error)))
		return report_failure_string(std::string("capture frame 0 failed: ") + error);
	input_demo_control_pulse_clear(&pulse);
	pulse.fire_primary_count = 1;
	fill_test_frame_state(&frame_state, 1);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0, &frame_state, error, sizeof(error)))
		return report_failure_string(std::string("capture frame 1 failed: ") + error);
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	fill_test_frame_state(&frame_state, 2);
	if (!input_demo_recorder_capture_frame(4000, &state, &pulse, 102, 1, 3, &frame_state, error, sizeof(error)))
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
	fill_test_checkpoint_escort_state(&settings.checkpoint_escort_state);
	fill_test_checkpoint_thief_state(&settings.checkpoint_thief_state);
	if (!input_demo_recorder_start(&settings, error, sizeof(error)))
		return report_failure_string(std::string("checkpoint recorder start failed: ") + error);
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	state.forward_thrust_time = 44;
	fill_test_frame_state(&frame_state, 0);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0, &frame_state, error, sizeof(error)))
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
	const std::string actual_result_path = demo_path + ".actual.json";
	input_demo_replay_frame frame;
	input_demo_player_cfg player_cfg;
	input_demo_result actual_result;
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

		input_demo_checkpoint_escort_state_clear(&escort_state);
		if (input_demo_replay_get_checkpoint_escort_state(&escort_state))
			return report_failure("new_level replay should not expose checkpoint escort state");
		input_demo_checkpoint_thief_state_clear(&thief_state);
		if (input_demo_replay_get_checkpoint_thief_state(&thief_state))
			return report_failure("new_level replay should not expose checkpoint thief state");
	}
	if (!input_demo_replay_actual_result_path() || std::string(input_demo_replay_actual_result_path()) != actual_result_path)
		return report_failure("replay actual result path mismatch");

	if (!input_demo_replay_get_current_frame(&frame, error, sizeof(error)))
		return report_failure_string(std::string("replay current frame 0 failed: ") + error);
	if (frame.frame != 0 || frame.frame_time != 3276 || frame.state.forward_thrust_time != 44 ||
		frame.pulse.fire_primary_count != 0 || frame.rng_state != 100 || frame.has_rng_call_count ||
		!frame.has_state || frame.state_result.player0.score != 12500 || frame.state_result.position.segment != 142)
		return report_failure("replay frame 0 mismatch");
	if (!input_demo_replay_advance_frame(error, sizeof(error)))
		return report_failure_string(std::string("replay advance 0 failed: ") + error);

	if (!input_demo_replay_get_current_frame(&frame, error, sizeof(error)))
		return report_failure_string(std::string("replay current frame 1 failed: ") + error);
	if (frame.frame != 1 || frame.frame_time != 3276 || frame.state.forward_thrust_time != 44 ||
		frame.pulse.fire_primary_count != 1 || frame.rng_state != 100 || frame.has_rng_call_count ||
		!frame.has_state || frame.state_result.player0.score != 12501 || frame.state_result.position.segment != 143)
		return report_failure("replay frame 1 mismatch");
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
	remove(actual_result_path.c_str());
	remove_test_dir(dir);
	return 0;
}

static int expect_checkpoint_replay_loader(void)
{
	const char *dir = "test_input_demo_replay_checkpoint_fixture";
	const std::string demo_path = std::string(dir) + "/checkpoint_replay.dximdemo";
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
	remove(actual_result_path.c_str());
	remove_test_dir(dir);
	return 0;
}

int main(void)
{
	if (expect_replay_loader())
		return 1;
	if (expect_checkpoint_replay_loader())
		return 1;
	puts("PASS");
	return 0;
}