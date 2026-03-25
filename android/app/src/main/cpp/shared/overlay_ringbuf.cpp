/*
 * overlay_ringbuf.cpp -- Ring buffer for overlay popup messages.
 *
 * Follows the same pattern as console_ringbuf.cpp but adds a "type" field
 * per entry (level/track/jukebox) so tests can filter by overlay kind.
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
#include "overlay_ringbuf.h"
}

#define OVERLAY_MAX_ENTRIES 32
#define OVERLAY_TEXT_LEN    128
#define OVERLAY_TYPE_LEN    16

struct overlay_entry {
	char type[OVERLAY_TYPE_LEN];
	char text[OVERLAY_TEXT_LEN];
	uint64_t seq;
};

static overlay_entry g_ring[OVERLAY_MAX_ENTRIES];
static uint64_t g_seq = 0;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

extern "C" void overlay_ringbuf_add(const char *type, const char *text)
{
	if (!type || !text)
		return;

	pthread_mutex_lock(&g_mutex);

	uint64_t s = g_seq++;
	int idx = (int) (s % OVERLAY_MAX_ENTRIES);

	strncpy(g_ring[idx].type, type, OVERLAY_TYPE_LEN - 1);
	g_ring[idx].type[OVERLAY_TYPE_LEN - 1] = '\0';

	strncpy(g_ring[idx].text, text, OVERLAY_TEXT_LEN - 1);
	g_ring[idx].text[OVERLAY_TEXT_LEN - 1] = '\0';

	g_ring[idx].seq = s;

	pthread_mutex_unlock(&g_mutex);
}

static int json_escape(char *dst, size_t dst_size, const char *src)
{
	int pos = 0;
	for (const char *p = src; *p && pos < (int) dst_size - 6; p++) {
		switch (*p) {
			case '"':
				dst[pos++] = '\\';
				dst[pos++] = '"';
				break;
			case '\\':
				dst[pos++] = '\\';
				dst[pos++] = '\\';
				break;
			case '\n':
				dst[pos++] = '\\';
				dst[pos++] = 'n';
				break;
			case '\r':
				dst[pos++] = '\\';
				dst[pos++] = 'r';
				break;
			case '\t':
				dst[pos++] = '\\';
				dst[pos++] = 't';
				break;
			default:
				if ((unsigned char) *p >= 0x20)
					dst[pos++] = *p;
				break;
		}
	}
	dst[pos] = '\0';
	return pos;
}

extern "C" char *overlay_ringbuf_get_json(uint64_t since_seq, int max_lines)
{
	if (max_lines <= 0)
		max_lines = OVERLAY_MAX_ENTRIES;
	if (max_lines > OVERLAY_MAX_ENTRIES)
		max_lines = OVERLAY_MAX_ENTRIES;

	pthread_mutex_lock(&g_mutex);

	uint64_t cur_seq = g_seq;

	if (since_seq == 0 && cur_seq > (uint64_t) max_lines)
		since_seq = cur_seq - (uint64_t) max_lines;

	if (cur_seq > OVERLAY_MAX_ENTRIES && since_seq < cur_seq - OVERLAY_MAX_ENTRIES)
		since_seq = cur_seq - OVERLAY_MAX_ENTRIES;

	struct {
		uint64_t seq;
		char type[OVERLAY_TYPE_LEN];
		char text[OVERLAY_TEXT_LEN];
	} collected[OVERLAY_MAX_ENTRIES];
	int count = 0;

	for (uint64_t s = since_seq; s < cur_seq && count < max_lines; s++) {
		int idx = (int) (s % OVERLAY_MAX_ENTRIES);
		if (g_ring[idx].seq == s) {
			collected[count].seq = s;
			memcpy(collected[count].type, g_ring[idx].type, OVERLAY_TYPE_LEN);
			memcpy(collected[count].text, g_ring[idx].text, OVERLAY_TEXT_LEN);
			count++;
		}
	}

	pthread_mutex_unlock(&g_mutex);

	size_t buf_size = 64 + (size_t) count * (OVERLAY_TEXT_LEN * 2 + OVERLAY_TYPE_LEN + 60);
	char *buf = (char *) malloc(buf_size);
	if (!buf)
		return NULL;

	int pos = snprintf(buf, buf_size, "{\"next_seq\":%" PRIu64 ",\"lines\":[", cur_seq);

	char escaped[OVERLAY_TEXT_LEN * 2];
	for (int i = 0; i < count; i++) {
		if (i > 0)
			buf[pos++] = ',';

		json_escape(escaped, sizeof(escaped), collected[i].type);
		pos += snprintf(buf + pos, buf_size - (size_t) pos,
		                "{\"seq\":%" PRIu64 ",\"type\":\"%s\",\"text\":\"",
		                collected[i].seq, escaped);

		pos += json_escape(buf + pos, buf_size - (size_t) pos, collected[i].text);

		pos += snprintf(buf + pos, buf_size - (size_t) pos, "\"}");
	}

	snprintf(buf + pos, buf_size - (size_t) pos, "]}");
	return buf;
}

#endif /* INTROSPECT_ON */
