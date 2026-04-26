#include "input_demo_recorder.h"

#include <stdio.h>

#include <string>
#include <vector>

#include "input_demo_fixture.h"
#include "input_demo_result.h"

namespace
{

struct input_demo_recorder_session {
	bool active;
	int game;
	std::string mission;
	int level;
	int difficulty;
	std::string rng_mode;
	std::vector<input_demo_control_frame> control_frames;
	std::vector<input_demo_rng_frame> rng_frames;

	input_demo_recorder_session()
	    : active(false), game(0), level(0), difficulty(0)
	{
	}
};

input_demo_recorder_session g_input_demo_recorder_session;

static int input_demo_recorder_copy_error(const std::string &message,
                                          char *error, size_t error_size)
{
	if (error && error_size)
		snprintf(error, error_size, "%s", message.c_str());
	return 0;
}

static const char *input_demo_recorder_game_name(int game)
{
	if (game == INPUT_DEMO_GAME_D1)
		return "d1";
	if (game == INPUT_DEMO_GAME_D2)
		return "d2";
	return NULL;
}

static void input_demo_recorder_build_result(input_demo_result *result,
                                             const input_demo_recorder_session &session,
                                             const input_demo_result *supplied_result)
{
	if (supplied_result)
		*result = *supplied_result;
	else
		input_demo_result_clear(result);
	result->version = 1;
	snprintf(result->game, sizeof(result->game), "%s", input_demo_recorder_game_name(session.game));
	snprintf(result->mission, sizeof(result->mission), "%s", session.mission.c_str());
	result->level = session.level;
	result->difficulty = session.difficulty;
	result->frame_count = static_cast<uint32_t>(session.control_frames.size());
}

static bool input_demo_recorder_build_demo(input_demo_file *demo,
                                           const input_demo_recorder_session &session,
                                           const input_demo_result *result,
                                           std::string *error)
{
	input_demo_control_state previous_state;
	std::string unused_text;
	int have_previous_frame_time = 0;
	int32_t previous_frame_time = 0;
	size_t i;

	if (!demo)
		return false;
	demo->metadata.version = 1;
	demo->metadata.game = input_demo_recorder_game_name(session.game);
	demo->metadata.mission = session.mission;
	demo->metadata.level = session.level;
	demo->metadata.difficulty = session.difficulty;
	demo->metadata.start_mode = "new_level";
	demo->metadata.rng_mode = session.rng_mode;
	demo->metadata.frame_count = static_cast<uint32_t>(session.control_frames.size());
	input_demo_result_clear(&demo->result);
	input_demo_recorder_build_result(&demo->result, session, result);
	demo->has_result = true;
	input_demo_control_state_clear(&previous_state);
	for (i = 0; i != session.control_frames.size(); ++i) {
		input_demo_file_frame frame;

		input_demo_control_record_clear(&frame.input);
		frame.input.frame = static_cast<uint32_t>(i);
		frame.input.has_frame_time = !have_previous_frame_time ||
		                             session.control_frames[i].frame_time != previous_frame_time;
		frame.input.frame_time = session.control_frames[i].frame_time;
		input_demo_control_state_update_from_transition(&frame.input.held,
		                                                &previous_state,
		                                                &session.control_frames[i].state,
		                                                session.game);
		input_demo_control_pulse_update_from_pulse(&frame.input.pulse,
		                                           &session.control_frames[i].pulse,
		                                           session.game);

		input_demo_rng_record_clear(&frame.rng);
		frame.rng.frame = static_cast<uint32_t>(i);
		frame.rng.state = session.rng_frames[i].state;
		frame.rng.has_call_count = session.rng_frames[i].has_call_count;
		frame.rng.call_count = session.rng_frames[i].call_count;
		demo->frames.push_back(frame);
		previous_state = session.control_frames[i].state;
		previous_frame_time = session.control_frames[i].frame_time;
		have_previous_frame_time = 1;
	}
	return input_demo_file_to_text(*demo, &unused_text, error);
}

static void input_demo_recorder_reset_session(void)
{
	g_input_demo_recorder_session = input_demo_recorder_session();
}

} // namespace

