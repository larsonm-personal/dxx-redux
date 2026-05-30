/*
 * Auto-join/auto-host globals and main-menu check for Android matchmaking.
 * The actual networking logic lives in net_udp Android shared helpers.
 */

#ifdef __ANDROID__

#include <string.h>

#include "auto_net.h"

#include "console.h"
#include "player.h"
#include "playsave.h"
#include "net_udp_android_autonet_shared.h"

/* --- globals, set by JNI before the main menu opens --- */

int auto_join_pending = 0;
char auto_join_host_addr[AUTO_NET_ADDR_LEN] = "127.0.0.1";
int auto_join_host_port = 42430;
int auto_join_my_port = 42424;

int auto_host_pending = 0;
int auto_host_my_port = 42424;
char auto_host_mission[64] = "";
int auto_host_mode = 0;
int auto_host_max_players = 4;
int auto_host_level_num = 1;
int auto_host_difficulty = 1;
int auto_host_coop_qol = 1;
int auto_host_full_death_spew = 1;
int auto_host_clients_can_request_rewind = 0;

char auto_net_callsign[10] = "";
char auto_net_client_id[AUTO_NET_CLIENT_ID_LEN] = "";

int auto_create_pilot(void)
{
	if (!auto_net_callsign[0])
		return 0;
	if (!(auto_join_pending || auto_host_pending))
		return 0;
	if (Players[Player_num].callsign[0] != 0)
		return 0;

	con_printf(CON_NORMAL, "auto_net: creating pilot '%s'\n", auto_net_callsign);
	strncpy(Players[Player_num].callsign, auto_net_callsign, CALLSIGN_LEN);
	Players[Player_num].callsign[CALLSIGN_LEN] = 0;
	new_player_config();

#ifdef ANDROID
	{
		extern void android_apply_gamepad_defaults(void);

		android_apply_gamepad_defaults();
	}
#endif

	write_player_file();
	return 1;
}

int check_auto_net(void)
{
	if (auto_join_pending) {
		auto_join_pending = 0;
		con_printf(CON_NORMAL, "auto_net: starting auto-join to %s:%d (my port %d)\n",
		           auto_join_host_addr, auto_join_host_port, auto_join_my_port);
		net_udp_auto_join(auto_join_host_addr, auto_join_host_port,
		                  auto_join_my_port);
		return 1;
	}

	if (auto_host_pending) {
		/* auto_host_pending is cleared early in net_udp_select_players
		 * (transferred to the file-scope net_auto_start_when_full flag).
		 * This prevents re-entry when the select-players menu closes and
		 * EVENT_WINDOW_ACTIVATED re-fires on the main menu. */
		con_printf(CON_NORMAL, "auto_net: starting auto-host on port %d "
		                       "(mission=%s mode=%d diff=%d max=%d lvl=%d qol=%d spew=%d client_rewind=%d)\n",
		           auto_host_my_port, auto_host_mission, auto_host_mode,
		           auto_host_difficulty, auto_host_max_players,
		           auto_host_level_num, auto_host_coop_qol,
		           auto_host_full_death_spew,
		           auto_host_clients_can_request_rewind);
		net_udp_auto_host(auto_host_my_port, auto_host_mission,
		                  auto_host_mode, auto_host_difficulty,
		                  auto_host_max_players, auto_host_level_num,
		                  auto_host_coop_qol, auto_host_full_death_spew);
		return 1;
	}

	return 0;
}

#endif /* __ANDROID__ */