#include "input_demo_rng_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	INPUT_DEMO_RNG_TRACE_KIND_RAND = 1,
	INPUT_DEMO_RNG_TRACE_KIND_SRAND = 2
};

typedef struct input_demo_rng_trace_event {
	uint32_t frame;
	int64_t game_time64;
	uint32_t call_count;
	uint32_t state_before;
	uint32_t state_after;
	uint32_t seed;
	int32_t result;
	int32_t line;
	int32_t object_num;
	int32_t object_signature;
	int32_t object_id;
	uint8_t stream;
	uint8_t kind;
	uint8_t has_context;
	uint8_t has_object_context;
	uint8_t has_state_before;
	uint8_t has_state_after;
	const char *file;
	const char *func;
} input_demo_rng_trace_event;

typedef struct input_demo_rng_trace_session {
	int active;
	int truncated;
	int write_on_stop;
	int has_context;
	uint32_t frame;
	int64_t game_time64;
	int has_object_context;
	int32_t object_num;
	int32_t object_signature;
	int32_t object_id;
	char *output_path;
	input_demo_rng_trace_event *events;
	size_t count;
	size_t capacity;
} input_demo_rng_trace_session;

static input_demo_rng_trace_session g_input_demo_rng_trace_session;

static char *copy_string(const char *text)
{
	char *copy;
	size_t size;

	if (!text)
		text = "";
	size = strlen(text) + 1;
	copy = (char *) malloc(size);
	if (!copy)
		return NULL;
	memcpy(copy, text, size);
	return copy;
}

static int copy_error(const char *message, char *error, size_t error_size)
{
	if (error && error_size) {
		snprintf(error, error_size, "%s", message ? message : "unknown error");
	}
	return 0;
}

static int ensure_capacity(size_t needed)
{
	size_t next_capacity;
	input_demo_rng_trace_event *next_events;

	if (needed <= g_input_demo_rng_trace_session.capacity)
		return 1;
	next_capacity = g_input_demo_rng_trace_session.capacity ? g_input_demo_rng_trace_session.capacity : 256;
	while (next_capacity < needed)
		next_capacity *= 2;
	next_events = (input_demo_rng_trace_event *) realloc(g_input_demo_rng_trace_session.events,
	                                                     next_capacity * sizeof(*next_events));
	if (!next_events) {
		g_input_demo_rng_trace_session.truncated = 1;
		g_input_demo_rng_trace_session.active = 0;
		return 0;
	}
	g_input_demo_rng_trace_session.events = next_events;
	g_input_demo_rng_trace_session.capacity = next_capacity;
	return 1;
}

static void append_event(const input_demo_rng_trace_event *event)
{
	if (!g_input_demo_rng_trace_session.active || !event)
		return;
	if (!ensure_capacity(g_input_demo_rng_trace_session.count + 1))
		return;
	g_input_demo_rng_trace_session.events[g_input_demo_rng_trace_session.count++] = *event;
}

