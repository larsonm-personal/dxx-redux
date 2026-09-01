#include <assert.h>
#include <stdint.h>

#include "net_udp_initial_sync_retry.h"

int main(void)
{
	android_net_udp_initial_sync_retry_state state;
	const int player_num = 1;
	const int64_t start = 1000;
	const int64_t window = 500;

	android_net_udp_initial_sync_retry_reset(&state);
	assert(!android_net_udp_initial_sync_retry_should_resend(
		&state, player_num, start));

	android_net_udp_initial_sync_retry_arm(
		&state, player_num, start, window);
	assert(android_net_udp_initial_sync_retry_should_resend(
		&state, player_num, start));
	assert(android_net_udp_initial_sync_retry_should_resend(
		&state, player_num, start + window));
	assert(!android_net_udp_initial_sync_retry_should_resend(
		&state, player_num, start + window + 1));

	android_net_udp_initial_sync_retry_arm(
		&state, player_num, start, window);
	android_net_udp_initial_sync_retry_confirm(&state, player_num);
	assert(!android_net_udp_initial_sync_retry_should_resend(
		&state, player_num, start + 1));

	android_net_udp_initial_sync_retry_arm(&state, -1, start, window);
	android_net_udp_initial_sync_retry_arm(
		&state, ANDROID_NET_UDP_INITIAL_SYNC_SLOTS, start, window);
	assert(!android_net_udp_initial_sync_retry_should_resend(
		&state, -1, start));
	assert(!android_net_udp_initial_sync_retry_should_resend(
		&state, ANDROID_NET_UDP_INITIAL_SYNC_SLOTS, start));

	return 0;
}
