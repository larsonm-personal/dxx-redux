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
#include <chrono>
#include <sstream>
#include <vector>
#include <nlohmann/json.hpp>
#include "input_demo_replay.h"

#include "input_demo_fixture.h"
#include "input_demo_codec.h"
#include "input_demo_limits.h"
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

static bool write_text_file(const char *path, const char *text)
{
	FILE *file = fopen(path, "wb");
	if (!file) return false;
	const size_t length = strlen(text);
	const bool written = fwrite(text, 1, length, file) == length;
	return fclose(file) == 0 && written;
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
	player_cfg->autoselect_only_once = 1;
	player_cfg->original_homing = 1;
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
	return ",\"player_cfg\":{\"auto_leveling\":1,\"persistent_debris\":1,\"headlight_active_default\":0,\"no_fire_autoselect\":1,\"cycle_autoselect_only\":1,\"select_after_fire\":0,\"classic_autoselect_weapon\":1,\"autoselect_only_once\":1,\"original_homing\":1,\"guidebot_routing_mode\":1,\"primary_order\":[9,8,7,6,5,4,3,2,1,0,255],\"secondary_order\":[9,8,4,3,1,5,0,255,7,6,2]}";
#else
	return ",\"player_cfg\":{\"auto_leveling\":1,\"persistent_debris\":1,\"no_fire_autoselect\":1,\"cycle_autoselect_only\":1,\"select_after_fire\":0,\"classic_autoselect_weapon\":1,\"autoselect_only_once\":1,\"original_homing\":1,\"guidebot_routing_mode\":1,\"primary_order\":[4,3,2,1,0,255,16],\"secondary_order\":[4,3,1,0,255,2]}";
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
	settings.ordered_input = 0; // Existing tests exercise the aggregate legacy codec
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
	(void) d_rand();
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
	const std::string trace_path = demo_path + INPUT_DEMO_RNG_TRACE_SUFFIX;
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
	settings.ordered_input = 0; // Existing tests exercise the aggregate legacy codec
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
		remove(trace_path.c_str());
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
		remove(trace_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("unexpected checkpoint recorder demo file: ") + text);
	}
	if (!input_demo_file_read(demo_path.c_str(), &parsed, &read_error)) {
		remove(demo_path.c_str());
		remove(trace_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("checkpoint recorder demo read failed: ") + read_error);
	}
	remove(demo_path.c_str());
	remove(trace_path.c_str());
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
	settings.ordered_input = 0; // Existing tests exercise the aggregate legacy codec
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

static int expect_truncate_recording(void)
{
	const char *dir = "test_input_demo_recorder_truncate_fixture";
	const std::string demo_path = std::string(dir) + "/recorded_truncate.dximdemo";
	const std::string trace_path = demo_path + INPUT_DEMO_RNG_TRACE_SUFFIX;
	input_demo_recorder_settings settings;
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	input_demo_file parsed;
	char error[256] = "";
	std::string read_error;
	std::string text;

	if (!make_test_dir(dir))
		return report_failure("could not create truncate recorder test directory");
	input_demo_recorder_settings_clear(&settings);
	settings.ordered_input = 0; // Existing tests exercise the aggregate legacy codec
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 1;
	settings.difficulty = 2;
	settings.rng_mode = input_demo_test_rng_mode();
	if (!input_demo_recorder_start(&settings, error, sizeof(error))) {
		remove_test_dir(dir);
		return report_failure_string(std::string("truncate recorder start failed: ") + error);
	}
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	state.forward_thrust_time = 44;
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0, NULL, NULL, error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("truncate capture frame 0 failed: ") + error);
	}
	input_demo_control_pulse_clear(&pulse);
	pulse.fire_primary_count = 1;
	if (!input_demo_recorder_stage_direct_command_change_difficulty(3, error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("truncate stage difficulty command failed: ") + error);
	}
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 101, 0, 0, NULL, NULL, error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("truncate capture frame 1 failed: ") + error);
	}
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 102, 0, 0, NULL, NULL, error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("truncate capture frame 2 failed: ") + error);
	}
	if (!input_demo_recorder_stage_direct_command_death_abort(error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("truncate stage pending death command failed: ") + error);
	}
	input_demo_rng_trace_set_context(0, 3276);
	input_demo_rng_trace_record_srand(0, __FILE__, __func__, __LINE__, 1, 1, 11, 1, 12, 1234);
	input_demo_rng_trace_set_context(1, 6552);
	input_demo_rng_trace_record_rand(0, __FILE__, __func__, __LINE__, 2, 1, 12, 1, 13, 77);
	if (input_demo_recorder_frame_count() != 3)
		return report_failure("truncate recorder expected 3 captured frames before truncate");
	if (input_demo_rng_trace_event_count() != 2)
		return report_failure("truncate recorder expected 2 rng events before truncate");
	if (!input_demo_recorder_truncate(1))
		return report_failure("truncate recorder frame truncate failed");
	if (!input_demo_rng_trace_truncate(1))
		return report_failure("truncate recorder rng truncate failed");
	if (input_demo_recorder_frame_count() != 1)
		return report_failure("truncate recorder expected 1 captured frame after truncate");
	if (input_demo_rng_trace_event_count() != 1)
		return report_failure("truncate recorder expected 1 rng event after truncate");
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 103, 0, 0, NULL, NULL, error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("truncate replacement capture failed: ") + error);
	}
	if (!input_demo_recorder_flush(demo_path.c_str(), error, sizeof(error))) {
		input_demo_recorder_cancel();
		remove_test_dir(dir);
		return report_failure_string(std::string("truncate recorder flush failed: ") + error);
	}
	if (!input_demo_file_read(demo_path.c_str(), &parsed, &read_error)) {
		remove(demo_path.c_str());
		remove(trace_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("truncate recorder demo read failed: ") + read_error);
	}
	if (!read_text_file(trace_path.c_str(), &text)) {
		remove(demo_path.c_str());
		remove(trace_path.c_str());
		remove_test_dir(dir);
		return report_failure("could not read truncate recorder rng trace file");
	}
	remove(demo_path.c_str());
	remove(trace_path.c_str());
	remove_test_dir(dir);
	if (parsed.metadata.version != 3 || parsed.frames.size() != 2 || parsed.metadata.frame_count != 2 || parsed.result.frame_count != 2 ||
	    parsed.frames[0].rng.state != 100 || parsed.frames[0].input.held.forward_thrust_time != 44 ||
	    parsed.frames[1].rng.state != 103 || !parsed.frames[0].events.empty() ||
	    !parsed.frames[1].events.empty())
		return report_failure("truncate recorder demo round trip mismatch");
	if (text.find("\"events\":1") == std::string::npos ||
	    text.find("\"seq\":0") == std::string::npos ||
	    text.find("\"seq\":1") != std::string::npos)
		return report_failure_string(std::string("unexpected truncate recorder rng trace file: ") + text);
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
	    text.find("\"diag_version\":2") == std::string::npos ||
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
	settings.ordered_input = 0; // Existing tests exercise the aggregate legacy codec
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
	settings.ordered_input = 0; // Existing tests exercise the aggregate legacy codec
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
	std::string diag_text;
	std::string diag_hash;
	if (!input_demo_state_trace_diag_to_json_text(&diag, &diag_text, &read_error) ||
	    !input_demo_sha256_hex(reinterpret_cast<const unsigned char *>(diag_text.data()),
	                           diag_text.size(), &diag_hash, &read_error))
		return report_failure_string(read_error);
	// Preserve every diagnostic field and array, including canonical key order
	if (diag_hash != "0f4d4f46a42cede41acb27d773a59861d792e73f0876e067bbd4bc10fbf04430")
		return report_failure_string("diagnostic serialization changed: " + diag_hash);
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

static int expect_long_recording(void)
{
	const char *path = "test_input_demo_long.dximdemo";
	const unsigned int frame_count = 3000;
	input_demo_recorder_settings settings;
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	input_demo_result snapshot;
	input_demo_state_trace_diag diag = {};
	input_demo_file parsed;
	char error[256] = "";
	std::string read_error;
	input_demo_recorder_settings_clear(&settings);
	settings.ordered_input = 0; // Existing tests exercise the aggregate legacy codec
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 7;
	settings.rng_mode = input_demo_test_rng_mode();
	settings.record_per_frame_state = 1;
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	fill_test_frame_state(&snapshot, 0);
	if (!input_demo_recorder_start(&settings, error, sizeof(error)))
		return report_failure(error);
	const auto start = std::chrono::steady_clock::now();
	for (unsigned int i = 0; i < frame_count; ++i) {
		diag.runtime_state_hash = i;
		if (i == 37 && !input_demo_recorder_stage_frame_event_json("{\"kind\":\"staged\"}", error, sizeof(error)))
			return report_failure(error);
		if (!input_demo_recorder_capture_frame(6554, &state, &pulse, i, 1, i,
		                                       &snapshot, &diag, error, sizeof(error)))
			return report_failure(error);
		if (i == 37 || i == frame_count - 1) {
			if (!input_demo_recorder_append_frame_event_json("{\"kind\":\"late1\"}", error, sizeof(error)) ||
			    !input_demo_recorder_append_frame_event_json("{\"kind\":\"late2\"}", error, sizeof(error)))
				return report_failure(error);
		}
	}
	// Rewind drops the last frame's late events and reuses the preceding input state
	if (!input_demo_recorder_truncate(frame_count - 1) ||
	    !input_demo_recorder_capture_frame(6554, &state, &pulse, frame_count - 1, 1, frame_count - 1,
	                                       &snapshot, &diag, error, sizeof(error)))
		return report_failure("long recording rewind failed");
	// Opening a directory as a file must fail without consuming the recording
	if (input_demo_recorder_flush(".", error, sizeof(error)) ||
	    !input_demo_recorder_is_active() || input_demo_recorder_frame_count() != frame_count)
		return report_failure("failed flush consumed the recording");
	const auto captured = std::chrono::steady_clock::now();
	if (!input_demo_recorder_flush(path, error, sizeof(error)))
		return report_failure(error);
	const auto flushed = std::chrono::steady_clock::now();
	printf("3000 diagnostic frames: capture %.3f s, finalize %.3f s\n",
	       std::chrono::duration<double>(captured - start).count(),
	       std::chrono::duration<double>(flushed - captured).count());
	if (!input_demo_file_read(path, &parsed, &read_error))
		return report_failure_string(read_error);
	remove(path);
	remove((std::string(path) + INPUT_DEMO_RNG_TRACE_SUFFIX).c_str());
	if (parsed.frames.size() != frame_count || parsed.result.frame_count != frame_count)
		return report_failure("long recording frame count mismatch");
	for (unsigned int i = 0; i < frame_count; ++i) {
		if (!parsed.frames[i].has_diag || !parsed.frames[i].has_state ||
		    parsed.frames[i].rng.state != i ||
		    parsed.frames[i].input.has_frame_time != (i == 0) ||
		    parsed.frames[i].events.size() != (i == 37 ? 3u : 0u) ||
		    parsed.frames[i].diag_json.find("\"runtime_state_hash\":" + std::to_string(i) + ",") == std::string::npos)
			return report_failure("long recording frame contents mismatch");
	}
	if (parsed.frames[37].events[0] != "{\"kind\":\"staged\"}" ||
	    parsed.frames[37].events[1] != "{\"kind\":\"late1\"}" ||
	    parsed.frames[37].events[2] != "{\"kind\":\"late2\"}")
		return report_failure("long recording event order mismatch");
	return 0;
}

