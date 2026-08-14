#include "boss_health_shared.h"
#include "boss_hud.h"
#include "ai.h"
#include "game.h"
#include "hudmsg.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"
#include "multi.h"
#include "newdemo.h"
#include "playsave.h"
#include "screens.h"
#include "text.h"
#include "window.h"

extern fix Gate_interval;
#ifdef DXX_BUILD_DESCENT_II
extern int Boss_invulnerable_dot;
#endif

static int difficulty_clamp(int difficulty)
{
	if (difficulty < 0)
		return 0;
	if (difficulty >= NDL)
		return NDL - 1;
	return difficulty;
}

void difficulty_refresh_runtime_parameters(void)
{
#ifdef DXX_BUILD_DESCENT_II
	Gate_interval = F1_0 * 4 - Difficulty_level * i2f(2) / 3;
	Boss_invulnerable_dot = F1_0 / 4 - i2f(Difficulty_level) / 8;
#else
	Gate_interval = F1_0 * 5 - Difficulty_level * F1_0 / 2;
#endif
}

void difficulty_reset_history(void)
{
	Difficulty_level_changed = 0;
	Difficulty_level_min_seen = difficulty_clamp(Difficulty_level);
	Difficulty_level_max_seen = Difficulty_level_min_seen;
}

void difficulty_include_current(void)
{
	int difficulty = difficulty_clamp(Difficulty_level);
	if (Difficulty_level_min_seen < 0 || Difficulty_level_min_seen >= NDL)
		Difficulty_level_min_seen = difficulty;
	if (Difficulty_level_max_seen < 0 || Difficulty_level_max_seen >= NDL)
		Difficulty_level_max_seen = difficulty;
	if (difficulty < Difficulty_level_min_seen)
		Difficulty_level_min_seen = difficulty;
	if (difficulty > Difficulty_level_max_seen)
		Difficulty_level_max_seen = difficulty;
}

void difficulty_restore_history(int changed, int min_level, int max_level)
{
	min_level = difficulty_clamp(min_level);
	max_level = difficulty_clamp(max_level);
	if (min_level > max_level) {
		min_level = difficulty_clamp(Difficulty_level);
		max_level = min_level;
		changed = 0;
	}
	Difficulty_level_changed = changed ? 1 : 0;
	Difficulty_level_min_seen = min_level;
	Difficulty_level_max_seen = max_level;
	difficulty_include_current();
}

int difficulty_can_show_live(void)
{
	if (!Game_wind || Screen_mode != SCREEN_GAME)
		return 0;
	if (Newdemo_state == ND_STATE_PLAYBACK || input_demo_replay_is_loaded())
		return 0;
	if ((Game_mode & GM_MULTI) && !(Game_mode & GM_MULTI_COOP))
		return 0;
	return 1;
}

int difficulty_can_change_live(void)
{
	if (!difficulty_can_show_live())
		return 0;
	if ((Game_mode & GM_MULTI_COOP) && !multi_i_am_master())
		return 0;
	return 1;
}

int difficulty_change_to(int difficulty, int flags)
{
	int old_difficulty = Difficulty_level;
	char error[128] = "";

	if (difficulty < 0 || difficulty >= NDL)
		return 0;
	if (!(flags & (DIFFICULTY_CHANGE_FROM_NETWORK | DIFFICULTY_CHANGE_FROM_REPLAY)) &&
	    !difficulty_can_change_live())
		return 0;
	if ((flags & DIFFICULTY_CHANGE_FROM_NETWORK) &&
	    ((Game_mode & GM_MULTI) && !(Game_mode & GM_MULTI_COOP)))
		return 0;
	if (difficulty == old_difficulty)
		return 1;

	difficulty_health_rescale_live_robots(old_difficulty, difficulty);
	Difficulty_level = difficulty;
	difficulty_refresh_runtime_parameters();
	boss_hud_refresh_maximum();
	if (Game_mode & GM_MULTI)
		Netgame.difficulty = (ubyte) difficulty;
	if (!(flags & DIFFICULTY_CHANGE_FROM_REPLAY)) {
		PlayerCfg.DefaultDifficulty = difficulty;
		write_player_file();
	}

	Difficulty_level_changed = 1;
	difficulty_include_current();

	if ((flags & DIFFICULTY_CHANGE_RECORD_DEMO) &&
	    Newdemo_state == ND_STATE_RECORDING &&
	    input_demo_recorder_is_active()) {
		input_demo_recorder_stage_direct_command_change_difficulty(
		    difficulty, error, sizeof(error));
	}

	if ((Game_mode & GM_MULTI_COOP) &&
	    !(flags & (DIFFICULTY_CHANGE_FROM_NETWORK | DIFFICULTY_CHANGE_FROM_REPLAY))) {
		multi_send_difficulty(difficulty);
	}

	HUD_init_message(HM_DEFAULT, "Difficulty changed to %s",
	                 MENU_DIFFICULTY_TEXT(difficulty));
	return 1;
}
