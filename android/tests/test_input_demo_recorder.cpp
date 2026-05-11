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

#include "input_demo_fixture.h"
#include "input_demo_recorder.h"
#include "input_demo_rng_trace.h"
#include "input_demo_result.h"
#include "input_demo_state_trace.h"

#ifndef DXX_INPUT_DEMO_BUILD_NUMBERi
#define DXX_INPUT_DEMO_BUILD_NUMBERi 0
#endif

#ifndef DXX_INPUT_DEMO_GIT_VERSION
#define DXX_INPUT_DEMO_GIT_VERSION "unknown"
#endif

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

static int input_demo_test_build_number(void)
{
	return DXX_INPUT_DEMO_BUILD_NUMBERi;
}

static const char *input_demo_test_git_version(void)
{
	return DXX_INPUT_DEMO_GIT_VERSION;
}

static const char *input_demo_test_arch(void)
{
#if defined(__aarch64__) || defined(_M_ARM64)
	return "arm64";
#elif defined(__arm__) || defined(_M_ARM)
	return "arm";
#elif defined(__x86_64__) || defined(_M_X64)
	return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
	return "x86";
#else
	return "unknown";
#endif
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

static std::string input_demo_test_player_cfg_header_json(void)
{
#if defined(INPUT_DEMO_TEST_D2)
	return ",\"player_cfg\":{\"auto_leveling\":1,\"persistent_debris\":1,\"headlight_active_default\":0,\"no_fire_autoselect\":1,\"cycle_autoselect_only\":1,\"select_after_fire\":0,\"classic_autoselect_weapon\":1,\"primary_order\":[9,8,7,6,5,4,3,2,1,0,255],\"secondary_order\":[9,8,4,3,1,5,0,255,7,6,2]}";
#else
	return ",\"player_cfg\":{\"auto_leveling\":1,\"persistent_debris\":1,\"no_fire_autoselect\":1,\"cycle_autoselect_only\":1,\"select_after_fire\":0,\"classic_autoselect_weapon\":1,\"primary_order\":[4,3,2,1,0,255,16],\"secondary_order\":[4,3,1,0,255,2]}";
#endif
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

static std::string input_demo_test_frame_state_json(int frame_index)
{
	input_demo_result result;
	std::string text;
	std::string error;

	fill_test_frame_state(&result, frame_index);
	if (!input_demo_result_snapshot_to_json_text(result, &text, &error))
		return std::string();
	return text;
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

static int expect_record_and_flush(void)
{
	const char *dir = "test_input_demo_recorder_fixture";
	const std::string demo_path = std::string(dir) + "/recorded.dximdemo";
	const std::string trace_path = demo_path + INPUT_DEMO_RNG_TRACE_SUFFIX;
	input_demo_recorder_settings settings;
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	input_demo_result result;
	input_demo_result frame_state;
	input_demo_file parsed;
	char error[256] = "";
	std::string read_error;
	std::string text;
	std::string expected;

	if (!make_test_dir(dir))
		return report_failure("could not create recorder test directory");
	input_demo_recorder_settings_clear(&settings);
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 1;
	settings.difficulty = 2;
	settings.rng_mode = input_demo_test_rng_mode();
	settings.has_player_cfg = 1;
	settings.record_per_frame_state = 1;
	fill_test_player_cfg(&settings.player_cfg);
	if (!input_demo_recorder_start(&settings, error, sizeof(error))) {
		remove_test_dir(dir);
		return report_failure_string(std::string("recorder start failed: ") + error);
	}
	input_demo_rng_trace_set_context(0, 3276);
	d_srand(1234);
	(void)d_rand();
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	state.forward_thrust_time = 44;
	fill_test_frame_state(&frame_state, 0);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0, &frame_state, NULL, error, sizeof(error)))
		return report_failure_string(std::string("capture frame 0 failed: ") + error);
	input_demo_control_pulse_clear(&pulse);
	pulse.fire_primary_count = 1;
	fill_test_frame_state(&frame_state, 1);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0, &frame_state, NULL, error, sizeof(error)))
		return report_failure_string(std::string("capture frame 1 failed: ") + error);
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	fill_test_frame_state(&frame_state, 2);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 102, 0, 0, &frame_state, NULL, error, sizeof(error)))
		return report_failure_string(std::string("capture frame 2 failed: ") + error);
	input_demo_result_clear(&result);
	snprintf(result.game, sizeof(result.game), "%s", input_demo_test_game_name());
	snprintf(result.mission, sizeof(result.mission), "%s", input_demo_test_game_name());
	result.level = 1;
	result.difficulty = 2;
	result.has_game_time64 = 1;
	result.game_time64 = 120;
	result.player0.present = 1;
	result.player0.energy = 67;
	result.player0.shields = 42;
	result.player0.score = 12500;
	result.player0.lives = 3;
	result.player0.laser_level = 1;
	result.player0.secondary_weapon = 1;
	result.player0.primary_ammo[1] = 200;
	result.player0.secondary_ammo[0] = 4;
	result.position.present = 1;
	result.position.segment = 142;
	result.position.x = 12345678;
	result.position.y = -8765432;
	result.position.z = 3456789;
	result.position.has_forward = 1;
	result.position.fx = 65536;
	result.level_summary.present = 1;
	result.level_summary.robots_alive = 23;
	result.level_summary.robots_killed = 8;
	result.level_summary.hostages_remaining = 2;
	result.level_summary.powerups_remaining = 15;
	result.level_summary.control_center_destroyed = 1;
	if (!input_demo_recorder_flush_with_result(demo_path.c_str(), &result, error, sizeof(error)))
		return report_failure_string(std::string("recorder flush failed: ") + error);
	if (input_demo_recorder_is_active())
		return report_failure("recorder should be inactive after flush");
	if (!read_text_file(demo_path.c_str(), &text))
		return report_failure("could not read recorder demo file");
	expected = std::string("{\"type\":\"header\",\"version\":3,\"game\":\"") + input_demo_test_game_name() +
		"\",\"mission\":\"" + input_demo_test_game_name() +
		"\",\"build_number\":" + std::to_string(input_demo_test_build_number()) + ",\"git_version\":\"" + input_demo_test_git_version() + "\",\"arch\":\"" + input_demo_test_arch() +
		"\",\"level\":1,\"difficulty\":2,\"start_mode\":\"new_level\",\"rng_mode\":\"" + input_demo_test_rng_mode() +
		"\",\"frame_count\":3" + input_demo_test_player_cfg_header_json() + "}\n" +
		"{\"type\":\"frame\",\"f\":0,\"ft\":3276,\"input\":{\"s\":{\"f\":44}},\"rng\":{\"s\":100},\"state\":" + input_demo_test_frame_state_json(0) + "}\n" +
		"{\"type\":\"frame\",\"f\":1,\"input\":{\"p\":{\"f1\":1}},\"rng\":{\"s\":100},\"state\":" + input_demo_test_frame_state_json(1) + "}\n" +
		"{\"type\":\"frame\",\"f\":2,\"input\":{\"s\":{\"f\":0}},\"rng\":{\"s\":102},\"state\":" + input_demo_test_frame_state_json(2) + "}\n" +
		"{\"type\":\"result\",\"result\":{\"version\":2,\"game\":\"" + input_demo_test_game_name() +
		"\",\"mission\":\"" + input_demo_test_game_name() +
		"\",\"level\":1,\"difficulty\":2,\"frame_count\":3,\"game_time64\":120,\"player0\":{\"energy\":67,\"shields\":42,\"score\":12500,\"lives\":3,\"laser_level\":1,\"primary_weapon\":0,\"secondary_weapon\":1,\"flags\":0,\"hostages\":0,\"primary_ammo\":[0,200,0,0,0,0,0,0,0,0,0,0,0,0,0,0],\"secondary_ammo\":[4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]},\"position\":{\"segment\":142,\"x\":12345678,\"y\":-8765432,\"z\":3456789,\"forward_x\":65536,\"forward_y\":0,\"forward_z\":0},\"level_summary\":{\"robots_alive\":23,\"robots_killed\":8,\"hostages_remaining\":2,\"powerups_remaining\":15,\"control_center_destroyed\":true,\"endlevel_completed\":false}}}\n";
	if (text != expected)
		return report_failure_string(std::string("unexpected recorder demo file: ") + text);
	if (!read_text_file(trace_path.c_str(), &text))
		return report_failure("could not read recorder rng trace file");
	if (text.find("\"type\":\"meta\"") == std::string::npos ||
		text.find("\"type\":\"srand\"") == std::string::npos ||
		text.find("\"type\":\"rand\"") == std::string::npos ||
		text.find("\"func\":\"expect_record_and_flush\"") == std::string::npos ||
		text.find("\"gt\":3276") == std::string::npos)
		return report_failure_string(std::string("unexpected recorder rng trace file: ") + text);
	if (!input_demo_file_read(demo_path.c_str(), &parsed, &read_error))
		return report_failure_string(std::string("recorder demo read failed: ") + read_error);
	if (parsed.frames.size() != 3 || parsed.frames[2].rng.state != 102 || !parsed.frames[1].has_state ||
	    parsed.frames[1].state.player0.score != 12501 || parsed.result.player0.score != 12500)
		return report_failure("recorder demo round trip mismatch");
	remove(demo_path.c_str());
	remove(trace_path.c_str());
	remove_test_dir(dir);
	return 0;
}

