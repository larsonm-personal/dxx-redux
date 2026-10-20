#ifndef DXX_REDUX_NET_UDP_INITIAL_SYNC_RETRY_H
#define DXX_REDUX_NET_UDP_INITIAL_SYNC_RETRY_H

#include <stdint.h>
#ifdef __ANDROID__
#include <time.h>
/* timer_query is frame-cached; signed sends can block across its updates */
static inline int64_t android_net_udp_initial_request_now(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (int64_t) now.tv_sec * 1000 + now.tv_nsec / 1000000;
}
#endif

#define ANDROID_NET_UDP_INITIAL_SYNC_SLOTS 8

typedef struct android_net_udp_initial_sync_retry_state {
	int64_t retry_deadline[ANDROID_NET_UDP_INITIAL_SYNC_SLOTS];
} android_net_udp_initial_sync_retry_state;

static inline void android_net_udp_initial_sync_retry_reset(
    android_net_udp_initial_sync_retry_state *state)
{
	int player_num;

	if (!state)
		return;
	for (player_num = 0;
	     player_num < ANDROID_NET_UDP_INITIAL_SYNC_SLOTS;
	     ++player_num)
		state->retry_deadline[player_num] = 0;
}

static inline void android_net_udp_initial_sync_retry_arm(
    android_net_udp_initial_sync_retry_state *state, int player_num,
    int64_t now, int64_t retry_window)
{
	if (!state || player_num < 0 ||
	    player_num >= ANDROID_NET_UDP_INITIAL_SYNC_SLOTS || retry_window <= 0)
		return;
	state->retry_deadline[player_num] = now + retry_window;
}

static inline int android_net_udp_initial_sync_retry_should_resend(
    const android_net_udp_initial_sync_retry_state *state, int player_num,
    int64_t now)
{
	if (!state || player_num < 0 ||
	    player_num >= ANDROID_NET_UDP_INITIAL_SYNC_SLOTS)
		return 0;
	return state->retry_deadline[player_num] != 0 &&
	       now <= state->retry_deadline[player_num];
}

static inline void android_net_udp_initial_sync_retry_confirm(
    android_net_udp_initial_sync_retry_state *state, int player_num)
{
	if (!state || player_num < 0 ||
	    player_num >= ANDROID_NET_UDP_INITIAL_SYNC_SLOTS)
		return;
	state->retry_deadline[player_num] = 0;
}

/* Initial cooperative join only: cap requests at two per second after the
 * blocking signed send finishes, and allow an answered proof two seconds */
typedef struct android_net_udp_initial_request_retry_state {
	int active;
	int64_t request_due;
	int64_t proof_hold_until;
} android_net_udp_initial_request_retry_state;

static inline void android_net_udp_initial_request_begin(
    android_net_udp_initial_request_retry_state *state, int active)
{
	state->active = active;
	state->request_due = 0;
	state->proof_hold_until = 0;
}

static inline void android_net_udp_initial_request_sent(
    android_net_udp_initial_request_retry_state *state, int64_t now, int64_t second)
{
	state->request_due = now + second / 2;
}

static inline void android_net_udp_initial_request_proof_sent(
    android_net_udp_initial_request_retry_state *state, int64_t now, int64_t second)
{
	state->proof_hold_until = now + second * 2;
}

static inline int android_net_udp_initial_request_due(
    const android_net_udp_initial_request_retry_state *state, int64_t now)
{
	return state->active && now >= state->request_due && now >= state->proof_hold_until;
}

#endif