static int expect_ordered_rewind_cancellation(void)
{
	input_demo_recorder_settings settings;
	input_demo_recorder_settings_clear(&settings);
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 1;
	settings.rng_mode = input_demo_test_rng_mode();
	input_demo_control_state state = {};
	input_demo_control_pulse pulse = {};
	char error[256] = "";
	input_demo_recorder_cancel();
	if (input_demo_recorder_cancel_for_rewind() || input_demo_recorder_is_active() || input_demo_recorder_frame_count())
		return report_failure("rewind cancellation changed an inactive recorder");
	for (int fire = 0; fire != 2; ++fire) {
		state = {};
		pulse = {};
		if (!input_demo_recorder_start(&settings, error, sizeof(error)) ||
		    !input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 1, 1, nullptr, nullptr, error, sizeof(error)) ||
		    !input_demo_recorder_begin_input_batch(100, 1, 3276, error, sizeof(error))) return report_failure_string(error);
		if (fire) {
			state.fire_secondary_state = 1;
			pulse.fire_secondary_count = 1;
			if (!input_demo_recorder_stage_input_sample(3276, &state, &pulse, error, sizeof(error))) return report_failure_string(error);
		}
		if (!input_demo_recorder_cancel_for_rewind() || input_demo_recorder_is_active() || input_demo_recorder_frame_count() ||
		    input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 1, 1, nullptr, nullptr, error, sizeof(error)) ||
		    input_demo_recorder_finish_input_tail(&state, &pulse, 100, 1, error, sizeof(error)) ||
		    input_demo_recorder_flush("test_input_demo_rewind_cancelled.dximdemo", error, sizeof(error)))
			return report_failure("rewind retained an active or flushable ordered recording");
	}
	// The temporary legacy writer remains unchanged by this ordered-only guard
	settings.ordered_input = 0;
	if (!input_demo_recorder_start(&settings, error, sizeof(error)) ||
	    input_demo_recorder_cancel_for_rewind() || !input_demo_recorder_is_active())
		return report_failure("ordered rewind guard changed a legacy writer");
	input_demo_recorder_cancel();
	return 0;
}

static int expect_ordered_command_domains(void)
{
	using json = nlohmann::ordered_json;
	std::string error;
	input_demo_replay_direct_command_event command;
	for (int key = -1; key <= 0x302; ++key) {
		const bool valid = key == 0x13 || (key >= 0x02 && key <= 0x0b) || (key >= 0x102 && key <= 0x10b);
		for (bool from_menu : { false, true }) {
			const json event = { { "kind", "direct_command" }, { "command", "guidebot_goal" }, { "special_key", key }, { "from_menu", from_menu } };
			if (input_demo_dispatch_command_from_json(event.dump(), INPUT_DEMO_GAME_D2, &command, &error) != valid)
				return report_failure("Guide-Bot command accepted an unsupported key or rejected a valid one");
		}
	}
	for (int marker = -1; marker <= 16; ++marker) {
		const json event = { { "kind", "direct_command" }, { "command", "drop_marker" }, { "player_marker_num", marker }, { "message", "ok" } };
		if (input_demo_dispatch_command_from_json(event.dump(), INPUT_DEMO_GAME_D2, &command, &error) != (marker >= 0 && marker < 9))
			return report_failure("marker command accepted an unsupported player marker slot");
	}
	for (const char *name : { "guidebot_goal", "drop_marker" }) {
		const json event = std::string(name) == "guidebot_goal" ? json({ { "kind", "direct_command" }, { "command", name }, { "special_key", INT32_MAX }, { "from_menu", false } }) : json({ { "kind", "direct_command" }, { "command", name }, { "player_marker_num", INT32_MAX }, { "message", "ok" } });
		if (input_demo_dispatch_command_from_json(event.dump(), INPUT_DEMO_GAME_D2, &command, &error))
			return report_failure("out-of-domain command integer was accepted");
	}
	return 0;
}

static int expect_all_weapons_command_domains(void)
{
	using json = nlohmann::ordered_json;
	const char *names[] = { "", "d1_scourge", "d1_bigred", "d2_honestbob" };
	std::string error, text;
	input_demo_replay_direct_command_event command;
	if (INPUT_DEMO_REPLAY_DIRECT_COMMAND_ENABLE_CHEATS != 15 || INPUT_DEMO_REPLAY_DIRECT_COMMAND_GRANT_ALL_WEAPONS != 16 ||
	    INPUT_DEMO_GRANT_ALL_WEAPONS_D1_SCOURGE != 1 || INPUT_DEMO_GRANT_ALL_WEAPONS_D1_BIGRED != 2 ||
	    INPUT_DEMO_GRANT_ALL_WEAPONS_D2_HONESTBOB != 3)
		return report_failure("all-weapons wire constants changed");
	for (int game : { INPUT_DEMO_GAME_D1, INPUT_DEMO_GAME_D2 }) {
		for (int grant = -1; grant <= 4; ++grant) {
			const bool supported = grant >= 1 && grant <= (game == INPUT_DEMO_GAME_D1 ? 2 : 3);
			if (input_demo_all_weapons_grant_supported_for_game(grant, game) != supported)
				return report_failure("all-weapons grant game domain mismatch");
		}
		for (int grant = 1; grant <= 3; ++grant) {
			const bool supported = game == INPUT_DEMO_GAME_D2 || grant != 3;
			const json event = { { "kind", "direct_command" }, { "command", "grant_all_weapons" }, { "grant", names[grant] } };
			if (input_demo_dispatch_command_from_json(event.dump(), game, &command, &error) != supported)
				return report_failure("all-weapons command accepted the wrong fixture game");
			if (!supported) continue;
			if (command.kind != INPUT_DEMO_REPLAY_DIRECT_COMMAND_GRANT_ALL_WEAPONS || command.value0 != grant || command.value1 || command.text[0])
				return report_failure("all-weapons command lost its semantic grant");
			input_demo_input_operation operation = {};
			operation.kind = INPUT_DEMO_INPUT_COMMAND;
			operation.frame_time = 7;
			operation.command = command;
			const json expected = { { "k", "command" }, { "ft", 7 }, { "event", event } };
			if (!input_demo_dispatch_operation_to_json(operation, game, &text, &error) || text != expected.dump())
				return report_failure("all-weapons command serialization changed");
			operation.command.value1 = 1;
			if (input_demo_dispatch_operation_to_json(operation, game, &text, &error))
				return report_failure("all-weapons serialization ignored an extra argument");
			operation.command.value1 = 0;
			operation.command.text[0] = 'x';
			if (input_demo_dispatch_operation_to_json(operation, game, &text, &error))
				return report_failure("all-weapons serialization ignored extra text");
		}
		const json enable = { { "kind", "direct_command" }, { "command", "enable_cheats" } };
		if (!input_demo_dispatch_command_from_json(enable.dump(), game, &command, &error) ||
		    command.kind != INPUT_DEMO_REPLAY_DIRECT_COMMAND_ENABLE_CHEATS || command.value0 || command.value1 || command.text[0])
			return report_failure("enable cheats command did not round trip");
		input_demo_input_operation operation = {};
		operation.kind = INPUT_DEMO_INPUT_COMMAND;
		operation.command = command;
		if (!input_demo_dispatch_operation_to_json(operation, game, &text, &error) ||
		    text != json({ { "k", "command" }, { "ft", 0 }, { "event", enable } }).dump())
			return report_failure("enable cheats command serialization changed");
		for (int field = 0; field < 3; ++field) {
			operation.command = command;
			if (field == 0) operation.command.value0 = 1;
			if (field == 1) operation.command.value1 = 1;
			if (field == 2) operation.command.text[0] = 'x';
			if (input_demo_dispatch_operation_to_json(operation, game, &text, &error))
				return report_failure("enable cheats serialization ignored arguments");
		}
		std::vector<json> invalid = {
			{ { "kind", "direct_command" }, { "command", "grant_all_weapons" } },
			{ { "kind", "direct_command" }, { "command", "grant_all_weapons" }, { "grant", "d1_scourge" }, { "extra", 0 } },
			{ { "kind", "direct_command" }, { "command", "enable_cheats" }, { "grant", "d1_scourge" } },
			{ { "kind", "direct_command" }, { "command", "enable_cheats" }, { "enabled", true } },
			{ { "kind", "direct_command" }, { "command", "ENABLE_CHEATS" } },
		};
		for (const json &value : { json(nullptr), json(false), json(1), json(1.0), json(""), json("scourge"), json("D1_SCOURGE"), json::array(), json::object() })
			invalid.push_back({ { "kind", "direct_command" }, { "command", "grant_all_weapons" }, { "grant", value } });
		for (const auto &event : invalid)
			if (input_demo_dispatch_command_from_json(event.dump(), game, &command, &error))
				return report_failure("malformed all-weapons command was accepted");
	}
	if (input_demo_all_weapons_grant_supported(INT32_MAX) || input_demo_all_weapons_grant_supported(INT32_MIN) ||
	    input_demo_all_weapons_grant_supported_for_game(1, 0))
		return report_failure("invalid all-weapons wire domain was accepted");
	return 0;
}

