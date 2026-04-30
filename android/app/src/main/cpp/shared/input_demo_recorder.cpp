#include "input_demo_recorder.h"

#include <stdio.h>

#include <zlib.h>

#include <string>
#include <vector>

#include "input_demo_codec.h"
#include "input_demo_fixture.h"
#include "input_demo_rng_trace.h"
#include "input_demo_result.h"

namespace
{

#ifndef DXX_INPUT_DEMO_BUILD_NUMBERi
#define DXX_INPUT_DEMO_BUILD_NUMBERi 0
#endif

#ifndef DXX_INPUT_DEMO_GIT_VERSION
#define DXX_INPUT_DEMO_GIT_VERSION "unknown"
#endif

#ifndef DXX_INPUT_DEMO_ARCH
#if defined(__aarch64__) || defined(_M_ARM64)
#define DXX_INPUT_DEMO_ARCH "arm64"
#elif defined(__arm__) || defined(_M_ARM)
#define DXX_INPUT_DEMO_ARCH "arm"
#elif defined(__x86_64__) || defined(_M_X64)
#define DXX_INPUT_DEMO_ARCH "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
#define DXX_INPUT_DEMO_ARCH "x86"
#else
#define DXX_INPUT_DEMO_ARCH "unknown"
#endif
#endif

struct input_demo_recorder_session {
	bool active;
	int game;
	std::string mission;
	int level;
	int difficulty;
	std::string rng_mode;
	bool has_player_cfg;
	input_demo_player_cfg player_cfg;
	bool has_checkpoint;
	std::string checkpoint_save_name;
	std::vector<unsigned char> checkpoint_data;
	int64_t checkpoint_start_gt;
	std::vector<input_demo_control_frame> control_frames;
	std::vector<input_demo_rng_frame> rng_frames;

