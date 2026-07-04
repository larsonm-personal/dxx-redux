/* Shared Android net_udp auto-join/host helper bodies. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pstypes.h"
#include "mission.h"
#include "multi.h"
#include "net_udp.h"
#include "player.h"
#include "text.h"
#include "timer.h"
#ifdef __ANDROID__
#include "android_log.h"
#endif
#include "net_udp_android_autonet_shared.h"

extern int udp_open_socket(int socknum, int port);
extern int udp_dns_filladdr(char *pszAddress, int port, struct _sockaddr *addr);
extern void net_udp_init(void);
extern void net_udp_close(void);
extern void net_udp_request_game_info(struct _sockaddr game_addr, int lite);
extern void net_udp_listen(void);
extern void net_udp_reset_connection_statuses(void);
extern int net_udp_do_join_game(ubyte join_as_obs);
extern void netgame_set_defaults(void);
extern int net_udp_start_game(void);
extern void net_udp_android_set_my_port(int my_port);
extern void net_udp_android_set_bind_loopback(int bind_loopback);
extern void net_log_comment(char *comment);

static int net_udp_android_restore_host_addr_player_index(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return multi_who_is_master();
#else
	return 0;
#endif
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

/*
 * Auto-join: connect to a host at host_addr:host_port without any UI.
 * Called from check_auto_net() when the launcher sets auto_join_pending.
 * Polls for game info with a 30-second timeout, then joins directly.
 * Returns 1 on success, 0 on failure.
 */
int net_udp_auto_join(const char *host_addr, int host_port, int my_port)
{
	struct _sockaddr host;
	fix64 start_time, last_req;
	char logbuf[128];
	int req_count = 0;
	int host_player_num;
	/* android port: check exit button during the blocking poll loop */
	extern volatile int android_force_quit;

	snprintf(logbuf, sizeof(logbuf), "auto_join: host=%s:%d my_port=%d", host_addr, host_port, my_port);
	net_log_comment(logbuf);

	multi_protocol = MULTI_PROTO_UDP;
	net_udp_init();
	net_udp_reset_connection_statuses();
	net_udp_android_set_my_port(my_port);
	/* Bind to loopback only when connecting to localhost (same-device proxy).
	 * For cross-device connections (e.g. emulator relay via 10.0.2.2),
	 * bind to INADDR_ANY so packets route through the virtual network. */
	net_udp_android_set_bind_loopback((strncmp(host_addr, "127.", 4) == 0 ||
	                                   strcmp(host_addr, "localhost") == 0)
	                                      ? 1
	                                      : 0);

	if (udp_open_socket(0, my_port) != 0) {
		net_udp_android_mpdiag("auto_join: failed to open socket on port %d", my_port);
		return 0;
	}

	if (udp_dns_filladdr((char *) host_addr, host_port, &host) < 0) {
		net_udp_android_mpdiag("auto_join: failed to resolve %s:%d", host_addr, host_port);
		net_udp_close();
		return 0;
	}

	multi_new_game();
	net_udp_reset_connection_statuses();
	N_players = 0;
	change_playernum_to(1);

	memcpy(&Netgame.players[0].protocol.udp.addr, &host, sizeof(struct _sockaddr));

	Netgame.protocol.udp.valid = 0;
	start_time = timer_query();
	last_req = 0;

	/* Poll for game info -- 30 second timeout */
	while (timer_query() < start_time + F1_0 * 30) {
		timer_update();

		/* android port: exit button sets this flag via JNI; break out
		 * immediately so the SDL_QUIT event gets processed after we
		 * return to the main menu event loop */
		if (android_force_quit) {
			net_udp_android_mpdiag("auto_join: aborted by exit button after %d reqs", req_count);
			net_udp_close();
			return 0;
		}

		if (timer_query() >= last_req + F1_0) {
			net_udp_request_game_info(host, 0);
			req_count++;
			last_req = timer_query();
		}

		timer_delay2(5);
		net_udp_listen();

		if (Netgame.protocol.udp.valid == -1) {
			net_udp_android_mpdiag("auto_join: version mismatch");
			net_udp_close();
			return 0;
		}

		if (Netgame.protocol.udp.valid == 1) {
			/* net_udp_process_game_info overwrites the chosen host slot's
			 * address with the GAME_INFO reply sender. Inside emulator and
			 * relay setups this is a NAT-mapped address, not the relay/proxy
			 * address we originally resolved. Restore the per-game host slot
			 * so later packets keep routing through the relay. */
			host_player_num = net_udp_android_restore_host_addr_player_index();
			memcpy(&Netgame.players[host_player_num].protocol.udp.addr, &host,
			       sizeof(struct _sockaddr));
			/* android port: after host migration the master slot may be
			 * non-zero. auto_join sets Player_num=1 above, so
			 * multi_i_am_master() would return true and valid_sender()
			 * would drop all OBJECT_DATA packets. Adjust if needed. */
			if (Player_num == multi_who_is_master()) {
				int pnum = (multi_who_is_master() + 1) % MAX_PLAYERS;
				net_udp_android_mpdiag("auto_join: adjusted Player_num %d->%d (master=%d)",
				                       Player_num, pnum, multi_who_is_master());
				change_playernum_to(pnum);
			}
			return net_udp_do_join_game(0);
		}
	}

	net_udp_android_mpdiag("auto_join: timeout waiting for host (sent %d reqs)", req_count);
	net_udp_close();
	return 0;
}

