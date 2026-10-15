#include "input_demo_cheats.h"
#include "input_demo_recorder.h"

#include <stdio.h>

#include <zlib.h>

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "input_demo_codec.h"
#include "input_demo_fixture.h"
#include "input_demo_limits.h"
#include "input_demo_rng_trace.h"
#include "input_demo_result.h"

#ifdef __ANDROID__
#include "android_native_build_info.h"
#define DXX_INPUT_DEMO_BUILD_NUMBERi DXX_ANDROID_VERSION_CODE
#define DXX_INPUT_DEMO_GIT_VERSION   DXX_ANDROID_GIT_COMMIT
// Exported so release verification can inspect the stripped APK libraries
extern "C" __attribute__((visibility("default"))) const char dxx_android_build_identity[] =
    DXX_ANDROID_BUILD_ID;
#endif

namespace
{

using ordered_json = nlohmann::ordered_json;

enum {
	INPUT_DEMO_RECORDED_DIAG = 1,
	INPUT_DEMO_RECORDED_EVENTS = 2
};

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
	std::string asset_fingerprints;
	int level;
	int difficulty;
	std::string rng_mode;
	bool has_level_start = false;
	input_demo_level_start level_start{};
	bool has_player_cfg;
	input_demo_player_cfg player_cfg;
	bool has_rear_view_baseline = false;
	input_demo_rear_view_baseline rear_view_baseline = {};
	bool has_view_projection = false;
	input_demo_view_projection view_projection = {};
	bool has_checkpoint;
	std::string checkpoint_save_name;
	std::vector<unsigned char> checkpoint_data;
	int64_t checkpoint_start_gt;
	bool has_checkpoint_collision_delay_last_play_time;
	int64_t checkpoint_collision_delay_last_play_time;
	input_demo_checkpoint_escort_state checkpoint_escort_state;
	input_demo_checkpoint_thief_state checkpoint_thief_state;
	bool record_per_frame_state;
	bool ordered_input;
	bool input_failed;
	std::string input_error;
	input_demo_input_batch pending_input;
	size_t pending_input_bytes;
	int32_t input_frame_time;
	int automap_phase = 0;
	bool has_input_tail;
	input_demo_file_frame input_tail;
	std::vector<input_demo_control_frame> control_frames;
	std::vector<std::string> frame_lines;
	std::vector<uint8_t> extended_frames;
	std::vector<std::string> pending_frame_events;
	size_t pending_frame_event_bytes;
	input_demo_control_pulse pending_pulse;

	input_demo_recorder_session()
	    : active(false), game(0), level(0), difficulty(0), has_player_cfg(false), has_checkpoint(false), checkpoint_start_gt(0), has_checkpoint_collision_delay_last_play_time(false), checkpoint_collision_delay_last_play_time(0), record_per_frame_state(false), ordered_input(false), input_failed(false), pending_input_bytes(64), input_frame_time(0), has_input_tail(false), pending_frame_event_bytes(0)
	{
		input_demo_player_cfg_clear(&player_cfg);
		input_demo_checkpoint_escort_state_clear(&checkpoint_escort_state);
		input_demo_checkpoint_thief_state_clear(&checkpoint_thief_state);
		input_demo_control_pulse_clear(&pending_pulse);
	}
};

input_demo_recorder_session g_input_demo_recorder_session;
const char *(*asset_provider)(void);

static int input_demo_recorder_copy_error(const std::string &message,
                                          char *error, size_t error_size)
{
	if (error && error_size)
		snprintf(error, error_size, "%s", message.c_str());
	return 0;
}

static uint8_t input_demo_recorder_saturating_add_u8(uint8_t left, uint8_t right)
{
	const unsigned int sum = (unsigned int) left + (unsigned int) right;

	return (sum > UINT8_MAX) ? UINT8_MAX : (uint8_t) sum;
}

static void input_demo_recorder_accumulate_pulse(input_demo_control_pulse *dst,
                                                 const input_demo_control_pulse *src)
{
	if (!dst || !src)
		return;
	dst->fire_primary_count = input_demo_recorder_saturating_add_u8(dst->fire_primary_count, src->fire_primary_count);
	dst->fire_secondary_count = input_demo_recorder_saturating_add_u8(dst->fire_secondary_count, src->fire_secondary_count);
	dst->fire_flare_count = input_demo_recorder_saturating_add_u8(dst->fire_flare_count, src->fire_flare_count);
	dst->drop_bomb_count = input_demo_recorder_saturating_add_u8(dst->drop_bomb_count, src->drop_bomb_count);
	dst->cycle_primary_count = input_demo_recorder_saturating_add_u8(dst->cycle_primary_count, src->cycle_primary_count);
	dst->cycle_secondary_count = input_demo_recorder_saturating_add_u8(dst->cycle_secondary_count, src->cycle_secondary_count);
	if (src->select_weapon_count)
		dst->select_weapon_count = src->select_weapon_count;
	dst->rear_view_count = input_demo_recorder_saturating_add_u8(dst->rear_view_count, src->rear_view_count);
	dst->automap_count = input_demo_recorder_saturating_add_u8(dst->automap_count, src->automap_count);
	dst->toggle_bomb_count = input_demo_recorder_saturating_add_u8(dst->toggle_bomb_count, src->toggle_bomb_count);
	dst->headlight_count = input_demo_recorder_saturating_add_u8(dst->headlight_count, src->headlight_count);
}

static bool input_demo_recorder_fail(std::string *error, const std::string &message)
{
	if (error)
		*error = message;
	return false;
}

