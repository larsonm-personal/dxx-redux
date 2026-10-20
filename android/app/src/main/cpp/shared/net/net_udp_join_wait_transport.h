#ifndef NET_UDP_JOIN_WAIT_TRANSPORT_H
#define NET_UDP_JOIN_WAIT_TRANSPORT_H

/* Included by both Android UDP engines, after transport/security declarations */
#include "net_udp_join_wait.h"
#include "coop/coop_endgame.h"

static uint32_t join_query_token, join_query_sent, join_query_received;
static fix64 join_query_times[16], join_query_last;
static int join_transfer_committed, join_previous_connected, join_previous_net_connected;
static fix64 join_test_hold_until;
static int join_test_early_sync, join_objects_complete;
/* Sender object indices are unique within an attempt, before local remapping */
static ubyte join_received_objects[MAX_OBJECTS];
static int join_received_clear;
static unsigned join_early_syncs;
static ubyte join_pending_end[UPID_MAX_SIZE];
static int join_pending_end_size, join_expected_objects;
static unsigned join_early_ends;
static int join_test_early_end;
static unsigned join_excluded_spew;
static int join_test_drop_mode;
static int join_test_allocation_armed;

int net_udp_join_wait_test_owned_object(void)
{
	if (!multi_i_am_master() || !(Game_mode & GM_MULTI_COOP) || Network_status != NETSTAT_PLAYING ||
	    Network_send_objects) return 0;
	for (int i = 0; i < N_players; ++i)
		if (i != Player_num && Players[i].connected != CONNECT_DISCONNECTED) return 0;
	/* Diagnostic fixture: force a real second-pass allocation on the joiner */
	for (int i = 0; i <= Highest_object_index; ++i) {
		if (Objects[i].type != OBJ_POWERUP || object_owner[i] != -1) continue;
		map_objnum_local_to_local(i);
		REJOIN_LOG("test_allocation_owned_object sender=%d owner=%d", i, Player_num);
		return 1;
	}
	return 0;
}

int net_udp_join_wait_test_allocation(void)
{
	if (!net_join_wait_active() || multi_i_am_master() || Network_status != NETSTAT_WAITING) return 0;
	join_test_allocation_armed = 1;
	REJOIN_LOG("test_allocation_armed");
	return 1;
}

int net_udp_join_test_fail_allocation(void)
{
	if (!join_test_allocation_armed || !net_join_wait_active()) return 0;
	join_test_allocation_armed = 0;
	REJOIN_LOG("test_allocation_failed");
	return 1;
}
static int join_test_drop_quit;
static int join_test_capture_dump, join_test_dump_stage;
static uint64_t join_test_dump_counter;
static unsigned join_test_drop_packets, join_test_drop_count;
static ubyte join_test_held_payload[UPID_MAX_SIZE];
static int join_test_held_size, join_test_held_release;

enum { NET_JOIN_RELEASE_NEXT,
	   NET_JOIN_RELEASE_END,
	   NET_JOIN_RELEASE_SYNC };

int net_udp_join_wait_test_fault(int mode)
{
	if (!multi_i_am_master() || !(Game_mode & GM_MULTI_COOP) || Network_send_objects ||
	    mode < 0 || mode > NET_JOIN_DROP_END_PERSISTENT) return 0;
	join_test_drop_quit = mode == NET_JOIN_DROP_SECOND_AND_QUIT;
	join_test_capture_dump = mode == NET_JOIN_STALE_DUMP_RETRY;
	join_test_dump_stage = 0;
	join_test_drop_mode = join_test_drop_quit || join_test_capture_dump ? NET_JOIN_DROP_SECOND : mode;
	join_test_drop_packets = join_test_drop_count = 0;
	join_test_held_size = 0;
	join_test_held_release = NET_JOIN_RELEASE_NEXT;
	REJOIN_LOG("test_fault_armed mode=%d", mode);
	return 1;
}

unsigned net_udp_join_test_drops(void)
{
	return join_test_drop_count;
}

static void net_udp_join_test_stale_dump(void)
{
	ubyte data[UPID_DUMP_SIZE] = { UPID_DUMP };
	PUT_INTEL_INT(data + 1, netgame_token);
	data[5] = DUMP_PKTTIMEOUT;
	android_net_udp_write_u64_le(data + 6, join_test_dump_counter);
	dxx_sendto(UDP_Socket[0], data, sizeof(data), 0,
	           (struct sockaddr *) &UDP_sync_player.player.protocol.udp.addr, sizeof(struct _sockaddr));
	++join_test_drop_count;
	REJOIN_LOG("test_stale_dump stage=%d counter=%llu", join_test_dump_stage, (unsigned long long) join_test_dump_counter);
}