static int expect_ordered_all_weapons_recording(void)
{
	using json = nlohmann::ordered_json;
	const char *path = "test_input_demo_all_weapons.dximdemo";
	input_demo_recorder_settings settings;
	input_demo_recorder_settings_clear(&settings);
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 1;
	settings.rng_mode = input_demo_test_rng_mode();
	const int first = settings.game == INPUT_DEMO_GAME_D1 ? INPUT_DEMO_GRANT_ALL_WEAPONS_D1_SCOURGE : INPUT_DEMO_GRANT_ALL_WEAPONS_D2_HONESTBOB;
	const int second = settings.game == INPUT_DEMO_GAME_D1 ? INPUT_DEMO_GRANT_ALL_WEAPONS_D1_BIGRED : first;
	input_demo_control_state state = {};
	input_demo_control_pulse pulse = {};
	char error[256] = "";
	if (!input_demo_recorder_start(&settings, error, sizeof(error)) ||
	    !input_demo_recorder_begin_input_batch(1, 0, 7, error, sizeof(error)) ||
	    !input_demo_recorder_stage_direct_command_enable_cheats(error, sizeof(error)) ||
	    !input_demo_recorder_stage_direct_command_grant_all_weapons(first, error, sizeof(error)) ||
	    !input_demo_recorder_stage_direct_command_grant_all_weapons(first, error, sizeof(error)) ||
	    !input_demo_recorder_stage_input_sample(8, &state, &pulse, error, sizeof(error)) ||
	    !input_demo_recorder_stage_direct_command_grant_all_weapons(second, error, sizeof(error)) ||
	    !input_demo_recorder_stage_direct_command_enable_cheats(error, sizeof(error)) ||
	    !input_demo_recorder_capture_frame(9, &state, &pulse, 1, 1, 0, nullptr, nullptr, error, sizeof(error)) ||
	    !input_demo_recorder_begin_input_batch(1, 0, 10, error, sizeof(error)) ||
	    !input_demo_recorder_stage_direct_command_grant_all_weapons(first, error, sizeof(error)) ||
	    !input_demo_recorder_finish_input_tail(&state, &pulse, 1, 0, error, sizeof(error)) ||
	    !input_demo_recorder_flush(path, error, sizeof(error)))
		return report_failure_string(error);
	input_demo_file parsed;
	std::string text, detail;
	if (!input_demo_file_read(path, &parsed, &detail) || !read_text_file(path, &text)) return report_failure_string(detail);
	const auto &ops = parsed.frames[0].dispatch.operations;
	if (parsed.metadata.version != 5 || ops.size() != 6 || parsed.input_tail.dispatch.operations.size() != 1 ||
	    ops[0].command.kind != INPUT_DEMO_REPLAY_DIRECT_COMMAND_ENABLE_CHEATS ||
	    ops[1].command.kind != INPUT_DEMO_REPLAY_DIRECT_COMMAND_GRANT_ALL_WEAPONS || ops[1].command.value0 != first ||
	    ops[2].command.kind != INPUT_DEMO_REPLAY_DIRECT_COMMAND_GRANT_ALL_WEAPONS || ops[2].command.value0 != first ||
	    ops[3].kind != INPUT_DEMO_INPUT_SAMPLE || ops[4].command.value0 != second ||
	    ops[5].command.kind != INPUT_DEMO_REPLAY_DIRECT_COMMAND_ENABLE_CHEATS ||
	    parsed.input_tail.dispatch.operations[0].command.value0 != first ||
	    parsed.input_tail.dispatch.operations[0].frame_time != 10 || !parsed.frames[0].events.empty())
		return report_failure("all-weapons recording changed order, repetitions or tail commands");
	if (!input_demo_replay_load(path, error, sizeof(error))) return report_failure_string(error);
	for (uint32_t i = 0; i < ops.size(); ++i) {
		input_demo_input_operation actual = {};
		if (!input_demo_replay_get_input_operation(0, i, &actual, error, sizeof(error)) ||
		    actual.kind != ops[i].kind || actual.command.kind != ops[i].command.kind || actual.command.value0 != ops[i].command.value0)
			return report_failure("all-weapons replay accessor lost command order");
	}
	input_demo_replay_unload();
	remove(path);
	remove((std::string(path) + INPUT_DEMO_RNG_TRACE_SUFFIX).c_str());

	// Legacy formats cannot silently downgrade these ordered-only commands to diagnostics
	for (const char *name : { "enable_cheats", "grant_all_weapons" }) {
		const json event = std::string(name) == "enable_cheats" ? json({ { "kind", "direct_command" }, { "command", name } }) : json({ { "kind", "direct_command" }, { "command", name }, { "grant", "d1_scourge" } });
		for (int version : { 2, 3, 4 }) {
			input_demo_file legacy = parsed;
			legacy.metadata.version = version;
			legacy.has_input_tail = false;
			legacy.frames[0].dispatch = input_demo_input_batch();
			if (!input_demo_file_to_text(legacy, &text, &detail)) return report_failure_string(detail);
			std::vector<json> rows;
			std::istringstream lines(text);
			for (std::string line; std::getline(lines, line);)
				if (!line.empty()) rows.push_back(json::parse(line));
			rows[1]["events"] = json::array({ event });
			text.clear();
			for (const auto &row : rows) text += row.dump() + "\n";
			input_demo_file rejected;
			if (input_demo_file_parse_text(text, &rejected, &detail)) return report_failure("legacy fixture accepted all-weapons command");
			legacy.frames[0].events.push_back(event.dump());
			if (input_demo_file_to_text(legacy, &text, &detail)) return report_failure("legacy writer accepted all-weapons command");
		}
	}
	settings.ordered_input = 0;
	if (!input_demo_recorder_start(&settings, error, sizeof(error))) return report_failure_string(error);
	if (input_demo_recorder_stage_direct_command_enable_cheats(error, sizeof(error)) ||
	    input_demo_recorder_stage_direct_command_grant_all_weapons(first, error, sizeof(error)))
		return report_failure("legacy recorder accepted all-weapons staging API");
	for (const char *event : { "{\"kind\":\"direct_command\",\"command\":\"enable_cheats\"}",
	                           "{\"kind\":\"direct_command\",\"command\":\"grant_all_weapons\",\"grant\":\"d1_scourge\"}" }) {
		if (input_demo_recorder_stage_frame_event_json(event, error, sizeof(error)) ||
		    input_demo_recorder_append_frame_event_json(event, error, sizeof(error)))
			return report_failure("legacy event API accepted all-weapons command");
	}
	input_demo_recorder_cancel();
	settings.ordered_input = 1;
	for (int grant : { -1, 0, 4, INT32_MAX, settings.game == INPUT_DEMO_GAME_D1 ? 3 : -2 }) {
		if (!input_demo_recorder_start(&settings, error, sizeof(error)) ||
		    !input_demo_recorder_begin_input_batch(1, 0, 1, error, sizeof(error))) return report_failure_string(error);
		if (input_demo_recorder_stage_direct_command_grant_all_weapons(grant, error, sizeof(error)) ||
		    input_demo_recorder_capture_frame(1, &state, &pulse, 1, 1, 0, nullptr, nullptr, error, sizeof(error)))
			return report_failure("invalid all-weapons grant did not poison ordered recording");
		input_demo_recorder_cancel();
	}
	return 0;
}

