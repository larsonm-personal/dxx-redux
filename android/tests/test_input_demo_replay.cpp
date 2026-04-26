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

static int write_test_fixture(const char *dir)
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
		return report_failure_string(std::string("recorder start failed: ") + error);
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
	if (!input_demo_recorder_capture_frame(4000, &state, &pulse, 102, 1, 3, error, sizeof(error)))
		return report_failure_string(std::string("capture frame 2 failed: ") + error);
	if (!input_demo_recorder_flush(dir, error, sizeof(error)))
		return report_failure_string(std::string("recorder flush failed: ") + error);
	return 0;
}

static int expect_replay_loader(void)
{
	const char *dir = "test_input_demo_replay_fixture";
	const std::string demo_path = std::string(dir) + "/demo.json5";
	const std::string input_path = std::string(dir) + "/inputs.p0.jsonl";
	const std::string rng_path = std::string(dir) + "/rng.p0.jsonl";
	const std::string result_path = std::string(dir) + "/result.json";
	input_demo_replay_frame frame;
	char error[256] = "";

	if (!make_test_dir(dir))
		return report_failure("could not create replay test directory");
	if (write_test_fixture(dir))
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
	if (input_demo_replay_frame_count() != 3)
		return report_failure("replay frame count mismatch");
	if (input_demo_replay_next_frame_index() != 0)
		return report_failure("replay cursor should start at frame 0");
	if (input_demo_replay_is_finished())
		return report_failure("replay should not start finished");

	if (!input_demo_replay_get_current_frame(&frame, error, sizeof(error)))
		return report_failure_string(std::string("replay current frame 0 failed: ") + error);
	if (frame.frame != 0 || frame.frame_time != 3276 || frame.state.forward_thrust_time != 44 ||
		frame.pulse.fire_primary_count != 0 || frame.rng_state != 100 || frame.has_rng_call_count)
		return report_failure("replay frame 0 mismatch");
	if (!input_demo_replay_advance_frame(error, sizeof(error)))
		return report_failure_string(std::string("replay advance 0 failed: ") + error);

	if (!input_demo_replay_get_current_frame(&frame, error, sizeof(error)))
		return report_failure_string(std::string("replay current frame 1 failed: ") + error);
	if (frame.frame != 1 || frame.frame_time != 3276 || frame.state.forward_thrust_time != 44 ||
		frame.pulse.fire_primary_count != 1 || frame.rng_state != 100 || frame.has_rng_call_count)
		return report_failure("replay frame 1 mismatch");
	if (!input_demo_replay_advance_frame(error, sizeof(error)))
		return report_failure_string(std::string("replay advance 1 failed: ") + error);

	if (!input_demo_replay_get_current_frame(&frame, error, sizeof(error)))
		return report_failure_string(std::string("replay current frame 2 failed: ") + error);
	if (frame.frame != 2 || frame.frame_time != 4000 || frame.state.forward_thrust_time != 0 ||
		frame.pulse.fire_primary_count != 0 || frame.rng_state != 102 || !frame.has_rng_call_count ||
		frame.rng_call_count != 3)
		return report_failure("replay frame 2 mismatch");
	if (!input_demo_replay_advance_frame(error, sizeof(error)))
		return report_failure_string(std::string("replay advance 2 failed: ") + error);

	if (!input_demo_replay_is_finished())
		return report_failure("replay should be finished after advancing all frames");
	if (input_demo_replay_get_current_frame(&frame, error, sizeof(error)))
		return report_failure("replay unexpectedly returned a frame after end of stream");
	input_demo_replay_unload();
	remove(demo_path.c_str());
	remove(input_path.c_str());
	remove(rng_path.c_str());
	remove(result_path.c_str());
	remove_test_dir(dir);
	return 0;
}

int main(void)
{
	if (expect_replay_loader())
		return 1;
	puts("PASS");
	return 0;
}