static bool input_demo_recorder_event_requires_ordered_input(const ordered_json &event)
{
	return event.contains("kind") && event["kind"] == "direct_command" && event.contains("command") &&
	       (event["command"] == "enable_cheats" || event["command"] == "grant_all_weapons");
}

static bool input_demo_recorder_canonicalize_event_json(const char *json_text,
                                                        std::string *canonical_json,
                                                        std::string *error)
{
	ordered_json parsed;

	if (!json_text || !json_text[0])
		return input_demo_recorder_fail(error, "missing frame event json");
	if (!canonical_json)
		return input_demo_recorder_fail(error, "missing frame event output");
	try {
		parsed = ordered_json::parse(json_text);
	} catch (const std::exception &e) {
		return input_demo_recorder_fail(error, std::string("invalid frame event json: ") + e.what());
	}
	if (!parsed.is_object())
		return input_demo_recorder_fail(error, "frame event json must be an object");
	if (!g_input_demo_recorder_session.ordered_input && input_demo_recorder_event_requires_ordered_input(parsed))
		return input_demo_recorder_fail(error, "all-weapons commands require ordered input format 5");
	*canonical_json = parsed.dump();
	return true;
}

static int input_demo_recorder_input_fail(const std::string &message, char *error, size_t error_size)
{
	auto &session = g_input_demo_recorder_session;
	session.input_failed = true;
	session.input_error = message;
	return input_demo_recorder_copy_error(message, error, error_size);
}

static int input_demo_recorder_append_operation(const input_demo_input_operation &operation, char *error, size_t error_size)
{
	auto &session = g_input_demo_recorder_session;
	if (!session.active) return input_demo_recorder_copy_error("input demo recorder is not active", error, error_size);
	if (!session.ordered_input) return 1;
	if (session.input_failed) return input_demo_recorder_copy_error(session.input_error, error, error_size);
	if (!session.pending_input.present || session.has_input_tail)
		return input_demo_recorder_input_fail("input operation requires an open batch", error, error_size);
	std::string text, detail;
	if (!input_demo_dispatch_operation_to_json(operation, session.game, &text, &detail))
		return input_demo_recorder_input_fail(detail, error, error_size);
	if (session.pending_input.operations.size() >= INPUT_DEMO_INPUT_OPERATION_MAX ||
	    text.size() + 1 > INPUT_DEMO_INPUT_BATCH_MAX_BYTES - session.pending_input_bytes)
		return input_demo_recorder_input_fail("input batch exceeds the supported limit", error, error_size);
	if (operation.kind == INPUT_DEMO_INPUT_AUTOMAP) {
		const int next = input_demo_automap_next_phase(session.automap_phase, operation.automap_phase);
		if (next < 0) return input_demo_recorder_input_fail("invalid ordered automap transition", error, error_size);
		session.automap_phase = next;
	}
	session.pending_input_bytes += text.size() + 1;
	session.pending_input.operations.push_back(operation);
	return 1;
}

static int input_demo_recorder_stage_canonical_frame_event(const std::string &canonical_json,
                                                           char *error, size_t error_size)
{
	if (!g_input_demo_recorder_session.active)
		return input_demo_recorder_copy_error("input demo recorder is not active", error, error_size);
	if (g_input_demo_recorder_session.ordered_input) {
		auto &session = g_input_demo_recorder_session;
		if (session.input_failed) return input_demo_recorder_copy_error(session.input_error, error, error_size);
		if (session.has_input_tail) return input_demo_recorder_input_fail("input tail is already finalized", error, error_size);
		const auto event = ordered_json::parse(canonical_json);
		if (event.contains("kind") && event["kind"] == "direct_command")
			return input_demo_recorder_input_fail("format 5 commands require the ordered command API", error, error_size);
		if (canonical_json.size() + 1 > INPUT_DEMO_RECORD_MAX_BYTES - session.pending_frame_event_bytes)
			return input_demo_recorder_input_fail("pending input diagnostics exceed the record limit", error, error_size);
		session.pending_frame_event_bytes += canonical_json.size() + 1;
	}
	g_input_demo_recorder_session.pending_frame_events.push_back(canonical_json);
	return 1;
}

static int input_demo_recorder_stage_direct_command_event(const ordered_json &event,
                                                          char *error, size_t error_size)
{
	auto &session = g_input_demo_recorder_session;
	if (session.active && !session.ordered_input && input_demo_recorder_event_requires_ordered_input(event))
		return input_demo_recorder_copy_error("all-weapons commands require ordered input format 5", error, error_size);
	std::string encoded;
	try {
		encoded = event.dump();
	} catch (const std::exception &ex) {
		return input_demo_recorder_input_fail(std::string("invalid input command text: ") + ex.what(), error, error_size);
	}
	if (session.active && session.ordered_input) {
		input_demo_input_operation operation = {};
		operation.kind = INPUT_DEMO_INPUT_COMMAND;
		operation.frame_time = session.input_frame_time;
		std::string detail;
		if (!input_demo_dispatch_command_from_json(encoded, session.game, &operation.command, &detail))
			return input_demo_recorder_input_fail(detail, error, error_size);
		return input_demo_recorder_append_operation(operation, error, error_size);
	}
	return input_demo_recorder_stage_canonical_frame_event(encoded, error, error_size);
}

