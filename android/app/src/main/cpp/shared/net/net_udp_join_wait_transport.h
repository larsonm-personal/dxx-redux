#ifndef NET_UDP_JOIN_WAIT_TRANSPORT_H
#define NET_UDP_JOIN_WAIT_TRANSPORT_H

/* Included by both Android UDP engines, after transport/security declarations */
#include "net_udp_join_wait.h"
#include "coop/coop_endgame.h"

static uint32_t join_query_token, join_query_sent, join_query_received;
static fix64 join_query_times[16], join_query_last;
static int join_transfer_committed, join_previous_connected, join_previous_net_connected;
static fix64 join_test_hold_until;

int net_udp_join_wait_test_hold(void)
{
	if (!multi_i_am_master() || !(Game_mode & GM_MULTI_COOP)) return 0;
	join_test_hold_until = timer_query() + i2f(60);
	return 1;
}

static int net_udp_join_transfer_obsolete(void)
{
	return UDP_sync_player.join_attempt && !join_transfer_committed &&
	       (UDP_sync_player.join_visit != coop_world_visit_current() || Network_status != NETSTAT_PLAYING ||
	        Endlevel_sequence || Control_center_destroyed || coop_travel_active() ||
	        coop_briefing_active() || multi_save_transfer_busy());
}

static void net_udp_join_cancel_transfer(void)
{
	if (!UDP_sync_player.join_attempt || join_transfer_committed) return;
	const int slot = UDP_sync_player.player.connected;
	if (UDP_sync_player.join_visit == coop_world_visit_current() && slot >= 0 && slot < MAX_PLAYERS) {
		Players[slot].connected = join_previous_connected;
		Netgame.players[slot].connected = join_previous_net_connected;
	}
	Network_send_objects = Network_sending_extras = Network_player_added = 0;
	Network_send_objnum = Player_joining_extras = VerifyPlayerJoined = -1;
	UDP_sync_player.join_attempt = 0;
	join_test_hold_until = 0;
}

static void net_udp_join_confirm_player(int player_num)
{
	if (!join_transfer_committed || !UDP_sync_player.join_attempt ||
	    player_num != UDP_sync_player.player.connected ||
	    UDP_sync_player.join_visit != coop_world_visit_current()) return;
	/* Gameplay confirmation ends the join envelope's lifetime. Later level or
	 * restore SYNC packets belong to the established session, not this attempt */
	COOPLOG("join transfer retired: player=%d visit=%llu", player_num,
	        (unsigned long long) UDP_sync_player.join_visit);
	UDP_sync_player.join_attempt = 0;
	UDP_sync_player.join_visit = 0;
	join_transfer_committed = 0;
	join_test_hold_until = 0;
}

static void net_udp_join_wait_reset(void)
{
	join_query_token = (uint32_t) generate_token();
	if (!join_query_token) join_query_token = 1; /* Zero denotes an ordinary participant request */
	join_query_sent = join_query_received = 0;
	join_query_last = 0;
	memset(join_query_times, 0, sizeof(join_query_times));
}

