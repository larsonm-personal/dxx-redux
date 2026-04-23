#include "net_udp_android.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include "android_crash_handler.h"
#include "android_log.h"
#endif

#include "strutil.h"
#include "player.h"
#include "multi.h"
#include "net_udp.h"

#ifdef __ANDROID__
void android_net_udp_mpdiag_pkt_dump(const char *label, const ubyte *buf, int len)
{
	static char msg[4096];
	int pos;
	int i;

	crash_breadcrumb_v("pktdump: %s len=%d", label, len);
	pos = snprintf(msg, sizeof(msg), "%s len=%d ", label, len);
	for (i = 0; i < len && pos + 2 < (int) sizeof(msg); i++)
		pos += snprintf(msg + pos, sizeof(msg) - pos, "%02x", buf[i]);
	crash_breadcrumb("pktdump: hex done, calling debug_log");
	debug_log(DLOG_NETWORK, "[PKTDUMP] %s", msg);
	crash_breadcrumb("pktdump: done");
}
#endif

static int android_net_udp_sockaddr_ip_equal(const struct _sockaddr *a,
                                             const struct _sockaddr *b)
{
#ifdef IPv6
	return memcmp(&a->sin6_addr, &b->sin6_addr, sizeof(a->sin6_addr)) == 0;
#else
	return a->sin_addr.s_addr == b->sin_addr.s_addr;
#endif
}

int android_net_udp_sockaddr_equal(const struct _sockaddr *a,
                                   const struct _sockaddr *b)
{
	const struct sockaddr_in *sa = (const struct sockaddr_in *) a;
	const struct sockaddr_in *sb = (const struct sockaddr_in *) b;

	return sa->sin_addr.s_addr == sb->sin_addr.s_addr &&
	       sa->sin_port == sb->sin_port;
}

int android_net_udp_find_player_by_identity(const char *callsign,
                                            struct _sockaddr *addr, int player_count, const struct player *players,
                                            const struct netgame_info *netgame)
{
	int i;

	for (i = 0; i < player_count; i++) {
		if (!d_stricmp(players[i].callsign, callsign) &&
		    android_net_udp_sockaddr_ip_equal(addr,
		                                      (struct _sockaddr *) &netgame->players[i].protocol.udp.addr))
			return i;
	}
#ifdef __ANDROID__
	for (i = 0; i < player_count; i++) {
		if (!players[i].connected &&
		    !d_stricmp(players[i].callsign, callsign))
			return i;
	}
#endif
	return -1;
}

int android_net_udp_rebind_for_hosting(int *udp_socket, int *udp_bind_loopback,
                                       const char *udp_my_port, unsigned int netgame_token,
                                       void (*close_socket)(int), int (*open_socket)(int, int),
                                       android_net_udp_log_message_fn log_message)
{
	char logbuf[128];
	int port;

	if (udp_socket[0] == -1)
		return -1;
	if (!*udp_bind_loopback)
		return 0;

	port = atoi(udp_my_port);
	snprintf(logbuf, sizeof(logbuf), "rebind_for_hosting: closing loopback socket, reopening on 0.0.0.0:%d", port);
	log_message(logbuf);
	close_socket(0);
	*udp_bind_loopback = 0;
	if (open_socket(0, port) != 0) {
		snprintf(logbuf, sizeof(logbuf), "rebind_for_hosting: FAILED to reopen socket on port %d", port);
		log_message(logbuf);
		return -1;
	}
	snprintf(logbuf, sizeof(logbuf), "rebind_for_hosting: socket=%d now on 0.0.0.0:%d token=%u",
	         udp_socket[0], port, netgame_token);
	log_message(logbuf);
	return 0;
}

void android_net_udp_send_p2p_reattempt_direct(unsigned int netgame_token,
                                               int player_num,
                                               const struct _sockaddr *connect_to_addr,
                                               int to_player,
                                               android_net_udp_send_direct_fn send_direct)
{
	ubyte buf[UPID_REATTEMPT_DIRECT_SIZE];
	int len = 0;

	if (!connect_to_addr || !send_direct)
		return;

	memset(buf, 0, sizeof(buf));
	buf[len] = UPID_REATTEMPT_DIRECT;
	len++;
	PUT_INTEL_INT(buf + len, netgame_token);
	len += 4;
	buf[len] = player_num;
	len++;
	memcpy(buf + len, connect_to_addr, sizeof(*connect_to_addr));

	send_direct(buf, sizeof(buf), to_player);
}

void android_net_udp_reattempt_direct(int pnum,
                                      int master_player_num,
                                      int i_am_master,
                                      struct connection_status *status,
                                      fix64 now)
{
	if (!status)
		return;
	if (pnum == master_player_num)
		return;
	if (i_am_master)
		return;

	status->holepunch_attempts = 0;
	status->last_direct_pong = now;
}

void android_net_udp_process_p2p_reattempt_direct(const ubyte *data,
                                                  int player_num,
                                                  int master_player_num,
                                                  int i_am_master,
                                                  int max_players,
                                                  fix64 now,
                                                  struct connection_status *statuses,
                                                  android_net_udp_log_message_fn log_message,
                                                  android_net_udp_update_address_fn update_address)
{
	struct _sockaddr new_address;
	int len = 0;
	int pnum;

	len++;
	len += 4;
	pnum = data[len];
	len++;

	if (pnum == master_player_num) {
		if (log_message)
			log_message("Attempting reconnect to master, illegal.");
		return;
	}

	if (pnum == player_num) {
		if (log_message)
			log_message("Attempting reconnect to self, illegal.");
		return;
	}

	if (pnum >= max_players) {
		if (log_message)
			log_message("Attempting connection to illegal player num.");
		return;
	}

	memcpy(&new_address, data + len, sizeof(new_address));
	if (update_address)
		update_address(pnum, new_address);
	android_net_udp_reattempt_direct(pnum, master_player_num, i_am_master,
	                                 statuses ? &statuses[pnum] : NULL, now);
}

void android_net_udp_reset_proxy(int pnum,
                                 int master_player_num,
                                 int i_am_master,
                                 struct connection_status *status,
                                 android_net_udp_log_connection_status_fn log_connection_status)
{
	if (!status)
		return;
	if (pnum == master_player_num)
		return;
	if (i_am_master)
		return;
	if (log_connection_status)
		log_connection_status(pnum, status->type);
	status->type = CONNT_PROXY;
	status->proxy_through = 0;
}