static void input_demo_recorder_append_encoded_event(std::string &line, bool has_events,
                                                     const std::string &event)
{
	// Events are the last member so render-time diagnostics can extend the current frame
	line.pop_back();
	if (has_events) {
		line.pop_back();
		line.push_back(',');
	} else {
		line += ",\"events\":[";
	}
	line += event;
	line += "]}";
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
	                    settings->has_checkpoint_start_gt || settings->checkpoint_escort_state.valid ||
	                    settings->checkpoint_thief_state.valid);
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
	if (!input_demo_checkpoint_size_supported(session.checkpoint_data.size()))
		return input_demo_recorder_fail(error, "checkpoint data exceeds the supported limit");
	checkpoint->format = "dgss";
	checkpoint->encoding = "base64";
	checkpoint->compression = "none";
	checkpoint->size = (uint32_t) session.checkpoint_data.size();
	if (!input_demo_sha256_hex(session.checkpoint_data.data(), session.checkpoint_data.size(), &checkpoint->sha256, error))
		return false;
	checkpoint->save_name = session.checkpoint_save_name;
	checkpoint->has_start_gt = 1;
	checkpoint->start_gt = session.checkpoint_start_gt;
	checkpoint->has_collision_delay_last_play_time = session.has_checkpoint_collision_delay_last_play_time ? 1 : 0;
	checkpoint->collision_delay_last_play_time = session.checkpoint_collision_delay_last_play_time;
	checkpoint->escort_state = session.checkpoint_escort_state;
	checkpoint->thief_state = session.checkpoint_thief_state;
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

static bool input_demo_recorder_build_envelope(input_demo_file *demo,
                                               const input_demo_recorder_session &session,
                                               const input_demo_result *result,
                                               std::string *error)
{
	if (!demo)
		return false;
	demo->metadata.version = 3;
	for (uint8_t extended : session.extended_frames)
		if (extended) {
			demo->metadata.version = 4;
			break;
		}
	if (session.ordered_input) {
		if (session.input_failed) return input_demo_recorder_fail(error, session.input_error);
		if (!session.has_input_tail || session.pending_input.present)
			return input_demo_recorder_fail(error, "ordered recording requires finalized input tail");
		demo->metadata.version = 5;
		demo->has_input_tail = true;
		demo->input_tail = session.input_tail;
	}
	demo->metadata.game = input_demo_recorder_game_name(session.game);
	demo->metadata.mission = session.mission;
	demo->metadata.asset_fingerprints = session.asset_fingerprints;
	demo->metadata.build_number = input_demo_recorder_build_number();
	demo->metadata.git_version = input_demo_recorder_git_version();
	demo->metadata.arch = input_demo_recorder_arch();
	demo->metadata.level = session.level;
	demo->metadata.difficulty = session.difficulty;
	demo->metadata.start_mode = session.has_checkpoint ? "save_checkpoint" : "new_level";
	demo->metadata.rng_mode = session.rng_mode;
	demo->metadata.frame_count = static_cast<uint32_t>(session.control_frames.size());
	demo->metadata.has_level_start = session.has_level_start;
	demo->metadata.level_start = session.level_start;
	demo->metadata.has_player_cfg = session.has_player_cfg;
	if (session.has_player_cfg)
		demo->metadata.player_cfg = session.player_cfg;
	demo->metadata.has_rear_view_baseline = session.has_rear_view_baseline;
	demo->metadata.rear_view_baseline = session.rear_view_baseline;
	demo->metadata.has_view_projection = session.has_view_projection;
	demo->metadata.view_projection = session.view_projection;
	if (session.has_checkpoint) {
		demo->metadata.start_save = session.checkpoint_save_name;
		demo->has_checkpoint = true;
		if (!input_demo_recorder_build_checkpoint(&demo->checkpoint, session, error))
			return false;
	}
	input_demo_result_clear(&demo->result);
	input_demo_recorder_build_result(&demo->result, session, result);
	demo->has_result = true;
	return true;
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
	settings->level_start = NULL;
	settings->has_player_cfg = 0;
	input_demo_player_cfg_clear(&settings->player_cfg);
	settings->checkpoint_save_name = NULL;
	settings->checkpoint_data = NULL;
	settings->checkpoint_size = 0;
	settings->has_checkpoint_start_gt = 0;
	settings->checkpoint_start_gt = 0;
	settings->has_checkpoint_collision_delay_last_play_time = 0;
	settings->checkpoint_collision_delay_last_play_time = 0;
	input_demo_checkpoint_escort_state_clear(&settings->checkpoint_escort_state);
	input_demo_checkpoint_thief_state_clear(&settings->checkpoint_thief_state);
	settings->record_per_frame_state = 0;
	settings->ordered_input = 1;
	settings->has_rear_view_baseline = 0;
	settings->rear_view_baseline = {};
	settings->has_view_projection = 0;
	settings->view_projection = {};
}

int input_demo_recorder_is_active(void)
{
	return g_input_demo_recorder_session.active ? 1 : 0;
}

uint32_t input_demo_recorder_frame_count(void)
{
	return static_cast<uint32_t>(g_input_demo_recorder_session.control_frames.size());
}

uint32_t input_demo_recorder_pending_input_operation_count(void)
{
	return static_cast<uint32_t>(g_input_demo_recorder_session.pending_input.operations.size());
}

int input_demo_recorder_get_view_projection(input_demo_view_projection *state)
{
	const auto &session = g_input_demo_recorder_session;
	if (!session.active || !session.has_view_projection || !state) return 0;
	*state = session.view_projection;
	return 1;
}

uint32_t input_demo_recorder_diagnostic_frame_index(void)
{
	const auto &session = g_input_demo_recorder_session;
	const uint32_t count = static_cast<uint32_t>(session.control_frames.size());
	if (session.ordered_input && (session.pending_input.present || session.has_input_tail)) return count;
	return count ? count - 1 : 0;
}

