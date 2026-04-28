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
	bool has_checkpoint;
	std::string checkpoint_save_name;
	std::vector<unsigned char> checkpoint_data;
	int64_t checkpoint_start_gt;
	bool has_checkpoint_next_laser_fire_delta;
	int32_t checkpoint_next_laser_fire_delta;
	bool has_checkpoint_next_missile_fire_delta;
	int32_t checkpoint_next_missile_fire_delta;
	bool has_checkpoint_last_laser_fired_delta;
	int32_t checkpoint_last_laser_fired_delta;
	bool has_checkpoint_auto_fire_fusion_delta;
	int32_t checkpoint_auto_fire_fusion_delta;
	std::vector<input_demo_control_frame> control_frames;
	std::vector<input_demo_rng_frame> rng_frames;

	input_demo_recorder_session()
	    : active(false), game(0), level(0), difficulty(0), has_checkpoint(false), checkpoint_start_gt(0),
	      has_checkpoint_next_laser_fire_delta(false), checkpoint_next_laser_fire_delta(0),
	      has_checkpoint_next_missile_fire_delta(false), checkpoint_next_missile_fire_delta(0),
	      has_checkpoint_last_laser_fired_delta(false), checkpoint_last_laser_fired_delta(0),
	      has_checkpoint_auto_fire_fusion_delta(false), checkpoint_auto_fire_fusion_delta(0)
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

static int input_demo_recorder_settings_have_checkpoint(const input_demo_recorder_settings *settings)
{
	return settings && (settings->checkpoint_data != NULL || settings->checkpoint_size != 0 ||
	                    (settings->checkpoint_save_name && settings->checkpoint_save_name[0]) ||
	                    settings->has_checkpoint_start_gt ||
	                    settings->has_checkpoint_next_laser_fire_delta ||
	                    settings->has_checkpoint_next_missile_fire_delta ||
	                    settings->has_checkpoint_last_laser_fired_delta ||
	                    settings->has_checkpoint_auto_fire_fusion_delta);
}

static void input_demo_recorder_base64_encode(const unsigned char *data,
                                              size_t data_size,
                                              std::string *encoded)
{
	static const char alphabet[] =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t i;

	encoded->clear();
	encoded->reserve(((data_size + 2) / 3) * 4);
	for (i = 0; i < data_size; i += 3) {
		const unsigned int b0 = data[i];
		const unsigned int b1 = i + 1 < data_size ? data[i + 1] : 0;
		const unsigned int b2 = i + 2 < data_size ? data[i + 2] : 0;
		const unsigned int word = (b0 << 16) | (b1 << 8) | b2;

		encoded->push_back(alphabet[(word >> 18) & 63]);
		encoded->push_back(alphabet[(word >> 12) & 63]);
		encoded->push_back(i + 1 < data_size ? alphabet[(word >> 6) & 63] : '=');
		encoded->push_back(i + 2 < data_size ? alphabet[word & 63] : '=');
	}
}

static uint32_t input_demo_recorder_sha256_rotr(uint32_t value, unsigned int bits)
{
	return (value >> bits) | (value << (32 - bits));
}

static void input_demo_recorder_sha256_transform(uint32_t state[8], const unsigned char block[64])
{
	static const uint32_t k[64] = {
		0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
		0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
		0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
		0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
		0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
		0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
		0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
		0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
	};
	uint32_t w[64];
	uint32_t a, b, c, d, e, f, g, h;
	int i;

	for (i = 0; i != 16; ++i)
		w[i] = ((uint32_t) block[i * 4] << 24) | ((uint32_t) block[i * 4 + 1] << 16) |
		       ((uint32_t) block[i * 4 + 2] << 8) | (uint32_t) block[i * 4 + 3];
	for (; i != 64; ++i) {
		const uint32_t s0 = input_demo_recorder_sha256_rotr(w[i - 15], 7) ^
		                    input_demo_recorder_sha256_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
		const uint32_t s1 = input_demo_recorder_sha256_rotr(w[i - 2], 17) ^
		                    input_demo_recorder_sha256_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);

		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];
	f = state[5];
	g = state[6];
	h = state[7];
	for (i = 0; i != 64; ++i) {
		const uint32_t s1 = input_demo_recorder_sha256_rotr(e, 6) ^ input_demo_recorder_sha256_rotr(e, 11) ^
		                    input_demo_recorder_sha256_rotr(e, 25);
		const uint32_t ch = (e & f) ^ (~e & g);
		const uint32_t temp1 = h + s1 + ch + k[i] + w[i];
		const uint32_t s0 = input_demo_recorder_sha256_rotr(a, 2) ^ input_demo_recorder_sha256_rotr(a, 13) ^
		                    input_demo_recorder_sha256_rotr(a, 22);
		const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
		const uint32_t temp2 = s0 + maj;

		h = g;
		g = f;
		f = e;
		e = d + temp1;
		d = c;
		c = b;
		b = a;
		a = temp1 + temp2;
	}
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	state[5] += f;
	state[6] += g;
	state[7] += h;
}