int net_udp_join_wait_test_kick(void)
{
	const int slot = UDP_sync_player.player.connected;
	if (!multi_i_am_master() || slot < 0 || slot >= N_players || slot == Player_num ||
	    Players[slot].connected != CONNECT_PLAYING) return 0;
	net_udp_dump_player(Netgame.players[slot].protocol.udp.addr, player_tokens[slot], DUMP_KICKED);
	return 1;
}

/* Android diagnostic only: called for the selected joiner's enveloped payload */
static int net_udp_join_test_payload_action(const ubyte *data, int length)
{
	if (join_test_dump_stage == 1 && UDP_sync_player.reconnect_identity.request_counter > join_test_dump_counter) {
		net_udp_join_test_stale_dump();
		join_test_dump_stage = 2;
	}
	if (!join_test_drop_mode) return NET_JOIN_TEST_SEND;
	if (join_test_drop_mode == NET_JOIN_DROP_END_PERSISTENT) {
		if (data[0] != UPID_OBJECT_DATA || length < UPID_OBJECT_HEADER_SIZE + 9 ||
		    GET_INTEL_INT(data + UPID_OBJECT_HEADER_SIZE) != -2) return NET_JOIN_TEST_SEND;
		++join_test_drop_count;
		REJOIN_LOG("test_drop_end_persistent attempt=%u count=%u", (unsigned) UDP_sync_player.join_attempt, join_test_drop_count);
		return NET_JOIN_TEST_DROP;
	}
	if (join_test_drop_mode == NET_JOIN_DROP_ALL_OBJECTS && data[0] == UPID_OBJECT_DATA) {
		/* Count one stream fault; keep per-packet proof in the diagnostic log */
		join_test_drop_count = 1;
		REJOIN_LOG("test_drop_all_objects attempt=%u bytes=%d", (unsigned) UDP_sync_player.join_attempt, length);
		if (length >= UPID_OBJECT_HEADER_SIZE + 9 && GET_INTEL_INT(data + UPID_OBJECT_HEADER_SIZE) == -2)
			join_test_drop_mode = 0;
		return NET_JOIN_TEST_DROP;
	}
	int first = 0, selected = 0;
	if (data[0] == UPID_OBJECT_DATA && length >= UPID_OBJECT_HEADER_SIZE + 9) {
		first = GET_INTEL_INT(data + UPID_OBJECT_HEADER_SIZE);
		if (first != -2) ++join_test_drop_packets;
		selected = ((join_test_drop_mode == NET_JOIN_DROP_FIRST || join_test_drop_mode == NET_JOIN_DUPLICATE_FIRST) && first == -1) ||
		           ((join_test_drop_mode == NET_JOIN_DROP_SECOND || join_test_drop_mode == NET_JOIN_DUPLICATE_SECOND || join_test_drop_mode == NET_JOIN_REORDER_SECOND ||
		             join_test_drop_mode == NET_JOIN_DELAY_SECOND_TO_END || join_test_drop_mode == NET_JOIN_DELAY_SECOND_TO_SYNC ||
		             join_test_drop_mode == NET_JOIN_STALE_ATTEMPT_SECOND || join_test_drop_mode == NET_JOIN_STALE_VISIT_SECOND || join_test_drop_mode == NET_JOIN_WRONG_SESSION_SECOND) &&
		            first >= 0 && join_test_drop_packets == 2) ||
		           ((join_test_drop_mode == NET_JOIN_DROP_TAIL || join_test_drop_mode == NET_JOIN_DUPLICATE_TAIL || join_test_drop_mode == NET_JOIN_REORDER_TAIL) && first >= 0 && Network_send_objnum > Highest_object_index) ||
		           ((join_test_drop_mode == NET_JOIN_DROP_END || join_test_drop_mode == NET_JOIN_DUPLICATE_END || join_test_drop_mode == NET_JOIN_REORDER_END) && first == -2);
	} else if (data[0] == UPID_SYNC) {
		selected = join_test_drop_mode == NET_JOIN_DROP_SYNC || join_test_drop_mode == NET_JOIN_DUPLICATE_SYNC;
	}
	if (!selected) return NET_JOIN_TEST_SEND;
	if (join_test_capture_dump) {
		join_test_dump_counter = UDP_sync_player.reconnect_identity.request_counter;
		join_test_dump_stage = 1;
		join_test_capture_dump = 0;
	}
	const int mode = join_test_drop_mode;
	const int action = mode == NET_JOIN_STALE_ATTEMPT_SECOND ? NET_JOIN_TEST_STALE_ATTEMPT : mode == NET_JOIN_STALE_VISIT_SECOND                                                                                                                                             ? NET_JOIN_TEST_STALE_VISIT
	                                                                                     : join_test_drop_mode == NET_JOIN_WRONG_SESSION_SECOND                                                                                                                              ? NET_JOIN_TEST_WRONG_SESSION
	                                                                                     : mode == NET_JOIN_REORDER_SECOND || mode == NET_JOIN_REORDER_TAIL || mode == NET_JOIN_REORDER_END || mode == NET_JOIN_DELAY_SECOND_TO_END || mode == NET_JOIN_DELAY_SECOND_TO_SYNC ? NET_JOIN_TEST_HOLD
	                                                                                     : join_test_drop_mode >= NET_JOIN_DUPLICATE_FIRST                                                                                                                                   ? NET_JOIN_TEST_DUPLICATE
	                                                                                                                                                                                                                                                                         : NET_JOIN_TEST_DROP;
	REJOIN_LOG("test_fault mode=%d action=%d attempt=%u visit=%llu type=%d bytes=%d packet=%u first=%d",
	           join_test_drop_mode, action, (unsigned) UDP_sync_player.join_attempt,
	           (unsigned long long) UDP_sync_player.join_visit, data[0], length, join_test_drop_packets, first);
	join_test_drop_mode = 0;
	++join_test_drop_count;
	if (action == NET_JOIN_TEST_HOLD) {
		if (length > (int) sizeof(join_test_held_payload)) return NET_JOIN_TEST_DROP;
		memcpy(join_test_held_payload, data, length);
		join_test_held_size = length;
		join_test_held_release = mode == NET_JOIN_DELAY_SECOND_TO_END ? NET_JOIN_RELEASE_END : mode == NET_JOIN_DELAY_SECOND_TO_SYNC ? NET_JOIN_RELEASE_SYNC
		                                                                                                                             : NET_JOIN_RELEASE_NEXT;
	}
	return action;
}