int input_demo_recorder_truncate(uint32_t frame_count)
{
	if (!g_input_demo_recorder_session.active)
		return 0;
	if (frame_count > g_input_demo_recorder_session.control_frames.size())
		return 0;
	g_input_demo_recorder_session.control_frames.resize(frame_count);
	g_input_demo_recorder_session.frame_lines.resize(frame_count);
	g_input_demo_recorder_session.extended_frames.resize(frame_count);
	g_input_demo_recorder_session.pending_frame_events.clear();
	g_input_demo_recorder_session.pending_frame_event_bytes = 0;
	g_input_demo_recorder_session.pending_input = input_demo_input_batch();
	g_input_demo_recorder_session.automap_phase = 0;
	g_input_demo_recorder_session.pending_input_bytes = 64;
	g_input_demo_recorder_session.has_input_tail = false;
	g_input_demo_recorder_session.input_tail = input_demo_file_frame();
	input_demo_control_pulse_clear(&g_input_demo_recorder_session.pending_pulse);
	return 1;
}

void input_demo_recorder_set_asset_provider(const char *(*provider)(void) )
{
	asset_provider = provider;
}

void input_demo_recorder_invalidate_assets(const char *reason)
{
	auto &session = g_input_demo_recorder_session;
	if (!session.active || session.asset_fingerprints.empty()) return;
	auto assets = ordered_json::parse(session.asset_fingerprints);
	if (assets["status"] != "complete") return;
	assets["status"] = "unknown";
	assets["error"] = reason;
	session.asset_fingerprints = assets.dump();
}

void input_demo_recorder_note_asset(const char *role, const char *sha256, uint64_t size)
{
	const auto &session = g_input_demo_recorder_session;
	if (!session.active || session.asset_fingerprints.empty()) return;
	const auto assets = ordered_json::parse(session.asset_fingerprints);
	if (assets["status"] != "complete") return;
	for (const auto &entry : assets["files"])
		if (entry["role"] == role && entry["sha256"] == sha256 && entry["size"] == size) return;
	input_demo_recorder_invalidate_assets("asset sources changed during recording");
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
	if (settings->has_rear_view_baseline && (!settings->ordered_input || !input_demo_rear_view_baseline_supported(&settings->rear_view_baseline)))
		return input_demo_recorder_copy_error("invalid or legacy rear-view baseline", error, error_size);
	if (settings->has_view_projection && (!settings->ordered_input || !input_demo_view_projection_supported(&settings->view_projection) ||
	                                      (settings->game == INPUT_DEMO_GAME_D1 && settings->view_projection.missile_view_enabled)))
		return input_demo_recorder_copy_error("invalid or legacy view projection", error, error_size);
	if (have_checkpoint) {
		if (!settings->checkpoint_save_name || !settings->checkpoint_save_name[0])
			return input_demo_recorder_copy_error("missing recorder checkpoint_save_name", error, error_size);
		if (!settings->checkpoint_data || !settings->checkpoint_size)
			return input_demo_recorder_copy_error("missing recorder checkpoint_data", error, error_size);
		if (!input_demo_checkpoint_size_supported(settings->checkpoint_size))
			return input_demo_recorder_copy_error("recorder checkpoint_data is too large", error, error_size);
		if (!settings->has_checkpoint_start_gt)
			return input_demo_recorder_copy_error("missing recorder checkpoint_start_gt", error, error_size);
	}

	if (have_checkpoint && settings->level_start)
		return input_demo_recorder_copy_error("level start cannot contain a checkpoint", error, error_size);
	input_demo_recorder_reset_session();
	input_demo_rng_trace_start();
	g_input_demo_recorder_session.has_level_start = settings->level_start != NULL;
	if (settings->level_start)
		g_input_demo_recorder_session.level_start = *settings->level_start;
	g_input_demo_recorder_session.active = true;
	g_input_demo_recorder_session.game = settings->game;
	g_input_demo_recorder_session.mission = settings->mission;
	g_input_demo_recorder_session.asset_fingerprints = asset_provider ? asset_provider() : ordered_json({ { "status", "unknown" }, { "mode", settings->game == INPUT_DEMO_GAME_D1 ? "native_d1" : "native_d2" }, { "files", ordered_json::array() }, { "error", "no engine asset sources" } }).dump();
	g_input_demo_recorder_session.level = settings->level;
	g_input_demo_recorder_session.difficulty = settings->difficulty;
	g_input_demo_recorder_session.rng_mode = settings->rng_mode;
	g_input_demo_recorder_session.has_player_cfg = settings->has_player_cfg ? true : false;
	if (settings->has_player_cfg)
		g_input_demo_recorder_session.player_cfg = settings->player_cfg;
	g_input_demo_recorder_session.has_rear_view_baseline = settings->has_rear_view_baseline != 0;
	g_input_demo_recorder_session.rear_view_baseline = settings->rear_view_baseline;
	g_input_demo_recorder_session.has_view_projection = settings->has_view_projection != 0;
	g_input_demo_recorder_session.view_projection = settings->view_projection;
	g_input_demo_recorder_session.has_checkpoint = have_checkpoint ? true : false;
	g_input_demo_recorder_session.record_per_frame_state = settings->record_per_frame_state ? true : false;
	g_input_demo_recorder_session.ordered_input = settings->ordered_input != 0;
	if (have_checkpoint) {
		g_input_demo_recorder_session.checkpoint_save_name = settings->checkpoint_save_name;
		g_input_demo_recorder_session.checkpoint_data.assign(settings->checkpoint_data,
		                                                     settings->checkpoint_data + settings->checkpoint_size);
		g_input_demo_recorder_session.checkpoint_start_gt = settings->checkpoint_start_gt;
		g_input_demo_recorder_session.has_checkpoint_collision_delay_last_play_time =
		    settings->has_checkpoint_collision_delay_last_play_time ? true : false;
		g_input_demo_recorder_session.checkpoint_collision_delay_last_play_time =
		    settings->checkpoint_collision_delay_last_play_time;
		g_input_demo_recorder_session.checkpoint_escort_state = settings->checkpoint_escort_state;
		g_input_demo_recorder_session.checkpoint_thief_state = settings->checkpoint_thief_state;
	}
	return 1;
}

