#ifndef MULTI_SAVE_TRANSFER_POLICY_H
#define MULTI_SAVE_TRANSFER_POLICY_H

#include <stdint.h>
#include "coop/coop_campaign.h"

/* One campaign archive plus its detached/current world and envelope metadata */
#define MULTI_SAVE_TRANSFER_MAX_BYTES (COOP_CAMPAIGN_MAX_BYTES + COOP_CAMPAIGN_MAX_WORLD_BYTES + 64u * 1024u)
#define MULTI_SAVE_TRANSFER_IDLE_MS   60000u

enum {
	MULTI_SAVE_TRANSFER_KIND_REWIND,
	MULTI_SAVE_TRANSFER_KIND_RESTORE,
	MULTI_SAVE_TRANSFER_KIND_LEVEL_RESTART,
	MULTI_SAVE_TRANSFER_KIND_WORLD,
	MULTI_SAVE_TRANSFER_KIND_FRESH_WORLD,
	MULTI_SAVE_TRANSFER_KIND_CAMPAIGN,
	MULTI_SAVE_TRANSFER_KIND_CHECKPOINT,
	MULTI_SAVE_TRANSFER_KIND_ROLLBACK
};

/* Allow a full burst at two rendered frames per second, plus bounded buffer
 * allocation/application waits. Actual progress also has a separate idle limit */
static inline uint64_t multi_save_transfer_limit_ms(unsigned total_chunks)
{
	return 120000u + ((uint64_t) total_chunks * 1000u + 127u) / 128u;
}

static inline int multi_save_transfer_expired(uint64_t now, uint64_t started,
                                              uint64_t progress, unsigned total_chunks)
{
	return (now >= started && now - started >= multi_save_transfer_limit_ms(total_chunks)) ||
	       (now >= progress && now - progress >= MULTI_SAVE_TRANSFER_IDLE_MS);
}

/* Preserve the previous eight chunks at 60 fps throughput, without starving
 * transfer progress on a slow renderer or flooding after a long stall */
enum {
	MULTI_SAVE_TRANSFER_CHUNKS_SECOND = 480,
	MULTI_SAVE_TRANSFER_CHUNKS_BURST = 64,
	MULTI_SAVE_TRANSFER_RELIABLE_WINDOW = 128
};

/* Bound unacknowledged traffic as well as wall-clock send rate. Rollback
 * sends a separate reliable packet to each required recipient */
static inline unsigned multi_save_transfer_window_chunks(unsigned pending, unsigned copies, unsigned remaining)
{
	unsigned available;
	if (!copies || pending >= MULTI_SAVE_TRANSFER_RELIABLE_WINDOW) return 0;
	available = (MULTI_SAVE_TRANSFER_RELIABLE_WINDOW - pending) / copies;
	return available < remaining ? available : remaining;
}

typedef struct multi_save_transfer_budget {
	uint64_t last_ms;
	unsigned credit_milli;
	int started;
} multi_save_transfer_budget;

static inline unsigned multi_save_transfer_chunk_budget(
    multi_save_transfer_budget *budget, uint64_t now_ms, unsigned remaining)
{
	unsigned available;
	if (!remaining) return 0;
	if (!budget->started) {
		budget->started = 1;
		budget->last_ms = now_ms;
		budget->credit_milli = 8000;
	} else if (now_ms > budget->last_ms) {
		uint64_t elapsed = now_ms - budget->last_ms;
		unsigned credit = elapsed >= 1000 ? MULTI_SAVE_TRANSFER_CHUNKS_BURST * 1000
		                                  : budget->credit_milli + (unsigned) elapsed * MULTI_SAVE_TRANSFER_CHUNKS_SECOND;
		budget->last_ms = now_ms;
		budget->credit_milli = credit > MULTI_SAVE_TRANSFER_CHUNKS_BURST * 1000
		                           ? MULTI_SAVE_TRANSFER_CHUNKS_BURST * 1000
		                           : credit;
	}
	available = budget->credit_milli / 1000;
	if (available > remaining) available = remaining;
	budget->credit_milli -= available * 1000;
	return available;
}

typedef enum multi_save_transfer_host_action {
	MULTI_SAVE_TRANSFER_HOST_APPLY_NOW = 0,
	MULTI_SAVE_TRANSFER_HOST_WAIT_FOR_CLIENTS = 1
} multi_save_transfer_host_action;

typedef enum multi_save_transfer_client_apply_action {
	MULTI_SAVE_TRANSFER_CLIENT_WAIT = 0,
	MULTI_SAVE_TRANSFER_CLIENT_APPLY = 1
} multi_save_transfer_client_apply_action;

static inline multi_save_transfer_host_action
multi_save_transfer_host_action_for_rewind(int has_connected_clients)
{
	return has_connected_clients
	           ? MULTI_SAVE_TRANSFER_HOST_WAIT_FOR_CLIENTS
	           : MULTI_SAVE_TRANSFER_HOST_APPLY_NOW;
}

static inline multi_save_transfer_client_apply_action
multi_save_transfer_client_apply_action_for_context(int at_frame_boundary,
                                                    int active,
                                                    int apply_pending,
                                                    int chunks_received,
                                                    int total_chunks)
{
	return at_frame_boundary && active && apply_pending && total_chunks > 0 &&
	               chunks_received == total_chunks
	           ? MULTI_SAVE_TRANSFER_CLIENT_APPLY
	           : MULTI_SAVE_TRANSFER_CLIENT_WAIT;
}

#endif