static int expect_record_and_flush_checkpoint(void)
{
	const char *dir = "test_input_demo_recorder_checkpoint_fixture";
	const std::string demo_path = std::string(dir) + "/recorded_checkpoint.dximdemo";
	const unsigned char checkpoint_data[] = { 'D', 'G', 'S', 'S', 24, 0, 0, 0 };
	input_demo_recorder_settings settings;
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	input_demo_result frame_state;
	input_demo_file parsed;
	char error[256] = "";
	std::string read_error;
	std::string text;
	std::string expected;

	if (!make_test_dir(dir))
		return report_failure("could not create checkpoint recorder test directory");
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
	if (!input_demo_recorder_start(&settings, error, sizeof(error))) {
		remove_test_dir(dir);
		return report_failure_string(std::string("checkpoint recorder start failed: ") + error);
	}
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	state.forward_thrust_time = 44;
	fill_test_frame_state(&frame_state, 0);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0, &frame_state, NULL, error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("checkpoint capture frame failed: ") + error);
	}
	if (!input_demo_recorder_flush(demo_path.c_str(), error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("checkpoint recorder flush failed: ") + error);
	}
	if (!read_text_file(demo_path.c_str(), &text)) {
		remove(demo_path.c_str());
		remove_test_dir(dir);
		return report_failure("could not read checkpoint recorder demo file");
	}
	expected = std::string("{\"type\":\"header\",\"version\":3,\"game\":\"") + input_demo_test_game_name() +
		"\",\"mission\":\"" + input_demo_test_game_name() +
		"\",\"build_number\":" + std::to_string(input_demo_test_build_number()) + ",\"git_version\":\"" + input_demo_test_git_version() + "\",\"arch\":\"" + input_demo_test_arch() +
		"\",\"level\":1,\"difficulty\":2,\"start_mode\":\"save_checkpoint\",\"rng_mode\":\"" + input_demo_test_rng_mode() +
		"\",\"frame_count\":1,\"start_save\":\"inputdemo_start.dgss\"" + input_demo_test_player_cfg_header_json() + "}\n" +
		"{\"type\":\"checkpoint\",\"format\":\"dgss\",\"encoding\":\"base64\",\"compression\":\"none\",\"size\":8,\"sha256\":\"077c5f8a7bd52bba7beb0ea8153f1005401b5ba52b797e04952bf14e542fd3b5\",\"save_name\":\"inputdemo_start.dgss\",\"start_gt\":124125,\"data\":\"REdTUxgAAAA=\"}\n" +
		"{\"type\":\"frame\",\"f\":0,\"ft\":3276,\"input\":{\"s\":{\"f\":44}},\"rng\":{\"s\":100},\"state\":" + input_demo_test_frame_state_json(0) + "}\n" +
		"{\"type\":\"result\",\"result\":{\"version\":2,\"game\":\"" + input_demo_test_game_name() +
		"\",\"mission\":\"" + input_demo_test_game_name() + "\",\"level\":1,\"difficulty\":2,\"frame_count\":1}}\n";
	if (text != expected) {
		remove(demo_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("unexpected checkpoint recorder demo file: ") + text);
	}
	if (!input_demo_file_read(demo_path.c_str(), &parsed, &read_error)) {
		remove(demo_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("checkpoint recorder demo read failed: ") + read_error);
	}
	remove(demo_path.c_str());
	remove_test_dir(dir);
	if (parsed.metadata.start_mode != "save_checkpoint" || !parsed.has_checkpoint ||
		!parsed.metadata.has_player_cfg || parsed.metadata.player_cfg.primary_order_count == 0 ||
		!parsed.frames[0].has_state || parsed.frames[0].state.player0.score != 12500 ||
		parsed.checkpoint.compression != "none" ||
		parsed.checkpoint.sha256 != "077c5f8a7bd52bba7beb0ea8153f1005401b5ba52b797e04952bf14e542fd3b5" ||
		parsed.checkpoint.data != "REdTUxgAAAA=")
		return report_failure("checkpoint recorder demo round trip mismatch");
	return 0;
}

