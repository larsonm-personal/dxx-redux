/*
 * console_ringbuf.cpp -- Thread-safe ring buffer for capturing con_printf output.
 *
 * Provides a fixed-size circular buffer that captures engine console output
 * and exposes it as JSON for the introspection API. This gives AI tools and
 * test scripts access to recent engine logs without relying on logcat.
 *
 * Guarded by INTROSPECT_ON -- only compiled into debug Android builds.
 */

#ifdef INTROSPECT_ON

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <inttypes.h>

extern "C" {
#include "console_ringbuf.h"
}

#define RINGBUF_MAX_LINES 512
#define RINGBUF_LINE_LEN  256

struct ringbuf_line {
	char text[RINGBUF_LINE_LEN];
	uint64_t seq;
};

static ringbuf_line g_ring[RINGBUF_MAX_LINES];
static uint64_t g_seq = 0;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

extern "C" void console_ringbuf_add(const char *line)
{
	if (!line)
		return;

	pthread_mutex_lock(&g_mutex);

	uint64_t s = g_seq++;
	int idx = (int) (s % RINGBUF_MAX_LINES);

	/* Copy and truncate to fit, strip trailing newline */
	strncpy(g_ring[idx].text, line, RINGBUF_LINE_LEN - 1);
	g_ring[idx].text[RINGBUF_LINE_LEN - 1] = '\0';

	size_t len = strlen(g_ring[idx].text);
	while (len > 0 && (g_ring[idx].text[len - 1] == '\n' ||
	                   g_ring[idx].text[len - 1] == '\r')) {
		g_ring[idx].text[--len] = '\0';
	}

	g_ring[idx].seq = s;

	pthread_mutex_unlock(&g_mutex);
}

/*
 * Build a JSON string with lines since `since_seq`.
 * If since_seq == 0, returns the last `max_lines` lines (default 50).
 * Caller must free() the result.
 */
extern "C" char *console_ringbuf_get_json(uint64_t since_seq, int max_lines)
{
	if (max_lines <= 0)
		max_lines = 50;
	if (max_lines > RINGBUF_MAX_LINES)
		max_lines = RINGBUF_MAX_LINES;

	pthread_mutex_lock(&g_mutex);

	uint64_t cur_seq = g_seq;

	/* If since_seq == 0, start from (cur_seq - max_lines) */
	if (since_seq == 0 && cur_seq > (uint64_t) max_lines)
		since_seq = cur_seq - (uint64_t) max_lines;

	/* Clamp: can't go further back than the ring buffer holds */
	if (cur_seq > RINGBUF_MAX_LINES && since_seq < cur_seq - RINGBUF_MAX_LINES)
		since_seq = cur_seq - RINGBUF_MAX_LINES;

	/* Collect matching lines into a temporary array */
	struct {
		uint64_t seq;
		char text[RINGBUF_LINE_LEN];
	} collected[RINGBUF_MAX_LINES];
	int count = 0;

	for (uint64_t s = since_seq; s < cur_seq && count < max_lines; s++) {
		int idx = (int) (s % RINGBUF_MAX_LINES);
		if (g_ring[idx].seq == s) {
			collected[count].seq = s;
			memcpy(collected[count].text, g_ring[idx].text, RINGBUF_LINE_LEN);
			count++;
		}
	}

	pthread_mutex_unlock(&g_mutex);

	/* Build JSON string manually (no nlohmann dependency here).
	 * Worst case per line: ~300 bytes for seq + escaped text.
	 * Header/footer: ~30 bytes. */
	size_t buf_size = 64 + (size_t) count * (RINGBUF_LINE_LEN * 2 + 40);
	char *buf = (char *) malloc(buf_size);
	if (!buf)
		return NULL;

	int pos = snprintf(buf, buf_size, "{\"next_seq\":%" PRIu64 ",\"lines\":[", cur_seq);

	for (int i = 0; i < count; i++) {
		if (i > 0)
			buf[pos++] = ',';

		pos += snprintf(buf + pos, buf_size - (size_t) pos,
		                "{\"seq\":%" PRIu64 ",\"text\":\"", collected[i].seq);

		/* JSON-escape the text */
		for (const char *p = collected[i].text; *p && pos < (int) buf_size - 10; p++) {
			switch (*p) {
				case '"':
					buf[pos++] = '\\';
					buf[pos++] = '"';
					break;
				case '\\':
					buf[pos++] = '\\';
					buf[pos++] = '\\';
					break;
				case '\n':
					buf[pos++] = '\\';
					buf[pos++] = 'n';
					break;
				case '\r':
					buf[pos++] = '\\';
					buf[pos++] = 'r';
					break;
				case '\t':
					buf[pos++] = '\\';
					buf[pos++] = 't';
					break;
				default:
					if ((unsigned char) *p >= 0x20)
						buf[pos++] = *p;
					/* skip other control chars */
					break;
			}
		}

		pos += snprintf(buf + pos, buf_size - (size_t) pos, "\"}");
	}

	snprintf(buf + pos, buf_size - (size_t) pos, "]}");
	return buf;
}

#endif /* INTROSPECT_ON */