static int expect_cockpit_projection_codec(void)
{
	using json = nlohmann::ordered_json;
	const char *path = "test_input_demo_view_projection.dximdemo";
	input_demo_recorder_settings settings;
	input_demo_recorder_settings_clear(&settings);
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 1;
	settings.rng_mode = input_demo_test_rng_mode();
	settings.has_view_projection = 1;
	const input_demo_view_projection initial = { 640, 480, 65536, 0x9000, 9000, 0, settings.game == INPUT_DEMO_GAME_D2 ? 1 : 0 };
	settings.view_projection = initial;
	input_demo_control_state state = {};
	input_demo_control_pulse pulse = {};
	input_demo_view_projection actual = {};
	char error[256] = "";
	input_demo_recorder_cancel();
	if (input_demo_recorder_get_view_projection(&actual)) return report_failure("inactive recorder exposed projection");
	if (!input_demo_recorder_start(&settings, error, sizeof(error)) ||
	    !input_demo_recorder_begin_input_batch(100, 2, 0, error, sizeof(error))) return report_failure_string(error);
	settings.view_projection.screen_width = 800;
	if (!input_demo_recorder_get_view_projection(&actual) || input_demo_recorder_get_view_projection(nullptr) ||
	    memcmp(&actual, &initial, sizeof(initial))) return report_failure("recorder projection is absent or aliases mutable caller settings");
	settings.view_projection = initial;
	const int cockpit_modes[] = { 0, 2, 3 };
	for (int current : cockpit_modes)
		for (int preferred : cockpit_modes)
			if (!input_demo_recorder_stage_input_cockpit(current * 1234, current, preferred, error, sizeof(error))) return report_failure_string(error);
	if (!input_demo_recorder_capture_frame(2000, &state, &pulse, 100, 1, 2, nullptr, nullptr, error, sizeof(error)) ||
	    !input_demo_recorder_begin_input_batch(100, 2, 0, error, sizeof(error)) ||
	    !input_demo_recorder_stage_input_cockpit(65536, 3, 0, error, sizeof(error)) ||
	    !input_demo_recorder_finish_input_tail(&state, &pulse, 100, 2, error, sizeof(error)) ||
	    !input_demo_recorder_flush(path, error, sizeof(error))) return report_failure_string(error);
	input_demo_file demo;
	std::string text, detail;
	if (!input_demo_file_read(path, &demo, &detail) || !input_demo_file_to_text(demo, &text, &detail) ||
	    !write_text_file(path, text.c_str()) || !input_demo_replay_load(path, error, sizeof(error))) return report_failure_string(detail + error);
	if (!input_demo_replay_get_view_projection(&actual) || input_demo_replay_get_view_projection(nullptr) || memcmp(&actual, &initial, sizeof(initial)))
		return report_failure("projection replay getter changed recorded geometry");
	for (int index = 0; index != 9; ++index) {
		input_demo_input_operation operation = {};
		if (!input_demo_replay_get_input_operation(0, index, &operation, error, sizeof(error)) ||
		    operation.kind != INPUT_DEMO_INPUT_COCKPIT || operation.current_cockpit != cockpit_modes[index / 3] || operation.preferred_cockpit != cockpit_modes[index % 3] ||
		    operation.frame_time != cockpit_modes[index / 3] * 1234) return report_failure("resolved cockpit operation round trip differs");
	}
	input_demo_replay_unload();
	if (input_demo_replay_get_view_projection(&actual)) return report_failure("unloaded replay exposed projection");
	std::vector<json> rows;
	std::istringstream lines(text);
	std::string line;
	while (std::getline(lines, line)) rows.push_back(json::parse(line));
	const auto parses = [&](const std::vector<json> &candidate) {
		std::string encoded;
		for (const auto &row : candidate) encoded += row.dump() + "\n";
		input_demo_file parsed;
		return input_demo_file_parse_text(encoded, &parsed, &detail);
	};
	const json maxima = { { "screen_width", 16384 }, { "screen_height", 16384 }, { "aspect", 16 * 65536 }, { "zoom", 16 * 65536 }, { "depth", 9000 }, { "classic_depth", 1 }, { "missile_view_enabled", settings.game == INPUT_DEMO_GAME_D2 ? 1 : 0 } };
	for (auto field = maxima.begin(); field != maxima.end(); ++field) {
		auto valid = rows;
		valid[0]["view_projection"][field.key()] = field.value();
		if (!parses(valid)) return report_failure_string("supported projection bound rejected: " + detail);
		for (const auto &value : json::array({ -1, field.value().get<int>() + 1, UINT64_MAX, false, 1.0, "1", nullptr })) {
			auto bad = rows;
			bad[0]["view_projection"][field.key()] = value;
			if (parses(bad)) return report_failure("invalid projection value accepted");
		}
		auto bad = rows;
		bad[0]["view_projection"].erase(field.key());
		if (parses(bad)) return report_failure("partial projection contract accepted");
		if (field.key() != "classic_depth" && field.key() != "missile_view_enabled") {
			bad = rows;
			bad[0]["view_projection"][field.key()] = 0;
			if (parses(bad)) return report_failure("zero projection field accepted");
		}
	}
	auto bad = rows;
	bad[0]["view_projection"]["screen_width"] = 16384;
	bad[0]["view_projection"]["screen_height"] = 4096;
	if (!parses(bad)) return report_failure("maximum projection pixel count rejected");
	bad[0]["view_projection"]["screen_height"] = 4097;
	if (parses(bad)) return report_failure("oversized projection pixel count accepted");
	bad = rows;
	bad[0]["view_projection"]["main_view_fov"] = 110;
	if (parses(bad)) return report_failure("unknown projection field accepted");
	for (const char *field : { "current_cockpit", "preferred_cockpit" }) {
		for (const auto &value : json::array({ -1, 1, 4, 5, UINT64_MAX, false, 1.0, "1", nullptr })) {
			bad = rows;
			bad[1]["dispatch"]["ops"][0][field] = value;
			if (parses(bad)) return report_failure("invalid resolved cockpit accepted");
		}
		bad = rows;
		bad[1]["dispatch"]["ops"][0].erase(field);
		if (parses(bad)) return report_failure("partial cockpit operation accepted");
	}
	bad = rows;
	bad[1]["dispatch"]["ops"][0]["s"] = json::object();
	if (parses(bad)) return report_failure("cockpit operation accepted controls payload");
	demo.metadata.has_view_projection = false;
	if (!input_demo_file_to_text(demo, &text, &detail) || !write_text_file(path, text.c_str()) ||
	    !input_demo_replay_load(path, error, sizeof(error))) return report_failure_string(detail + error);
	actual = initial;
	if (input_demo_replay_get_view_projection(&actual) || memcmp(&actual, &initial, sizeof(initial)))
		return report_failure("absent projection changed native startup output");
	input_demo_replay_unload();
	remove(path);
	remove((std::string(path) + INPUT_DEMO_RNG_TRACE_SUFFIX).c_str());
	input_demo_recorder_cancel();
	settings.has_view_projection = 0;
	if (!input_demo_recorder_start(&settings, error, sizeof(error)) || input_demo_recorder_get_view_projection(&actual))
		return report_failure("absent recorder projection was synthesized");
	input_demo_recorder_cancel();
	settings.has_view_projection = 1;
	settings.view_projection.depth = 9001;
	if (input_demo_recorder_start(&settings, error, sizeof(error))) return report_failure("recorder accepted unsupported projection");
	settings.view_projection = initial;
	if (!input_demo_recorder_start(&settings, error, sizeof(error)) ||
	    !input_demo_recorder_begin_input_batch(100, 2, 0, error, sizeof(error))) return report_failure_string(error);
	if (input_demo_recorder_stage_input_cockpit(0, 5, 0, error, sizeof(error)) ||
	    input_demo_recorder_finish_input_tail(&state, &pulse, 100, 2, error, sizeof(error)))
		return report_failure("invalid cockpit operation did not poison capture");
	input_demo_recorder_cancel();
	return 0;
}

static int expect_pending_input_operation_count(void)
{
	input_demo_recorder_settings settings;
	input_demo_recorder_settings_clear(&settings);
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 1;
	settings.rng_mode = input_demo_test_rng_mode();
	input_demo_control_state state = {};
	input_demo_control_pulse pulse = {};
	char error[256] = "";
	input_demo_recorder_cancel();
	if (input_demo_recorder_pending_input_operation_count()) return report_failure("inactive recorder retained pending operation count");
	if (!input_demo_recorder_start(&settings, error, sizeof(error)) ||
	    input_demo_recorder_pending_input_operation_count() ||
	    !input_demo_recorder_begin_input_batch(100, 2, 0, error, sizeof(error)) ||
	    input_demo_recorder_pending_input_operation_count()) return report_failure("empty active input batch is not empty");
	if (!input_demo_recorder_stage_input_reset(0, error, sizeof(error)) ||
	    input_demo_recorder_pending_input_operation_count() != 1 ||
	    !input_demo_recorder_stage_input_controls(1234, &state, &pulse, error, sizeof(error)) ||
	    input_demo_recorder_pending_input_operation_count() != 2 ||
	    input_demo_recorder_pending_input_operation_count() != 2)
		return report_failure("pending operation count is wrong or mutates on read");
	if (!input_demo_recorder_capture_frame(2000, &state, &pulse, 100, 1, 2, nullptr, nullptr, error, sizeof(error)) ||
	    input_demo_recorder_pending_input_operation_count() ||
	    !input_demo_recorder_begin_input_batch(100, 2, 0, error, sizeof(error)) ||
	    !input_demo_recorder_stage_input_clear(0, error, sizeof(error)) ||
	    input_demo_recorder_pending_input_operation_count() != 1)
		return report_failure("pending operation count did not reset at frame boundary");
	if (!input_demo_recorder_finish_input_tail(&state, &pulse, 100, 2, error, sizeof(error)) ||
	    input_demo_recorder_pending_input_operation_count()) return report_failure("finalized tail remains pending");
	input_demo_recorder_cancel();
	if (!input_demo_recorder_start(&settings, error, sizeof(error)) ||
	    !input_demo_recorder_begin_input_batch(100, 2, 0, error, sizeof(error)) ||
	    !input_demo_recorder_stage_input_reset(0, error, sizeof(error))) return report_failure_string(error);
	input_demo_recorder_cancel();
	if (input_demo_recorder_pending_input_operation_count() || input_demo_recorder_is_active())
		return report_failure("cancellation retained pending operations");
	return 0;
}