static int expect_stage_consumed_pulse(void)
{
	const char *dir = "test_input_demo_recorder_staged_pulse_fixture";
	const std::string demo_path = std::string(dir) + "/recorded_staged_pulse.dximdemo";
	const std::string trace_path = demo_path + INPUT_DEMO_RNG_TRACE_SUFFIX;
	input_demo_recorder_settings settings;
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	input_demo_control_pulse staged_pulse;
	input_demo_result frame_state;
	input_demo_file parsed;
	char error[256] = "";
	std::string read_error;
	std::string text;

	if (!make_test_dir(dir))
		return report_failure("could not create staged pulse recorder test directory");
	input_demo_recorder_settings_clear(&settings);
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 1;
	settings.difficulty = 2;
	settings.rng_mode = input_demo_test_rng_mode();
	if (!input_demo_recorder_start(&settings, error, sizeof(error))) {
		remove_test_dir(dir);
		return report_failure_string(std::string("staged pulse recorder start failed: ") + error);
	}
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	input_demo_control_pulse_clear(&staged_pulse);
	staged_pulse.fire_secondary_count = 2;
	staged_pulse.select_weapon_count = 4;
	input_demo_recorder_stage_pulse(&staged_pulse);
	pulse.fire_primary_count = 1;
	fill_test_frame_state(&frame_state, 0);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0, &frame_state, NULL, error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("staged pulse capture frame 0 failed: ") + error);
	}
	input_demo_control_pulse_clear(&pulse);
	fill_test_frame_state(&frame_state, 1);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 101, 0, 0, &frame_state, NULL, error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("staged pulse capture frame 1 failed: ") + error);
	}
	if (!input_demo_recorder_flush(demo_path.c_str(), error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("staged pulse recorder flush failed: ") + error);
	}
	if (!read_text_file(demo_path.c_str(), &text)) {
		remove(demo_path.c_str());
		remove(trace_path.c_str());
		remove_test_dir(dir);
		return report_failure("could not read staged pulse recorder demo file");
	}
	if (text.find("\"f1\":1") == std::string::npos ||
		text.find("\"f2\":2") == std::string::npos ||
		text.find("\"sw\":4") == std::string::npos) {
		remove(demo_path.c_str());
		remove(trace_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("unexpected staged pulse recorder demo file: ") + text);
	}
	if (!input_demo_file_read(demo_path.c_str(), &parsed, &read_error)) {
		remove(demo_path.c_str());
		remove(trace_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("staged pulse recorder demo read failed: ") + read_error);
	}
	remove(demo_path.c_str());
	remove(trace_path.c_str());
	remove_test_dir(dir);
	if (parsed.frames.size() != 2 ||
		!parsed.frames[0].input.pulse.has_fire_primary_count || parsed.frames[0].input.pulse.fire_primary_count != 1 ||
		!parsed.frames[0].input.pulse.has_fire_secondary_count || parsed.frames[0].input.pulse.fire_secondary_count != 2 ||
		!parsed.frames[0].input.pulse.has_select_weapon_count || parsed.frames[0].input.pulse.select_weapon_count != 4 ||
		parsed.frames[1].input.pulse.has_fire_primary_count ||
		parsed.frames[1].input.pulse.has_fire_secondary_count ||
		parsed.frames[1].input.pulse.has_select_weapon_count)
		return report_failure("staged pulse recorder demo round trip mismatch");
	return 0;
}

