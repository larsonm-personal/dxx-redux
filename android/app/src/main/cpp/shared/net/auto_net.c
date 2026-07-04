/*
 * Auto-join/auto-host globals and main-menu check for Android matchmaking.
 * The actual networking logic lives in net_udp Android shared helpers.
 */

#ifdef __ANDROID__

#include <limits.h>
#include <string.h>

#include "auto_net.h"

#include "args.h"
#include "config.h"
#include "console.h"
#include "coop_save.h"
#include "physfsx.h"
#include "player.h"
#include "playsave.h"
#include "strutil.h"
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
int auto_host_player_spew_no_expire = 1;
int auto_host_clients_can_request_rewind = 0;

char auto_net_callsign[10] = "";
char auto_net_client_id[AUTO_NET_CLIENT_ID_LEN] = "";

int auto_net_is_transient_callsign(const char *callsign)
{
	return auto_net_callsign[0] && callsign && !d_stricmp(callsign, auto_net_callsign);
}

static int auto_net_can_load_pilot_callsign(const char *callsign, int allow_transient)
{
	return callsign && callsign[0] &&
	       d_stricmp(callsign, COOP_AUTOSAVE_CALLSIGN) &&
	       (allow_transient || !auto_net_is_transient_callsign(callsign));
}

static int auto_net_is_real_pilot_callsign(const char *callsign)
{
	return auto_net_can_load_pilot_callsign(callsign, 0);
}

static int auto_net_find_fallback_pilot(char *callsign)
{
	char **list;
	char **entry;
	int found = 0;
	static const char *const types[] = { ".plr", NULL };

	if (!callsign)
		return 0;
	callsign[0] = '\0';
	list = PHYSFSX_findFiles(GameArg.SysUsePlayersDir ? "Players/" : "", types);
	if (!list)
		return 0;
	for (entry = list; *entry; entry++) {
		char candidate[CALLSIGN_LEN + 1];
		char filename[PATH_MAX];
		char *dot = strstr(*entry, ".plr");
		size_t len;

		if (!dot || dot == *entry || dot[4])
			continue;
		len = (size_t) (dot - *entry);
		if (len > CALLSIGN_LEN)
			continue;
		memset(candidate, 0, sizeof(candidate));
		memcpy(candidate, *entry, len);
		if (!auto_net_is_real_pilot_callsign(candidate))
			continue;
		snprintf(filename, sizeof(filename), GameArg.SysUsePlayersDir ? "Players/%s" : "%s", *entry);
		if (!plr_is_selectable(filename))
			continue;
		strncpy(callsign, candidate, CALLSIGN_LEN);
		callsign[CALLSIGN_LEN] = '\0';
		found = 1;
		break;
	}
	PHYSFS_freeList(list);
	return found;
}

static int auto_net_load_prefs_from_callsign(const char *callsign, int allow_transient)
{
	char original[CALLSIGN_LEN + 1];
	int result;

	if (!auto_net_can_load_pilot_callsign(callsign, allow_transient))
		return 0;
	memcpy(original, Players[Player_num].callsign, sizeof(original));
	strncpy(Players[Player_num].callsign, callsign, CALLSIGN_LEN);
	Players[Player_num].callsign[CALLSIGN_LEN] = '\0';
	result = read_player_file() == EZERO;
	memcpy(Players[Player_num].callsign, original, sizeof(original));
	if (result && !auto_net_is_transient_callsign(callsign)) {
		strncpy(GameCfg.LastPlayer, callsign, CALLSIGN_LEN);
		GameCfg.LastPlayer[CALLSIGN_LEN] = '\0';
	}
	return result;
}

static int auto_net_load_recent_pilot_prefs(void)
{
	char fallback[CALLSIGN_LEN + 1];
	int last_is_transient = auto_net_is_transient_callsign(GameCfg.LastPlayer);

	if (!last_is_transient && auto_net_load_prefs_from_callsign(GameCfg.LastPlayer, 0))
		return 1;
	if (auto_net_find_fallback_pilot(fallback))
		return auto_net_load_prefs_from_callsign(fallback, 0);
	if (last_is_transient)
		return auto_net_load_prefs_from_callsign(GameCfg.LastPlayer, 1);
	return 0;
}

int auto_create_pilot(void)
{
	int loaded_recent_prefs;

	if (!auto_net_callsign[0])
		return 0;
	if (!(auto_join_pending || auto_host_pending))
		return 0;
	if (Players[Player_num].callsign[0] != 0)
		return 0;

	con_printf(CON_NORMAL, "auto_net: creating pilot '%s'\n", auto_net_callsign);
	strncpy(Players[Player_num].callsign, auto_net_callsign, CALLSIGN_LEN);
	Players[Player_num].callsign[CALLSIGN_LEN] = 0;
	loaded_recent_prefs = auto_net_load_recent_pilot_prefs();
	if (!loaded_recent_prefs)
		new_player_config();

#ifdef ANDROID
	if (!loaded_recent_prefs) {
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
		                       "(mission=%s mode=%d diff=%d max=%d lvl=%d qol=%d spew=%d "
		                       "player_spew_no_expire=%d client_rewind=%d)\n",
		           auto_host_my_port, auto_host_mission, auto_host_mode,
		           auto_host_difficulty, auto_host_max_players,
		           auto_host_level_num, auto_host_coop_qol,
		           auto_host_full_death_spew,
		           auto_host_player_spew_no_expire,
		           auto_host_clients_can_request_rewind);
		net_udp_auto_host(auto_host_my_port, auto_host_mission,
		                  auto_host_mode, auto_host_difficulty,
		                  auto_host_max_players, auto_host_level_num,
		                  auto_host_coop_qol, auto_host_full_death_spew,
		                  auto_host_player_spew_no_expire);
		return 1;
	}

	return 0;
}

#endif /* __ANDROID__ */
