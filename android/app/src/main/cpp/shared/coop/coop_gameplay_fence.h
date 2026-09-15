#ifndef COOP_GAMEPLAY_FENCE_H
#define COOP_GAMEPLAY_FENCE_H

#include <stddef.h>
#include <stdint.h>
#include "multi.h"

/* A transport-authenticated session owns monotonically increasing world visits.
 * Pin these bytes when payloads are queued, including for relayed/retried data.
 * They must never be reconstructed from the world active at retransmit time */
enum { COOP_GAMEPLAY_STAMP_BYTES = 12,
	   COOP_GAMEPLAY_STAMP_TAG = 0xd7,
	   COOP_GAMEPLAY_STAMP_VERSION = 0x10,
	   COOP_GAMEPLAY_STAMP_FROZEN = 1 };

typedef struct coop_gameplay_stamp {
	uint64_t visit;
	int16_t level;
	uint8_t frozen;
} coop_gameplay_stamp;

static inline int coop_gameplay_stamp_valid(const coop_gameplay_stamp *stamp)
{
	return stamp && stamp->frozen <= 1 && stamp->level >= -127 && stamp->level <= 127 &&
	       (stamp->level || (!stamp->visit && !stamp->frozen));
}

static inline int coop_gameplay_next_visit(uint64_t current, uint64_t *next)
{
	if (!next || current == UINT64_MAX) return 0;
	*next = current + 1;
	return 1;
}

static inline int coop_gameplay_stamp_write(unsigned char *data, size_t capacity,
                                            const coop_gameplay_stamp *stamp)
{
	if (!data || capacity < COOP_GAMEPLAY_STAMP_BYTES || !coop_gameplay_stamp_valid(stamp)) return 0;
	data[0] = COOP_GAMEPLAY_STAMP_TAG;
	data[1] = COOP_GAMEPLAY_STAMP_VERSION | stamp->frozen;
	data[2] = (unsigned char) stamp->level;
	data[3] = (unsigned char) ((uint16_t) stamp->level >> 8);
	for (unsigned i = 0; i < 8; ++i) data[4 + i] = (unsigned char) (stamp->visit >> (8 * i));
	return 1;
}

static inline int coop_gameplay_stamp_read(coop_gameplay_stamp *stamp,
                                           const unsigned char *data, size_t size)
{
	coop_gameplay_stamp decoded = { 0 };
	if (!stamp || !data || size != COOP_GAMEPLAY_STAMP_BYTES || data[0] != COOP_GAMEPLAY_STAMP_TAG ||
	    (data[1] & ~COOP_GAMEPLAY_STAMP_FROZEN) != COOP_GAMEPLAY_STAMP_VERSION) return 0;
	unsigned level = data[2] | (unsigned) data[3] << 8;
	decoded.level = (int16_t) (level < 0x8000 ? (int) level : (int) level - 0x10000);
	decoded.frozen = data[1] & COOP_GAMEPLAY_STAMP_FROZEN;
	for (unsigned i = 0; i < 8; ++i) decoded.visit |= (uint64_t) data[4 + i] << (8 * i);
	if (!coop_gameplay_stamp_valid(&decoded)) return 0;
	*stamp = decoded;
	return 1;
}

/* These handlers authenticate their sender and/or own operation identity.
 * World mutations and world-bound requests are scoped by default, including
 * score, ship status, damage, pickups, exit, save and rewind requests */
static inline int coop_gameplay_session_control(unsigned type)
{
	switch (type) {
		case MULTI_MESSAGE:
		case MULTI_OBS_MESSAGE:
		case MULTI_TYPING_STATE:
		case MULTI_QUIT:
		case MULTI_COOP_RESTORE_INV:
		case MULTI_COOP_RESTORE_STATUS:
		case MULTI_COOP_BRIEFING:
		case MULTI_COOP_TRAVEL:
		case MULTI_REWIND_RESULT:
		case MULTI_REWIND_SAVE_BEGIN:
		case MULTI_REWIND_SAVE_CHUNK:
		case MULTI_REWIND_SAVE_APPLY:
		case MULTI_REWIND_SAVE_READY:
			return 1;
		default: return 0;
	}
}

/* Apply only after transport authentication and message-length validation.
 * During death/drop settlement the source remains open until its drain barrier;
 * capture, loading and release waits close it to gameplay on both ends */
static inline int coop_gameplay_message_allowed(unsigned type, const coop_gameplay_stamp *sent,
                                                const coop_gameplay_stamp *current, int receiver_closed)
{
	if (!coop_gameplay_stamp_valid(sent) || !coop_gameplay_stamp_valid(current)) return 0;
	if (coop_gameplay_session_control(type)) return 1;
	/* Recovery may drain pickups and freeze replies while the current mine is
	 * closed. Its epoch/revisions do not authorize crossing a world replacement */
	if (type == MULTI_COOP_RECOVERY)
		return sent->visit && current->visit && sent->visit == current->visit && sent->level == current->level;
	return !receiver_closed && !sent->frozen && sent->visit && current->visit &&
	       sent->visit == current->visit && sent->level == current->level;
}

/* The terminal visit has no running mine, but the score screens still exchange
 * connection status. Never admit the departed world's frozen or live packets */
static inline int coop_gameplay_endlevel_stamp_allowed(const coop_gameplay_stamp *sent,
                                                       const coop_gameplay_stamp *current, int ending_campaign)
{
	if (!coop_gameplay_stamp_valid(sent) || !coop_gameplay_stamp_valid(current)) return 0;
	if (ending_campaign)
		return current->frozen && sent->frozen && current->visit &&
		       sent->visit == current->visit && sent->level == current->level;
	return coop_gameplay_message_allowed(MULTI_ENDLEVEL_START, sent, current, current->frozen);
}

enum { COOP_INVENTORY_DISCARD = -1,
	   COOP_INVENTORY_WAIT,
	   COOP_INVENTORY_APPLY };

/* A rejoin inventory may arrive before SYNC. Never adopt its world identity;
 * wait for normal synchronization, then compare the original received stamp */
static inline int coop_gameplay_inventory_action(const coop_gameplay_stamp *sent,
                                                 const coop_gameplay_stamp *current, int world_ready)
{
	if (!coop_gameplay_stamp_valid(sent) || !sent->visit || !coop_gameplay_stamp_valid(current) ||
	    sent->visit < current->visit) return COOP_INVENTORY_DISCARD;
	if (!world_ready || current->frozen) return COOP_INVENTORY_WAIT;
	return sent->visit == current->visit && sent->level == current->level ? COOP_INVENTORY_APPLY : COOP_INVENTORY_DISCARD;
}

#endif
