#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "args.h"
#include "config.h"
#include "console.h"
#include "coop_save.h"
#include "physfsx.h"
#include "player.h"
#include "playsave.h"
#include "state.h"
#include "strutil.h"

#include "android_log.h"
#include "android_resume_pilot.h"

static int android_is_real_pilot_callsign(const char *callsign)
{
	return callsign && callsign[0] && d_stricmp(callsign, COOP_AUTOSAVE_CALLSIGN);
}

static int android_find_fallback_pilot_callsign(char *callsign)
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
		if (!android_is_real_pilot_callsign(candidate))
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

void android_repair_player_callsign_for_autosave(const char *game_name)
{
	char fallback[CALLSIGN_LEN + 1];
	const char *repair_callsign = GameCfg.LastPlayer;
	const char *repair_source = "last_player";

	if (android_is_real_pilot_callsign(Players[Player_num].callsign))
		return;
	if (!android_is_real_pilot_callsign(repair_callsign)) {
		if (android_find_fallback_pilot_callsign(fallback)) {
			repair_callsign = fallback;
			repair_source = "player_file";
		} else {
			repair_callsign = "player";
			repair_source = "default";
		}
	}
	debug_log(DLOG_GAME,
	          "autosave repair: game=%s current='%s' last='%s' repair='%s' source=%s",
	          game_name, Players[Player_num].callsign, GameCfg.LastPlayer,
	          repair_callsign, repair_source);
	strncpy(Players[Player_num].callsign, repair_callsign, CALLSIGN_LEN);
	Players[Player_num].callsign[CALLSIGN_LEN] = '\0';
	strncpy(GameCfg.LastPlayer, repair_callsign, CALLSIGN_LEN);
	GameCfg.LastPlayer[CALLSIGN_LEN] = '\0';
}

int android_load_pilot_from_resume_save(const char *save_path, const char *game_name)
{
	char save_callsign[CALLSIGN_LEN + 1];
	char filename[PATH_MAX] = "";
	const char *load_source = "current";
	int j;

	if (!save_path || !save_path[0]) {
		debug_log(DLOG_GAME, "startup resume prep: game=%s result=failed reason=empty_path",
		          game_name);
		return 0;
	}
	if (!state_get_save_file_callsign((char *) save_path, save_callsign, sizeof(save_callsign))) {
		int can_fallback = android_is_real_pilot_callsign(Players[Player_num].callsign);

		con_printf(CON_URGENT, "startup resume: could not read save callsign\n");
		debug_log(DLOG_GAME,
		          "startup resume prep: game=%s path='%s' result=%s reason=save_callsign_unreadable current='%s'",
		          game_name, save_path, can_fallback ? "fallback_current" : "failed",
		          Players[Player_num].callsign);
		return can_fallback;
	}
	if (!d_stricmp(save_callsign, COOP_AUTOSAVE_CALLSIGN)) {
		int can_fallback = android_is_real_pilot_callsign(Players[Player_num].callsign);

		con_printf(CON_URGENT, "startup resume: save uses sentinel pilot '%s'\n", save_callsign);
		debug_log(DLOG_GAME,
		          "startup resume prep: game=%s path='%s' result=%s reason=sentinel_save current='%s'",
		          game_name, save_path, can_fallback ? "fallback_current" : "failed",
		          Players[Player_num].callsign);
		return can_fallback;
	}
	if (strcmp(Players[Player_num].callsign, save_callsign)) {
		if (GameArg.SysUsePlayersDir)
			strcpy(filename, "Players/");
		strncat(filename, save_callsign, CALLSIGN_LEN);
		filename[(GameArg.SysUsePlayersDir ? 8 : 0) + CALLSIGN_LEN] = '\0';
		for (j = GameArg.SysUsePlayersDir ? 8 : 0; filename[j] != '\0'; j++) {
			if (filename[j] == ' ')
				filename[j] = '\0';
		}
		if (!strstr(filename, ".plr"))
			strcat(filename, ".plr");
		if (!PHYSFSX_exists(filename, 0)) {
			con_printf(CON_URGENT,
			           "startup resume: pilot file not found for '%s'; using save callsign\n",
			           save_callsign);
			strcpy(Players[Player_num].callsign, save_callsign);
			load_source = "save_header";
		} else if (!plr_is_selectable(filename)) {
			con_printf(CON_URGENT,
			           "startup resume: pilot file not selectable for '%s'; using save callsign\n",
			           save_callsign);
			strcpy(Players[Player_num].callsign, save_callsign);
			load_source = "save_header";
		} else {
			*strstr(filename, ".plr") = '\0';
			strcpy(Players[Player_num].callsign, GameArg.SysUsePlayersDir ? &filename[8] : filename);
			read_player_file();
			WriteConfigFile();
			load_source = "player_file";
		}
	}
	debug_log(DLOG_GAME,
	          "startup resume prep: game=%s path='%s' result=ready callsign='%s' source=%s",
	          game_name, save_path, Players[Player_num].callsign, load_source);
	return 1;
}
