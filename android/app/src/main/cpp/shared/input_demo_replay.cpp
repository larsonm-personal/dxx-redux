#include "input_demo_replay.h"

#include <stdio.h>
#include <string.h>

#include <fstream>
#include <limits>

#include <nlohmann/json.hpp>
#include <zlib.h>

#include <string>
#include <vector>

#include "input_demo_codec.h"
#include "input_demo_fixture.h"
#include "input_demo_rng_trace.h"
#include "input_demo_state_trace.h"

namespace
{

using ordered_json = nlohmann::ordered_json;

struct input_demo_replay_session {
	bool loaded;
	int game;
	std::string actual_result_path;
	bool has_expected_result;
	input_demo_result expected_result;
	std::string mission;
	int level;
	int difficulty;
	std::string start_mode;
	std::string rng_mode;
	bool has_player_cfg;
	input_demo_player_cfg player_cfg;
	bool has_checkpoint;
	std::string checkpoint_save_name;
	std::vector<uint8_t> checkpoint_data;
	int64_t checkpoint_start_gt;
	bool has_checkpoint_collision_delay_last_play_time;
	int64_t checkpoint_collision_delay_last_play_time;
	bool has_legacy_fx_rng_state;
	uint32_t legacy_fx_rng_state;
	bool has_legacy_fx_rng_call_count;
	uint32_t legacy_fx_rng_call_count;
	input_demo_checkpoint_escort_state checkpoint_escort_state;
	input_demo_checkpoint_thief_state checkpoint_thief_state;
	int64_t final_game_time64;
	uint32_t next_frame_index;
	std::vector<input_demo_replay_frame> frames;
	std::vector<std::vector<std::string>> frame_events;