static int expect_ordered_rear_view_codec(void)
{
	using json = nlohmann::ordered_json;
	const char *path = "test_input_demo_ordered_rear_view.dximdemo";
	input_demo_recorder_settings settings;
	input_demo_recorder_settings_clear(&settings);
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 1;
	settings.rng_mode = input_demo_test_rng_mode();
	settings.has_rear_view_baseline = 1;
	settings.rear_view_baseline = { 1, 0, 1, 4 };
	input_demo_control_state state = {};
	input_demo_control_pulse pulse = {};
	char error[256] = "";
	if (!input_demo_recorder_start(&settings, error, sizeof(error)) ||
	    !input_demo_recorder_begin_input_batch(100, 2, 0, error, sizeof(error))) return report_failure_string(error);
	const int actions[] = { INPUT_DEMO_REAR_VIEW_ARM, INPUT_DEMO_REAR_VIEW_RELEASE_OFF, INPUT_DEMO_REAR_VIEW_ENTER,
		                    INPUT_DEMO_REAR_VIEW_TOGGLE_OFF, INPUT_DEMO_REAR_VIEW_RESET };
	for (int action : actions)
		if (!input_demo_recorder_stage_input_rear_view(action * 1234, action, error, sizeof(error))) return report_failure_string(error);
	if (!input_demo_recorder_capture_frame(2000, &state, &pulse, 100, 1, 2, nullptr, nullptr, error, sizeof(error)) ||
	    !input_demo_recorder_begin_input_batch(100, 2, 0, error, sizeof(error)) ||
	    !input_demo_recorder_stage_input_rear_view(0, INPUT_DEMO_REAR_VIEW_ENTER, error, sizeof(error)) ||
	    !input_demo_recorder_finish_input_tail(&state, &pulse, 100, 2, error, sizeof(error)) ||
	    !input_demo_recorder_flush(path, error, sizeof(error))) return report_failure_string(error);
	input_demo_file demo;
	std::string text, detail;
	if (!input_demo_file_read(path, &demo, &detail) || !input_demo_file_to_text(demo, &text, &detail) ||
	    !write_text_file(path, text.c_str()) || !input_demo_replay_load(path, error, sizeof(error))) return report_failure_string(detail + error);
	input_demo_rear_view_baseline baseline = {};
	if (!input_demo_replay_get_rear_view_baseline(&baseline) || input_demo_replay_get_rear_view_baseline(nullptr) ||
	    std::memcmp(&baseline, &settings.rear_view_baseline, sizeof(baseline)))
		return report_failure("rear-view baseline round trip differs");
	for (unsigned i = 0; i < sizeof(actions) / sizeof(*actions); ++i) {
		input_demo_input_operation operation = {};
		if (!input_demo_replay_get_input_operation(0, i, &operation, error, sizeof(error)) ||
		    operation.kind != INPUT_DEMO_INPUT_REAR_VIEW || operation.rear_view_action != actions[i] || operation.frame_time != actions[i] * 1234)
			return report_failure("rear-view action or input-time order differs");
	}
	input_demo_replay_unload();
	if (input_demo_replay_get_rear_view_baseline(&baseline)) return report_failure("unloaded replay exposed rear-view baseline");
	std::vector<json> rows;
	std::istringstream lines(text);
	std::string line;
	while (std::getline(lines, line)) rows.push_back(json::parse(line));
	const auto parses = [&](const std::vector<json> &candidate) {
		std::string encoded;
		for (const auto &row : candidate) encoded += row.dump() + "\n";
		input_demo_file parsed;
		return input_demo_file_parse_text(encoded, &parsed, &detail);
	};
	for (const char *field : { "active", "leave_mode", "current_cockpit", "preferred_cockpit" }) {
		for (const auto &value : json::array({ -1, 5, INT32_MAX, UINT64_MAX, false, 1.0, "1", nullptr })) {
			auto bad = rows;
			bad[0]["rear_view"][field] = value;
			if (parses(bad)) return report_failure("invalid rear-view baseline value accepted");
		}
		auto bad = rows;
		bad[0]["rear_view"].erase(field);
		if (parses(bad)) return report_failure("partial rear-view baseline accepted");
	}
	for (const auto &action : json::array({ 0, 1, true, nullptr, "", "toggle", "=", "ENTER", json::object() })) {
		auto bad = rows;
		bad[1]["dispatch"]["ops"][0]["action"] = action;
		if (parses(bad)) return report_failure("invalid rear-view action accepted");
	}
	auto bad = rows;
	bad[1]["dispatch"]["ops"][0]["phase"] = "active";
	if (parses(bad)) return report_failure("rear-view operation accepted unrelated fields");
	bad = rows;
	bad[0]["rear_view"]["entry_time"] = 0;
	if (parses(bad)) return report_failure("rear-view baseline accepted unknown wall-clock field");
	// An absent baseline must remain absent, not silently install a zero state
	demo.metadata.has_rear_view_baseline = false;
	if (!input_demo_file_to_text(demo, &text, &detail) || !write_text_file(path, text.c_str()) ||
	    !input_demo_replay_load(path, error, sizeof(error))) return report_failure_string(detail + error);
	baseline = { 1, 1, 4, 3 };
	if (input_demo_replay_get_rear_view_baseline(&baseline) || baseline.active != 1 || baseline.leave_mode != 1 || baseline.current_cockpit != 4 || baseline.preferred_cockpit != 3)
		return report_failure("absent rear-view baseline changed output state");
	input_demo_replay_unload();
	remove(path);
	remove((std::string(path) + INPUT_DEMO_RNG_TRACE_SUFFIX).c_str());
	for (int action : { 0, -1, 6, INT32_MAX }) {
		if (!input_demo_recorder_start(&settings, error, sizeof(error)) ||
		    !input_demo_recorder_begin_input_batch(100, 2, 0, error, sizeof(error))) return report_failure_string(error);
		if (input_demo_recorder_stage_input_rear_view(1, action, error, sizeof(error)) ||
		    input_demo_recorder_finish_input_tail(&state, &pulse, 100, 2, error, sizeof(error)))
			return report_failure("invalid rear-view action did not poison capture");
		input_demo_recorder_cancel();
	}
	settings.rear_view_baseline.active = 2;
	if (input_demo_recorder_start(&settings, error, sizeof(error))) return report_failure("recorder accepted invalid rear-view baseline");
	return 0;
}