void input_demo_recorder_cancel(void)
{
	input_demo_recorder_reset_session();
}

int input_demo_recorder_cancel_for_rewind(void)
{
	if (!input_demo_recorder_uses_ordered_input()) return 0;
	input_demo_recorder_cancel();
	return 1;
}

int input_demo_recorder_capture_frame(int32_t frame_time,
                                      const input_demo_control_state *state,
                                      const input_demo_control_pulse *pulse,
                                      uint32_t rng_state,
                                      int has_rng_call_count,
                                      uint32_t rng_call_count,
                                      const input_demo_result *frame_state,
                                      const input_demo_state_trace_diag *frame_diag,
                                      char *error, size_t error_size)
{
	input_demo_control_frame control_frame;
	input_demo_file_frame frame;
	std::string line;
	std::string shared_error;
	auto &session = g_input_demo_recorder_session;

	if (!session.active)
		return input_demo_recorder_copy_error("input demo recorder is not active", error, error_size);
	if (!state || !pulse)
		return input_demo_recorder_copy_error("missing frame control state", error, error_size);

	input_demo_control_frame_clear(&control_frame);
	control_frame.frame = static_cast<uint32_t>(session.control_frames.size());
	control_frame.frame_time = frame_time;
	control_frame.state = *state;
	if (session.ordered_input) {
		if (session.input_failed) return input_demo_recorder_copy_error(session.input_error, error, error_size);
		if (session.automap_phase) return input_demo_recorder_input_fail("simulation frame cannot run while ordered automap is open", error, error_size);
		if (session.has_input_tail || !has_rng_call_count)
			return input_demo_recorder_input_fail("ordered frame requires RNG count and no finalized tail", error, error_size);
		control_frame.pulse = *pulse;
		frame.dispatch = session.pending_input;
		if (!frame.dispatch.present) {
			frame.dispatch.present = true;
			frame.dispatch.pre_rng_state = rng_state;
			frame.dispatch.pre_rng_count = rng_call_count;
		}
	} else {
		input_demo_recorder_accumulate_pulse(&control_frame.pulse, &session.pending_pulse);
		input_demo_recorder_accumulate_pulse(&control_frame.pulse, pulse);
	}

	input_demo_control_state previous_state;
	input_demo_control_state_clear(&previous_state);
	frame.input.frame = control_frame.frame;
	frame.input.has_frame_time = session.control_frames.empty() ||
	                             session.control_frames.back().frame_time != frame_time;
	frame.input.frame_time = frame_time;
	if (!session.control_frames.empty())
		previous_state = session.control_frames.back().state;
	input_demo_control_state_update_from_transition(&frame.input.held, &previous_state, state, session.game);
	input_demo_control_pulse_update_from_pulse(&frame.input.pulse, &control_frame.pulse, session.game);
	frame.rng.frame = control_frame.frame;
	frame.rng.state = rng_state;
	frame.rng.has_call_count = has_rng_call_count ? 1 : 0;
	frame.rng.call_count = rng_call_count;
	if (frame_state && session.record_per_frame_state) {
		frame.has_state = true;
		frame.state = *frame_state;
	}
	if (!input_demo_frame_to_json_line(frame, session.game, &line, &shared_error))
		return input_demo_recorder_copy_error(shared_error, error, error_size);
	const bool has_diag = frame_diag && session.record_per_frame_state;
	if (has_diag) {
		std::string diag_json;
		if (!input_demo_state_trace_diag_to_json_text(frame_diag, &diag_json, &shared_error))
			return input_demo_recorder_copy_error(shared_error, error, error_size);
		// This object was just encoded by the engine; do not parse and rebuild it
		line.pop_back();
		line += ",\"diag\":";
		line += diag_json;
		line.push_back('}');
	}
	bool has_events = false;
	for (const auto &event : session.pending_frame_events) {
		input_demo_recorder_append_encoded_event(line, has_events, event);
		has_events = true;
	}
	if (session.ordered_input && !input_demo_record_size_supported(line.size()))
		return input_demo_recorder_input_fail("ordered frame exceeds the record limit", error, error_size);
	session.control_frames.push_back(control_frame);
	session.frame_lines.push_back(std::move(line));
	session.extended_frames.push_back((has_diag ? INPUT_DEMO_RECORDED_DIAG : 0) |
	                                  (has_events ? INPUT_DEMO_RECORDED_EVENTS : 0));
	session.pending_frame_events.clear();
	session.pending_frame_event_bytes = 0;
	session.pending_input = input_demo_input_batch();
	session.pending_input_bytes = 64;
	input_demo_control_pulse_clear(&session.pending_pulse);
	return 1;
}

int input_demo_recorder_uses_ordered_input(void)
{
	return g_input_demo_recorder_session.active && g_input_demo_recorder_session.ordered_input;
}

