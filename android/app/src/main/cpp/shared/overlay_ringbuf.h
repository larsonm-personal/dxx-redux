/*
 * overlay_ringbuf.h -- Ring buffer for overlay popup messages (track/level).
 *
 * Records the last N overlay messages so they can be read asynchronously
 * via the introspection API. Avoids timing issues with trying to catch
 * transient overlay text as it appears on screen.
 *
 * Guarded by INTROSPECT_ON -- only compiled into debug Android builds.
 */

#ifndef OVERLAY_RINGBUF_H
#define OVERLAY_RINGBUF_H

#ifdef INTROSPECT_ON

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Add an overlay message. type is "level", "track", or "jukebox". */
void overlay_ringbuf_add(const char *type, const char *text);

/*
 * Return a JSON string with overlay entries since `since_seq`.
 * If since_seq == 0, returns the last `max_lines` entries.
 * Caller must free() the returned string.
 *
 * Format: {"next_seq": N, "lines": [{"seq": M, "type": "...", "text": "..."}]}
 */
char *overlay_ringbuf_get_json(uint64_t since_seq, int max_lines);

#ifdef __cplusplus
}
#endif

#else

/* No-op stubs for non-introspection builds */
static inline void overlay_ringbuf_add(const char *type, const char *text)
{
	(void) type;
	(void) text;
}

#endif /* INTROSPECT_ON */
#endif /* OVERLAY_RINGBUF_H */
