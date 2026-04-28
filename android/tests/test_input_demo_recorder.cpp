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
	const std::string demo_path = std::string(dir) + "/recorded.dximdemo";
	input_demo_recorder_settings settings;
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	input_demo_result result;
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
	if (!input_demo_recorder_flush_with_result(demo_path.c_str(), &result, error, sizeof(error)))
		return report_failure_string(std::string("recorder flush failed: ") + error);
	if (input_demo_recorder_is_active())
		return report_failure("recorder should be inactive after flush");
	if (!read_text_file(demo_path.c_str(), &text))
		return report_failure("could not read recorder demo file");
	expected = std::string("{\"type\":\"header\",\"version\":1,\"game\":\"") + input_demo_test_game_name() +
		"\",\"mission\":\"" + input_demo_test_game_name() +
		"\",\"level\":1,\"difficulty\":2,\"start_mode\":\"new_level\",\"rng_mode\":\"" + input_demo_test_rng_mode() +
		"\",\"frame_count\":3}\n" +
		"{\"type\":\"frame\",\"f\":0,\"ft\":3276,\"input\":{\"s\":{\"f\":44}},\"rng\":{\"s\":100}}\n" +
		"{\"type\":\"frame\",\"f\":1,\"input\":{\"p\":{\"f1\":1}},\"rng\":{\"s\":100}}\n" +
		"{\"type\":\"frame\",\"f\":2,\"input\":{\"s\":{\"f\":0}},\"rng\":{\"s\":102}}\n" +
		"{\"type\":\"result\",\"result\":{\"v\":1,\"g\":\"" + input_demo_test_game_name() +
		"\",\"m\":\"" + input_demo_test_game_name() +
		"\",\"l\":1,\"d\":2,\"fr\":3,\"gt\":120,\"p0\":{\"e\":67,\"s\":42,\"sc\":12500,\"li\":3,\"ll\":1,\"sw\":1,\"pa\":{\"1\":200},\"sa\":{\"0\":4}},\"pos\":{\"sg\":142,\"x\":12345678,\"y\":-8765432,\"z\":3456789,\"fx\":65536,\"fy\":0,\"fz\":0},\"lv\":{\"ra\":23,\"rk\":8,\"hr\":2,\"pr\":15,\"cc\":true}}}\n";
	if (text != expected)
		return report_failure_string(std::string("unexpected recorder demo file: ") + text);
	if (!input_demo_file_read(demo_path.c_str(), &parsed, &read_error))
		return report_failure_string(std::string("recorder demo read failed: ") + read_error);
	if (parsed.frames.size() != 3 || parsed.frames[2].rng.state != 102 || parsed.result.player0.score != 12500)
		return report_failure("recorder demo round trip mismatch");
	remove(demo_path.c_str());
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
	if (!input_demo_recorder_capture_frame(3276, &state, &pulse, 100, 0, 0, error, sizeof(error))) {
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
	expected = std::string("{\"type\":\"header\",\"version\":1,\"game\":\"") + input_demo_test_game_name() +
		"\",\"mission\":\"" + input_demo_test_game_name() +
		"\",\"level\":1,\"difficulty\":2,\"start_mode\":\"save_checkpoint\",\"rng_mode\":\"" + input_demo_test_rng_mode() +
		"\",\"frame_count\":1,\"start_save\":\"inputdemo_start.dgss\"}\n" +
		"{\"type\":\"checkpoint\",\"format\":\"dgss\",\"encoding\":\"base64\",\"size\":8,\"sha256\":\"077c5f8a7bd52bba7beb0ea8153f1005401b5ba52b797e04952bf14e542fd3b5\",\"save_name\":\"inputdemo_start.dgss\",\"start_gt\":124125,\"data\":\"REdTUxgAAAA=\"}\n" +
		"{\"type\":\"frame\",\"f\":0,\"ft\":3276,\"input\":{\"s\":{\"f\":44}},\"rng\":{\"s\":100}}\n" +
		"{\"type\":\"result\",\"result\":{\"v\":1,\"g\":\"" + input_demo_test_game_name() +
		"\",\"m\":\"" + input_demo_test_game_name() + "\",\"l\":1,\"d\":2,\"fr\":1}}\n";
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
		parsed.checkpoint.sha256 != "077c5f8a7bd52bba7beb0ea8153f1005401b5ba52b797e04952bf14e542fd3b5" ||
		parsed.checkpoint.data != "REdTUxgAAAA=" || parsed.checkpoint.has_next_laser_fire_delta ||
		parsed.checkpoint.has_next_missile_fire_delta || parsed.checkpoint.has_last_laser_fired_delta ||
		parsed.checkpoint.has_auto_fire_fusion_delta)
		return report_failure("checkpoint recorder demo round trip mismatch");
	return 0;
}

int main(void)
{
	if (expect_record_and_flush())
		return 1;
	if (expect_record_and_flush_checkpoint())
		return 1;
	puts("PASS");
	return 0;
}