int input_demo_recorder_begin_input_batch(uint32_t rng_state, uint32_t rng_count,
                                          int32_t input_frame_time, char *error, size_t error_size)
{
	auto &session = g_input_demo_recorder_session;
	if (!session.active) return input_demo_recorder_copy_error("input demo recorder is not active", error, error_size);
	if (!session.ordered_input) return 1;
	if (session.input_failed) return input_demo_recorder_copy_error(session.input_error, error, error_size);
	if (session.has_input_tail || input_frame_time < 0 || input_frame_time > INPUT_DEMO_FRAME_TIME_MAX)
		return input_demo_recorder_input_fail("invalid input time or already finalized tail", error, error_size);
	if (!session.pending_input.present) {
		session.pending_input.present = true;
		session.pending_input.pre_rng_state = rng_state;
		session.pending_input.pre_rng_count = rng_count;
	}
	session.input_frame_time = input_frame_time;
	return 1;
}

int input_demo_recorder_stage_input_reset(int32_t frame_time, char *error, size_t error_size)
{
	input_demo_input_operation operation = {};
	operation.kind = INPUT_DEMO_INPUT_RESET;
	operation.frame_time = frame_time;
	g_input_demo_recorder_session.input_frame_time = frame_time;
	return input_demo_recorder_append_operation(operation, error, error_size);
}

int input_demo_recorder_stage_input_clear(int32_t frame_time, char *error, size_t error_size)
{
	input_demo_input_operation operation = {};
	operation.kind = INPUT_DEMO_INPUT_CLEAR;
	operation.frame_time = frame_time;
	g_input_demo_recorder_session.input_frame_time = frame_time;
	return input_demo_recorder_append_operation(operation, error, error_size);
}

int input_demo_recorder_stage_input_automap(int32_t frame_time, int32_t phase,
                                            char *error, size_t error_size)
{
	input_demo_input_operation operation = {};
	operation.kind = INPUT_DEMO_INPUT_AUTOMAP;
	operation.frame_time = frame_time;
	operation.automap_phase = phase;
	g_input_demo_recorder_session.input_frame_time = frame_time;
	return input_demo_recorder_append_operation(operation, error, error_size);
}

int input_demo_recorder_stage_input_rear_view(int32_t frame_time, int32_t action,
                                              char *error, size_t error_size)
{
	input_demo_input_operation operation = {};
	operation.kind = INPUT_DEMO_INPUT_REAR_VIEW;
	operation.frame_time = frame_time;
	operation.rear_view_action = action;
	g_input_demo_recorder_session.input_frame_time = frame_time;
	return input_demo_recorder_append_operation(operation, error, error_size);
}

int input_demo_recorder_stage_input_cockpit(int32_t frame_time, int32_t current_cockpit, int32_t preferred_cockpit,
                                            char *error, size_t error_size)
{
	input_demo_input_operation operation = {};
	operation.kind = INPUT_DEMO_INPUT_COCKPIT;
	operation.frame_time = frame_time;
	operation.current_cockpit = current_cockpit;
	operation.preferred_cockpit = preferred_cockpit;
	g_input_demo_recorder_session.input_frame_time = frame_time;
	return input_demo_recorder_append_operation(operation, error, error_size);
}

static int input_demo_recorder_stage_control_operation(int kind, int32_t frame_time,
                                                       const input_demo_control_state *state,
                                                       const input_demo_control_pulse *pulse,
                                                       char *error, size_t error_size)
{
	if (!state || !pulse) return input_demo_recorder_input_fail("missing input sample controls", error, error_size);
	input_demo_input_operation operation = {};
	operation.kind = kind;
	operation.frame_time = frame_time;
	operation.state = *state;
	operation.pulse = *pulse;
	g_input_demo_recorder_session.input_frame_time = frame_time;
	return input_demo_recorder_append_operation(operation, error, error_size);
}

int input_demo_recorder_stage_input_sample(int32_t frame_time,
                                           const input_demo_control_state *state,
                                           const input_demo_control_pulse *pulse,
                                           char *error, size_t error_size)
{
	return input_demo_recorder_stage_control_operation(INPUT_DEMO_INPUT_SAMPLE, frame_time, state, pulse, error, error_size);
}

int input_demo_recorder_stage_input_controls(int32_t frame_time,
                                             const input_demo_control_state *state,
                                             const input_demo_control_pulse *pulse,
                                             char *error, size_t error_size)
{
	return input_demo_recorder_stage_control_operation(INPUT_DEMO_INPUT_CONTROLS, frame_time, state, pulse, error, error_size);
}

int input_demo_recorder_finish_input_tail(const input_demo_control_state *state,
                                          const input_demo_control_pulse *pulse,
                                          uint32_t rng_state, uint32_t rng_count,
                                          char *error, size_t error_size)
{
	auto &session = g_input_demo_recorder_session;
	if (!session.active) return input_demo_recorder_copy_error("input demo recorder is not active", error, error_size);
	if (!session.ordered_input) return 1;
	if (session.input_failed) return input_demo_recorder_copy_error(session.input_error, error, error_size);
	if (!state || !pulse || session.has_input_tail)
		return input_demo_recorder_input_fail("missing tail controls or duplicate input tail", error, error_size);
	if (session.automap_phase != 0 && session.automap_phase != INPUT_DEMO_AUTOMAP_ACTIVE)
		return input_demo_recorder_input_fail("input tail cannot end during incomplete automap entry", error, error_size);
	auto &tail = session.input_tail;
	tail.input.frame = static_cast<uint32_t>(session.control_frames.size());
	tail.input.has_frame_time = 1;
	tail.input.frame_time = 1;
	input_demo_control_state_update_from_state(&tail.input.held, state, session.game);
	input_demo_control_pulse_update_from_pulse(&tail.input.pulse, pulse, session.game);
	tail.rng.frame = tail.input.frame;
	tail.rng.state = rng_state;
	tail.rng.has_call_count = 1;
	tail.rng.call_count = rng_count;
	tail.dispatch = session.pending_input;
	tail.events = session.pending_frame_events;
	if (!tail.dispatch.present) {
		tail.dispatch.present = true;
		tail.dispatch.pre_rng_state = rng_state;
		tail.dispatch.pre_rng_count = rng_count;
	}
	std::string encoded, detail;
	if (!input_demo_frame_to_json_line(tail, session.game, &encoded, &detail))
		return input_demo_recorder_input_fail(detail, error, error_size);
	if (!input_demo_record_size_supported(encoded.size()))
		return input_demo_recorder_input_fail("ordered input tail exceeds the record limit", error, error_size);
	session.pending_frame_events.clear();
	session.pending_frame_event_bytes = 0;
	session.pending_input = input_demo_input_batch();
	session.pending_input_bytes = 64;
	session.has_input_tail = true;
	return 1;
}

