#ifndef STATE_CHECKPOINT_H
#define STATE_CHECKPOINT_H

#include "rewind_file.h"

#ifdef __cplusplus
extern "C" {
#endif

struct guidebot_save_stream;
typedef void (*state_checkpoint_callback)(uint64_t token, int ok,
                                          const rewind_memory_buffer *buffer);

/* Android game-thread API: callbacks run only from poll, never on the worker */
int state_checkpoint_initialize(void);
int state_checkpoint_submit(const char *description, int save_kind, uint64_t token,
                            state_checkpoint_callback callback);
typedef struct state_checkpoint_attachment {
	const char *filename;
	const char *text;
	/* -1 replaces text; nonnegative merges an autosave entry into a slot history */
	int history_slot;
} state_checkpoint_attachment;
int state_checkpoint_submit_disk(const char *description, int save_kind, const char *filename,
                                 const state_checkpoint_attachment *attachments, unsigned attachment_count,
                                 uint64_t token, state_checkpoint_callback callback);
/* Refresh at explicit secret changes/context boundaries, never during capture */
int state_checkpoint_refresh_companion(const char *source);
int state_checkpoint_submit_slot(const char *description, int save_kind, int slot,
                                 uint64_t token, state_checkpoint_callback callback);
int state_checkpoint_writes_file(void);
/* Game-thread campaign mutation boundary; consumes malloc-owned bytes even on failure */
int state_checkpoint_cache_campaign(unsigned char *data, size_t size);
void state_checkpoint_get_campaign(const unsigned char **data, size_t *size);
/* Returns 1 when deferred, 0 for synchronous saves, -1 on capture failure */
int state_checkpoint_defer_campaign(rewind_file *file, const unsigned char *data, size_t size, uint32_t checksum);
void state_checkpoint_poll(void);
/* Only explicit synchronous saves/shutdown may wait for outstanding jobs */
void state_checkpoint_drain(void);
int state_checkpoint_defer_metadata(struct guidebot_save_stream *stream);

typedef struct state_checkpoint_stats {
	unsigned submitted, completed, failed, deferred, pending;
	int64_t capture_us, max_capture_us, worker_us;
	unsigned campaign_generations;
	size_t campaign_bytes;
} state_checkpoint_stats;
void state_checkpoint_get_stats(state_checkpoint_stats *stats);

#ifdef __cplusplus
}
#endif
#endif