static int expect_write_state_trace(void)
{
	const char *dir = "test_input_demo_state_trace_fixture";
	const std::string trace_path = std::string(dir) + "/replay.actual_state.jsonl";
	input_demo_result state;
	input_demo_state_trace_diag diag;
	std::string text;
	char error[256] = "";

	if (!make_test_dir(dir))
		return report_failure("could not create state trace test directory");
	input_demo_result_clear(&state);
	snprintf(state.game, sizeof(state.game), "%s", input_demo_test_game_name());
	snprintf(state.mission, sizeof(state.mission), "%s", input_demo_test_game_name());
	state.level = 2;
	state.difficulty = 1;
	state.frame_count = 3;
	state.has_game_time64 = 1;
	state.game_time64 = 2233467;
	state.player0.present = 1;
	state.player0.energy = 85;
	state.player0.shields = 155;
	state.level_summary.present = 1;
	state.level_summary.powerups_remaining = 66;
	memset(&diag, 0, sizeof(diag));
	diag.runtime_state_hash = 424242u;
	diag.object_allocator_num_objects = 88;
	diag.object_signature_seed = 19479;
	diag.object_free_list_count = 912;
	diag.object_free_list_hash = 789u;
	diag.object_free_head0 = 95;
	diag.object_free_head1 = 96;
	diag.object_free_head2 = 97;
	diag.object_free_head3 = 98;
	diag.object_homer_frame_count = 12u;
	diag.object_current_homer_frame_time = 3276;
	diag.object_do_homer_frame = 1;
	diag.weapon_next_laser_delta = 700;
	diag.weapon_next_missile_delta = 800;
	diag.weapon_last_laser_delta = -900;
	diag.weapon_next_flare_delta = 1000;
	diag.weapon_auto_fusion_delta = -2233467;
	diag.weapon_last_omega_delta = -1200;
	diag.weapon_global_laser_firing_count = 2;
	diag.weapon_global_missile_firing_count = 1;
	diag.weapon_fusion_charge = 42;
	diag.weapon_spreadfire_toggle = 1;
	diag.weapon_missile_gun = 3;
	diag.weapon_proximity_dropped = 1;
	diag.weapon_helix_orientation = 2;
	diag.weapon_smartmines_dropped = 0;
	diag.highest_object_index = 95;
	diag.live_object_count = 3;
	diag.live_object_hash = 1234u;
	diag.object_slot_bucket_size = INPUT_DEMO_OBJECT_SLOT_BUCKET_SIZE;
	diag.object_slot_counts[0] = 1;
	diag.object_slot_counts[3] = 2;
	diag.object_slot_hashes[0] = 101u;
	diag.object_slot_hashes[3] = 202u;
	diag.segment_object_list_count = 3;
	diag.segment_object_list_hash = 5678u;
	diag.segment_object_link_error_count = 0;
	if (!input_demo_state_trace_start(trace_path.c_str(),
					     "replay",
					     input_demo_test_game_name(),
					     input_demo_test_game_name(),
					     2,
					     1,
					     "save_checkpoint",
					     3,
					     error,
					     sizeof(error))) {
		remove_test_dir(dir);
		return report_failure_string(std::string("state trace start failed: ") + error);
	}
	if (!input_demo_state_trace_write_frame(810, 2622, 2636896831u, 1, 22066u, &diag, &state, error, sizeof(error))) {
		input_demo_state_trace_stop();
		remove_test_dir(dir);
		return report_failure_string(std::string("state trace write failed: ") + error);
	}
	input_demo_state_trace_stop();
	if (!read_text_file(trace_path.c_str(), &text)) {
		remove_test_dir(dir);
		return report_failure("could not read state trace file");
	}
	remove(trace_path.c_str());
	remove_test_dir(dir);
	if (text.find("\"type\":\"meta\"") == std::string::npos ||
		text.find("\"source\":\"replay\"") == std::string::npos ||
		text.find("\"f\":810") == std::string::npos ||
		text.find("\"ft\":2622") == std::string::npos ||
		text.find("\"rng\":{\"s\":2636896831,\"c\":22066}") == std::string::npos ||
		text.find("\"runtime_state_hash\":424242") == std::string::npos ||
		text.find("\"object_signature_seed\":19479") == std::string::npos ||
		text.find("\"object_free_head0\":95") == std::string::npos ||
		text.find("\"weapon_last_laser_delta\":-900") == std::string::npos ||
		text.find("\"weapon_spreadfire_toggle\":1") == std::string::npos ||
		text.find("\"highest_object_index\":95") == std::string::npos ||
		text.find("\"object_slot_bucket_size\":32") == std::string::npos ||
		text.find("\"object_slot_hashes\":[101") == std::string::npos ||
		text.find("\"segment_object_list_hash\":5678") == std::string::npos ||
		text.find("\"game_time64\":2233467") == std::string::npos)
		return report_failure_string(std::string("unexpected state trace text: ") + text);
	return 0;
}