/*
 * Auto-host: load mission, configure game params, open sockets, then
 * enter the player-select screen (host clicks "Start Game" manually).
 * Returns 1 on success, 0 on failure.
 */
int net_udp_auto_host(int my_port, const char *mission, int mode,
                      int difficulty, int max_players, int level_num,
                      int coop_qol, int full_death_spew,
                      int player_spew_no_expire)
{
	multi_protocol = MULTI_PROTO_UDP;
	net_udp_init();
	net_udp_reset_connection_statuses();
	change_playernum_to(0);

	net_udp_android_set_my_port(my_port);
	net_udp_android_set_bind_loopback(0); /* host binds to INADDR_ANY for cross-device joins */

	netgame_set_defaults();

	/* Load the requested mission */
	if (!load_mission_by_name((char *) mission)) {
		net_udp_android_mpdiag("auto_host: mission '%s' not found", mission);
		return 0;
	}

	/* Set game parameters */
	Netgame.gamemode = mode;
	Netgame.difficulty = difficulty;
	Netgame.max_numplayers = max_players;
	Netgame.levelnum = level_num;
#ifdef __ANDROID__
	if (mode == NETGAME_COOPERATIVE)
		debug_log(DLOG_COOP_DESYNC,
		          "[COOP] auto_host Netgame assigned: mission=%s current_mission=%s mode=%d diff=%d max=%d level=%d player_num=%d callsign='%s'",
		          mission, Current_mission_filename, Netgame.gamemode,
		          Netgame.difficulty, Netgame.max_numplayers,
		          Netgame.levelnum, Player_num, Players[Player_num].callsign);
#endif
	if (coop_qol)
		Netgame.game_flags |= NETGAME_FLAG_COOP_QOL;
	else
		Netgame.game_flags &= ~NETGAME_FLAG_COOP_QOL;
	Netgame.FullDeathSpew = full_death_spew ? 1 : 0;
	Netgame.PlayerSpewNoExpire = player_spew_no_expire ? 1 : 0;
	Netgame.RefusePlayers = 1; /* android port: require host approval for mid-game joins */
	strcpy(Netgame.mission_name, Current_mission_filename);
	strcpy(Netgame.mission_title, Current_mission_longname);
	sprintf(Netgame.game_name, "%s%s", Players[Player_num].callsign, TXT_S_GAME);

	/* This mirrors net_udp_start_game but we call it directly so we
	 * enter select_players and the host can click Start. */
	return net_udp_start_game();
}