static void write_event_stream(FILE *out, uint8_t stream)
{
	if (!stream)
		return;
	fprintf(out, ",\"stream\":%u", (unsigned int) stream);
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

static void write_json_path_string(FILE *out, const char *text)
{
	const unsigned char *cursor = (const unsigned char *) (text ? text : "");

	fputc('"', out);
	for (; *cursor; ++cursor) {
		switch (*cursor) {
			case '\\':
				fputc('/', out);
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

static int write_trace_file(const char *path, char *error, size_t error_size)
{
	FILE *out;
	size_t i;

	if (!path || !path[0])
		return copy_error("missing rng trace path", error, error_size);
	out = fopen(path, "wb");
	if (!out)
		return copy_error("could not open rng trace file", error, error_size);
	fprintf(out,
	        "{\"type\":\"meta\",\"version\":1,\"events\":%llu,\"truncated\":%s}\n",
	        (unsigned long long) g_input_demo_rng_trace_session.count,
	        g_input_demo_rng_trace_session.truncated ? "true" : "false");
	for (i = 0; i < g_input_demo_rng_trace_session.count; ++i) {
		const input_demo_rng_trace_event *event = &g_input_demo_rng_trace_session.events[i];

		fprintf(out,
		        "{\"type\":\"%s\",\"seq\":%llu,\"frame\":%u,\"gt\":%lld,\"call_count\":%u",
		        event->kind == INPUT_DEMO_RNG_TRACE_KIND_SRAND ? "srand" : "rand",
		        (unsigned long long) i,
		        event->frame,
		        (long long) event->game_time64,
		        event->call_count);
		write_event_stream(out, event->stream);
		if (!event->has_context)
			fputs(",\"has_context\":false", out);
		if (event->has_object_context)
			fprintf(out,
			        ",\"ctx_obj\":%d,\"ctx_sig\":%d,\"ctx_id\":%d",
			        event->object_num,
			        event->object_signature,
			        event->object_id);
		if (event->has_state_before)
			fprintf(out, ",\"state_before\":%u", event->state_before);
		if (event->has_state_after)
			fprintf(out, ",\"state_after\":%u", event->state_after);
		if (event->kind == INPUT_DEMO_RNG_TRACE_KIND_SRAND)
			fprintf(out, ",\"seed\":%u", event->seed);
		else
			fprintf(out, ",\"result\":%d", event->result);
		fprintf(out, ",\"line\":%d,\"file\":", event->line);
		write_json_path_string(out, event->file);
		fputs(",\"func\":", out);
		write_json_string(out, event->func);
		fputs("}\n", out);
	}
	if (fclose(out) != 0)
		return copy_error("could not finish rng trace file", error, error_size);
	return 1;
}

void input_demo_rng_trace_start(void)
{
	input_demo_rng_trace_reset();
	g_input_demo_rng_trace_session.active = 1;
}

int input_demo_rng_trace_start_replay(const char *path, char *error, size_t error_size)
{
	if (!path || !path[0])
		return copy_error("missing rng trace path", error, error_size);
	input_demo_rng_trace_reset();
	g_input_demo_rng_trace_session.output_path = copy_string(path);
	if (!g_input_demo_rng_trace_session.output_path)
		return copy_error("could not allocate rng trace path", error, error_size);
	g_input_demo_rng_trace_session.active = 1;
	g_input_demo_rng_trace_session.write_on_stop = 1;
	return 1;
}

void input_demo_rng_trace_reset(void)
{
	free(g_input_demo_rng_trace_session.events);
	free(g_input_demo_rng_trace_session.output_path);
	memset(&g_input_demo_rng_trace_session, 0, sizeof(g_input_demo_rng_trace_session));
}

int input_demo_rng_trace_stop(char *error, size_t error_size)
{
	int result = 1;

	if (g_input_demo_rng_trace_session.write_on_stop)
		result = write_trace_file(g_input_demo_rng_trace_session.output_path, error, error_size);
	input_demo_rng_trace_reset();
	return result;
}

int input_demo_rng_trace_is_active(void)
{
	return g_input_demo_rng_trace_session.active ? 1 : 0;
}

void input_demo_rng_trace_set_context(uint32_t frame, int64_t game_time64)
{
	if (!g_input_demo_rng_trace_session.active)
		return;
	g_input_demo_rng_trace_session.has_context = 1;
	g_input_demo_rng_trace_session.frame = frame;
	g_input_demo_rng_trace_session.game_time64 = game_time64;
}

void input_demo_rng_trace_set_object_context(int object_num,
	int object_signature,
	int object_id)
{
	if (!g_input_demo_rng_trace_session.active)
		return;
	g_input_demo_rng_trace_session.has_object_context = 1;
	g_input_demo_rng_trace_session.object_num = object_num;
	g_input_demo_rng_trace_session.object_signature = object_signature;
	g_input_demo_rng_trace_session.object_id = object_id;
}

void input_demo_rng_trace_clear_object_context(void)
{
	g_input_demo_rng_trace_session.has_object_context = 0;
	g_input_demo_rng_trace_session.object_num = 0;
	g_input_demo_rng_trace_session.object_signature = 0;
	g_input_demo_rng_trace_session.object_id = 0;
}

void input_demo_rng_trace_record_rand(int stream,
                                      const char *file,
                                      const char *func,
                                      int line,
                                      uint32_t call_count,
                                      int has_state_before,
                                      uint32_t state_before,
                                      int has_state_after,
                                      uint32_t state_after,
                                      int result)
{
	input_demo_rng_trace_event event;

	memset(&event, 0, sizeof(event));
	event.frame = g_input_demo_rng_trace_session.frame;
	event.game_time64 = g_input_demo_rng_trace_session.game_time64;
	event.call_count = call_count;
	event.state_before = state_before;
	event.state_after = state_after;
	event.result = result;
	event.line = line;
	event.object_num = g_input_demo_rng_trace_session.object_num;
	event.object_signature = g_input_demo_rng_trace_session.object_signature;
	event.object_id = g_input_demo_rng_trace_session.object_id;
	event.stream = (uint8_t) stream;
	event.kind = INPUT_DEMO_RNG_TRACE_KIND_RAND;
	event.has_context = g_input_demo_rng_trace_session.has_context ? 1 : 0;
	event.has_object_context = g_input_demo_rng_trace_session.has_object_context ? 1 : 0;
	event.has_state_before = has_state_before ? 1 : 0;
	event.has_state_after = has_state_after ? 1 : 0;
	event.file = file ? file : "";
	event.func = func ? func : "";
	append_event(&event);
}

void input_demo_rng_trace_record_srand(int stream,
                                       const char *file,
                                       const char *func,
                                       int line,
                                       uint32_t call_count,
                                       int has_state_before,
                                       uint32_t state_before,
                                       int has_state_after,
                                       uint32_t state_after,
                                       uint32_t seed)
{
	input_demo_rng_trace_event event;

	memset(&event, 0, sizeof(event));
	event.frame = g_input_demo_rng_trace_session.frame;
	event.game_time64 = g_input_demo_rng_trace_session.game_time64;
	event.call_count = call_count;
	event.state_before = state_before;
	event.state_after = state_after;
	event.seed = seed;
	event.line = line;
	event.object_num = g_input_demo_rng_trace_session.object_num;
	event.object_signature = g_input_demo_rng_trace_session.object_signature;
	event.object_id = g_input_demo_rng_trace_session.object_id;
	event.stream = (uint8_t) stream;
	event.kind = INPUT_DEMO_RNG_TRACE_KIND_SRAND;
	event.has_context = g_input_demo_rng_trace_session.has_context ? 1 : 0;
	event.has_object_context = g_input_demo_rng_trace_session.has_object_context ? 1 : 0;
	event.has_state_before = has_state_before ? 1 : 0;
	event.has_state_after = has_state_after ? 1 : 0;
	event.file = file ? file : "";
	event.func = func ? func : "";
	append_event(&event);
}

size_t input_demo_rng_trace_event_count(void)
{
	return g_input_demo_rng_trace_session.count;
}

int input_demo_rng_trace_write_to_path(const char *path, char *error, size_t error_size)
{
	return write_trace_file(path, error, error_size);
}

int input_demo_rng_trace_write_sidecar_for_demo(const char *demo_path,
                                                char *error,
                                                size_t error_size)
{
	char *sidecar_path;
	size_t path_size;
	int result;

	if (!demo_path || !demo_path[0])
		return copy_error("missing demo path for rng trace sidecar", error, error_size);
	path_size = strlen(demo_path) + sizeof(INPUT_DEMO_RNG_TRACE_SUFFIX);
	sidecar_path = (char *) malloc(path_size);
	if (!sidecar_path)
		return copy_error("could not allocate rng trace sidecar path", error, error_size);
	snprintf(sidecar_path, path_size, "%s%s", demo_path, INPUT_DEMO_RNG_TRACE_SUFFIX);
	result = input_demo_rng_trace_write_to_path(sidecar_path, error, error_size);
	free(sidecar_path);
	return result;
}