static int net_udp_join_test_held_ready(const ubyte *data, int length)
{
	if (!join_test_held_size) return 0;
	if (join_test_held_release == NET_JOIN_RELEASE_NEXT) return 1;
	if (join_test_held_release == NET_JOIN_RELEASE_SYNC) return data[0] == UPID_SYNC;
	return data[0] == UPID_OBJECT_DATA && length >= UPID_OBJECT_HEADER_SIZE + 9 &&
	       GET_INTEL_INT(data + UPID_OBJECT_HEADER_SIZE) == -2;
}

static int net_udp_join_test_take_held(ubyte *data)
{
	const int length = join_test_held_size;
	if (!length) return 0;
	memcpy(data, join_test_held_payload, length);
	join_test_held_size = 0;
	join_test_held_release = NET_JOIN_RELEASE_NEXT;
	REJOIN_LOG("test_fault_release type=%d bytes=%d", data[0], length);
	return length;
}

unsigned net_udp_join_excluded_spew(void)
{
	return join_excluded_spew;
}

int net_udp_join_wait_test_early_end(void)
{
	if (!multi_i_am_master() || !(Game_mode & GM_MULTI_COOP)) return 0;
	join_test_early_end = 1;
	return 1;
}

unsigned net_udp_join_early_ends(void)
{
	return join_early_ends;
}

static void net_udp_join_defer_end(const ubyte *data, int length, int expected, int received)
{
	if (length > (int) sizeof(join_pending_end)) return;
	/* Keep the transfer count and allocator state intact until its tail arrives */
	memcpy(join_pending_end, data, length);
	join_pending_end_size = length;
	join_expected_objects = expected;
	++join_early_ends;
	REJOIN_LOG("objects_end_deferred expected=%d received=%d attempt=%u", expected, received, (unsigned) join_query_token);
}

int net_udp_join_wait_test_early_sync(void)
{
	if (!multi_i_am_master() || !(Game_mode & GM_MULTI_COOP)) return 0;
	join_test_early_sync = 1;
	return 1;
}

int net_udp_join_objects_complete(void)
{
	return join_objects_complete;
}
unsigned net_udp_join_early_syncs(void)
{
	return join_early_syncs;
}

static int net_udp_join_accept_sync(void)
{
	net_join_wait_note_transfer();
	if (join_objects_complete) return 1;
	++join_early_syncs;
	REJOIN_LOG("sync_deferred attempt=%u allocated=%d highest=%d count=%u", (unsigned) join_query_token, num_objects, Highest_object_index, join_early_syncs);
	/* The final SYNC or its periodic resend will complete the join after the
	 * object end marker verifies the transfer and rebuilds the allocator */
	return 0;
}

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