static int expect_record_and_flush_events(void)
{
	const char *dir = "test_input_demo_recorder_events_fixture";
	const std::string demo_path = std::string(dir) + "/recorded_events.dximdemo";
	const std::string trace_path = demo_path + INPUT_DEMO_RNG_TRACE_SUFFIX;
	input_demo_recorder_settings settings;
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	input_demo_result frame_state;
	input_demo_file parsed;
	char error[256] = "";
	std::string read_error;
	std::string text;

	if (!make_test_dir(dir))
		return report_failure("could not create recorder events test directory");
	input_demo_recorder_settings_clear(&settings);
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 1;
	settings.difficulty = 2;
	settings.rng_mode = input_demo_test_rng_mode();
	settings.record_per_frame_state = 1;
	if (!input_demo_recorder_start(&settings, error, sizeof(error))) {
		remove_test_dir(dir);
		return report_failure_string(std::string("events recorder start failed: ") + error);
	}
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	fill_test_frame_state(&frame_state, 0);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0, &frame_state, NULL, error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("events capture frame 0 failed: ") + error);
	}
	if (!input_demo_recorder_append_frame_event_json(
			"{\"kind\":\"score\",\"gt\":3276,\"score_kind\":\"normal\",\"delta\":200,\"score\":12700}",
			error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("append score event failed: ") + error);
	}
	fill_test_frame_state(&frame_state, 1);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 101, 0, 0, &frame_state, NULL, error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("events capture frame 1 failed: ") + error);
	}
	if (!input_demo_recorder_append_frame_event_json(
			"{\"kind\":\"robot_damage\",\"gt\":6552,\"robot_obj\":68,\"robot_sig\":3769,\"robot_id\":39,\"damage\":589824,\"shields_before\":1900544,\"shields_after\":1310720,\"dead\":false,\"x\":-22577346,\"y\":-2494632,\"z\":-11045352}",
			error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("append robot damage event failed: ") + error);
	}
	if (!input_demo_recorder_flush(demo_path.c_str(), error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("events recorder flush failed: ") + error);
	}
	if (!read_text_file(demo_path.c_str(), &text)) {
		remove(demo_path.c_str());
		remove(trace_path.c_str());
		remove_test_dir(dir);
		return report_failure("could not read recorder events demo file");
	}
	if (text.find("\"version\":4") == std::string::npos ||
		text.find("\"events\":[{\"kind\":\"score\",\"gt\":3276,\"score_kind\":\"normal\",\"delta\":200,\"score\":12700}]") == std::string::npos ||
		text.find("\"events\":[{\"kind\":\"robot_damage\",\"gt\":6552,\"robot_obj\":68,\"robot_sig\":3769,\"robot_id\":39,\"damage\":589824,\"shields_before\":1900544,\"shields_after\":1310720,\"dead\":false,\"x\":-22577346,\"y\":-2494632,\"z\":-11045352}]") == std::string::npos) {
		remove(demo_path.c_str());
		remove(trace_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("unexpected recorder events demo file: ") + text);
	}
	if (!input_demo_file_read(demo_path.c_str(), &parsed, &read_error)) {
		remove(demo_path.c_str());
		remove(trace_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("events recorder demo read failed: ") + read_error);
	}
	remove(demo_path.c_str());
	remove(trace_path.c_str());
	remove_test_dir(dir);
	if (parsed.metadata.version != 4 || parsed.frames.size() != 2 ||
		parsed.frames[0].events.size() != 1 || parsed.frames[1].events.size() != 1 ||
		parsed.frames[0].events[0].find("\"kind\":\"score\"") == std::string::npos ||
		parsed.frames[1].events[0].find("\"kind\":\"robot_damage\"") == std::string::npos)
		return report_failure("events recorder demo round trip mismatch");
	return 0;
}