extern "C" {

void input_demo_recorder_settings_clear(input_demo_recorder_settings *settings)
{
	if (!settings)
		return;
	settings->game = 0;
	settings->mission = NULL;
	settings->level = 0;
	settings->difficulty = 0;
	settings->rng_mode = NULL;
}

int input_demo_recorder_is_active(void)
{
	return g_input_demo_recorder_session.active ? 1 : 0;
}

uint32_t input_demo_recorder_frame_count(void)
{
	return static_cast<uint32_t>(g_input_demo_recorder_session.control_frames.size());
}

int input_demo_recorder_start(const input_demo_recorder_settings *settings,
                              char *error, size_t error_size)
{
	if (!settings)
		return input_demo_recorder_copy_error("missing recorder settings", error, error_size);
	if (g_input_demo_recorder_session.active)
		return input_demo_recorder_copy_error("input demo recorder is already active", error, error_size);
	if (!input_demo_recorder_game_name(settings->game))
		return input_demo_recorder_copy_error("invalid recorder game id", error, error_size);
	if (!settings->mission || !settings->mission[0])
		return input_demo_recorder_copy_error("missing recorder mission id", error, error_size);
	if (!settings->rng_mode || !settings->rng_mode[0])
		return input_demo_recorder_copy_error("missing recorder rng_mode", error, error_size);
	if (settings->level == 0)
		return input_demo_recorder_copy_error("input demo recorder requires a real level", error, error_size);
	if (settings->difficulty < 0 || settings->difficulty > 4)
		return input_demo_recorder_copy_error("input demo recorder received an invalid difficulty", error, error_size);

	input_demo_recorder_reset_session();
	g_input_demo_recorder_session.active = true;
	g_input_demo_recorder_session.game = settings->game;
	g_input_demo_recorder_session.mission = settings->mission;
	g_input_demo_recorder_session.level = settings->level;
	g_input_demo_recorder_session.difficulty = settings->difficulty;
	g_input_demo_recorder_session.rng_mode = settings->rng_mode;
	return 1;
}

void input_demo_recorder_cancel(void)
{
	input_demo_recorder_reset_session();
}

int input_demo_recorder_capture_frame(int32_t frame_time,
                                      const input_demo_control_state *state,
                                      const input_demo_control_pulse *pulse,
                                      uint32_t rng_state,
                                      int has_rng_call_count,
                                      uint32_t rng_call_count,
                                      char *error, size_t error_size)
{
	input_demo_control_frame control_frame;
	input_demo_rng_frame rng_frame;

	if (!g_input_demo_recorder_session.active)
		return input_demo_recorder_copy_error("input demo recorder is not active", error, error_size);
	if (!state || !pulse)
		return input_demo_recorder_copy_error("missing frame control state", error, error_size);

	input_demo_control_frame_clear(&control_frame);
	control_frame.frame = static_cast<uint32_t>(g_input_demo_recorder_session.control_frames.size());
	control_frame.frame_time = frame_time;
	control_frame.state = *state;
	control_frame.pulse = *pulse;

	input_demo_rng_frame_clear(&rng_frame);
	rng_frame.frame = control_frame.frame;
	rng_frame.state = rng_state;
	rng_frame.has_call_count = has_rng_call_count ? 1 : 0;
	rng_frame.call_count = rng_call_count;

	g_input_demo_recorder_session.control_frames.push_back(control_frame);
	g_input_demo_recorder_session.rng_frames.push_back(rng_frame);
	return 1;
}

int input_demo_recorder_flush_with_result(const char *demo_path,
                                          const input_demo_result *result,
                                          char *error, size_t error_size)
{
	input_demo_file demo;
	std::string shared_error;

	if (!g_input_demo_recorder_session.active)
		return input_demo_recorder_copy_error("input demo recorder is not active", error, error_size);
	if (!demo_path || !demo_path[0])
		return input_demo_recorder_copy_error("missing demo file path", error, error_size);
	if (g_input_demo_recorder_session.control_frames.empty())
		return input_demo_recorder_copy_error("input demo recorder captured no frames", error, error_size);
	if (g_input_demo_recorder_session.control_frames.size() != g_input_demo_recorder_session.rng_frames.size())
		return input_demo_recorder_copy_error("input demo recorder frame streams are out of sync", error, error_size);
	if (!input_demo_recorder_build_demo(&demo, g_input_demo_recorder_session, result, &shared_error))
		return input_demo_recorder_copy_error(shared_error, error, error_size);
	if (!input_demo_file_write(demo_path, demo, &shared_error))
		return input_demo_recorder_copy_error(shared_error, error, error_size);
	input_demo_recorder_reset_session();
	return 1;
}

int input_demo_recorder_flush(const char *demo_path,
                              char *error, size_t error_size)
{
	return input_demo_recorder_flush_with_result(demo_path, NULL, error, error_size);
}
}