/* Independent peers and a reordered/lossy transport, including the inventory
 * award that a ledger-only test cannot observe */
#include <stdio.h>
#include <stdlib.h>
#include "coop_pickup_lock.h"

#define CHECK(c)                                                    \
	do {                                                            \
		if (!(c)) {                                                 \
			fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); \
			exit(1);                                                \
		}                                                           \
	} while (0)
typedef struct peer_state {
	coop_pickup_lock lock;
	uint32_t clock;
	int gear, awards;
} peer_state;
typedef struct message {
	int from, to, reply, gear;
	uint32_t request, life;
} message;
static peer_state peers[3];
static message queue[4096];
static int queued;
static unsigned rng;

static unsigned random_next(void)
{
	rng = rng * 1664525u + 1013904223u;
	return rng;
}

static void send_message(int from, int to, int reply, uint32_t request, uint32_t life)
{
	CHECK(queued < 4096);
	queue[queued++] = (message) { from, to, reply, peers[from].gear, request, life };
}

static void request_peers(int p, unsigned mask)
{
	for (int i = 0; i < 3; ++i)
		if (mask & (1u << i)) send_message(p, i, 0, peers[p].lock.request, peers[p].lock.life);
}

static void begin(int p, unsigned members)
{
	coop_pickup_lock_begin(&peers[p].lock, ++peers[p].clock, 7, (uint8_t) members);
	request_peers(p, members);
}

static void release(int p)
{
	coop_pickup_lock *lock = &peers[p].lock;
	for (int i = 0; i < 3; ++i)
		if (lock->deferred[i]) send_message(p, i, 1, lock->deferred[i], lock->deferred_life[i]);
	memset(lock, 0, sizeof(*lock));
}

static void deliver(message m)
{
	peer_state *p = &peers[m.to];
	if (p->clock < m.request) p->clock = m.request;
	if (!m.reply) {
		if (coop_pickup_lock_request(&p->lock, m.to, m.from, m.request, m.life))
			send_message(m.to, m.from, 1, m.request, m.life);
	} else if (coop_pickup_lock_reply(&p->lock, m.from, m.request, m.life)) {
		p->gear &= m.gear;
	}
}

static void collect_ready(void)
{
	for (int i = 0; i < 3; ++i)
		if (coop_pickup_lock_enter(&peers[i].lock)) {
			peers[i].awards += peers[i].gear;
			peers[i].gear = 0;
			release(i);
		}
	CHECK(peers[0].awards + peers[1].awards + peers[2].awards <= 1);
}

static void reset(void)
{
	memset(peers, 0, sizeof(peers));
	for (int i = 0; i < 3; ++i) peers[i].gear = 1;
	queued = 0;
}

static void race(unsigned seed)
{
	reset();
	rng = seed;
	for (int i = 0; i < 3; ++i) begin(i, 7u & ~(1u << i));
	for (int step = 0; step < 10000; ++step) {
		if (queued) {
			int index = (int) (random_next() % (unsigned) queued);
			message m = queue[index];
			queue[index] = queue[--queued];
			unsigned action = random_next();
			if (action % 7) deliver(m);      /* Loss */
			if (action % 5 == 0) deliver(m); /* Duplicate delivery */
		}
		collect_ready();
		if (step % 31 == 0)
			for (int i = 0; i < 3; ++i) request_peers(i, peers[i].lock.waiting);
		if (!queued && !peers[0].lock.request && !peers[1].lock.request && !peers[2].lock.request) {
			CHECK(peers[0].awards + peers[1].awards + peers[2].awards == 1);
			return;
		}
	}
	CHECK(0);
}

static void cancellation_and_membership(void)
{
	reset();
	begin(0, 2);
	message old_reply = { 1, 0, 1, 1, peers[0].lock.request, 7 };
	release(0); /* Moved away or died before any award */
	begin(0, 2);
	deliver(old_reply);
	CHECK(peers[0].lock.waiting == 2);
	message wrong_life = { 1, 0, 1, 1, peers[0].lock.request, 6 };
	deliver(wrong_life);
	CHECK(peers[0].lock.waiting == 2);
	CHECK(coop_pickup_lock_members(&peers[0].lock, 6) == 4);
	CHECK(peers[0].lock.waiting == 6); /* Joining peer participates too */
	CHECK(coop_pickup_lock_members(&peers[0].lock, 4) == 0);
	CHECK(peers[0].lock.waiting == 4); /* Departed host removed, request survives */
	CHECK(!coop_pickup_lock_enter(&peers[0].lock));
	CHECK(coop_pickup_lock_reply(&peers[0].lock, 2, peers[0].lock.request, 7));
	CHECK(coop_pickup_lock_enter(&peers[0].lock));
	CHECK(!coop_pickup_lock_request(&peers[0].lock, 0, 2, 1, 7));
	CHECK(!coop_pickup_lock_enter(&peers[0].lock)); /* Cannot award twice */
}

int main(void)
{
	for (unsigned seed = 1; seed <= 1000; ++seed) race(seed);
	cancellation_and_membership();
	puts("Pickup permission: 1000 three-peer loss/reorder races, cancellation and membership passed");
	return 0;
}
