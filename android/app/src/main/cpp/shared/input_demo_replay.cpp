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
	int64_t final_game_time64;
	uint32_t next_frame_index;
	std::vector<input_demo_replay_frame> frames;

	input_demo_replay_session()
	    : loaded(false), game(0), has_expected_result(false), level(0), difficulty(0), has_player_cfg(false), has_checkpoint(false),
	      checkpoint_start_gt(0), final_game_time64(0), next_frame_index(0)
	{
		input_demo_result_clear(&expected_result);
		input_demo_player_cfg_clear(&player_cfg);
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

static int base64_value(unsigned char value)
{
	if (value >= 'A' && value <= 'Z')
		return value - 'A';
	if (value >= 'a' && value <= 'z')
		return value - 'a' + 26;
	if (value >= '0' && value <= '9')
		return value - '0' + 52;
	if (value == '+')
		return 62;
	if (value == '/')
		return 63;
	return -1;
}

static bool base64_decode(const std::string &encoded,
                          std::vector<uint8_t> *decoded,
                          std::string *error)
{
	size_t i;

	if (!decoded)
		return fail(error, "missing checkpoint decode output");
	if (encoded.size() % 4)
		return fail(error, "checkpoint base64 length must be a multiple of 4");
	decoded->clear();
	decoded->reserve((encoded.size() / 4) * 3);
	for (i = 0; i != encoded.size(); i += 4) {
		const unsigned char c0 = (unsigned char) encoded[i];
		const unsigned char c1 = (unsigned char) encoded[i + 1];
		const unsigned char c2 = (unsigned char) encoded[i + 2];
		const unsigned char c3 = (unsigned char) encoded[i + 3];
		const int v0 = base64_value(c0);
		const int v1 = base64_value(c1);
		const int v2 = c2 == '=' ? 0 : base64_value(c2);
		const int v3 = c3 == '=' ? 0 : base64_value(c3);
		const unsigned int word = ((unsigned int) v0 << 18) | ((unsigned int) v1 << 12) |
		                          ((unsigned int) v2 << 6) | (unsigned int) v3;

		if (v0 < 0 || v1 < 0 || (c2 != '=' && v2 < 0) || (c3 != '=' && v3 < 0))
			return fail(error, "checkpoint base64 contains invalid characters");
		if (c2 == '=' && c3 != '=')
			return fail(error, "checkpoint base64 has invalid padding");
		decoded->push_back((uint8_t) ((word >> 16) & 255));
		if (c2 != '=')
			decoded->push_back((uint8_t) ((word >> 8) & 255));
		if (c3 != '=')
			decoded->push_back((uint8_t) (word & 255));
	}
	return true;
}

static uint32_t sha256_rotr(uint32_t value, unsigned int bits)
{
	return (value >> bits) | (value << (32 - bits));
}

static void sha256_transform(uint32_t state[8], const unsigned char block[64])
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
		const uint32_t s0 = sha256_rotr(w[i - 15], 7) ^ sha256_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
		const uint32_t s1 = sha256_rotr(w[i - 2], 17) ^ sha256_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);

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
		const uint32_t s1 = sha256_rotr(e, 6) ^ sha256_rotr(e, 11) ^ sha256_rotr(e, 25);
		const uint32_t ch = (e & f) ^ (~e & g);
		const uint32_t temp1 = h + s1 + ch + k[i] + w[i];
		const uint32_t s0 = sha256_rotr(a, 2) ^ sha256_rotr(a, 13) ^ sha256_rotr(a, 22);
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

static void sha256_hex(const uint8_t *data, size_t data_size, std::string *hex)
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
		sha256_transform(state, data + i * 64);
	if (tail_size)
		memcpy(final_block, data + full_blocks * 64, tail_size);
	final_block[tail_size] = 0x80;
	if (tail_size >= 56)
		sha256_transform(state, final_block);
	final_block[(tail_size >= 56) ? 64 + 56 : 56] = (unsigned char) (bit_length >> 56);
	final_block[(tail_size >= 56) ? 64 + 57 : 57] = (unsigned char) (bit_length >> 48);
	final_block[(tail_size >= 56) ? 64 + 58 : 58] = (unsigned char) (bit_length >> 40);
	final_block[(tail_size >= 56) ? 64 + 59 : 59] = (unsigned char) (bit_length >> 32);
	final_block[(tail_size >= 56) ? 64 + 60 : 60] = (unsigned char) (bit_length >> 24);
	final_block[(tail_size >= 56) ? 64 + 61 : 61] = (unsigned char) (bit_length >> 16);
	final_block[(tail_size >= 56) ? 64 + 62 : 62] = (unsigned char) (bit_length >> 8);
	final_block[(tail_size >= 56) ? 64 + 63 : 63] = (unsigned char) bit_length;
	sha256_transform(state, final_block + ((tail_size >= 56) ? 64 : 0));
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

static bool load_checkpoint(const input_demo_checkpoint &checkpoint,
                            input_demo_replay_session *session,
                            std::string *error)
{
	std::vector<uint8_t> decoded;
	std::string actual_sha256;

	if (!session)
		return fail(error, "missing replay session output");
	if (!base64_decode(checkpoint.data, &decoded, error))
		return false;
	if (decoded.size() != checkpoint.size)
		return fail(error, "checkpoint decoded size does not match metadata");
	sha256_hex(decoded.data(), decoded.size(), &actual_sha256);
	if (actual_sha256 != checkpoint.sha256)
		return fail(error, "checkpoint sha256 does not match metadata");
	session->has_checkpoint = true;
	session->checkpoint_save_name = checkpoint.save_name;
	session->checkpoint_data = decoded;
	session->checkpoint_start_gt = checkpoint.start_gt;
	return true;
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
	g_input_demo_replay_session.frames = frames;
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
	return g_input_demo_replay_session.final_game_time64;
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
	if (g_input_demo_replay_session.has_checkpoint && result->has_game_time64) {
		if (result->game_time64 < g_input_demo_replay_session.checkpoint_start_gt)
			return copy_error("input demo replay checkpoint start_gt exceeds expected gt", error, error_size);
		result->game_time64 -= g_input_demo_replay_session.checkpoint_start_gt;
	}
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