	input_demo_replay_session()
	    : loaded(false), game(0), has_expected_result(false), level(0), difficulty(0), has_player_cfg(false), has_checkpoint(false),
	      checkpoint_start_gt(0), has_checkpoint_collision_delay_last_play_time(false), checkpoint_collision_delay_last_play_time(0),
	      has_legacy_fx_rng_state(false), legacy_fx_rng_state(0), has_legacy_fx_rng_call_count(false), legacy_fx_rng_call_count(0),
	      final_game_time64(0), next_frame_index(0)
	{
		input_demo_result_clear(&expected_result);
		input_demo_player_cfg_clear(&player_cfg);
		input_demo_checkpoint_escort_state_clear(&checkpoint_escort_state);
		input_demo_checkpoint_thief_state_clear(&checkpoint_thief_state);
	}
};

input_demo_replay_session g_input_demo_replay_session;

static int copy_error(const std::string &message, char *error, size_t error_size)
{
	if (error && error_size)
		snprintf(error, error_size, "%s", message.c_str());
	return 0;
}

static std::string actual_result_path_from_demo_path(const char *path)
{
	return std::string(path ? path : "input_demo") + ".actual.json";
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

static bool parse_direct_command_event_json(const std::string &json_text,
                                            bool *is_direct_command,
                                            input_demo_replay_direct_command_event *event,
                                            std::string *error)
{
	ordered_json parsed;
	ordered_json::const_iterator it;
	std::string command;

	if (!is_direct_command || !event)
		return fail(error, "missing direct command parse output");
	*is_direct_command = false;
	input_demo_replay_direct_command_event_clear(event);
	try {
		parsed = ordered_json::parse(json_text);
	} catch (const std::exception &ex) {
		return fail(error, std::string("frame event parse failed: ") + ex.what());
	}
	if (!parsed.is_object())
		return fail(error, "frame event must be an object");
	it = parsed.find("kind");
	if (it == parsed.end())
		return fail(error, "frame event is missing kind");
	if (!it->is_string())
		return fail(error, "frame event kind must be a string");
	if (it->get<std::string>() != "direct_command")
		return true;
	*is_direct_command = true;
	it = parsed.find("command");
	if (it == parsed.end())
		return fail(error, "direct command event is missing command");
	if (!it->is_string())
		return fail(error, "direct command event command must be a string");
	command = it->get<std::string>();
	if (command == "guidebot_goal") {
		it = parsed.find("special_key");
		if (it == parsed.end() || !it->is_number_integer())
			return fail(error, "guidebot goal event is missing special_key");
		event->kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_GOAL;
		event->value0 = it->get<int32_t>();
		it = parsed.find("from_menu");
		if (it != parsed.end()) {
			if (!it->is_boolean())
				return fail(error, "guidebot goal event from_menu must be a boolean");
			event->value1 = it->get<bool>() ? 1 : 0;
		}
		return true;
	}
	if (command == "drop_marker") {
		std::string message;

		it = parsed.find("player_marker_num");
		if (it == parsed.end() || !it->is_number_integer())
			return fail(error, "drop marker event is missing player_marker_num");
		event->kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_DROP_MARKER;
		event->value0 = it->get<int32_t>();
		it = parsed.find("message");
		if (it == parsed.end() || !it->is_string())
			return fail(error, "drop marker event is missing message");
		message = it->get<std::string>();
		snprintf(event->text, sizeof(event->text), "%s", message.c_str());
		return true;
	}
	if (command == "drop_current_weapon") {
		event->kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_DROP_CURRENT_WEAPON;
		return true;
	}
	if (command == "drop_secondary_weapon") {
		event->kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_DROP_SECONDARY_WEAPON;
		return true;
	}
	if (command == "drop_flag") {
		event->kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_DROP_FLAG;
		return true;
	}
	if (command == "escort_release_control") {
		event->kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_ESCORT_RELEASE_CONTROL;
		return true;
	}
	if (command == "guidebot_spawn") {
		event->kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_SPAWN;
		return true;
	}
	if (command == "guidebot_find_secret") {
		event->kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_FIND_SECRET;
		return true;
	}
	if (command == "guidebot_find_unexplored") {
		event->kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_FIND_UNEXPLORED;
		return true;
	}
	if (command == "guidebot_warp_to_me") {
		event->kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_WARP_TO_ME;
		return true;
	}
	if (command == "death_abort") {
		event->kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_DEATH_ABORT;
		return true;
	}
	if (command == "change_difficulty") {
		it = parsed.find("difficulty");
		if (it == parsed.end() || !it->is_number_integer())
			return fail(error, "change difficulty event is missing difficulty");
		event->value0 = it->get<int32_t>();
		if (event->value0 < 0 || event->value0 > 4)
			return fail(error, "change difficulty event difficulty is out of range");
		event->kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_CHANGE_DIFFICULTY;
		return true;
	}
	return fail(error, std::string("unknown direct command event command: ") + command);
}

static bool zlib_decompress(const std::vector<uint8_t> &compressed,
                            uint32_t expected_size,
                            std::vector<uint8_t> *decoded,
                            std::string *error)
{
	uLongf decoded_size = expected_size;
	int z_result;

	if (!decoded)
		return fail(error, "missing checkpoint decompress output");
	if (compressed.size() > std::numeric_limits<uLong>::max())
		return fail(error, "checkpoint compressed payload is too large");
	decoded->assign(expected_size, 0);
	z_result = uncompress(decoded->data(), &decoded_size, compressed.data(), (uLong) compressed.size());
	if (z_result != Z_OK)
		return fail(error, std::string("checkpoint zlib decompress failed: ") + std::to_string(z_result));
	if (decoded_size != expected_size)
		return fail(error, "checkpoint decoded size does not match metadata");
	decoded->resize((size_t) decoded_size);
	return true;
}

static bool load_checkpoint(const input_demo_checkpoint &checkpoint,
                            input_demo_replay_session *session,
                            std::string *error)
{
	std::vector<uint8_t> decoded;
	std::vector<uint8_t> raw_checkpoint;
	std::string actual_sha256;

	if (!session)
		return fail(error, "missing replay session output");
	if (!input_demo_base64_decode(checkpoint.data, &decoded, error))
		return false;
	if (checkpoint.compression == "zlib") {
		if (!zlib_decompress(decoded, checkpoint.size, &raw_checkpoint, error))
			return false;
	} else {
		raw_checkpoint = decoded;
		if (raw_checkpoint.size() != checkpoint.size)
			return fail(error, "checkpoint decoded size does not match metadata");
	}
	if (!input_demo_sha256_hex(raw_checkpoint.data(), raw_checkpoint.size(), &actual_sha256, error))
		return false;
	if (actual_sha256 != checkpoint.sha256)
		return fail(error, "checkpoint sha256 does not match metadata");
	session->has_checkpoint = true;
	session->checkpoint_save_name = checkpoint.save_name;
	session->checkpoint_data = raw_checkpoint;
	session->checkpoint_start_gt = checkpoint.start_gt;
	session->has_checkpoint_collision_delay_last_play_time = checkpoint.has_collision_delay_last_play_time ? true : false;
	session->checkpoint_collision_delay_last_play_time = checkpoint.collision_delay_last_play_time;
	session->checkpoint_escort_state = checkpoint.escort_state;
	session->checkpoint_thief_state = checkpoint.thief_state;
	return true;
}

static void load_legacy_fx_rng_seed_from_sidecar(const char *demo_path,
                                                 input_demo_replay_session *session)
{
	std::ifstream input;
	std::string line;

	if (!demo_path || !demo_path[0] || !session)
		return;
	input.open(std::string(demo_path) + INPUT_DEMO_RNG_TRACE_SUFFIX);
	if (!input.is_open())
		return;
	while (std::getline(input, line)) {
		ordered_json parsed;
		ordered_json::const_iterator it;
		std::string type;
		uint32_t call_count;

		parsed = ordered_json::parse(line, nullptr, false);
		if (parsed.is_discarded() || !parsed.is_object())
			continue;
		it = parsed.find("type");
		if (it == parsed.end() || !it->is_string())
			continue;
		type = it->get<std::string>();
		if (type != "rand" && type != "srand")
			continue;
		it = parsed.find("stream");
		if (it == parsed.end() || !it->is_number_integer() || it->get<int>() != 1)
			continue;
		it = parsed.find("call_count");
		if (it == parsed.end() || !it->is_number_unsigned())
			continue;
		call_count = it->get<uint32_t>();
		if (type == "rand") {
			it = parsed.find("state_before");
			if (it == parsed.end() || !it->is_number_unsigned())
				continue;
			session->has_legacy_fx_rng_state = true;
			session->legacy_fx_rng_state = it->get<uint32_t>();
			session->has_legacy_fx_rng_call_count = true;
			session->legacy_fx_rng_call_count = call_count ? call_count - 1 : 0;
			return;
		}
		it = parsed.find("seed");
		if (it == parsed.end() || !it->is_number_unsigned())
			continue;
		session->has_legacy_fx_rng_state = true;
		session->legacy_fx_rng_state = it->get<uint32_t>();
		session->has_legacy_fx_rng_call_count = true;
		session->legacy_fx_rng_call_count = call_count;
		return;
	}
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
	char error[256] = "";

	if (!input_demo_rng_trace_stop(error, sizeof(error)) && error[0])
		fprintf(stderr, "Input demo replay rng trace stop failed: %s\n", error);
	input_demo_state_trace_stop();
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
	input_demo_result_clear(&frame->state_result);
}

void input_demo_replay_direct_command_event_clear(input_demo_replay_direct_command_event *event)
{
	if (!event)
		return;
	memset(event, 0, sizeof(*event));
	event->kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_NONE;
}

int input_demo_replay_is_loaded(void)
{
	return g_input_demo_replay_session.loaded ? 1 : 0;
}

void input_demo_replay_unload(void)
{
	reset_session();
}

int input_demo_replay_load(const char *demo_path, char *error, size_t error_size)
{
	input_demo_file demo;
	std::vector<input_demo_control_record> control_records;
	std::vector<input_demo_rng_record> rng_records;
	std::vector<input_demo_replay_frame> frames;
	std::string replay_error;
	int game;
	size_t i;

	if (!demo_path || !demo_path[0])
		return copy_error("missing replay demo file path", error, error_size);
	if (!input_demo_file_read(demo_path, &demo, &replay_error))
		return copy_error(replay_error, error, error_size);
	game = game_id_from_name(demo.metadata.game);
	if (!game)
		return copy_error("metadata game must be d1 or d2", error, error_size);
	for (i = 0; i != demo.frames.size(); ++i) {
		control_records.push_back(demo.frames[i].input);
		rng_records.push_back(demo.frames[i].rng);
	}
	if (!expand_control_records(control_records, demo.metadata.frame_count, game, &frames, &replay_error))
		return copy_error(replay_error, error, error_size);
	if (!apply_rng_records(rng_records, &frames, &replay_error))
		return copy_error(replay_error, error, error_size);
	for (i = 0; i != demo.frames.size(); ++i) {
		if (!demo.frames[i].has_state)
			continue;
		frames[i].has_state = 1;
		frames[i].state_result = demo.frames[i].state;
	}
	reset_session();
	g_input_demo_replay_session.loaded = true;
	g_input_demo_replay_session.game = game;
	g_input_demo_replay_session.has_expected_result = demo.has_result;
	g_input_demo_replay_session.expected_result = demo.result;
	g_input_demo_replay_session.actual_result_path = actual_result_path_from_demo_path(demo_path);
	g_input_demo_replay_session.mission = demo.metadata.mission;
	g_input_demo_replay_session.level = demo.metadata.level;
	g_input_demo_replay_session.difficulty = demo.metadata.difficulty;
	g_input_demo_replay_session.start_mode = demo.metadata.start_mode;
	g_input_demo_replay_session.rng_mode = demo.metadata.rng_mode;
	g_input_demo_replay_session.has_player_cfg = demo.metadata.has_player_cfg;
	if (demo.metadata.has_player_cfg)
		g_input_demo_replay_session.player_cfg = demo.metadata.player_cfg;
	if (demo.has_checkpoint && !load_checkpoint(demo.checkpoint, &g_input_demo_replay_session, &replay_error)) {
		reset_session();
		return copy_error(replay_error, error, error_size);
	}
	if (g_input_demo_replay_session.has_checkpoint)
		load_legacy_fx_rng_seed_from_sidecar(demo_path, &g_input_demo_replay_session);
	g_input_demo_replay_session.frames = frames;
	g_input_demo_replay_session.frame_events.resize(frames.size());
	for (i = 0; i != demo.frames.size() && i != g_input_demo_replay_session.frame_events.size(); ++i)
		g_input_demo_replay_session.frame_events[i] = demo.frames[i].events;
	for (i = 0; i != g_input_demo_replay_session.frames.size(); ++i)
		g_input_demo_replay_session.final_game_time64 += g_input_demo_replay_session.frames[i].frame_time;
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

int64_t input_demo_replay_final_game_time64(void)
{
	return g_input_demo_replay_session.final_game_time64 +
	       g_input_demo_replay_session.checkpoint_start_gt;
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

const char *input_demo_replay_actual_result_path(void)
{
	return g_input_demo_replay_session.loaded ? g_input_demo_replay_session.actual_result_path.c_str() : NULL;
}

void input_demo_replay_set_actual_result_path(const char *path)
{
	if (!g_input_demo_replay_session.loaded)
		return;
	if (path && path[0])
		g_input_demo_replay_session.actual_result_path = path;
	else
		g_input_demo_replay_session.actual_result_path.clear();
}

int input_demo_replay_has_player_cfg(void)
{
	return g_input_demo_replay_session.loaded && g_input_demo_replay_session.has_player_cfg ? 1 : 0;
}

int input_demo_replay_get_player_cfg(input_demo_player_cfg *player_cfg)
{
	if (!input_demo_replay_has_player_cfg() || !player_cfg)
		return 0;
	*player_cfg = g_input_demo_replay_session.player_cfg;
	return 1;
}

int input_demo_replay_has_checkpoint(void)
{
	return g_input_demo_replay_session.loaded && g_input_demo_replay_session.has_checkpoint ? 1 : 0;
}

const char *input_demo_replay_checkpoint_save_name(void)
{
	return input_demo_replay_has_checkpoint() ? g_input_demo_replay_session.checkpoint_save_name.c_str() : NULL;
}

const uint8_t *input_demo_replay_checkpoint_data(void)
{
	return input_demo_replay_has_checkpoint() ? g_input_demo_replay_session.checkpoint_data.data() : NULL;
}

size_t input_demo_replay_checkpoint_size(void)
{
	return input_demo_replay_has_checkpoint() ? g_input_demo_replay_session.checkpoint_data.size() : 0;
}

int64_t input_demo_replay_checkpoint_start_gt(void)
{
	return input_demo_replay_has_checkpoint() ? g_input_demo_replay_session.checkpoint_start_gt : 0;
}

int input_demo_replay_get_checkpoint_collision_delay_last_play_time(int64_t *last_play_time)
{
	if (!input_demo_replay_has_checkpoint() || !last_play_time ||
	    !g_input_demo_replay_session.has_checkpoint_collision_delay_last_play_time)
		return 0;
	*last_play_time = g_input_demo_replay_session.checkpoint_collision_delay_last_play_time;
	return 1;
}

int input_demo_replay_get_legacy_fx_rng_seed(uint32_t *state,
                                             uint32_t *call_count)
{
	if (!input_demo_replay_has_checkpoint() || !state || !call_count ||
	    !g_input_demo_replay_session.has_legacy_fx_rng_state ||
	    !g_input_demo_replay_session.has_legacy_fx_rng_call_count)
		return 0;
	*state = g_input_demo_replay_session.legacy_fx_rng_state;
	*call_count = g_input_demo_replay_session.legacy_fx_rng_call_count;
	return 1;
}

int input_demo_replay_get_checkpoint_escort_state(input_demo_checkpoint_escort_state *escort_state)
{
	if (!input_demo_replay_has_checkpoint() || !escort_state || !g_input_demo_replay_session.checkpoint_escort_state.valid)
		return 0;
	*escort_state = g_input_demo_replay_session.checkpoint_escort_state;
	return 1;
}

int input_demo_replay_get_checkpoint_thief_state(input_demo_checkpoint_thief_state *thief_state)
{
	if (!input_demo_replay_has_checkpoint() || !thief_state || !g_input_demo_replay_session.checkpoint_thief_state.valid)
		return 0;
	*thief_state = g_input_demo_replay_session.checkpoint_thief_state;
	return 1;
}

int input_demo_replay_get_expected_result(input_demo_result *result,
                                          char *error, size_t error_size)
{
	if (!g_input_demo_replay_session.loaded)
		return copy_error("input demo replay is not loaded", error, error_size);
	if (!g_input_demo_replay_session.has_expected_result)
		return copy_error("input demo replay has no result trailer", error, error_size);
	if (!result)
		return copy_error("missing expected result output", error, error_size);
	*result = g_input_demo_replay_session.expected_result;
	return 1;
}

int input_demo_replay_compare_result(const input_demo_result *actual,
                                     char *error, size_t error_size)
{
	input_demo_result expected;

	if (!input_demo_replay_get_expected_result(&expected, error, error_size))
		return 0;
	return input_demo_result_compare(&expected, actual, error, error_size);
}

int input_demo_replay_get_current_frame_direct_command_count(uint32_t *count,
                                                             char *error, size_t error_size)
{
	const std::vector<std::string> *frame_events;
	std::string replay_error;
	input_demo_replay_direct_command_event event;
	bool is_direct_command;
	size_t i;

	if (!g_input_demo_replay_session.loaded)
		return copy_error("input demo replay is not loaded", error, error_size);
	if (input_demo_replay_is_finished())
		return copy_error("input demo replay is at end of stream", error, error_size);
	if (!count)
		return copy_error("missing direct command count output", error, error_size);
	*count = 0;
	if (g_input_demo_replay_session.next_frame_index >= g_input_demo_replay_session.frame_events.size())
		return 1;
	frame_events = &g_input_demo_replay_session.frame_events[g_input_demo_replay_session.next_frame_index];
	for (i = 0; i != frame_events->size(); ++i) {
		if (!parse_direct_command_event_json((*frame_events)[i], &is_direct_command, &event, &replay_error))
			return copy_error(replay_error, error, error_size);
		if (is_direct_command)
			(*count)++;
	}
	return 1;
}

int input_demo_replay_get_current_frame_direct_command_event(uint32_t direct_command_index,
                                                             input_demo_replay_direct_command_event *event,
                                                             char *error, size_t error_size)
{
	const std::vector<std::string> *frame_events;
	std::string replay_error;
	input_demo_replay_direct_command_event parsed_event;
	bool is_direct_command;
	uint32_t current_index = 0;
	size_t i;

	if (!g_input_demo_replay_session.loaded)
		return copy_error("input demo replay is not loaded", error, error_size);
	if (input_demo_replay_is_finished())
		return copy_error("input demo replay is at end of stream", error, error_size);
	if (!event)
		return copy_error("missing direct command event output", error, error_size);
	input_demo_replay_direct_command_event_clear(event);
	if (g_input_demo_replay_session.next_frame_index >= g_input_demo_replay_session.frame_events.size())
		return copy_error("direct command event index out of range", error, error_size);
	frame_events = &g_input_demo_replay_session.frame_events[g_input_demo_replay_session.next_frame_index];
	for (i = 0; i != frame_events->size(); ++i) {
		if (!parse_direct_command_event_json((*frame_events)[i], &is_direct_command, &parsed_event, &replay_error))
			return copy_error(replay_error, error, error_size);
		if (!is_direct_command)
			continue;
		if (current_index == direct_command_index) {
			*event = parsed_event;
			return 1;
		}
		current_index++;
	}
	return copy_error("direct command event index out of range", error, error_size);
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

int input_demo_replay_get_next_frame(input_demo_replay_frame *frame,
                                     char *error, size_t error_size)
{
	uint32_t frame_index;

	if (!g_input_demo_replay_session.loaded)
		return copy_error("input demo replay is not loaded", error, error_size);
	if (input_demo_replay_is_finished())
		return copy_error("input demo replay is at end of stream", error, error_size);
	if (!frame)
		return copy_error("missing replay frame output", error, error_size);
	frame_index = g_input_demo_replay_session.next_frame_index + 1;
	if (frame_index >= g_input_demo_replay_session.frames.size())
		return copy_error("input demo replay has no next frame", error, error_size);
	*frame = g_input_demo_replay_session.frames[frame_index];
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