	input_demo_recorder_session()
	    : active(false), game(0), level(0), difficulty(0), has_player_cfg(false), has_checkpoint(false), checkpoint_start_gt(0)
	{
		input_demo_player_cfg_clear(&player_cfg);
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

static bool input_demo_recorder_fail(std::string *error, const std::string &message)
{
	if (error)
		*error = message;
	return false;
}

static const char *input_demo_recorder_game_name(int game)
{
	if (game == INPUT_DEMO_GAME_D1)
		return "d1";
	if (game == INPUT_DEMO_GAME_D2)
		return "d2";
	return NULL;
}

static int input_demo_recorder_build_number(void)
{
	return DXX_INPUT_DEMO_BUILD_NUMBERi;
}

static const char *input_demo_recorder_git_version(void)
{
	return DXX_INPUT_DEMO_GIT_VERSION;
}

static const char *input_demo_recorder_arch(void)
{
	return DXX_INPUT_DEMO_ARCH;
}

static int input_demo_recorder_settings_have_checkpoint(const input_demo_recorder_settings *settings)
{
	return settings && (settings->checkpoint_data != NULL || settings->checkpoint_size != 0 ||
	                    (settings->checkpoint_save_name && settings->checkpoint_save_name[0]) ||
	                    settings->has_checkpoint_start_gt);
}

static bool input_demo_recorder_zlib_compress(const unsigned char *data,
                                              size_t data_size,
                                              std::vector<unsigned char> *compressed,
                                              std::string *error)
{
	uLongf compressed_size;
	int z_result;

	if (!compressed)
		return input_demo_recorder_fail(error, "missing checkpoint compression output");
	compressed_size = compressBound((uLong) data_size);
	compressed->resize((size_t) compressed_size);
	z_result = compress2(compressed->data(), &compressed_size, data, (uLong) data_size, Z_BEST_COMPRESSION);
	if (z_result != Z_OK)
		return input_demo_recorder_fail(error, std::string("checkpoint zlib compress failed: ") + std::to_string(z_result));
	compressed->resize((size_t) compressed_size);
	return true;
}

static bool input_demo_recorder_build_checkpoint(input_demo_checkpoint *checkpoint,
                                                 const input_demo_recorder_session &session,
                                                 std::string *error)
{
	std::vector<unsigned char> compressed_data;
	const unsigned char *encoded_data = NULL;
	size_t encoded_size = 0;

	if (!checkpoint)
		return input_demo_recorder_fail(error, "missing checkpoint output");
	if (session.checkpoint_save_name.empty())
		return input_demo_recorder_fail(error, "missing checkpoint save name");
	if (session.checkpoint_data.empty())
		return input_demo_recorder_fail(error, "missing checkpoint data");
	checkpoint->format = "dgss";
	checkpoint->encoding = "base64";
	checkpoint->compression = "none";
	checkpoint->size = (uint32_t) session.checkpoint_data.size();
	if (!input_demo_sha256_hex(session.checkpoint_data.data(), session.checkpoint_data.size(), &checkpoint->sha256, error))
		return false;
	checkpoint->save_name = session.checkpoint_save_name;
	checkpoint->has_start_gt = 1;
	checkpoint->start_gt = session.checkpoint_start_gt;
	if (!input_demo_recorder_zlib_compress(session.checkpoint_data.data(), session.checkpoint_data.size(),
	                                       &compressed_data, error))
		return false;
	if (compressed_data.size() < session.checkpoint_data.size()) {
		checkpoint->compression = "zlib";
		encoded_data = compressed_data.data();
		encoded_size = compressed_data.size();
	} else {
		encoded_data = session.checkpoint_data.data();
		encoded_size = session.checkpoint_data.size();
	}
	if (!input_demo_base64_encode(encoded_data, encoded_size, &checkpoint->data, error))
		return false;
	return true;
}

static void input_demo_recorder_build_result(input_demo_result *result,
                                             const input_demo_recorder_session &session,
                                             const input_demo_result *supplied_result)
{
	if (supplied_result)
		*result = *supplied_result;
	else
		input_demo_result_clear(result);
	result->version = 2;
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
	demo->metadata.version = 2;
	demo->metadata.game = input_demo_recorder_game_name(session.game);
	demo->metadata.mission = session.mission;
	demo->metadata.build_number = input_demo_recorder_build_number();
	demo->metadata.git_version = input_demo_recorder_git_version();
	demo->metadata.arch = input_demo_recorder_arch();
	demo->metadata.level = session.level;
	demo->metadata.difficulty = session.difficulty;
	demo->metadata.start_mode = session.has_checkpoint ? "save_checkpoint" : "new_level";
	demo->metadata.rng_mode = session.rng_mode;
	demo->metadata.frame_count = static_cast<uint32_t>(session.control_frames.size());
	demo->metadata.has_player_cfg = session.has_player_cfg;
	if (session.has_player_cfg)
		demo->metadata.player_cfg = session.player_cfg;
	if (session.has_checkpoint) {
		demo->metadata.start_save = session.checkpoint_save_name;
		demo->has_checkpoint = true;
		if (!input_demo_recorder_build_checkpoint(&demo->checkpoint, session, error))
			return false;
	}
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
	input_demo_rng_trace_reset();
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
	settings->has_player_cfg = 0;
	input_demo_player_cfg_clear(&settings->player_cfg);
	settings->checkpoint_save_name = NULL;
	settings->checkpoint_data = NULL;
	settings->checkpoint_size = 0;
	settings->has_checkpoint_start_gt = 0;
	settings->checkpoint_start_gt = 0;
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
	const int have_checkpoint = input_demo_recorder_settings_have_checkpoint(settings);

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
	if (have_checkpoint) {
		if (!settings->checkpoint_save_name || !settings->checkpoint_save_name[0])
			return input_demo_recorder_copy_error("missing recorder checkpoint_save_name", error, error_size);
		if (!settings->checkpoint_data || !settings->checkpoint_size)
			return input_demo_recorder_copy_error("missing recorder checkpoint_data", error, error_size);
		if (settings->checkpoint_size > UINT32_MAX)
			return input_demo_recorder_copy_error("recorder checkpoint_data is too large", error, error_size);
		if (!settings->has_checkpoint_start_gt)
			return input_demo_recorder_copy_error("missing recorder checkpoint_start_gt", error, error_size);
	}

	input_demo_recorder_reset_session();
	input_demo_rng_trace_start();
	g_input_demo_recorder_session.active = true;
	g_input_demo_recorder_session.game = settings->game;
	g_input_demo_recorder_session.mission = settings->mission;
	g_input_demo_recorder_session.level = settings->level;
	g_input_demo_recorder_session.difficulty = settings->difficulty;
	g_input_demo_recorder_session.rng_mode = settings->rng_mode;
	g_input_demo_recorder_session.has_player_cfg = settings->has_player_cfg ? true : false;
	if (settings->has_player_cfg)
		g_input_demo_recorder_session.player_cfg = settings->player_cfg;
	g_input_demo_recorder_session.has_checkpoint = have_checkpoint ? true : false;
	if (have_checkpoint) {
		g_input_demo_recorder_session.checkpoint_save_name = settings->checkpoint_save_name;
		g_input_demo_recorder_session.checkpoint_data.assign(settings->checkpoint_data,
		                                                     settings->checkpoint_data + settings->checkpoint_size);
		g_input_demo_recorder_session.checkpoint_start_gt = settings->checkpoint_start_gt;
	}
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
	if (!input_demo_rng_trace_write_sidecar_for_demo(demo_path, error, error_size)) {
		input_demo_recorder_reset_session();
		return input_demo_recorder_copy_error(std::string("demo saved but rng trace write failed: ") +
		                                          (error && error[0] ? error : "unknown error"),
		                                      error, error_size);
	}
	input_demo_recorder_reset_session();
	return 1;
}

int input_demo_recorder_flush(const char *demo_path,
                              char *error, size_t error_size)
{
	return input_demo_recorder_flush_with_result(demo_path, NULL, error, error_size);
}
}