static void net_udp_join_retire_transfer(int include_published)
{
	if (!UDP_sync_player.join_attempt || (join_transfer_committed && !include_published)) return;
	const int slot = UDP_sync_player.player.connected;
	if (UDP_sync_player.join_visit == coop_world_visit_current() && slot >= 0 && slot < MAX_PLAYERS) {
		Players[slot].connected = include_published ? CONNECT_DISCONNECTED : join_previous_connected;
		Netgame.players[slot].connected = include_published ? CONNECT_DISCONNECTED : join_previous_net_connected;
		/* Old reliable obligations must not time out the replacement attempt */
		for (int i = 0; i < UDP_MDATA_STOR_QUEUE_SIZE; ++i)
			UDP_mdata_queue[i].player_ack[slot] = 1;
	}
	REJOIN_LOG("transfer_cancel attempt=%u published=%d slot=%d", (unsigned) UDP_sync_player.join_attempt, join_transfer_committed, slot);
	join_transfer_committed = 0;
	Network_send_objects = Network_sending_extras = Network_player_added = 0;
	Network_send_objnum = Player_joining_extras = VerifyPlayerJoined = -1;
	UDP_sync_player.join_attempt = 0;
	join_test_hold_until = 0;
	join_test_early_sync = 0;
	join_test_early_end = 0;
	join_test_held_size = 0;
	join_test_held_release = NET_JOIN_RELEASE_NEXT;
}

static void net_udp_join_cancel_transfer(void)
{
	net_udp_join_retire_transfer(0);
}

/* Requests have already passed signature validation; known slots also prove
 * their return route before reaching welcome_player */
static int net_udp_join_replace_transfer(const UDP_sequence_packet *request, int player, int proven)
{
	if (!UDP_sync_player.join_attempt || !request->join_attempt ||
	    !android_net_udp_reconnect_public_key_equal(&request->reconnect_identity, &UDP_sync_player.reconnect_identity)) return 1;
	if (request->join_attempt == UDP_sync_player.join_attempt) return 0;
	if (request->join_visit != UDP_sync_player.join_visit ||
	    !android_net_udp_reconnect_counter_is_newer(request->reconnect_identity.request_counter,
	                                                UDP_sync_player.reconnect_identity.request_counter)) return 0;
	if (player >= 0 ? (!proven || player != UDP_sync_player.player.connected) : !sockaddr_equal(&request->player.protocol.udp.addr, &UDP_sync_player.player.protocol.udp.addr)) return 0;
	REJOIN_LOG("transfer_replace old=%u new=%u", (unsigned) UDP_sync_player.join_attempt, (unsigned) request->join_attempt);
	net_udp_join_retire_transfer(1);
	return 1;
}

static void net_udp_join_confirm_player(int player_num)
{
	if (!join_transfer_committed || !UDP_sync_player.join_attempt ||
	    player_num != UDP_sync_player.player.connected ||
	    UDP_sync_player.join_visit != coop_world_visit_current()) return;
	/* Gameplay confirmation ends the join envelope's lifetime. Later level or
	 * restore SYNC packets belong to the established session, not this attempt */
	if (join_test_dump_stage == 2) {
		net_udp_join_test_stale_dump();
		join_test_dump_stage = 0;
	}
	COOPLOG("join transfer retired: player=%d visit=%llu", player_num,
	        (unsigned long long) UDP_sync_player.join_visit);
	UDP_sync_player.join_attempt = 0;
	UDP_sync_player.join_visit = 0;
	join_transfer_committed = 0;
	join_test_hold_until = 0;
}

static void net_udp_join_wait_reset(void)
{
	android_net_udp_auth_retire_request();
	memset(join_received_objects, 0, sizeof(join_received_objects));
	join_received_clear = 0;
	join_pending_end_size = 0;
	join_early_ends = 0;
	join_objects_complete = 0;
	join_early_syncs = 0;
	join_query_token = (uint32_t) generate_token();
	if (!join_query_token) join_query_token = 1; /* Zero denotes an ordinary participant request */
	join_query_sent = join_query_received = 0;
	join_query_last = 0;
	memset(join_query_times, 0, sizeof(join_query_times));
}

/* A refusal belongs to this signed request, not an earlier occupant of a slot */
static void net_udp_dump_join_request(const UDP_sequence_packet *request, int why)
{
	ubyte data[UPID_DUMP_SIZE] = { UPID_DUMP };
	PUT_INTEL_INT(data + 1, request->token ? request->token : netgame_token);
	data[5] = why;
	android_net_udp_write_u64_le(data + 6, request->reconnect_identity.request_counter);
	dxx_sendto(UDP_Socket[0], data, sizeof(data), 0,
	           (struct sockaddr *) &request->player.protocol.udp.addr, sizeof(struct _sockaddr));
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
	/* Informational capability only: a restarted selected client must still
	 * load and prove its existing identity through the ordinary ready path */
	if (net_launcher_initial_load && Network_status == NETSTAT_WAITING)
		status.phase = NET_JOIN_INITIAL_LOAD;
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
