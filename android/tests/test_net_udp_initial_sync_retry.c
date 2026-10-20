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

	/* Initial requests wait after a completed send, including signing/I/O */
	android_net_udp_initial_request_retry_state request;
	android_net_udp_initial_request_begin(&request, 1);
	android_net_udp_initial_request_sent(&request, 1450, 1000);
	assert(!android_net_udp_initial_request_due(&request, 1949));
	assert(android_net_udp_initial_request_due(&request, 1950));
	/* Never replace a freshly answered proof during its two-second window */
	android_net_udp_initial_request_proof_sent(&request, 1800, 1000);
	assert(!android_net_udp_initial_request_due(&request, 3799));
	assert(android_net_udp_initial_request_due(&request, 3800));
	android_net_udp_initial_request_sent(&request, 4100, 1000);
	assert(!android_net_udp_initial_request_due(&request, 4599));
	assert(android_net_udp_initial_request_due(&request, 4600));
	/* Next-level, restore, and reconnect callers keep the existing cadence */
	android_net_udp_initial_request_begin(&request, 0);
	assert(!android_net_udp_initial_request_due(&request, 100000));
	/* A new session cannot inherit the preceding session's proof hold */
	android_net_udp_initial_request_begin(&request, 1);
	android_net_udp_initial_request_sent(&request, 100, 1000);
	assert(android_net_udp_initial_request_due(&request, 600));

	/* Initial request traffic is bounded at two requests per second per peer */
	int requests = 1;
	android_net_udp_initial_request_begin(&request, 1);
	android_net_udp_initial_request_sent(&request, 0, 1000);
	for (int now = 1; now < 1000; ++now) {
		if (android_net_udp_initial_request_due(&request, now)) {
			++requests;
			android_net_udp_initial_request_sent(&request, now, 1000);
		}
	}
	assert(requests == 2);

	return 0;
}