void input_demo_recorder_stage_pulse(const input_demo_control_pulse *pulse)
{
	if (!g_input_demo_recorder_session.active || !pulse || g_input_demo_recorder_session.ordered_input)
		return;
	input_demo_recorder_accumulate_pulse(&g_input_demo_recorder_session.pending_pulse, pulse);
}

int input_demo_recorder_stage_frame_event_json(const char *json_text,
                                               char *error, size_t error_size)
{
	std::string canonical_json;
	std::string shared_error;

	if (!g_input_demo_recorder_session.active)
		return input_demo_recorder_copy_error("input demo recorder is not active", error, error_size);
	if (!input_demo_recorder_canonicalize_event_json(json_text, &canonical_json, &shared_error))
		return input_demo_recorder_copy_error(shared_error, error, error_size);
	return input_demo_recorder_stage_canonical_frame_event(canonical_json, error, error_size);
}

int input_demo_recorder_append_frame_event_json(const char *json_text,
                                                char *error, size_t error_size)
{
	std::string canonical_json;
	std::string shared_error;
	auto &session = g_input_demo_recorder_session;

	if (!session.active)
		return input_demo_recorder_copy_error("input demo recorder is not active", error, error_size);
	if (!input_demo_recorder_canonicalize_event_json(json_text, &canonical_json, &shared_error))
		return session.ordered_input ? input_demo_recorder_input_fail(shared_error, error, error_size) : input_demo_recorder_copy_error(shared_error, error, error_size);
	if (session.ordered_input) {
		if (session.input_failed) return input_demo_recorder_copy_error(session.input_error, error, error_size);
		if (session.has_input_tail) return input_demo_recorder_input_fail("input tail is already finalized", error, error_size);
		const auto event = ordered_json::parse(canonical_json);
		if (event.contains("kind") && event["kind"] == "direct_command")
			return input_demo_recorder_input_fail("format 5 commands require ordered input operations", error, error_size);
	}
	if (!session.record_per_frame_state)
		return 1;
	// Ordered input effects precede their simulation frame, including frame zero
	if (session.ordered_input && (session.pending_input.present || session.frame_lines.empty()))
		return input_demo_recorder_stage_canonical_frame_event(canonical_json, error, error_size);
	if (session.frame_lines.empty())
		return input_demo_recorder_copy_error("input demo recorder has no current frame", error, error_size);
	const bool has_events = (session.extended_frames.back() & INPUT_DEMO_RECORDED_EVENTS) != 0;
	if (session.ordered_input && !input_demo_record_size_supported(
	                                 session.frame_lines.back().size() + canonical_json.size() + (has_events ? 1 : 12)))
		return input_demo_recorder_input_fail("ordered frame diagnostics exceed the record limit", error, error_size);
	input_demo_recorder_append_encoded_event(session.frame_lines.back(), has_events, canonical_json);
	session.extended_frames.back() |= INPUT_DEMO_RECORDED_EVENTS;
	return 1;
}

int input_demo_recorder_stage_cheat(int cheat, char *error, size_t error_size)
{
	if (cheat < INPUT_DEMO_CHEAT_D1_ENABLE || cheat > INPUT_DEMO_CHEAT_D2_ALL_WEAPONS)
		return input_demo_recorder_copy_error("unsupported demo cheat", error, error_size);
	ordered_json event;
	event["kind"] = "direct_command";
	event["command"] = "cheat";
	event["cheat"] = cheat;
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
}

int input_demo_recorder_stage_direct_command_guidebot_goal(int special_key,
                                                           int from_menu,
                                                           char *error, size_t error_size)
{
	ordered_json event = ordered_json::object();

	event["kind"] = "direct_command";
	event["command"] = "guidebot_goal";
	event["special_key"] = special_key;
	event["from_menu"] = from_menu ? true : false;
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
}

int input_demo_recorder_stage_direct_command_drop_marker(int player_marker_num,
                                                         const char *message,
                                                         char *error, size_t error_size)
{
	ordered_json event = ordered_json::object();

	event["kind"] = "direct_command";
	event["command"] = "drop_marker";
	event["player_marker_num"] = player_marker_num;
	event["message"] = message ? message : "";
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
}

int input_demo_recorder_stage_direct_command_delete_marker(int player_marker_num, int expected_signature,
                                                           char *error, size_t error_size)
{
	ordered_json event = { { "kind", "direct_command" }, { "command", "delete_marker" }, { "player_marker_num", player_marker_num }, { "expected_signature", expected_signature } };
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
}