static int expect_ordered_automap_codec(void)
{
	using json = nlohmann::ordered_json;
	const char *path = "test_input_demo_ordered_automap.dximdemo";
	input_demo_recorder_settings settings;
	input_demo_recorder_settings_clear(&settings);
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 1;
	settings.rng_mode = input_demo_test_rng_mode();
	input_demo_control_state state = {};
	input_demo_control_pulse pulse = {};
	state.fire_secondary_state = 3;
	pulse.fire_secondary_count = 4;
	char error[256] = "";
	if (!input_demo_recorder_start(&settings, error, sizeof(error)) ||
	    !input_demo_recorder_begin_input_batch(100, 2, 0, error, sizeof(error)) ||
	    !input_demo_recorder_stage_input_controls(100, &state, &pulse, error, sizeof(error)) ||
	    !input_demo_recorder_stage_input_automap(101, INPUT_DEMO_AUTOMAP_BEGIN, error, sizeof(error)) ||
	    !input_demo_recorder_stage_input_clear(102, error, sizeof(error)) ||
	    !input_demo_recorder_stage_input_automap(103, INPUT_DEMO_AUTOMAP_DEPTHS, error, sizeof(error)) ||
	    !input_demo_recorder_stage_input_automap(104, INPUT_DEMO_AUTOMAP_ACTIVE, error, sizeof(error)) ||
	    !input_demo_recorder_stage_input_automap(105, INPUT_DEMO_AUTOMAP_CLOSE, error, sizeof(error))) return report_failure_string(error);
	if (settings.game == INPUT_DEMO_GAME_D2 && !input_demo_recorder_stage_direct_command_delete_marker(8, 12345, error, sizeof(error))) return report_failure_string(error);
	if (!input_demo_recorder_capture_frame(2000, &state, &pulse, 100, 1, 2, nullptr, nullptr, error, sizeof(error)) ||
	    !input_demo_recorder_begin_input_batch(100, 2, 2000, error, sizeof(error)) ||
	    !input_demo_recorder_stage_input_automap(0, INPUT_DEMO_AUTOMAP_BEGIN, error, sizeof(error))) return report_failure_string(error);
	state = {};
	pulse = {};
	if (!input_demo_recorder_stage_input_controls(65536, &state, &pulse, error, sizeof(error)) ||
	    !input_demo_recorder_stage_input_automap(65536, INPUT_DEMO_AUTOMAP_DEPTHS, error, sizeof(error)) ||
	    !input_demo_recorder_stage_input_automap(65536, INPUT_DEMO_AUTOMAP_ACTIVE, error, sizeof(error)) ||
	    !input_demo_recorder_finish_input_tail(&state, &pulse, 100, 2, error, sizeof(error)) ||
	    !input_demo_recorder_flush(path, error, sizeof(error))) return report_failure_string(error);
	input_demo_file demo;
	std::string detail, text;
	if (!input_demo_file_read(path, &demo, &detail)) return report_failure_string(detail);
	if (demo.frames[0].dispatch.operations[0].kind != INPUT_DEMO_INPUT_CONTROLS ||
	    demo.frames[0].dispatch.operations[3].automap_phase != INPUT_DEMO_AUTOMAP_DEPTHS ||
	    demo.input_tail.dispatch.operations[0].automap_phase != INPUT_DEMO_AUTOMAP_BEGIN)
		return report_failure("automap operation round trip lost order or phase");
	// The same alias resolves against current residual controls for either controls operation
	demo.frames[0].dispatch.operations[0].state_is_residual = 1;
	demo.input_tail.dispatch.operations[1].state_is_residual = 1;
	if (!input_demo_file_to_text(demo, &text, &detail) || !write_text_file(path, text.c_str()) ||
	    !input_demo_replay_load(path, error, sizeof(error))) return report_failure_string(detail + error);
	input_demo_input_operation op = {};
	if (!input_demo_replay_get_input_operation(0, 0, &op, error, sizeof(error)) ||
	    op.kind != INPUT_DEMO_INPUT_CONTROLS || op.state.fire_secondary_state != 3 || op.pulse.fire_secondary_count != 4 ||
	    !input_demo_replay_get_input_operation(1, 1, &op, error, sizeof(error)) ||
	    op.kind != INPUT_DEMO_INPUT_CONTROLS || op.state.fire_secondary_state || op.pulse.fire_secondary_count || op.frame_time != 65536)
		return report_failure("controls alias used the wrong frame or inherited pulse");
	if (settings.game == INPUT_DEMO_GAME_D2 &&
	    (!input_demo_replay_get_input_operation(0, 6, &op, error, sizeof(error)) ||
	     op.command.kind != INPUT_DEMO_REPLAY_DIRECT_COMMAND_DELETE_MARKER || op.command.value0 != 8 || op.command.value1 != 12345))
		return report_failure("delete marker command did not retain resolved slot");
	input_demo_replay_unload();
	remove(path);
	remove((std::string(path) + INPUT_DEMO_RNG_TRACE_SUFFIX).c_str());
	std::vector<json> rows;
	std::istringstream lines(text);
	std::string line;
	while (std::getline(lines, line)) rows.push_back(json::parse(line));
	const auto parses = [&](const std::vector<json> &candidate) {
		std::string encoded;
		for (const auto &row : candidate) encoded += row.dump() + "\n";
		input_demo_file parsed;
		return input_demo_file_parse_text(encoded, &parsed, &detail);
	};
	for (const auto &phase : json::array({ "", "open", "BEGIN", "=", 1, true, nullptr, json::object() })) {
		auto bad = rows;
		bad[1]["dispatch"]["ops"][1]["phase"] = phase;
		if (parses(bad)) return report_failure("unsupported automap phase was accepted");
	}
	for (const auto &sequence : std::vector<std::vector<std::string>>{
	         { "depths" }, { "active" }, { "close" }, { "begin" }, { "begin", "active", "close" }, { "begin", "begin", "close" }, { "begin", "depths", "depths", "close" }, { "begin", "depths", "active", "active", "close" }, { "begin", "close", "close" } }) {
		auto bad = rows;
		bad[1]["dispatch"]["ops"] = json::array();
		for (const auto &phase : sequence) bad[1]["dispatch"]["ops"].push_back({ { "k", "automap" }, { "ft", 1 }, { "phase", phase } });
		if (parses(bad)) return report_failure("invalid automap transition or open simulation frame was accepted");
	}
	for (const auto &sequence : std::vector<std::vector<std::string>>{ { "begin", "close" }, { "begin", "depths", "close" } }) {
		auto valid = rows;
		valid[1]["dispatch"]["ops"] = json::array();
		for (const auto &phase : sequence) valid[1]["dispatch"]["ops"].push_back({ { "k", "automap" }, { "ft", 1 }, { "phase", phase } });
		if (!parses(valid)) return report_failure_string("partial automap entry rejected: " + detail);
	}
	for (const char *key : { "s", "p", "event" }) {
		auto bad = rows;
		bad[1]["dispatch"]["ops"][1][key] = json::object();
		if (parses(bad)) return report_failure("automap accepted an unrelated payload");
	}
	for (const auto &invalid : json::array({ false, 0, "previous", json::array() })) {
		auto bad = rows;
		bad[1]["dispatch"]["ops"][0]["s"] = invalid;
		if (parses(bad)) return report_failure("controls operation accepted invalid state alias");
	}
	for (int marker : { -1, 0, 8, 9, INT32_MAX }) {
		const json event = { { "kind", "direct_command" }, { "command", "delete_marker" }, { "player_marker_num", marker }, { "expected_signature", INT32_MAX } };
		input_demo_replay_direct_command_event command;
		if (input_demo_dispatch_command_from_json(event.dump(), settings.game, &command, &detail) !=
		    (settings.game == INPUT_DEMO_GAME_D2 && marker >= 0 && marker < 9))
			return report_failure("delete marker command accepted wrong game or slot");
	}
	for (const auto &signature : json::array({ -1, UINT64_MAX, false, 1.0, "1", nullptr })) {
		const json event = { { "kind", "direct_command" }, { "command", "delete_marker" }, { "player_marker_num", 0 }, { "expected_signature", signature } };
		input_demo_replay_direct_command_event command;
		if (input_demo_dispatch_command_from_json(event.dump(), INPUT_DEMO_GAME_D2, &command, &detail))
			return report_failure("delete marker accepted invalid expected signature");
	}
	for (int phase : { 0, -1, 5, INT32_MAX }) {
		if (!input_demo_recorder_start(&settings, error, sizeof(error)) ||
		    !input_demo_recorder_begin_input_batch(100, 2, 0, error, sizeof(error))) return report_failure_string(error);
		if (input_demo_recorder_stage_input_automap(1, phase, error, sizeof(error)) ||
		    input_demo_recorder_finish_input_tail(&state, &pulse, 100, 2, error, sizeof(error)))
			return report_failure("invalid automap phase did not poison capture");
		input_demo_recorder_cancel();
	}
	if (!input_demo_recorder_start(&settings, error, sizeof(error)) ||
	    !input_demo_recorder_begin_input_batch(100, 2, 0, error, sizeof(error)) ||
	    !input_demo_recorder_stage_input_automap(1, INPUT_DEMO_AUTOMAP_BEGIN, error, sizeof(error))) return report_failure_string(error);
	if (input_demo_recorder_capture_frame(2000, &state, &pulse, 100, 1, 2, nullptr, nullptr, error, sizeof(error)))
		return report_failure("recorder accepted simulation while automap is open");
	input_demo_recorder_cancel();
	return 0;
}

static int expect_ordered_input_diagnostics(void)
{
	const char *path = "test_input_demo_ordered_events.dximdemo";
	input_demo_recorder_settings settings;
	input_demo_recorder_settings_clear(&settings);
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 1;
	settings.rng_mode = input_demo_test_rng_mode();
	settings.record_per_frame_state = 1;
	input_demo_control_state state = {};
	input_demo_control_pulse pulse = {};
	char error[256] = "";
	const auto event = [&](const char *kind) {
		const std::string text = std::string("{\"kind\":\"") + kind + "\"}";
		return input_demo_recorder_append_frame_event_json(text.c_str(), error, sizeof(error));
	};
	if (!input_demo_recorder_start(&settings, error, sizeof(error)) ||
	    input_demo_recorder_diagnostic_frame_index() != 0 ||
	    !input_demo_recorder_begin_input_batch(100, 1, 3276, error, sizeof(error)) ||
	    !event("pre0") ||
	    !input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 1, 1, nullptr, nullptr, error, sizeof(error)) ||
	    input_demo_recorder_diagnostic_frame_index() != 0 || !event("sim0") ||
	    !input_demo_recorder_begin_input_batch(101, 2, 3276, error, sizeof(error)) ||
	    input_demo_recorder_diagnostic_frame_index() != 1 || !event("pre1") ||
	    !input_demo_recorder_capture_frame(3276, &state, &pulse, 101, 1, 2, nullptr, nullptr, error, sizeof(error)) ||
	    input_demo_recorder_diagnostic_frame_index() != 1 || !event("sim1") ||
	    !input_demo_recorder_begin_input_batch(102, 3, 3276, error, sizeof(error)) ||
	    input_demo_recorder_diagnostic_frame_index() != 2 || !event("tail") ||
	    !input_demo_recorder_finish_input_tail(&state, &pulse, 102, 3, error, sizeof(error)) ||
	    input_demo_recorder_diagnostic_frame_index() != 2 ||
	    !input_demo_recorder_flush(path, error, sizeof(error)))
		return report_failure_string(std::string("ordered input diagnostics: ") + error);
	input_demo_file parsed;
	std::string detail, text;
	if (!input_demo_file_read(path, &parsed, &detail) || !read_text_file(path, &text)) return report_failure_string(detail);
	remove(path);
	remove((std::string(path) + INPUT_DEMO_RNG_TRACE_SUFFIX).c_str());
	const std::vector<std::string> first = { "{\"kind\":\"pre0\"}", "{\"kind\":\"sim0\"}" };
	const std::vector<std::string> second = { "{\"kind\":\"pre1\"}", "{\"kind\":\"sim1\"}" };
	const std::vector<std::string> tail = { "{\"kind\":\"tail\"}" };
	if (parsed.frames[0].events != first || parsed.frames[1].events != second || parsed.input_tail.events != tail)
		return report_failure("ordered input diagnostics were dropped or attributed to the wrong frame");
	if (!input_demo_file_to_text(parsed, &text, &detail) || !input_demo_file_parse_text(text, &parsed, &detail) ||
	    parsed.input_tail.events != tail) return report_failure("tail diagnostic round trip failed");
	parsed.input_tail.events.push_back("{\"kind\":\"direct_command\",\"command\":\"death_abort\"}");
	if (input_demo_file_to_text(parsed, &text, &detail)) return report_failure("tail telemetry accepted an executable command");
	// Both pending input diagnostics and later simulation appends remain bounded
	const std::string oversized = "{\"kind\":\"diagnostic\",\"text\":\"" + std::string(INPUT_DEMO_RECORD_MAX_BYTES, 'x') + "\"}";
	for (int pending = 0; pending != 2; ++pending) {
		if (!input_demo_recorder_start(&settings, error, sizeof(error)) ||
		    !input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 1, 1, nullptr, nullptr, error, sizeof(error))) return 1;
		if (pending && !input_demo_recorder_begin_input_batch(100, 1, 3276, error, sizeof(error))) return 1;
		if (input_demo_recorder_append_frame_event_json(oversized.c_str(), error, sizeof(error)) ||
		    input_demo_recorder_finish_input_tail(&state, &pulse, 100, 1, error, sizeof(error)))
			return report_failure("diagnostic overflow did not fail closed");
		input_demo_recorder_cancel();
	}
	return 0;
}

