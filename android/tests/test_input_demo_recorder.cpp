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
#include "input_demo_result.h"

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
	const std::string demo_path = std::string(dir) + "/demo.json5";
	const std::string input_path = std::string(dir) + "/inputs.p0.jsonl";
	const std::string rng_path = std::string(dir) + "/rng.p0.jsonl";
	const std::string result_path = std::string(dir) + "/result.json";
	input_demo_recorder_settings settings;
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	input_demo_result result;
	char error[256] = "";
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
	if (!input_demo_recorder_start(&settings, error, sizeof(error))) {
		remove_test_dir(dir);
		return report_failure_string(std::string("recorder start failed: ") + error);
	}
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	state.forward_thrust_time = 44;
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0, error, sizeof(error)))
		return report_failure_string(std::string("capture frame 0 failed: ") + error);
	input_demo_control_pulse_clear(&pulse);
	pulse.fire_primary_count = 1;
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0, error, sizeof(error)))
		return report_failure_string(std::string("capture frame 1 failed: ") + error);
	input_demo_control_state_clear(&state);
	input_demo_control_pulse_clear(&pulse);
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 102, 0, 0, error, sizeof(error)))
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
	if (!input_demo_recorder_flush_with_result(dir, &result, error, sizeof(error)))
		return report_failure_string(std::string("recorder flush failed: ") + error);
	if (input_demo_recorder_is_active())
		return report_failure("recorder should be inactive after flush");
	if (!read_text_file(input_path.c_str(), &text))
		return report_failure("could not read recorder control stream");
	expected = std::string("{\"f\":0,\"ft\":3276,\"s\":{\"f\":44}}\n") +
		"{\"f\":1,\"p\":{\"f1\":1}}\n" +
		"{\"f\":2,\"s\":{\"f\":0}}\n";
	if (text != expected)
		return report_failure_string(std::string("unexpected recorder control stream: ") + text);
	if (!read_text_file(rng_path.c_str(), &text))
		return report_failure("could not read recorder rng stream");
	expected = std::string("{\"f\":0,\"n\":2,\"s\":100}\n") +
		"{\"f\":2,\"s\":102}\n";
	if (text != expected)
		return report_failure_string(std::string("unexpected recorder rng stream: ") + text);
	if (!read_text_file(demo_path.c_str(), &text))
		return report_failure("could not read recorder metadata");
	expected = std::string("{\n") +
		"    \"version\": 1,\n" +
		"    \"game\": \"" + input_demo_test_game_name() + "\",\n" +
		"    \"mission\": \"" + input_demo_test_game_name() + "\",\n" +
		"    \"level\": 1,\n" +
		"    \"difficulty\": 2,\n" +
		"    \"start_mode\": \"new_level\",\n" +
		"    \"rng_mode\": \"" + input_demo_test_rng_mode() + "\",\n" +
		"    \"frame_count\": 3,\n" +
		"    \"streams\": [\n" +
		"        {\n" +
		"            \"player\": 0,\n" +
		"            \"input\": \"inputs.p0.jsonl\",\n" +
		"            \"rng\": \"rng.p0.jsonl\"\n" +
		"        }\n" +
		"    ],\n" +
		"    \"result\": \"result.json\"\n" +
		"}\n";
	if (text != expected)
		return report_failure_string(std::string("unexpected recorder metadata: ") + text);
	if (!read_text_file(result_path.c_str(), &text))
		return report_failure("could not read recorder result file");
	expected = std::string("{\n") +
		"  \"v\": 1,\n" +
		"  \"g\": \"" + input_demo_test_game_name() + "\",\n" +
		"  \"m\": \"" + input_demo_test_game_name() + "\",\n" +
		"  \"l\": 1,\n" +
		"  \"d\": 2,\n" +
		"  \"fr\": 3,\n" +
		"  \"gt\": 120,\n" +
		"  \"p0\": {\n" +
		"    \"e\": 67,\n" +
		"    \"s\": 42,\n" +
		"    \"sc\": 12500,\n" +
		"    \"li\": 3,\n" +
		"    \"ll\": 1,\n" +
		"    \"sw\": 1,\n" +
		"    \"pa\": {\n" +
		"      \"1\": 200\n" +
		"    },\n" +
		"    \"sa\": {\n" +
		"      \"0\": 4\n" +
		"    }\n" +
		"  },\n" +
		"  \"pos\": {\n" +
		"    \"sg\": 142,\n" +
		"    \"x\": 12345678,\n" +
		"    \"y\": -8765432,\n" +
		"    \"z\": 3456789,\n" +
		"    \"fx\": 65536,\n" +
		"    \"fy\": 0,\n" +
		"    \"fz\": 0\n" +
		"  },\n" +
		"  \"lv\": {\n" +
		"    \"ra\": 23,\n" +
		"    \"rk\": 8,\n" +
		"    \"hr\": 2,\n" +
		"    \"pr\": 15,\n" +
		"    \"cc\": true\n" +
		"  }\n" +
		"}\n";
	if (text != expected)
		return report_failure_string(std::string("unexpected recorder result file: ") + text);
	remove(demo_path.c_str());
	remove(input_path.c_str());
	remove(rng_path.c_str());
	remove(result_path.c_str());
	remove_test_dir(dir);
	return 0;
}

int main(void)
{
	if (expect_record_and_flush())
		return 1;
	puts("PASS");
	return 0;
}