static void net_udp_join_status(ubyte *request, struct _sockaddr address)
{
	if (!(Game_mode & GM_MULTI_COOP) || (uint32_t) GET_INTEL_INT(request + 1) != netgame_token ||
	    GET_INTEL_INT(request + 13) != MULTI_PROTO_VERSION) return;
	net_join_status status = { 0 };
	coop_transition_policy briefing;
	coop_presentation_progress local;
	coop_briefing_get_state(&briefing, &local);
	status.phase = NET_JOIN_READY;
	status.level = Current_level_num;
	status.visit = coop_world_visit_current();
	if (coop_endgame_active() || coop_endgame_released()) status.phase = NET_JOIN_COMPLETE;
	else if (multi_save_transfer_busy()) status.phase = NET_JOIN_RESTORE;
	else if (coop_briefing_active()) {
		status.phase = coop_flyout_active() ? NET_JOIN_FLYOUT : NET_JOIN_PREPARING;
		if (briefing.phase == COOP_PHASE_BRIEFING) {
			if (!coop_flyout_active() && !coop_briefing_suppressed_for_restore()) status.phase = NET_JOIN_BRIEFING;
			const uint64_t now = (uint64_t) timer_query() * 1000 / F1_0;
			status.remaining_ms = briefing.deadline_ms > now ? (uint32_t) (briefing.deadline_ms - now) : 0;
			status.duration_ms = coop_flyout_active() ? 60000 : COOP_BRIEFING_LIMIT_MS;
			status.briefing = briefing.generation;
			status.host_ready = !!(briefing.presentation_ready & (1u << briefing.host));
			for (int i = 0; i < MAX_PLAYERS; ++i) {
				status.total += !!(briefing.participants & (1u << i));
				status.completed += !!(briefing.presentation_ready & (1u << i));
			}
		}
	} else if (Network_status == NETSTAT_ENDLEVEL) status.phase = NET_JOIN_SCORES;
	else if (Endlevel_sequence) status.phase = NET_JOIN_FLYOUT;
	else if (Control_center_destroyed) {
		status.phase = NET_JOIN_ESCAPE;
		if (Countdown_seconds_left > 0) {
			status.remaining_ms = (uint32_t) Countdown_seconds_left * 1000;
			status.duration_ms = (uint32_t) (Total_countdown_time > Countdown_seconds_left ? Total_countdown_time : Countdown_seconds_left) * 1000;
		}
	} else if (coop_travel_active()) status.phase = NET_JOIN_TRAVEL;
	else if (Network_status == NETSTAT_WAITING || Network_status == NETSTAT_STARTING) status.phase = NET_JOIN_PREPARING;
	ubyte response[UPID_JOIN_STATUS_SIZE] = { UPID_JOIN_STATUS };
	PUT_INTEL_INT(response + 1, GET_INTEL_INT(request + 5));
	PUT_INTEL_INT(response + 5, GET_INTEL_INT(request + 9));
	PUT_INTEL_INT(response + 9, netgame_token);
	PUT_INTEL_INT(response + 13, status.phase);
	PUT_INTEL_INT(response + 17, status.level);
	coop_world_visit_write(response + 21, status.visit);
	coop_world_visit_write(response + 29, status.briefing);
	PUT_INTEL_INT(response + 37, status.remaining_ms);
	PUT_INTEL_INT(response + 41, status.duration_ms);
	PUT_INTEL_INT(response + 45, status.completed);
	PUT_INTEL_INT(response + 49, status.total);
	PUT_INTEL_INT(response + 53, status.host_ready);
	dxx_sendto(UDP_Socket[0], response, sizeof(response), 0, (struct sockaddr *) &address, sizeof(address));
}

static void net_udp_join_status_receive(const ubyte *data, struct _sockaddr address)
{
	const uint32_t sequence = GET_INTEL_INT(data + 5);
	if (!net_join_wait_active() || !is_master_ip(address) ||
	    (uint32_t) GET_INTEL_INT(data + 1) != join_query_token ||
	    (uint32_t) GET_INTEL_INT(data + 9) != netgame_token ||
	    !sequence || sequence > join_query_sent || sequence <= join_query_received ||
	    join_query_sent - sequence >= 16) return;
	net_join_status status;
	status.phase = GET_INTEL_INT(data + 13);
	status.level = (int32_t) GET_INTEL_INT(data + 17);
	status.visit = coop_world_visit_read(data + 21);
	status.briefing = coop_world_visit_read(data + 29);
	status.remaining_ms = GET_INTEL_INT(data + 37);
	status.duration_ms = GET_INTEL_INT(data + 41);
	status.completed = GET_INTEL_INT(data + 45);
	status.total = GET_INTEL_INT(data + 49);
	status.host_ready = GET_INTEL_INT(data + 53);
	join_query_received = sequence;
	const fix64 elapsed = timer_query() - join_query_times[sequence % 16];
	net_join_wait_receive(&status, elapsed > 0 ? (unsigned) (elapsed * 1000 / F1_0) : 0);
}

void net_udp_join_wait_poll(void)
{
	if (!net_join_wait_active() || UDP_Socket[0] < 0) return;
	const fix64 now = timer_query();
	if (!join_query_sent || now >= join_query_last + F1_0 / 4) {
		ubyte request[UPID_JOIN_QUERY_SIZE] = { UPID_JOIN_QUERY };
		PUT_INTEL_INT(request + 1, netgame_token);
		PUT_INTEL_INT(request + 5, join_query_token);
		PUT_INTEL_INT(request + 9, ++join_query_sent);
		PUT_INTEL_INT(request + 13, MULTI_PROTO_VERSION);
		join_query_times[join_query_sent % 16] = join_query_last = now;
		struct _sockaddr address = Netgame.players[multi_who_is_master()].protocol.udp.addr;
		dxx_sendto(UDP_Socket[0], request, sizeof(request), 0, (struct sockaddr *) &address, sizeof(address));
	}
	net_udp_listen();
}

#endif
