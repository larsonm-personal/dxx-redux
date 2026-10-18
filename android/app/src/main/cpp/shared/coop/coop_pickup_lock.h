#ifndef COOP_PICKUP_LOCK_H
#define COOP_PICKUP_LOCK_H

#include <stdint.h>
#include <string.h>

/* Per-object Ricart-Agrawala permission. Requests are ordered by a Lamport
 * clock, then player slot. Every active peer must reply before inventory is
 * touched. A reply carries that peer's remaining recovery contents, so packet
 * reordering cannot grant permission ahead of an earlier collection report */
enum { COOP_PICKUP_PEERS = 8 };
typedef struct coop_pickup_lock {
	uint32_t request, life;
	uint32_t deferred[COOP_PICKUP_PEERS], deferred_life[COOP_PICKUP_PEERS];
	uint8_t participants, waiting, held;
} coop_pickup_lock;

static inline void coop_pickup_lock_begin(coop_pickup_lock *lock, uint32_t request,
                                          uint32_t life, uint8_t participants)
{
	lock->request = request;
	lock->life = life;
	lock->participants = lock->waiting = participants;
	lock->held = 0;
}

/* Return the newly joined peers that need our outstanding request too */
static inline uint8_t coop_pickup_lock_members(coop_pickup_lock *lock, uint8_t members)
{
	uint8_t added = members & ~lock->participants;
	lock->participants = members;
	lock->waiting = (lock->waiting & members) | added;
	return added;
}

static inline int coop_pickup_lock_request(coop_pickup_lock *lock, int local, int sender,
                                           uint32_t request, uint32_t life)
{
	if (lock->request && (lock->held || lock->request < request ||
	                      (lock->request == request && local < sender))) {
		lock->deferred[sender] = request;
		lock->deferred_life[sender] = life;
		return 0;
	}
	return 1;
}

static inline int coop_pickup_lock_reply(coop_pickup_lock *lock, int sender,
                                         uint32_t request, uint32_t life)
{
	if (!lock->request || lock->request != request || lock->life != life ||
	    !(lock->waiting & (1u << sender))) return 0;
	lock->waiting &= ~(1u << sender);
	return 1;
}

static inline int coop_pickup_lock_enter(coop_pickup_lock *lock)
{
	if (!lock->request || lock->waiting || lock->held) return 0;
	lock->held = 1;
	return 1;
}

#endif
