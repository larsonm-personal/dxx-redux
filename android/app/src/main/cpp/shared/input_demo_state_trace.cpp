#include "input_demo_state_trace.h"

#include <stdio.h>
#include <string.h>

#include "input_demo_replay.h"

namespace
{

enum {
	INPUT_DEMO_STATE_TRACE_MAX_SOURCE = 32,
	INPUT_DEMO_STATE_TRACE_MAX_GAME = 8,
	INPUT_DEMO_STATE_TRACE_MAX_MISSION = 64,
	INPUT_DEMO_STATE_TRACE_MAX_START_MODE = 32,
	INPUT_DEMO_STATE_TRACE_MAX_STATE_JSON = 8192
};

typedef struct input_demo_state_trace_session {
	int active;
	FILE *file;
	char source[INPUT_DEMO_STATE_TRACE_MAX_SOURCE];
} input_demo_state_trace_session;

static input_demo_state_trace_session g_input_demo_state_trace_session;

static int copy_error(const char *message, char *error, size_t error_size)
{
	if (error && error_size)
		snprintf(error, error_size, "%s", message ? message : "unknown error");
	return 0;
}

static void write_json_string(FILE *out, const char *text)
{
	const unsigned char *cursor = (const unsigned char *) (text ? text : "");

	fputc('"', out);
	for (; *cursor; ++cursor) {
		switch (*cursor) {
			case '\\':
				fputs("\\\\", out);
				break;
			case '"':
				fputs("\\\"", out);
				break;
			case '\b':
				fputs("\\b", out);
				break;
			case '\f':
				fputs("\\f", out);
				break;
			case '\n':
				fputs("\\n", out);
				break;
			case '\r':
				fputs("\\r", out);
				break;
			case '\t':
				fputs("\\t", out);
				break;
			default:
				if (*cursor < 0x20)
					fprintf(out, "\\u%04x", (unsigned int) *cursor);
				else
					fputc(*cursor, out);
				break;
		}
	}
	fputc('"', out);
}

static void reset_session(void)
{
	if (g_input_demo_state_trace_session.file) {
		fclose(g_input_demo_state_trace_session.file);
		g_input_demo_state_trace_session.file = NULL;
	}
	memset(&g_input_demo_state_trace_session, 0, sizeof(g_input_demo_state_trace_session));
}

static int game_name_from_id(int game, char *name, size_t name_size)
{
	if (!name || !name_size)
		return 0;
	if (game == INPUT_DEMO_GAME_D1) {
		snprintf(name, name_size, "%s", "d1");
		return 1;
	}
	if (game == INPUT_DEMO_GAME_D2) {
		snprintf(name, name_size, "%s", "d2");
		return 1;
	}
	return 0;
}

} // namespace

extern "C" {

int input_demo_state_trace_is_active(void)
{
	return g_input_demo_state_trace_session.active ? 1 : 0;
}

void input_demo_state_trace_stop(void)
{
	reset_session();
}

int input_demo_state_trace_start(const char *path,
	                             const char *source,
	                             const char *game,
	                             const char *mission,
	                             int level,
	                             int difficulty,
	                             const char *start_mode,
	                             uint32_t frame_count,
	                             char *error,
	                             size_t error_size)
{
	FILE *file;

	if (!path || !path[0])
		return copy_error("missing input demo state trace path", error, error_size);
	if (!source || !source[0])
		return copy_error("missing input demo state trace source", error, error_size);
	if (!game || !game[0])
		return copy_error("missing input demo state trace game", error, error_size);
	if (!mission || !mission[0])
		return copy_error("missing input demo state trace mission", error, error_size);
	if (!start_mode || !start_mode[0])
		return copy_error("missing input demo state trace start_mode", error, error_size);
	reset_session();
	file = fopen(path, "wb");
	if (!file)
		return copy_error("could not open input demo state trace file", error, error_size);
	g_input_demo_state_trace_session.file = file;
	g_input_demo_state_trace_session.active = 1;
	snprintf(g_input_demo_state_trace_session.source,
	         sizeof(g_input_demo_state_trace_session.source),
	         "%s",
	         source);
	fputs("{\"type\":\"meta\",\"version\":1,\"source\":", file);
	write_json_string(file, source);
	fputs(",\"game\":", file);
	write_json_string(file, game);
	fputs(",\"mission\":", file);
	write_json_string(file, mission);
	fprintf(file,
	        ",\"level\":%d,\"difficulty\":%d,\"start_mode\":",
	        level,
	        difficulty);
	write_json_string(file, start_mode);
	fprintf(file, ",\"frame_count\":%u}\n", frame_count);
	fflush(file);
	return 1;
}

int input_demo_state_trace_start_replay(const char *path,
	                                    char *error,
	                                    size_t error_size)
{
	char game[INPUT_DEMO_STATE_TRACE_MAX_GAME] = "";
	const char *mission;
	const char *start_mode;

	if (!input_demo_replay_is_loaded())
		return copy_error("input demo replay is not loaded", error, error_size);
	if (!game_name_from_id(input_demo_replay_game(), game, sizeof(game)))
		return copy_error("input demo replay game is invalid", error, error_size);
	mission = input_demo_replay_mission();
	start_mode = input_demo_replay_start_mode();
	return input_demo_state_trace_start(path,
	                                   "replay",
	                                   game,
	                                   mission,
	                                   input_demo_replay_level(),
	                                   input_demo_replay_difficulty(),
	                                   start_mode,
	                                   input_demo_replay_frame_count(),
	                                   error,
	                                   error_size);
	}

int input_demo_state_trace_write_frame(uint32_t frame,
	                                   int32_t frame_time,
	                                   uint32_t rng_state,
	                                   int has_rng_call_count,
	                                   uint32_t rng_call_count,
	                                   const input_demo_result *state,
	                                   char *error,
	                                   size_t error_size)
{
	char state_json[INPUT_DEMO_STATE_TRACE_MAX_STATE_JSON] = "";
	FILE *file;

	if (!g_input_demo_state_trace_session.active || !g_input_demo_state_trace_session.file)
		return copy_error("input demo state trace is not active", error, error_size);
	if (!state)
		return copy_error("missing input demo state trace frame state", error, error_size);
	if (!input_demo_result_snapshot_to_json_buffer(state, state_json, sizeof(state_json)))
		return copy_error("could not encode input demo state trace frame state", error, error_size);
	file = g_input_demo_state_trace_session.file;
	fputs("{\"type\":\"frame_state\",\"source\":", file);
	write_json_string(file, g_input_demo_state_trace_session.source);
	fprintf(file,
	        ",\"f\":%u,\"ft\":%d,\"rng\":{\"s\":%u",
	        frame,
	        frame_time,
	        rng_state);
	if (has_rng_call_count)
		fprintf(file, ",\"c\":%u", rng_call_count);
	fprintf(file, "},\"state\":%s}\n", state_json);
	if (fflush(file) != 0)
		return copy_error("could not flush input demo state trace file", error, error_size);
	return 1;
}

}