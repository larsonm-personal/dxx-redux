/* Shared Android net_udp P2P/proxy helper bodies. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pstypes.h"
#include "multi.h"
#include "net_udp.h"
#include "net_udp_android.h"
#include "player.h"
#include "timer.h"
#ifdef __ANDROID__
#include "android_log.h"
#endif
#include "net_udp_p2p_proxy_shared.h"

extern void drop_rx_packet(ubyte *data, char *reason);
extern int is_any_player_ip(struct _sockaddr addr);
extern void net_udp_process_packet(ubyte *data, struct _sockaddr sender_addr, int length,
                                   int is_proxy);
extern ushort port_from_sockaddr(struct _sockaddr addr);
extern void net_udp_send_p2p_reattempt_direct(int to_player, int connect_to_player);
extern void net_udp_send_p2p_ping(int to_player, int force_direct, fix64 time);
extern void net_log_comment(char *comment);
extern int net_udp_android_send_raw(const void *data, int len, const struct _sockaddr *addr);
extern uint netgame_token;
extern const ubyte MAX_HOLEPUNCH_ATTEMPTS;
extern struct connection_status connection_statuses[8];
extern fix64 last_direct_attempt[MAX_PLAYERS][MAX_PLAYERS];

static int net_udp_ping_host_player_index(void)
{
	return multi_who_is_master();
}

static void net_udp_android_mpdiag(const char *fmt, ...)
{
	char buf[256];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	net_log_comment(buf);
#ifdef __ANDROID__
	debug_log(DLOG_NETWORK, "[MPDIAG] %s", buf);
#endif
}

void net_udp_send_p2p_pong(int to_player, fix64 time, int direct_ping)
{
	ubyte buf[UPID_P2P_PONG_SIZE];
	int len = 0;

	memset(&buf, 0, UPID_P2P_PONG_SIZE);

	buf[len] = UPID_P2P_PONG;
	len++;
	PUT_INTEL_INT(buf + len, netgame_token);
	len += 4;
	buf[len] = Player_num;
	len++;
	memcpy(buf + len, &time, 8);
	len += 8;
	buf[len] = direct_ping;
	len++;

	if (direct_ping)
		net_udp_send_to_player_direct(buf, sizeof(buf), to_player);
	else
		net_udp_send_to_player(buf, sizeof(buf), to_player);
}

void net_udp_process_p2p_pong(ubyte *data, struct _sockaddr sender_addr, int data_len)
{
	int len = 1;
	int from_player;
	fix64 sent_time;
	int direct_pong;

	(void) data_len;
	len += 4;
	from_player = data[len];
	len++;
	memcpy(&sent_time, data + len, 8);
	len += 8;
	direct_pong = data[len];
	len++;

	if (from_player >= MAX_PLAYERS || from_player == Player_num)
		return;

	Netgame.players[from_player].ping = f2i(fixmul(timer_query() - sent_time, i2f(1000)));

	if (Netgame.players[from_player].ping < 0)
		Netgame.players[from_player].ping = 0;

	if (Netgame.players[from_player].ping > 9999)
		Netgame.players[from_player].ping = 9999;

	if (from_player == multi_who_is_master())
		return;

	if (direct_pong) {
		connection_statuses[from_player].last_direct_pong = timer_query();
		connection_statuses[from_player].type = CONNT_DIRECT;

		if (memcmp(&Netgame.players[from_player].protocol.udp.addr, &sender_addr,
		           sizeof(struct _sockaddr))) {
			update_address_for_player(from_player, sender_addr);
		}
	}
}

void update_address_for_player(int pnum, struct _sockaddr new_addr)
{
	if (!multi_i_am_master() && is_observer())
		return;

	if (port_from_sockaddr(new_addr) != 0 &&
	    memcmp(&new_addr, &Netgame.players[pnum].protocol.udp.addr,
	           sizeof(struct _sockaddr))) {
		memcpy(&Netgame.players[pnum].protocol.udp.addr, &new_addr,
		       sizeof(struct _sockaddr));
	}
}

static void net_udp_log_reset_proxy_transition(int pnum, int old_type)
{
#ifdef __ANDROID__
	net_udp_android_mpdiag("CONNTYPE[resetProxy]: P%d %d->PROXY", pnum, old_type);
#else
	(void) pnum;
	(void) old_type;
#endif
}

void resetProxy(int pnum)
{
	android_net_udp_reset_proxy(pnum, multi_who_is_master(), multi_i_am_master(),
	                            &connection_statuses[pnum], net_udp_log_reset_proxy_transition);
}

void reattemptDirect(int pnum)
{
	android_net_udp_reattempt_direct(pnum, multi_who_is_master(), multi_i_am_master(),
	                                 &connection_statuses[pnum], timer_query());
}

void net_udp_send_to_player(ubyte *data, int len, int to_player)
{
	if (connection_statuses[to_player].type == CONNT_DIRECT)
		net_udp_send_to_player_direct(data, len, to_player);
	else if (connection_statuses[to_player].type == CONNT_PROXY)
		net_udp_send_to_player_proxy(data, len, to_player,
		                             connection_statuses[to_player].proxy_through);
	else
		net_udp_send_to_player_proxy(data, len, to_player, multi_who_is_master());
}

void net_udp_send_to_player_direct(ubyte *data, int len, int to_player)
{
	net_udp_android_send_raw(data, len, &Netgame.players[to_player].protocol.udp.addr);
}

void net_udp_send_to_player_proxy(ubyte *data, int data_len, int to_player, int through_player)
{
	ubyte *buf;
	int len = 0;

	if (connection_statuses[through_player].type != CONNT_DIRECT)
		return;

	buf = malloc(data_len + UPID_PROXY_HEADER_SIZE);

	buf[len] = UPID_PROXY;
	len++;
	PUT_INTEL_INT(buf + len, netgame_token);
	len += 4;
	buf[len] = to_player;
	len++;
	buf[len] = Player_num;
	len++;
	memcpy(buf + len, data, data_len);
	len += data_len;

	net_udp_android_send_raw(buf, len, &Netgame.players[through_player].protocol.udp.addr);
	free(buf);
}

void net_udp_process_proxy(ubyte *data, struct _sockaddr sender_addr, int data_len)
{
	int from_player = data[6];
	int to_player;

	if (from_player < 0 || from_player > MAX_PLAYERS - 1) {
		drop_rx_packet(data, "from invalid player");
		return;
	}

	if (!is_any_player_ip(sender_addr)) {
		drop_rx_packet(data, "from non-player ip");
		return;
	}

	to_player = data[5];

	if (to_player == Player_num) {
		ubyte *contents = data + UPID_PROXY_HEADER_SIZE;

		net_udp_process_packet(contents, sender_addr,
		                       data_len - UPID_PROXY_HEADER_SIZE, 1);
	} else {
		fix64 last_attempt;

		if (connection_statuses[to_player].type != CONNT_DIRECT) {
			drop_rx_packet(data, "proxy to non-direct player");
			return;
		}

		net_udp_send_to_player_direct(data, data_len, to_player);

		last_attempt = last_direct_attempt[from_player][to_player];
		if (timer_query() - last_attempt > F1_0 * 15) {
			net_udp_send_p2p_reattempt_direct(from_player, to_player);
			net_udp_send_p2p_reattempt_direct(to_player, from_player);

			last_direct_attempt[from_player][to_player] = timer_query();
			last_direct_attempt[to_player][from_player] = timer_query();
		}
	}
}

void net_udp_p2p_ping_frame(fix64 time)
{
	static fix64 lastPing[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	fix64 pingTimeSetup = F1_0 / 10;
	fix64 pingTimeHeartbeat = F1_0;
	int i;

	for (i = 0; i < MAX_PLAYERS; i++) {
		int sentping = 0;

		if (i == Player_num)
			continue;
		if (!Players[i].connected)
			continue;

		if (is_observer() && !multi_i_am_master() && i != multi_who_is_master())
			continue;

		if (connection_statuses[i].holepunch_attempts < MAX_HOLEPUNCH_ATTEMPTS) {
			if (time > lastPing[i] + pingTimeSetup) {
				net_udp_send_p2p_ping(i, 1, time);
				sentping = 1;
				connection_statuses[i].holepunch_attempts++;
			}
		}

		if (time > lastPing[i] + pingTimeHeartbeat) {
			net_udp_send_p2p_ping(i, 0, time);
			sentping = 1;
		}

		if (sentping)
			lastPing[i] = time;
	}
}

void net_udp_process_ping(ubyte *data, int data_len, struct _sockaddr sender_addr)
{
	fix64 host_ping_time = 0;
	ubyte buf[UPID_PONG_SIZE];
	int i;
	int len = 0;
	int host_player_num = net_udp_ping_host_player_index();

	(void) data_len;
	if (memcmp((struct _sockaddr *) &Netgame.players[host_player_num].protocol.udp.addr,
	           (struct _sockaddr *) &sender_addr, sizeof(struct _sockaddr)))
		return;

	len++;
	memcpy(&host_ping_time, &data[len], 8);
	len += 8;
	for (i = 1; i < MAX_PLAYERS; i++) {
		Netgame.players[i].ping = GET_INTEL_INT(&(data[len]));
		len += 4;
	}

	/* Prevent clients from timing out the host during level sync or other
	 * periods when PDATA isn't flowing. Pings prove the host is reachable. */
	Netgame.players[host_player_num].LastPacketTime = timer_query();

	buf[0] = UPID_PONG;
	buf[1] = Player_num;
	memcpy(&buf[2], &host_ping_time, 8);

	net_udp_android_send_raw(buf, sizeof(buf), &sender_addr);
}