static int expect_ordered_input_codec(void)
{
	using json = nlohmann::ordered_json;
	const char *path = "test_input_demo_ordered.dximdemo";
	input_demo_recorder_settings settings;
	input_demo_recorder_settings_clear(&settings);
	if (!settings.ordered_input) return report_failure("new recordings must default to ordered input");
	settings.game = input_demo_test_game_id();
	settings.mission = input_demo_test_game_name();
	settings.level = 1;
	settings.rng_mode = input_demo_test_rng_mode();
	input_demo_control_state state = {};
	input_demo_control_pulse pulse = {};
	char error[256] = "";
	const auto check = [&](bool success) {
		if (!success) fprintf(stderr, "ordered input: %s\n", error);
		return success;
	};
	if (!check(input_demo_recorder_start(&settings, error, sizeof(error))) ||
	    !check(input_demo_recorder_begin_input_batch(100, 2, 0, error, sizeof(error))) ||
	    !check(input_demo_recorder_stage_input_reset(0, error, sizeof(error)))) return 1;
	state.fire_secondary_state = 1;
	pulse.fire_secondary_count = 1;
	pulse.fire_primary_count = 2;
	if (!check(input_demo_recorder_stage_input_sample(3276, &state, &pulse, error, sizeof(error))) ||
	    !check(input_demo_recorder_begin_input_batch(999, 99, 4096, error, sizeof(error))) ||
	    !check(input_demo_recorder_stage_direct_command_change_difficulty(3, error, sizeof(error)))) return 1;
	state.fire_secondary_state = 0;
	pulse.fire_secondary_count = 0;
	if (!check(input_demo_recorder_stage_input_sample(3276, &state, &pulse, error, sizeof(error)))) return 1;
	input_demo_control_pulse staged = {};
	staged.fire_primary_count = 99;
	input_demo_recorder_stage_pulse(&staged);
	if (!check(input_demo_recorder_capture_frame(2000, &state, &pulse, 101, 1, 3, nullptr, nullptr, error, sizeof(error))) ||
	    !check(input_demo_recorder_capture_frame(4000, &state, &pulse, 102, 1, 4, nullptr, nullptr, error, sizeof(error)))) return 1;
	if (input_demo_recorder_flush(path, error, sizeof(error))) return report_failure("ordered flush accepted missing tail");
	if (!check(input_demo_recorder_begin_input_batch(102, 4, 4000, error, sizeof(error))) ||
	    !check(input_demo_recorder_stage_input_reset(4000, error, sizeof(error))) ||
	    !check(input_demo_recorder_stage_direct_command_select_weapon_exact(1, 1, error, sizeof(error))) ||
	    !check(input_demo_recorder_stage_input_clear(1638, error, sizeof(error)))) return 1;
	state = {};
	pulse = {};
	if (!check(input_demo_recorder_finish_input_tail(&state, &pulse, 103, 5, error, sizeof(error))) ||
	    !check(input_demo_recorder_flush(path, error, sizeof(error)))) return 1;
	input_demo_file parsed;
	std::string text, detail;
	if (!input_demo_file_read(path, &parsed, &detail) || !read_text_file(path, &text)) return report_failure_string(detail);
	if (parsed.metadata.version != 5 || !parsed.has_input_tail || parsed.frames.size() != 2 ||
	    parsed.frames[0].dispatch.operations.size() != 4 || parsed.frames[0].dispatch.pre_rng_state != 100 ||
	    parsed.frames[0].dispatch.pre_rng_count != 2 || parsed.frames[0].input.pulse.fire_primary_count != 2 ||
	    parsed.frames[1].dispatch.operations.size() || !parsed.frames[1].dispatch.present ||
	    parsed.input_tail.dispatch.operations.size() != 3)
		return report_failure("ordered batch round trip or residual pulse mismatch");
	if (!check(input_demo_replay_load(path, error, sizeof(error))) || input_demo_replay_format_version() != 5) return 1;
	input_demo_input_batch_info info = {};
	input_demo_input_operation operation = {};
	if (!check(input_demo_replay_get_input_batch_info(0, &info, error, sizeof(error))) ||
	    info.operation_count != 4 || info.pre_rng_state != 100 || info.post_rng_count != 3 || info.is_tail ||
	    info.residual_pulse.fire_primary_count != 2 || info.residual_pulse.fire_secondary_count ||
	    !check(input_demo_replay_get_input_operation(0, 1, &operation, error, sizeof(error))) ||
	    operation.kind != INPUT_DEMO_INPUT_SAMPLE || operation.frame_time != 3276 || operation.pulse.fire_secondary_count != 1 ||
	    !check(input_demo_replay_get_input_operation(0, 2, &operation, error, sizeof(error))) ||
	    operation.kind != INPUT_DEMO_INPUT_COMMAND || operation.frame_time != 4096 || operation.command.value0 != 3 ||
	    !check(input_demo_replay_get_input_batch_info(2, &info, error, sizeof(error))) ||
	    !info.is_tail || info.operation_count != 3 || info.pre_rng_state != 102 || info.post_rng_state != 103 ||
	    !check(input_demo_replay_get_input_operation(2, 2, &operation, error, sizeof(error))) ||
	    operation.kind != INPUT_DEMO_INPUT_CLEAR || operation.frame_time != 1638 ||
	    input_demo_replay_get_input_batch_info(3, &info, error, sizeof(error)) ||
	    input_demo_replay_get_input_operation(0, 4, &operation, error, sizeof(error)))
		return report_failure("ordered replay accessor boundary mismatch");
	input_demo_replay_unload();
	remove(path);
	remove((std::string(path) + INPUT_DEMO_RNG_TRACE_SUFFIX).c_str());
	std::string duplicate = text;
	const auto duplicate_at = duplicate.find("\"k\":\"reset\",\"ft\":0");
	if (duplicate_at == std::string::npos) return report_failure("missing reset operation for duplicate-key test");
	duplicate.replace(duplicate_at, strlen("\"k\":\"reset\",\"ft\":0"), "\"k\":\"reset\",\"ft\":0,\"ft\":0");
	if (input_demo_file_parse_text(duplicate, &parsed, &detail) || detail.find("duplicate key") == std::string::npos)
		return report_failure("duplicate operation key was not rejected");
	std::vector<json> records;
	std::istringstream lines(text);
	for (std::string line; std::getline(lines, line);)
		if (!line.empty()) records.push_back(json::parse(line));
	const auto rejects = [&](const std::vector<json> &changed) {
		std::string candidate;
		for (const auto &row : changed) candidate += row.dump() + "\n";
		input_demo_file output;
		std::string reason;
		if (input_demo_file_parse_text(candidate, &output, &reason)) {
			fprintf(stderr, "accepted invalid ordered input: %s\n", candidate.c_str());
			return false;
		}
		return true;
	};
	// A tail cannot be the initial RNG boundary: every recording has a simulation frame
	for (const auto &invalid_count : json::array({ 0, -1, false, 0.0, "0", nullptr, 4294967296ULL })) {
		auto tail_only = std::vector<json>{ records.front(), records[records.size() - 2], records.back() };
		tail_only.front()["frame_count"] = invalid_count;
		tail_only[1]["f"] = 0;
		tail_only.back()["result"]["frame_count"] = 0;
		if (!rejects(tail_only)) return 1;
		if (invalid_count.is_number_integer() && invalid_count == 0) {
			std::string candidate;
			for (const auto &row : tail_only) candidate += row.dump() + "\n";
			if (!write_text_file(path, candidate.c_str()) || input_demo_replay_load(path, error, sizeof(error)) ||
			    input_demo_replay_is_loaded() || !strstr(error, "frame_count must be positive"))
				return report_failure("zero-frame replay was not explicitly rejected before runtime");
			remove(path);
		}
	}
	input_demo_file empty_demo;
	if (!input_demo_file_parse_text(text, &empty_demo, &detail)) return report_failure_string(detail);
	empty_demo.metadata.frame_count = 0;
	empty_demo.frames.clear();
	empty_demo.result.frame_count = 0;
	empty_demo.input_tail.input.frame = empty_demo.input_tail.rng.frame = 0;
	std::string empty_text;
	if (input_demo_file_to_text(empty_demo, &empty_text, &detail) || detail.find("frame_count must be positive") == std::string::npos)
		return report_failure("writer accepted a zero-frame input-tail recording");
	auto bad = records;
	bad[0].erase("frame_count");
	if (!rejects(bad)) return 1;
	bad = records;
	bad[0]["version"] = 4;
	if (!rejects(bad)) return 1;
	bad = records;
	bad.erase(bad.end() - 2);
	if (!rejects(bad)) return 1;
	bad = records;
	bad.insert(bad.end() - 1, bad[bad.size() - 2]);
	if (!rejects(bad)) return 1;
	bad = records;
	std::swap(bad[1], bad[bad.size() - 2]);
	if (!rejects(bad)) return 1;
	bad = records;
	bad[1].erase("dispatch");
	if (!rejects(bad)) return 1;
	bad = records;
	bad[1]["dispatch"]["ops"][0]["k"] = "unknown";
	if (!rejects(bad)) return 1;
	bad = records;
	bad[1]["dispatch"]["ops"][0]["ft"] = -1;
	if (!rejects(bad)) return 1;
	bad = records;
	bad[1]["dispatch"]["ops"][0]["ft"] = INPUT_DEMO_FRAME_TIME_MAX + 1;
	if (!rejects(bad)) return 1;
	bad = records;
	bad[1]["dispatch"]["ops"][0]["extra"] = true;
	if (!rejects(bad)) return 1;
	bad = records;
	bad[1]["dispatch"]["ops"][1].erase("s");
	if (!rejects(bad)) return 1;
	bad = records;
	bad[1]["dispatch"]["ops"][1]["p"]["f2"] = 256;
	if (!rejects(bad)) return 1;
	bad = records;
	bad[1]["dispatch"]["pre_rng"].erase("c");
	if (!rejects(bad)) return 1;
	bad = records;
	bad[1]["rng"].erase("c");
	if (!rejects(bad)) return 1;
	bad = records;
	bad[1]["events"] = json::array({ { { "kind", "direct_command" }, { "command", "death_abort" } } });
	if (!rejects(bad)) return 1;
	bad = records;
	bad[1]["dispatch"]["ops"][2]["event"]["difficulty"] = 2147483648ULL;
	if (!rejects(bad)) return 1;
	bad = records;
	bad[1]["dispatch"]["ops"] = json::array();
	for (unsigned i = 0; i <= INPUT_DEMO_INPUT_OPERATION_MAX; ++i)
		bad[1]["dispatch"]["ops"].push_back({ { "k", "reset" }, { "ft", 1 } });
	if (!rejects(bad)) return 1;
	bad = records;
	bad[1]["dispatch"]["ops"][0]["padding"] = std::string(INPUT_DEMO_INPUT_BATCH_MAX_BYTES, 'x');
	if (!rejects(bad)) return 1;
	bad = records;
	bad[bad.size() - 2]["ft"] = 1;
	if (!rejects(bad)) return 1;
	for (const auto &invalid_clear : json::array({ { { "k", "clear" }, { "ft", false } },
	                                               { { "k", "clear" }, { "ft", -1 } },
	                                               { { "k", "clear" }, { "ft", INPUT_DEMO_FRAME_TIME_MAX + 1 } },
	                                               { { "k", "clear" }, { "ft", 1 }, { "s", json::object() } },
	                                               { { "k", "clear" }, { "ft", 1 }, { "p", json::object() } },
	                                               { { "k", "clear" } } })) {
		bad = records;
		bad[bad.size() - 2]["dispatch"]["ops"][2] = invalid_clear;
		if (!rejects(bad)) return 1;
	}
	if (settings.game == INPUT_DEMO_GAME_D1) {
		bad = records;
		bad[1]["dispatch"]["ops"][1]["s"]["ab"] = 1;
		if (!rejects(bad)) return 1;
	}
	// Aliases name the decoded current frame, never the previous frame or accumulated pulses
	auto aliases = records;
	aliases[2]["input"]["s"] = { { "f2s", 7 } };
	aliases[2]["dispatch"]["pre_rng"] = "=";
	aliases[2]["dispatch"]["ops"] = json::array({ { { "k", "sample" }, { "ft", 1234 }, { "s", "=" } } });
	aliases[aliases.size() - 2]["input"]["s"] = { { "f2s", 5 } };
	aliases[aliases.size() - 2]["dispatch"]["pre_rng"] = "=";
	aliases[aliases.size() - 2]["dispatch"]["ops"].push_back({ { "k", "sample" }, { "ft", 4000 }, { "s", "=" } });
	std::string alias_text;
	for (const auto &row : aliases) alias_text += row.dump() + "\n";
	if (!input_demo_file_parse_text(alias_text, &parsed, &detail) ||
	    !input_demo_file_to_text(parsed, &alias_text, &detail) ||
	    !input_demo_file_parse_text(alias_text, &parsed, &detail) ||
	    !parsed.frames[1].dispatch.pre_rng_is_post || !parsed.frames[1].dispatch.operations[0].state_is_residual ||
	    !input_demo_file_write(path, parsed, &detail) || !check(input_demo_replay_load(path, error, sizeof(error))))
		return report_failure_string("ordered alias round trip: " + detail);
	if (!check(input_demo_replay_get_input_batch_info(1, &info, error, sizeof(error))) ||
	    info.pre_rng_state != 102 || info.pre_rng_count != 4 ||
	    !check(input_demo_replay_get_input_operation(1, 0, &operation, error, sizeof(error))) ||
	    operation.state.fire_secondary_state != 7 || operation.pulse.fire_primary_count || operation.pulse.fire_secondary_count ||
	    !check(input_demo_replay_get_input_operation(2, 3, &operation, error, sizeof(error))) ||
	    operation.state.fire_secondary_state != 5 ||
	    !check(input_demo_replay_get_input_batch_info(2, &info, error, sizeof(error))) ||
	    info.pre_rng_state != 103 || info.pre_rng_count != 5)
		return report_failure("ordered alias resolved against wrong boundary");
	input_demo_replay_unload();
	remove(path);
	bad = aliases;
	bad[2]["dispatch"]["ops"][0]["s"] = "previous";
	if (!rejects(bad)) return 1;
	bad = aliases;
	bad[2]["dispatch"]["ops"][0]["s"] = nullptr;
	if (!rejects(bad)) return 1;
	bad = aliases;
	bad[2]["dispatch"]["ops"][0]["p"] = "=";
	if (!rejects(bad)) return 1;
	bad = aliases;
	bad[2]["dispatch"]["pre_rng"] = "previous";
	if (!rejects(bad)) return 1;
	bad = aliases;
	bad[2]["dispatch"]["pre_rng"] = 0;
	if (!rejects(bad)) return 1;

	// Finalizing an empty or effectful tail must not make a no-frame capture publishable
	for (int effectful = 0; effectful != 2; ++effectful) {
		input_demo_control_state tail_state = {};
		input_demo_control_pulse tail_pulse = {};
		tail_state.fire_secondary_state = tail_pulse.fire_secondary_count = effectful;
		if (!check(input_demo_recorder_start(&settings, error, sizeof(error))) ||
		    !check(input_demo_recorder_begin_input_batch(1, 0, 1, error, sizeof(error)))) return 1;
		if (effectful && !check(input_demo_recorder_stage_input_sample(1, &tail_state, &tail_pulse, error, sizeof(error)))) return 1;
		if (!check(input_demo_recorder_finish_input_tail(&tail_state, &tail_pulse, 1, 0, error, sizeof(error))) ||
		    input_demo_recorder_frame_count() || input_demo_recorder_flush(path, error, sizeof(error)) ||
		    !strstr(error, "captured no frames") || read_text_file(path, &empty_text))
			return report_failure("recorder published a zero-frame input-tail recording");
		input_demo_recorder_cancel();
	}

	// Invalid native text must fail through the C API, never throw across engine C frames
	if (!check(input_demo_recorder_start(&settings, error, sizeof(error))) ||
	    !check(input_demo_recorder_begin_input_batch(1, 0, 1, error, sizeof(error)))) return 1;
	if (input_demo_recorder_stage_direct_command_drop_marker(0, "\xff", error, sizeof(error)) ||
	    input_demo_recorder_finish_input_tail(&state, &pulse, 1, 0, error, sizeof(error)))
		return report_failure("invalid native marker text was not rejected");
	input_demo_recorder_cancel();

	// Exceeding a batch bound is sticky: callers cannot ignore failure and flush a truncated recording
	if (!check(input_demo_recorder_start(&settings, error, sizeof(error))) ||
	    !check(input_demo_recorder_begin_input_batch(1, 0, 1, error, sizeof(error)))) return 1;
	for (unsigned i = 0; i < INPUT_DEMO_INPUT_OPERATION_MAX; ++i)
		if (!check(input_demo_recorder_stage_input_reset(1, error, sizeof(error)))) return 1;
	if (input_demo_recorder_stage_input_reset(1, error, sizeof(error)) ||
	    input_demo_recorder_capture_frame(1, &state, &pulse, 1, 1, 0, nullptr, nullptr, error, sizeof(error)) ||
	    input_demo_recorder_finish_input_tail(&state, &pulse, 1, 0, error, sizeof(error)))
		return report_failure("overflow did not poison ordered recording");
	input_demo_recorder_cancel();
	// Rewind discards pending operations and invalidates a formerly finalized tail
	if (!check(input_demo_recorder_start(&settings, error, sizeof(error))) ||
	    !check(input_demo_recorder_capture_frame(1, &state, &pulse, 1, 1, 0, nullptr, nullptr, error, sizeof(error))) ||
	    !check(input_demo_recorder_begin_input_batch(1, 0, 1, error, sizeof(error))) ||
	    !check(input_demo_recorder_stage_input_reset(1, error, sizeof(error))) ||
	    !check(input_demo_recorder_finish_input_tail(&state, &pulse, 1, 0, error, sizeof(error))) ||
	    !input_demo_recorder_truncate(1)) return 1;
	if (input_demo_recorder_flush(path, error, sizeof(error))) return report_failure("rewind retained a stale input tail");
	if (!check(input_demo_recorder_finish_input_tail(&state, &pulse, 1, 0, error, sizeof(error))) ||
	    !check(input_demo_recorder_flush(path, error, sizeof(error)))) return 1;
	if (!input_demo_file_read(path, &parsed, &detail) || !parsed.input_tail.dispatch.operations.empty())
		return report_failure("rewind retained discarded pending operations");
	remove(path);
	remove((std::string(path) + INPUT_DEMO_RNG_TRACE_SUFFIX).c_str());
	return 0;
}

int main(void)
{
	if (expect_ordered_input_codec()) return 1;
	if (expect_ordered_automap_codec()) return 1;
	if (expect_ordered_rear_view_codec()) return 1;
	if (expect_pending_input_operation_count()) return 1;
	if (expect_cockpit_projection_codec()) return 1;
	if (expect_ordered_input_diagnostics()) return 1;
	if (expect_ordered_command_domains()) return 1;
	if (expect_all_weapons_command_domains()) return 1;
	if (expect_ordered_all_weapons_recording()) return 1;
	if (expect_ordered_rewind_cancellation()) return 1;
	if (expect_record_and_flush())
		return 1;
	if (expect_record_and_flush_checkpoint())
		return 1;
	if (expect_stage_consumed_pulse())
		return 1;
	if (expect_truncate_recording())
		return 1;
	if (expect_write_state_trace())
		return 1;
	if (expect_record_and_flush_events())
		return 1;
	if (expect_record_and_flush_diag())
		return 1;
	if (expect_long_recording())
		return 1;
	puts("PASS");
	return 0;
}