static int expect_record_and_flush_diag(void)
{
	const char *dir = "test_input_demo_recorder_diag_fixture";
	const std::string demo_path = std::string(dir) + "/recorded_diag.dximdemo";
	const std::string trace_path = demo_path + INPUT_DEMO_RNG_TRACE_SUFFIX;
	input_demo_recorder_settings settings;
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	input_demo_result frame_state;
	input_demo_state_trace_diag diag;
	input_demo_file parsed;
	char error[256] = "";
	std::string read_error;
	std::string text;

	if (!make_test_dir(dir))
		return report_failure("could not create recorder diag test directory");
	input_demo_recorder_settings_clear(&settings);
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 1;
	settings.difficulty = 2;
	settings.rng_mode = input_demo_test_rng_mode();
	settings.record_per_frame_state = 1;
	if (!input_demo_recorder_start(&settings, error, sizeof(error))) {
		remove_test_dir(dir);
		return report_failure_string(std::string("diag recorder start failed: ") + error);
	}
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	fill_test_frame_state(&frame_state, 0);
	memset(&diag, 0, sizeof(diag));
	diag.awareness_events = 7;
	diag.runtime_state_hash = 2468u;
	diag.object_allocator_num_objects = 219;
	diag.object_signature_seed = 19480;
	diag.object_free_list_count = 781;
	diag.object_free_list_hash = 13579u;
	diag.object_free_head0 = 348;
	diag.object_free_head1 = 349;
	diag.object_free_head2 = 350;
	diag.object_free_head3 = 351;
	diag.object_homer_frame_count = 333u;
	diag.object_current_homer_frame_time = 1638;
	diag.object_do_homer_frame = 1;
	diag.weapon_next_laser_delta = 444;
	diag.weapon_next_missile_delta = 555;
	diag.weapon_last_laser_delta = -666;
	diag.weapon_next_flare_delta = 777;
	diag.weapon_auto_fusion_delta = -888;
	diag.weapon_last_omega_delta = -999;
	diag.weapon_global_laser_firing_count = 2;
	diag.weapon_global_missile_firing_count = 1;
	diag.weapon_fusion_charge = 123;
	diag.weapon_spreadfire_toggle = 1;
	diag.weapon_missile_gun = 2;
	diag.weapon_proximity_dropped = 3;
	diag.weapon_helix_orientation = 4;
	diag.weapon_smartmines_dropped = 5;
	diag.player_weapon_count = 2;
	diag.player_weapon_hash = 123456789u;
	diag.highest_object_index = 347;
	diag.live_object_count = 219;
	diag.live_object_hash = 987654321u;
	diag.object_slot_bucket_size = INPUT_DEMO_OBJECT_SLOT_BUCKET_SIZE;
	diag.object_slot_counts[0] = 9;
	diag.object_slot_counts[7] = 5;
	diag.object_slot_hashes[0] = 111u;
	diag.object_slot_hashes[7] = 222u;
	diag.robot_object_count = 93;
	diag.robot_state_hash = 314159265u;
	diag.fireball_object_count = 17;
	diag.fireball_state_hash = 271828182u;
	diag.debris_object_count = 4;
	diag.debris_state_hash = 42424242u;
	diag.segment_object_list_count = 219;
	diag.segment_object_list_hash = 11235813u;
	diag.segment_object_link_error_count = 0;
	diag.player_weapon_obj0 = 58;
	diag.player_weapon_sig0 = 19479;
	diag.player_weapon_id0 = 32;
	diag.player_weapon_obj1 = 115;
	diag.player_weapon_sig1 = 19474;
	diag.player_weapon_id1 = 32;
	diag.player_weapon_obj2 = -1;
	diag.player_weapon_sig2 = -1;
	diag.player_weapon_id2 = -1;
	diag.player_weapon_obj3 = -1;
	diag.player_weapon_sig3 = -1;
	diag.player_weapon_id3 = -1;
	diag.ai_probe_skip_obj = -1;
	diag.ai_probe_skip_sig = -1;
	diag.ai_probe_skip_id = -1;
	diag.ai_probe_timeslice_obj = -1;
	diag.ai_probe_timeslice_sig = -1;
	diag.ai_probe_timeslice_id = -1;
	diag.ai_probe_process_obj = -1;
	diag.ai_probe_process_sig = -1;
	diag.ai_probe_process_id = -1;
	diag.ai_probe_phys_skip_obj = -1;
	diag.ai_probe_phys_skip_sig = -1;
	diag.ai_probe_phys_skip_id = -1;
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0, &frame_state, &diag, error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("diag capture frame failed: ") + error);
	}
	if (!input_demo_recorder_flush(demo_path.c_str(), error, sizeof(error))) {
		remove_test_dir(dir);
		return report_failure_string(std::string("diag recorder flush failed: ") + error);
	}
	if (!read_text_file(demo_path.c_str(), &text)) {
		remove(demo_path.c_str());
		remove(trace_path.c_str());
		remove_test_dir(dir);
		return report_failure("could not read recorder diag demo file");
	}
	if (text.find("\"diag\":{\"awareness_events\":7") == std::string::npos ||
		text.find("\"runtime_state_hash\":2468") == std::string::npos ||
		text.find("\"object_free_list_hash\":13579") == std::string::npos ||
		text.find("\"weapon_auto_fusion_delta\":-888") == std::string::npos ||
		text.find("\"player_weapon_count\":2") == std::string::npos ||
		text.find("\"highest_object_index\":347") == std::string::npos ||
		text.find("\"live_object_count\":219") == std::string::npos ||
		text.find("\"object_slot_counts\":[9") == std::string::npos ||
		text.find("\"debris_state_hash\":42424242") == std::string::npos ||
		text.find("\"segment_object_list_hash\":11235813") == std::string::npos ||
		text.find("\"player_weapon_obj0\":58") == std::string::npos ||
		text.find("\"player_weapon_obj1\":115") == std::string::npos) {
		remove(demo_path.c_str());
		remove(trace_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("unexpected recorder diag demo file: ") + text);
	}
	if (!input_demo_file_read(demo_path.c_str(), &parsed, &read_error)) {
		remove(demo_path.c_str());
		remove(trace_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("diag recorder demo read failed: ") + read_error);
	}
	remove(demo_path.c_str());
	remove(trace_path.c_str());
	remove_test_dir(dir);
	if (parsed.frames.size() != 1 || !parsed.frames[0].has_diag ||
		parsed.frames[0].diag_json.find("\"runtime_state_hash\":2468") == std::string::npos ||
		parsed.frames[0].diag_json.find("\"object_signature_seed\":19480") == std::string::npos ||
		parsed.frames[0].diag_json.find("\"weapon_last_omega_delta\":-999") == std::string::npos ||
		parsed.frames[0].diag_json.find("\"live_object_hash\":987654321") == std::string::npos ||
		parsed.frames[0].diag_json.find("\"object_slot_hashes\":[111") == std::string::npos ||
		parsed.frames[0].diag_json.find("\"segment_object_list_count\":219") == std::string::npos ||
		parsed.frames[0].diag_json.find("\"player_weapon_hash\":123456789") == std::string::npos ||
		parsed.frames[0].diag_json.find("\"fireball_object_count\":17") == std::string::npos ||
		parsed.frames[0].diag_json.find("\"player_weapon_obj1\":115") == std::string::npos)
		return report_failure("diag recorder demo round trip mismatch");
	return 0;
}

int main(void)
{
	if (expect_record_and_flush())
		return 1;
	if (expect_record_and_flush_checkpoint())
		return 1;
	if (expect_stage_consumed_pulse())
		return 1;
	if (expect_write_state_trace())
		return 1;
	if (expect_record_and_flush_events())
		return 1;
	if (expect_record_and_flush_diag())
		return 1;
	puts("PASS");
	return 0;
}