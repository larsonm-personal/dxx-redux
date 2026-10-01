#ifndef NET_UDP_SCORE_CATCHUP_H
#define NET_UDP_SCORE_CATCHUP_H

/* Included by both Android UDP engines after their transport declarations */
static struct {
	ubyte packet[UPID_ENDLEVEL_H_SIZE];
	int size;
	coop_gameplay_stamp source;
	fix64 next_reply[MAX_PLAYERS];
#ifdef INTROSPECT_ON
	int drop_completion;
	unsigned dropped, replies;
#endif
} android_score_catchup;

static void net_udp_score_catchup_reset(void)
{
	memset(&android_score_catchup, 0, sizeof(android_score_catchup));
}

/* Keep the original bytes and world identity, including the final send when
 * scores close. Later WAITING announcements must not replace this snapshot */
static int net_udp_score_catchup_remember(const ubyte *data, int size)
{
	coop_gameplay_stamp source = coop_gameplay_current_stamp();
	if (!(Game_mode & GM_MULTI_COOP) || !source.visit || source.frozen ||
	    Network_status != NETSTAT_ENDLEVEL ||
	    (Players[Player_num].connected != CONNECT_END_MENU &&
	     Players[Player_num].connected != CONNECT_DIED_IN_MINE)) return 0;
	if (android_score_catchup.source.visit != source.visit)
		memset(android_score_catchup.next_reply, 0, sizeof(android_score_catchup.next_reply));
	memcpy(android_score_catchup.packet, data, size);
	android_score_catchup.size = size;
	android_score_catchup.source = source;
#ifdef INTROSPECT_ON
	if (android_score_catchup.drop_completion) {
		++android_score_catchup.dropped;
		return 1;
	}
#endif
	return 0;
}

/* The caller has checked packet length, role, session token and sender address.
 * Answer only the preceding ordinary level while waiting for this level's sync.
 * Old statistics, connection states and countdowns never enter the new world */
static void net_udp_score_catchup_reply(const ubyte *data, int size, int peer)
{
	coop_gameplay_stamp sent, current = coop_gameplay_current_stamp();
	const coop_gameplay_stamp *source = &android_score_catchup.source;
	if (!(Game_mode & GM_MULTI_COOP) || Network_status != NETSTAT_WAITING ||
	    !android_score_catchup.size || peer < 0 || peer >= N_players || peer == Player_num ||
	    coop_travel_active() || coop_endgame_active() || multi_save_transfer_restoring() ||
	    (uint32_t) GET_INTEL_INT(android_score_catchup.packet + 1) != netgame_token ||
	    current.level != source->level + 1 ||
	    (multi_i_am_master() ? current.visit - source->visit != 1 || Players[peer].connected != CONNECT_WAITING
	                         : current.visit != source->visit) ||
	    !coop_gameplay_stamp_read(&sent, data + size - COOP_GAMEPLAY_STAMP_BYTES, COOP_GAMEPLAY_STAMP_BYTES) ||
	    sent.frozen || sent.visit != source->visit || sent.level != source->level) return;
	const int state = data[0] == UPID_ENDLEVEL_C ? data[6] : data[6 + peer * 5];
	if (state != CONNECT_END_MENU && state != CONNECT_DIED_IN_MINE) return;
	const fix64 now = timer_query();
	Netgame.players[peer].LastPacketTime = now;
	if (now < android_score_catchup.next_reply[peer]) return;
	android_score_catchup.next_reply[peer] = now + F1_0;
	debug_log(DLOG_COOP_DESYNC, "[COOP] score catch-up: peer=%d source=%d visit=%llu destination=%d",
	          peer, source->level, (unsigned long long) source->visit, current.level);
#ifdef INTROSPECT_ON
	if (android_score_catchup.drop_completion && ++android_score_catchup.replies <= 2) return;
#endif
	dxx_sendto(UDP_Socket[0], android_score_catchup.packet, android_score_catchup.size, 0,
	           (struct sockaddr *) &Netgame.players[peer].protocol.udp.addr, sizeof(struct _sockaddr));
}

#ifdef INTROSPECT_ON
int net_udp_test_score_catchup(int verify)
{
	if (!verify) {
		android_score_catchup.drop_completion = 1;
		android_score_catchup.dropped = android_score_catchup.replies = 0;
		return 1;
	}
	android_score_catchup.drop_completion = 0;
	debug_log(DLOG_COOP_DESYNC, "[COOP] score catch-up test: dropped=%u replies=%u",
	          android_score_catchup.dropped, android_score_catchup.replies);
	return android_score_catchup.dropped && android_score_catchup.replies >= 3;
}
#endif

#endif
