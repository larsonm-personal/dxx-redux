/*
 * console_ringbuf.h -- Thread-safe ring buffer for capturing con_printf output.
 *
 * Guarded by INTROSPECT_ON -- only compiled into debug Android builds.
 * Zero performance impact in release builds (all calls compile to nothing).
 */

#ifndef CONSOLE_RINGBUF_H
#define CONSOLE_RINGBUF_H

#ifdef INTROSPECT_ON

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Add a line to the ring buffer (called from con_printf in d1/d2). */
void console_ringbuf_add(const char *line);

/*
 * Return a JSON string containing lines since `since_seq`.
 * If since_seq == 0, returns the last `max_lines` lines.
 * Caller must free() the returned string.
 *
 * Format: {"next_seq": N, "lines": [{"seq": M, "text": "..."}]}
 */
char *console_ringbuf_get_json(uint64_t since_seq, int max_lines);

#ifdef __cplusplus
}
#endif

#endif /* INTROSPECT_ON */
#endif /* CONSOLE_RINGBUF_H */
