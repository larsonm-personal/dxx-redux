#include "input_demo_replay.h"

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "input_demo_fixture.h"

namespace
{

struct input_demo_replay_session {
	bool loaded;
	int game;
	std::string mission;
	int level;
	int difficulty;
	std::string start_mode;
	std::string rng_mode;
	uint32_t next_frame_index;
	std::vector<input_demo_replay_frame> frames;

	input_demo_replay_session()
	    : loaded(false), game(0), level(0), difficulty(0), next_frame_index(0)
	{
	}
};

input_demo_replay_session g_input_demo_replay_session;

static int copy_error(const std::string &message, char *error, size_t error_size)
{
	if (error && error_size)
		snprintf(error, error_size, "%s", message.c_str());
	return 0;
}

static std::string join_path(const std::string &dir, const std::string &name)
{
	std::string joined(dir);

	if (!joined.empty() && joined[joined.size() - 1] != '/' && joined[joined.size() - 1] != '\\')
		joined.push_back('/');
	joined += name;
	return joined;
}

static std::string dirname_from_path(const char *path)
{
	std::string full(path ? path : "");
	std::string::size_type slash = full.find_last_of("/\\");

	if (slash == std::string::npos)
		return ".";
	if (!slash)
		return full.substr(0, 1);
	return full.substr(0, slash);
}

static int game_id_from_name(const std::string &game_name)
{
	if (game_name == "d1")
		return INPUT_DEMO_GAME_D1;
	if (game_name == "d2")
		return INPUT_DEMO_GAME_D2;
	return 0;
}

static bool fail(std::string *error, const std::string &message)
{
	if (error)
		*error = message;
	return false;
}

static bool expand_control_records(const std::vector<input_demo_control_record> &records,
                                   uint32_t expected_frame_count, int game,
                                   std::vector<input_demo_replay_frame> *frames, std::string *error)
{
	std::vector<input_demo_replay_frame> expanded;
	input_demo_control_state state;
	int have_frame_time = 0;
	int32_t frame_time = 0;
	size_t i;

	if (!frames)
		return fail(error, "missing replay frame output");
	input_demo_control_state_clear(&state);
	for (i = 0; i != records.size(); ++i) {
		const input_demo_control_record &record = records[i];
		uint32_t run_index;

		if (record.frame != expanded.size())
			return fail(error, "control replay records must be contiguous");
		if (record.run_length != 1 && !input_demo_control_pulse_update_is_empty(&record.pulse))
			return fail(error, "control replay records with pulses must use n: 1");
		if (record.has_frame_time) {
			frame_time = record.frame_time;
			have_frame_time = 1;
		} else if (!have_frame_time) {
			return fail(error, "first control replay record must define ft");
		}
		input_demo_control_state_apply_update(&state, &record.held, game);
		for (run_index = 0; run_index != record.run_length; ++run_index) {
			input_demo_replay_frame frame;

			input_demo_replay_frame_clear(&frame);
			frame.frame = static_cast<uint32_t>(expanded.size());
			frame.frame_time = frame_time;
			frame.state = state;
			if (!run_index)
				input_demo_control_pulse_apply_update(&frame.pulse, &record.pulse, game);
			expanded.push_back(frame);
		}
	}
	if (expanded.size() != expected_frame_count)
		return fail(error, "control replay frame count does not match metadata");
	*frames = expanded;
	return true;
}

static bool apply_rng_records(const std::vector<input_demo_rng_record> &records,
                              std::vector<input_demo_replay_frame> *frames, std::string *error)
{
	uint32_t frame_index = 0;
	size_t i;

	if (!frames)
		return fail(error, "missing replay frame output");
	for (i = 0; i != records.size(); ++i) {
		const input_demo_rng_record &record = records[i];
		uint32_t run_index;

		if (record.frame != frame_index)
			return fail(error, "rng replay records must be contiguous");
		for (run_index = 0; run_index != record.run_length; ++run_index) {
			if (frame_index >= frames->size())
				return fail(error, "rng replay frame count exceeds control replay frame count");
			(*frames)[frame_index].rng_state = record.state;
			if (!run_index && record.has_call_count) {
				(*frames)[frame_index].has_rng_call_count = 1;
				(*frames)[frame_index].rng_call_count = record.call_count;
			}
			frame_index++;
		}
	}
	if (frame_index != frames->size())
		return fail(error, "rng replay frame count does not match control replay frame count");
	return true;
}

static void reset_session(void)
{
	g_input_demo_replay_session = input_demo_replay_session();
}

} // namespace