static void input_demo_recorder_sha256_hex(const unsigned char *data,
                                           size_t data_size,
                                           std::string *hex)
{
	static const char digits[] = "0123456789abcdef";
	uint32_t state[8] = {
		0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
		0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
	};
	unsigned char final_block[128] = { 0 };
	unsigned char digest[32];
	const uint64_t bit_length = (uint64_t) data_size * 8u;
	const size_t full_blocks = data_size / 64;
	const size_t tail_size = data_size % 64;
	size_t i;

	for (i = 0; i != full_blocks; ++i)
		input_demo_recorder_sha256_transform(state, data + i * 64);
	if (tail_size)
		memcpy(final_block, data + full_blocks * 64, tail_size);
	final_block[tail_size] = 0x80;
	if (tail_size >= 56)
		input_demo_recorder_sha256_transform(state, final_block);
	final_block[(tail_size >= 56) ? 64 + 56 : 56] = (unsigned char) (bit_length >> 56);
	final_block[(tail_size >= 56) ? 64 + 57 : 57] = (unsigned char) (bit_length >> 48);
	final_block[(tail_size >= 56) ? 64 + 58 : 58] = (unsigned char) (bit_length >> 40);
	final_block[(tail_size >= 56) ? 64 + 59 : 59] = (unsigned char) (bit_length >> 32);
	final_block[(tail_size >= 56) ? 64 + 60 : 60] = (unsigned char) (bit_length >> 24);
	final_block[(tail_size >= 56) ? 64 + 61 : 61] = (unsigned char) (bit_length >> 16);
	final_block[(tail_size >= 56) ? 64 + 62 : 62] = (unsigned char) (bit_length >> 8);
	final_block[(tail_size >= 56) ? 64 + 63 : 63] = (unsigned char) bit_length;
	input_demo_recorder_sha256_transform(state, final_block + ((tail_size >= 56) ? 64 : 0));
	for (i = 0; i != 8; ++i) {
		digest[i * 4] = (unsigned char) (state[i] >> 24);
		digest[i * 4 + 1] = (unsigned char) (state[i] >> 16);
		digest[i * 4 + 2] = (unsigned char) (state[i] >> 8);
		digest[i * 4 + 3] = (unsigned char) state[i];
	}
	hex->resize(64);
	for (i = 0; i != 32; ++i) {
		(*hex)[i * 2] = digits[digest[i] >> 4];
		(*hex)[i * 2 + 1] = digits[digest[i] & 15];
	}
}

