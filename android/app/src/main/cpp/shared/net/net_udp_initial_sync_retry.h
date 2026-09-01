#ifndef DXX_REDUX_NET_UDP_INITIAL_SYNC_RETRY_H
#define DXX_REDUX_NET_UDP_INITIAL_SYNC_RETRY_H

#include <stdint.h>

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

#endif