extern "C" {

void input_demo_replay_frame_clear(input_demo_replay_frame *frame)
{
	if (!frame)
		return;
	memset(frame, 0, sizeof(*frame));
	input_demo_control_state_clear(&frame->state);
	input_demo_control_pulse_clear(&frame->pulse);
}

int input_demo_replay_is_loaded(void)
{
	return g_input_demo_replay_session.loaded ? 1 : 0;
}

void input_demo_replay_unload(void)
{
	reset_session();
}

int input_demo_replay_load(const char *metadata_path, char *error, size_t error_size)
{
	input_demo_metadata metadata;
	std::vector<input_demo_control_record> control_records;
	std::vector<input_demo_rng_record> rng_records;
	std::vector<input_demo_replay_frame> frames;
	std::string base_dir;
	std::string input_path;
	std::string rng_path;
	std::string replay_error;
	int game;

	if (!metadata_path || !metadata_path[0])
		return copy_error("missing replay metadata path", error, error_size);
	if (!input_demo_metadata_read_json5_file(metadata_path, &metadata, &replay_error))
		return copy_error(replay_error, error, error_size);
	if (metadata.streams.size() != 1)
		return copy_error("single-player replay requires exactly one stream", error, error_size);
	if (metadata.streams[0].player != 0)
		return copy_error("single-player replay requires stream player 0", error, error_size);
	game = game_id_from_name(metadata.game);
	if (!game)
		return copy_error("metadata game must be d1 or d2", error, error_size);
	base_dir = dirname_from_path(metadata_path);
	input_path = join_path(base_dir, metadata.streams[0].input_path);
	rng_path = join_path(base_dir, metadata.streams[0].rng_path);
	if (!input_demo_control_records_read_jsonl_file(input_path.c_str(), game, &control_records, &replay_error))
		return copy_error(replay_error, error, error_size);
	if (!input_demo_rng_records_read_jsonl_file(rng_path.c_str(), &rng_records, &replay_error))
		return copy_error(replay_error, error, error_size);
	if (!expand_control_records(control_records, metadata.frame_count, game, &frames, &replay_error))
		return copy_error(replay_error, error, error_size);
	if (!apply_rng_records(rng_records, &frames, &replay_error))
		return copy_error(replay_error, error, error_size);
	reset_session();
	g_input_demo_replay_session.loaded = true;
	g_input_demo_replay_session.game = game;
	g_input_demo_replay_session.mission = metadata.mission;
	g_input_demo_replay_session.level = metadata.level;
	g_input_demo_replay_session.difficulty = metadata.difficulty;
	g_input_demo_replay_session.start_mode = metadata.start_mode;
	g_input_demo_replay_session.rng_mode = metadata.rng_mode;
	g_input_demo_replay_session.frames = frames;
	return 1;
}

int input_demo_replay_is_finished(void)
{
	return g_input_demo_replay_session.loaded &&
	       g_input_demo_replay_session.next_frame_index >= g_input_demo_replay_session.frames.size();
}

uint32_t input_demo_replay_frame_count(void)
{
	return static_cast<uint32_t>(g_input_demo_replay_session.frames.size());
}

uint32_t input_demo_replay_next_frame_index(void)
{
	return g_input_demo_replay_session.next_frame_index;
}

int input_demo_replay_game(void)
{
	return g_input_demo_replay_session.game;
}

const char *input_demo_replay_mission(void)
{
	return g_input_demo_replay_session.loaded ? g_input_demo_replay_session.mission.c_str() : NULL;
}

int input_demo_replay_level(void)
{
	return g_input_demo_replay_session.level;
}

int input_demo_replay_difficulty(void)
{
	return g_input_demo_replay_session.difficulty;
}

const char *input_demo_replay_start_mode(void)
{
	return g_input_demo_replay_session.loaded ? g_input_demo_replay_session.start_mode.c_str() : NULL;
}

const char *input_demo_replay_rng_mode(void)
{
	return g_input_demo_replay_session.loaded ? g_input_demo_replay_session.rng_mode.c_str() : NULL;
}

int input_demo_replay_get_current_frame(input_demo_replay_frame *frame,
                                        char *error, size_t error_size)
{
	if (!g_input_demo_replay_session.loaded)
		return copy_error("input demo replay is not loaded", error, error_size);
	if (input_demo_replay_is_finished())
		return copy_error("input demo replay is at end of stream", error, error_size);
	if (!frame)
		return copy_error("missing replay frame output", error, error_size);
	*frame = g_input_demo_replay_session.frames[g_input_demo_replay_session.next_frame_index];
	return 1;
}

int input_demo_replay_advance_frame(char *error, size_t error_size)
{
	if (!g_input_demo_replay_session.loaded)
		return copy_error("input demo replay is not loaded", error, error_size);
	if (input_demo_replay_is_finished())
		return copy_error("input demo replay is at end of stream", error, error_size);
	g_input_demo_replay_session.next_frame_index++;
	return 1;
}
}