int input_demo_recorder_stage_direct_command_drop_current_weapon(char *error,
                                                                 size_t error_size)
{
	ordered_json event = ordered_json::object();

	event["kind"] = "direct_command";
	event["command"] = "drop_current_weapon";
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
}

int input_demo_recorder_stage_direct_command_drop_secondary_weapon(char *error,
                                                                   size_t error_size)
{
	ordered_json event = ordered_json::object();

	event["kind"] = "direct_command";
	event["command"] = "drop_secondary_weapon";
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
}

int input_demo_recorder_stage_direct_command_drop_flag(char *error,
                                                       size_t error_size)
{
	ordered_json event = ordered_json::object();

	event["kind"] = "direct_command";
	event["command"] = "drop_flag";
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
}

int input_demo_recorder_stage_direct_command_escort_release_control(char *error,
                                                                    size_t error_size)
{
	ordered_json event = ordered_json::object();

	event["kind"] = "direct_command";
	event["command"] = "escort_release_control";
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
}

int input_demo_recorder_stage_direct_command_guidebot_spawn(char *error,
                                                            size_t error_size)
{
	ordered_json event = ordered_json::object();

	event["kind"] = "direct_command";
	event["command"] = "guidebot_spawn";
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
}

int input_demo_recorder_stage_direct_command_guidebot_find_secret(char *error,
                                                                  size_t error_size)
{
	ordered_json event = ordered_json::object();

	event["kind"] = "direct_command";
	event["command"] = "guidebot_find_secret";
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
}

int input_demo_recorder_stage_direct_command_guidebot_find_unexplored(char *error,
                                                                      size_t error_size)
{
	ordered_json event = ordered_json::object();

	event["kind"] = "direct_command";
	event["command"] = "guidebot_find_unexplored";
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
}

int input_demo_recorder_stage_direct_command_guidebot_warp_to_me(char *error,
                                                                 size_t error_size)
{
	ordered_json event = ordered_json::object();

	event["kind"] = "direct_command";
	event["command"] = "guidebot_warp_to_me";
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
}

int input_demo_recorder_stage_direct_command_death_abort(char *error,
                                                         size_t error_size)
{
	ordered_json event = ordered_json::object();

	event["kind"] = "direct_command";
	event["command"] = "death_abort";
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
}

int input_demo_recorder_stage_direct_command_change_difficulty(int difficulty,
                                                               char *error,
                                                               size_t error_size)
{
	ordered_json event = ordered_json::object();

	if (difficulty < 0 || difficulty > 4)
		return input_demo_recorder_copy_error("difficulty direct command is out of range", error, error_size);
	event["kind"] = "direct_command";
	event["command"] = "change_difficulty";
	event["difficulty"] = difficulty;
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
}

int input_demo_recorder_stage_direct_command_select_weapon_exact(int weapon_class,
                                                                 int weapon_index,
                                                                 char *error,
                                                                 size_t error_size)
{
	ordered_json event;
	if (weapon_class < 0 || weapon_class > 1 || weapon_index < 0 || weapon_index > 9)
		return input_demo_recorder_copy_error("exact weapon selection is out of range", error, error_size);
	event["kind"] = "direct_command";
	event["command"] = "select_weapon_exact";
	event["weapon_class"] = weapon_class;
	event["weapon_index"] = weapon_index;
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
}

int input_demo_recorder_stage_direct_command_enable_cheats(char *error, size_t error_size)
{
	const ordered_json event = { { "kind", "direct_command" }, { "command", "enable_cheats" } };
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
}

int input_demo_recorder_stage_direct_command_grant_all_weapons(int grant, char *error, size_t error_size)
{
	const char *name;
	switch (grant) {
		case INPUT_DEMO_GRANT_ALL_WEAPONS_D1_SCOURGE: name = "d1_scourge"; break;
		case INPUT_DEMO_GRANT_ALL_WEAPONS_D1_BIGRED: name = "d1_bigred"; break;
		case INPUT_DEMO_GRANT_ALL_WEAPONS_D2_HONESTBOB: name = "d2_honestbob"; break;
		default: return input_demo_recorder_input_fail("unsupported all-weapons grant", error, error_size);
	}
	const ordered_json event = { { "kind", "direct_command" }, { "command", "grant_all_weapons" }, { "grant", name } };
	return input_demo_recorder_stage_direct_command_event(event, error, error_size);
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
	if (g_input_demo_recorder_session.control_frames.size() != g_input_demo_recorder_session.frame_lines.size())
		return input_demo_recorder_copy_error("input demo recorder frame streams are out of sync", error, error_size);
	// One final source check catches removals and external changes; loader events
	// retain a sticky unknown even when a changed source later returns to its start bytes
	if (asset_provider) {
		const auto actual = ordered_json::parse(asset_provider());
		const auto expected = ordered_json::parse(g_input_demo_recorder_session.asset_fingerprints);
		if (actual["status"] != "complete" || actual["mode"] != expected["mode"] || actual["files"].size() != expected["files"].size())
			input_demo_recorder_invalidate_assets("asset sources changed or became unknown during recording");
		else
			for (const auto &entry : actual["files"])
				input_demo_recorder_note_asset(entry["role"].get<std::string>().c_str(), entry["sha256"].get<std::string>().c_str(), entry["size"].get<uint64_t>());
	}
	if (!input_demo_recorder_build_envelope(&demo, g_input_demo_recorder_session, result, &shared_error))
		return input_demo_recorder_copy_error(shared_error, error, error_size);
	if (!input_demo_file_write_recorded(demo_path, demo, g_input_demo_recorder_session.frame_lines, &shared_error))
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