static bool input_demo_recorder_build_checkpoint(input_demo_checkpoint *checkpoint,
                                                 const input_demo_recorder_session &session,
                                                 std::string *error)
{
	if (!checkpoint)
		return input_demo_recorder_fail(error, "missing checkpoint output");
	if (session.checkpoint_save_name.empty())
		return input_demo_recorder_fail(error, "missing checkpoint save name");
	if (session.checkpoint_data.empty())
		return input_demo_recorder_fail(error, "missing checkpoint data");
	checkpoint->format = "dgss";
	checkpoint->encoding = "base64";
	checkpoint->size = (uint32_t) session.checkpoint_data.size();
	input_demo_recorder_sha256_hex(session.checkpoint_data.data(), session.checkpoint_data.size(), &checkpoint->sha256);
	checkpoint->save_name = session.checkpoint_save_name;
	checkpoint->has_start_gt = 1;
	checkpoint->start_gt = session.checkpoint_start_gt;
	if (session.has_checkpoint_next_laser_fire_delta) {
		checkpoint->has_next_laser_fire_delta = 1;
		checkpoint->next_laser_fire_delta = session.checkpoint_next_laser_fire_delta;
	}
	if (session.has_checkpoint_next_missile_fire_delta) {
		checkpoint->has_next_missile_fire_delta = 1;
		checkpoint->next_missile_fire_delta = session.checkpoint_next_missile_fire_delta;
	}
	if (session.has_checkpoint_last_laser_fired_delta) {
		checkpoint->has_last_laser_fired_delta = 1;
		checkpoint->last_laser_fired_delta = session.checkpoint_last_laser_fired_delta;
	}
	if (session.has_checkpoint_auto_fire_fusion_delta) {
		checkpoint->has_auto_fire_fusion_delta = 1;
		checkpoint->auto_fire_fusion_delta = session.checkpoint_auto_fire_fusion_delta;
	}
	input_demo_recorder_base64_encode(session.checkpoint_data.data(), session.checkpoint_data.size(), &checkpoint->data);
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
	demo->metadata.start_mode = session.has_checkpoint ? "save_checkpoint" : "new_level";
	demo->metadata.rng_mode = session.rng_mode;
	demo->metadata.frame_count = static_cast<uint32_t>(session.control_frames.size());
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
	settings->checkpoint_save_name = NULL;
	settings->checkpoint_data = NULL;
	settings->checkpoint_size = 0;
	settings->has_checkpoint_start_gt = 0;
	settings->checkpoint_start_gt = 0;
	settings->has_checkpoint_next_laser_fire_delta = 0;
	settings->checkpoint_next_laser_fire_delta = 0;
	settings->has_checkpoint_next_missile_fire_delta = 0;
	settings->checkpoint_next_missile_fire_delta = 0;
	settings->has_checkpoint_last_laser_fired_delta = 0;
	settings->checkpoint_last_laser_fired_delta = 0;
	settings->has_checkpoint_auto_fire_fusion_delta = 0;
	settings->checkpoint_auto_fire_fusion_delta = 0;
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
	g_input_demo_recorder_session.active = true;
	g_input_demo_recorder_session.game = settings->game;
	g_input_demo_recorder_session.mission = settings->mission;
	g_input_demo_recorder_session.level = settings->level;
	g_input_demo_recorder_session.difficulty = settings->difficulty;
	g_input_demo_recorder_session.rng_mode = settings->rng_mode;
	g_input_demo_recorder_session.has_checkpoint = have_checkpoint ? true : false;
	if (have_checkpoint) {
		g_input_demo_recorder_session.checkpoint_save_name = settings->checkpoint_save_name;
		g_input_demo_recorder_session.checkpoint_data.assign(settings->checkpoint_data,
		                                                     settings->checkpoint_data + settings->checkpoint_size);
		g_input_demo_recorder_session.checkpoint_start_gt = settings->checkpoint_start_gt;
		g_input_demo_recorder_session.has_checkpoint_next_laser_fire_delta =
		    settings->has_checkpoint_next_laser_fire_delta ? true : false;
		g_input_demo_recorder_session.checkpoint_next_laser_fire_delta = settings->checkpoint_next_laser_fire_delta;
		g_input_demo_recorder_session.has_checkpoint_next_missile_fire_delta =
		    settings->has_checkpoint_next_missile_fire_delta ? true : false;
		g_input_demo_recorder_session.checkpoint_next_missile_fire_delta = settings->checkpoint_next_missile_fire_delta;
		g_input_demo_recorder_session.has_checkpoint_last_laser_fired_delta =
		    settings->has_checkpoint_last_laser_fired_delta ? true : false;
		g_input_demo_recorder_session.checkpoint_last_laser_fired_delta = settings->checkpoint_last_laser_fired_delta;
		g_input_demo_recorder_session.has_checkpoint_auto_fire_fusion_delta =
		    settings->has_checkpoint_auto_fire_fusion_delta ? true : false;
		g_input_demo_recorder_session.checkpoint_auto_fire_fusion_delta = settings->checkpoint_auto_fire_fusion_delta;
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
	input_demo_recorder_reset_session();
	return 1;
}

int input_demo_recorder_flush(const char *demo_path,
                              char *error, size_t error_size)
{
	return input_demo_recorder_flush_with_result(demo_path, NULL, error, error_size);
}
}