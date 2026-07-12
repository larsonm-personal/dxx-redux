#include "startup_resume_shared.h"

#include "args.h"
#include "console.h"
#include "state.h"
#include "strutil.h"

#ifdef __ANDROID__
#include "game.h"
#include "player.h"

#include "android_log.h"
#include "android_resume_pilot.h"
#endif

int startup_find_cmd_arg(const char *name)
{
	int i;

	for (i = 1; i < Num_args; ++i)
		if (!d_stricmp(Args[i], name))
			return i;
	return 0;
}

void startup_apply_pilot_arg(void)
{
#ifdef __ANDROID__
	const int arg_index = startup_find_cmd_arg("-pilot");

	if (GameArg.SysPilot || !arg_index)
		return;
	if (arg_index + 1 >= Num_args || !Args[arg_index + 1] ||
	    !Args[arg_index + 1][0] || Args[arg_index + 1][0] == '-')
		return;
	GameArg.SysPilot = Args[arg_index + 1];
#endif
}

int startup_resume_save_from_cmdline(const char *game_name)
{
	const int arg_index = startup_find_cmd_arg("-resume-save");
	int restored;

	if (!arg_index)
		return 0;
	if (arg_index + 1 >= Num_args || !Args[arg_index + 1] ||
	    !Args[arg_index + 1][0]) {
		con_printf(CON_URGENT, "startup resume: missing save path\n");
		return 0;
	}

#ifdef __ANDROID__
	debug_log(DLOG_GAME,
	          "startup resume check: game=%s Num_args=%d arg_index=%d current_callsign='%s'",
	          game_name, Num_args, arg_index, Players[Player_num].callsign);
	if (!android_load_pilot_from_resume_save(Args[arg_index + 1], game_name)) {
		con_printf(CON_URGENT,
		           "startup resume: could not prepare pilot for '%s'\n",
		           Args[arg_index + 1]);
		debug_log(DLOG_GAME,
		          "startup resume aborted: could not prepare pilot for '%s'",
		          Args[arg_index + 1]);
		return 0;
	}
#endif
	con_printf(CON_NORMAL, "startup resume: restoring '%s'\n",
	           Args[arg_index + 1]);
#ifdef __ANDROID__
	debug_log(DLOG_GAME,
	          "startup resume restore begin: game=%s path='%s' callsign='%s'",
	          game_name, Args[arg_index + 1], Players[Player_num].callsign);
#endif
	restored = state_restore_all_path(0, Args[arg_index + 1]);
#ifdef __ANDROID__
	debug_log(DLOG_GAME,
	          "startup resume restore result: game=%s path='%s' restored=%d callsign='%s'",
	          game_name, Args[arg_index + 1], restored,
	          Players[Player_num].callsign);
#endif
	if (!restored)
		con_printf(CON_URGENT, "startup resume: restore failed for '%s'\n",
		           Args[arg_index + 1]);
#ifdef __ANDROID__
	if (restored)
		game_flush_inputs();
#endif
	return restored;
}
