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

static std::string input_demo_recorder_join_path(const char *dir, const char *name)
{
	std::string joined(dir ? dir : "");
	if (!joined.empty() && joined[joined.size() - 1] != '/' && joined[joined.size() - 1] != '\\')
		joined.push_back('/');
	joined += name;
	return joined;
}

static bool input_demo_recorder_write_minimal_result(const char *path,
                                                     const input_demo_recorder_session &session, std::string *error)
{
	input_demo_result result;
	char write_error[256] = "";

	input_demo_result_clear(&result);
	snprintf(result.game, sizeof(result.game), "%s", input_demo_recorder_game_name(session.game));
	snprintf(result.mission, sizeof(result.mission), "%s", session.mission.c_str());
	result.level = session.level;
	result.difficulty = session.difficulty;
	result.frame_count = static_cast<uint32_t>(session.control_frames.size());
	if (!input_demo_result_write_json_file(path, &result, write_error, sizeof(write_error))) {
		if (error)
			*error = write_error;
		return false;
	}
	return true;
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

int input_demo_recorder_flush(const char *fixture_dir,
                              char *error, size_t error_size)
{
	std::vector<input_demo_control_record> control_records;
	std::vector<input_demo_rng_record> rng_records;
	input_demo_metadata metadata;
	input_demo_stream_file stream;
	std::string shared_error;
	std::string input_path;
	std::string rng_path;
	std::string metadata_path;
	std::string result_path;

	if (!g_input_demo_recorder_session.active)
		return input_demo_recorder_copy_error("input demo recorder is not active", error, error_size);
	if (!fixture_dir || !fixture_dir[0])
		return input_demo_recorder_copy_error("missing fixture directory", error, error_size);
	if (g_input_demo_recorder_session.control_frames.empty())
		return input_demo_recorder_copy_error("input demo recorder captured no frames", error, error_size);
	if (g_input_demo_recorder_session.control_frames.size() != g_input_demo_recorder_session.rng_frames.size())
		return input_demo_recorder_copy_error("input demo recorder frame streams are out of sync", error, error_size);
	if (!input_demo_control_records_coalesce_frames(g_input_demo_recorder_session.control_frames,
	                                                g_input_demo_recorder_session.game, &control_records, &shared_error))
		return input_demo_recorder_copy_error(shared_error, error, error_size);
	if (!input_demo_rng_records_coalesce_frames(g_input_demo_recorder_session.rng_frames,
	                                            &rng_records, &shared_error))
		return input_demo_recorder_copy_error(shared_error, error, error_size);

	input_path = input_demo_recorder_join_path(fixture_dir, "inputs.p0.jsonl");
	rng_path = input_demo_recorder_join_path(fixture_dir, "rng.p0.jsonl");
	metadata_path = input_demo_recorder_join_path(fixture_dir, "demo.json5");
	result_path = input_demo_recorder_join_path(fixture_dir, "result.json");
	if (!input_demo_control_records_write_jsonl_file(input_path.c_str(),
	                                                 g_input_demo_recorder_session.game, control_records, &shared_error))
		return input_demo_recorder_copy_error(shared_error, error, error_size);
	if (!input_demo_rng_records_write_jsonl_file(rng_path.c_str(), rng_records, &shared_error))
		return input_demo_recorder_copy_error(shared_error, error, error_size);
	metadata.version = 1;
	metadata.game = input_demo_recorder_game_name(g_input_demo_recorder_session.game);
	metadata.mission = g_input_demo_recorder_session.mission;
	metadata.level = g_input_demo_recorder_session.level;
	metadata.difficulty = g_input_demo_recorder_session.difficulty;
	metadata.start_mode = "new_level";
	metadata.rng_mode = g_input_demo_recorder_session.rng_mode;
	metadata.frame_count = static_cast<uint32_t>(g_input_demo_recorder_session.control_frames.size());
	stream.player = 0;
	stream.input_path = "inputs.p0.jsonl";
	stream.rng_path = "rng.p0.jsonl";
	metadata.streams.push_back(stream);
	metadata.result_path = "result.json";
	if (!input_demo_metadata_write_json5_file(metadata_path.c_str(), metadata, &shared_error))
		return input_demo_recorder_copy_error(shared_error, error, error_size);
	if (!input_demo_recorder_write_minimal_result(result_path.c_str(),
	                                              g_input_demo_recorder_session, &shared_error))
		return input_demo_recorder_copy_error(shared_error, error, error_size);
	input_demo_recorder_reset_session();
	return 1;
}
}