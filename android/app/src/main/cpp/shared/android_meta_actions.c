/*
 * android_meta_actions.c -- Dispatch table for extra controller/touch controls.
 *
 * Meta actions inject SDL key sequences into the event queue, bypassing
 * kc_joystick[].  This lets us map controller/touch buttons to game
 * functions that are normally keyboard-only (quicksave, guide bot
 * commands, weapon selection, etc.).
 *
 * The dispatch table is the source of truth for action-to-key mappings.
 * Kotlin (TouchBindings.kt) only stores IDs and display labels.
 */

#ifdef ANDROID

#include <jni.h>
#include <SDL.h>
#include <string.h>
#include <android/log.h>
#include "android_meta_actions.h"
#include "android_crash_handler.h"
#include "android_log.h"
#include "android_rewind.h"
#include "coop/coop_level_restart.h"
#include "cntrlcen.h"
#include "android_save_meta.h"
#include "game.h"
#include "fuelcen.h"
#include "hudmsg.h"
#include "inferno.h"
#include "input_demo_hooks.h"
#include "key.h"
#include "multi.h"
#include "matcen_mode.h"
#include "newdemo.h"
#include "object.h"
#include "playsave.h"
#include "screens.h"
#include "state.h"
#include "weapon.h"
#include "window.h"

volatile int android_force_quit = 0;
volatile int android_escort_release_pending = 0;
volatile int android_escort_spawn_pending = 0;
volatile int android_escort_find_secret_pending = 0;
volatile int android_escort_find_unexplored_pending = 0;
volatile int android_escort_next_goal_pending = 0;
volatile int android_escort_warp_to_me_pending = 0;
volatile int android_demo_record_toggle_pending = 0;
volatile int android_rewind_pending = 0;
volatile int android_coop_restart_level_pending = 0;
volatile int android_reactor_pause_toggle_pending = 0;
volatile int android_matcen_mode_cycle_pending = 0;
volatile int android_weapon_select_pending = 0;
extern int HandleSystemKey(int key);

#ifdef DXX_BUILD_DESCENT_II
#define ANDROID_GAME_LABEL           "D2"
#define ANDROID_STATE_SAVE_MENU()    state_save_all(0, NULL, 0)
#define ANDROID_STATE_RESTORE_MENU() state_restore_all(1, 0, NULL)
#else
#define ANDROID_GAME_LABEL           "D1"
#define ANDROID_STATE_SAVE_MENU()    state_save_all(0)
#define ANDROID_STATE_RESTORE_MENU() state_restore_all(1)
#endif

#define LOG_TAG   "DXX-MetaAction"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

/* Maximum number of SDL keys in a single meta action sequence */
#define MAX_SEQ 4

extern int (*window_get_callback(window *wind))(window *, d_event *, void *);
extern int pause_handler(window *wind, d_event *event, char *msg);

typedef struct {
	int action_id;
	int flags;            /* META_FLAG_INSTANT etc. */
	int key_count;        /* number of keys in sequence */
	SDLKey keys[MAX_SEQ]; /* keys to inject, modifiers first */
} meta_action_entry_t;

/*
 * Dispatch table.  For multi-key combos (e.g. ALT+F2), list the
 * modifier first.  The dispatch code injects them in order for
 * press (down) and reverse order for release (up).
 *
 * Instant actions inject a full down+up cycle on press and ignore release.
 */
static const meta_action_entry_t dispatch_table[] = {
	/* Quick save/load: instant (tap to trigger) */
	{ META_QUICK_SAVE, META_FLAG_INSTANT, 2, { SDLK_LALT, SDLK_F2 } },
	{ META_QUICK_LOAD, META_FLAG_INSTANT, 2, { SDLK_LALT, SDLK_F3 } },

	/* Game menu: hold ESC */
	{ META_GAME_MENU, 0, 1, { SDLK_ESCAPE } },

	/* Guide bot menu: instant SHIFT+F4 */
	{ META_GUIDE_BOT_MENU, META_FLAG_INSTANT, 2, { SDLK_LSHIFT, SDLK_F4 } },

	/* D2 cockpit camera windows: instant SHIFT+F1/F2 */
	{ META_CYCLE_LEFT_VIEW, META_FLAG_INSTANT, 2, { SDLK_LSHIFT, SDLK_F1 } },
	{ META_CYCLE_RIGHT_VIEW, META_FLAG_INSTANT, 2, { SDLK_LSHIFT, SDLK_F2 } },

	/* Guide bot direct commands: instant SHIFT+digit */
	{ META_GUIDE_FIND_ENERGY, META_FLAG_INSTANT, 2, { SDLK_LSHIFT, SDLK_1 } },
	{ META_GUIDE_FIND_REACTOR, META_FLAG_INSTANT, 2, { SDLK_LSHIFT, SDLK_2 } },
	{ META_GUIDE_FIND_SHIELD, META_FLAG_INSTANT, 2, { SDLK_LSHIFT, SDLK_3 } },
	{ META_GUIDE_FIND_POWERUP, META_FLAG_INSTANT, 2, { SDLK_LSHIFT, SDLK_4 } },
	{ META_GUIDE_FIND_ROBOT, META_FLAG_INSTANT, 2, { SDLK_LSHIFT, SDLK_5 } },
	{ META_GUIDE_FIND_HOSTAGE, META_FLAG_INSTANT, 2, { SDLK_LSHIFT, SDLK_6 } },
	{ META_GUIDE_SCRAM, META_FLAG_INSTANT, 2, { SDLK_LSHIFT, SDLK_7 } },
	{ META_GUIDE_FIND_ITEMS, META_FLAG_INSTANT, 2, { SDLK_LSHIFT, SDLK_8 } },
	{ META_GUIDE_FIND_EXIT, META_FLAG_INSTANT, 2, { SDLK_LSHIFT, SDLK_9 } },
	{ META_GUIDE_CLEAR_GOAL, META_FLAG_INSTANT, 2, { SDLK_LSHIFT, SDLK_0 } },

	/* Multiplayer HUD: instant F7 */
	{ META_MULTIPLAYER_HUD, META_FLAG_INSTANT, 1, { SDLK_F7 } },

	/* Drop flag (CTF): instant ALT+0 */
	{ META_DROP_FLAG, META_FLAG_INSTANT, 2, { SDLK_LALT, SDLK_0 } },

	/* Drop marker: instant F4 */
	{ META_DROP_MARKER, META_FLAG_INSTANT, 1, { SDLK_F4 } },

	/* Direct weapon selection: instant digit keys */
	{ META_WEAPON_1, META_FLAG_INSTANT, 1, { SDLK_1 } },
	{ META_WEAPON_2, META_FLAG_INSTANT, 1, { SDLK_2 } },
	{ META_WEAPON_3, META_FLAG_INSTANT, 1, { SDLK_3 } },
	{ META_WEAPON_4, META_FLAG_INSTANT, 1, { SDLK_4 } },
	{ META_WEAPON_5, META_FLAG_INSTANT, 1, { SDLK_5 } },
	{ META_WEAPON_6, META_FLAG_INSTANT, 1, { SDLK_6 } },
	{ META_WEAPON_7, META_FLAG_INSTANT, 1, { SDLK_7 } },
	{ META_WEAPON_8, META_FLAG_INSTANT, 1, { SDLK_8 } },
	{ META_WEAPON_9, META_FLAG_INSTANT, 1, { SDLK_9 } },
	{ META_WEAPON_10, META_FLAG_INSTANT, 1, { SDLK_0 } },

	/* Pause: hold */
	{ META_PAUSE, 0, 1, { SDLK_PAUSE } },
};

#define DISPATCH_TABLE_SIZE (sizeof(dispatch_table) / sizeof(dispatch_table[0]))

static void inject_sdl_key(SDLKey sym, int down)
{
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	ev.key.keysym.sym = sym;
	ev.key.keysym.mod = KMOD_NONE;
	ev.key.keysym.unicode = 0;
	crash_breadcrumb_v("inject_sdl_key sym=%d down=%d", (int) sym, down);
	SDL_PushEvent(&ev);
}

static int android_front_window_can_consume_exit_autosave(void)
{
	window *front;
	int (*callback)(window *, d_event *, void *);

	if (!Game_wind || Screen_mode != SCREEN_GAME || (Game_mode & GM_MULTI))
		return 0;

	front = window_get_front();
	if (front == Game_wind)
		return 1;
	if (!front)
		return 0;

	callback = window_get_callback(front);
	return callback == (int (*)(window *, d_event *, void *)) pause_handler;
}

static void android_push_force_quit_event(const char *reason)
{
	SDL_Event ev;

	g_android_autosave_request_kind = 0;
	android_force_quit = 1;
	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_QUIT;
	SDL_PushEvent(&ev);
	LOGI("meta_action_dispatch: pushed force quit (%s)", reason);
}

static void android_clear_saveload_requests(void)
{
	g_android_open_save_menu = 0;
	g_android_open_load_menu = 0;
}

int android_handle_pause_saveload_request(window *wind)
{
	if (g_android_difficulty_request >= 0) {
		window_close(wind);
		return 1;
	}

	if (!g_android_open_save_menu && !g_android_open_load_menu && !g_android_open_game_menu) {
		if (g_android_autosave_request_kind) {
			window_close(wind);
			return 1;
		}
		return 0;
	}

	if (g_android_open_game_menu) {
		window_close(wind);
		return 1;
	}

	if (g_android_open_save_menu) {
		if (Player_is_dead) {
			android_clear_saveload_requests();
			return 0;
		}
	} else if ((Game_mode & GM_MULTI) && !(Game_mode & GM_MULTI_COOP)) {
		android_clear_saveload_requests();
		return 0;
	}

	window_close(wind);
	return 1;
}

int android_handle_ingame_saveload_request(void)
{
	if (g_android_autosave_request_kind) {
		int request_kind = g_android_autosave_request_kind;
		int slotnum = request_kind == ANDROID_SAVE_META_KIND_AUTO_EXIT ? ANDROID_SAVE_META_SLOT_AUTO_EXIT : ANDROID_SAVE_META_SLOT_AUTO_MINIMIZE;
		const char *desc = request_kind == ANDROID_SAVE_META_KIND_AUTO_EXIT ? "AUTO EXIT" : "AUTO SAVE";
		int saved = 0;

		g_android_autosave_request_kind = 0;
		if (Player_is_dead) {
			debug_log(DLOG_GAME, "autosave skipped: " ANDROID_GAME_LABEL " player is dead");
		} else {
			saved = state_android_save_to_slot(slotnum, desc, request_kind);
		}

		if (saved)
			debug_log(DLOG_GAME, "autosave saved: " ANDROID_GAME_LABEL " slot %d", slotnum);

		if (request_kind == ANDROID_SAVE_META_KIND_AUTO_EXIT) {
			SDL_Event ev;

			android_force_quit = 1;
			memset(&ev, 0, sizeof(ev));
			ev.type = SDL_QUIT;
			SDL_PushEvent(&ev);
			debug_log(DLOG_GAME, "autosave exit queued: " ANDROID_GAME_LABEL " slot %d", slotnum);
			return 1;
		}

		return saved;
	}

	if (g_android_difficulty_request >= 0) {
		int difficulty = g_android_difficulty_request;
		g_android_difficulty_request = -1;
		return difficulty_change_to(difficulty, DIFFICULTY_CHANGE_RECORD_DEMO);
	}

	if (g_android_open_game_menu) {
		g_android_open_game_menu = 0;
		if (Game_wind && Screen_mode == SCREEN_GAME && window_get_front() == Game_wind) {
			HandleSystemKey(KEY_ESC);
			return 1;
		}
		return 0;
	}

	if (g_android_open_save_menu) {
		android_clear_saveload_requests();
		if (!Player_is_dead) {
			ANDROID_STATE_SAVE_MENU();
			return 1;
		}
		return 0;
	}

	if (g_android_open_load_menu) {
		android_clear_saveload_requests();
		if (!((Game_mode & GM_MULTI) && !(Game_mode & GM_MULTI_COOP))) {
			ANDROID_STATE_RESTORE_MENU();
			return 1;
		}
	}

	if (android_demo_record_toggle_pending) {
		android_demo_record_toggle_pending = 0;
		return newdemo_toggle_quick_recording();
	}

	if (android_rewind_pending) {
		int rewind_result;

		android_rewind_pending = 0;
		if ((Game_mode & GM_MULTI_COOP) && !multi_i_am_master()) {
			multi_send_rewind_request();
			return 1;
		}
		if (Game_mode & GM_MULTI_COOP)
			rewind_result = multi_perform_rewind_request(Player_num, NULL);
		else
			rewind_result = android_rewind_request(NULL);
		if (rewind_result == ANDROID_REWIND_STATUS_NOT_HOST) {
			multi_send_rewind_request();
			return 1;
		}
		if (rewind_result == ANDROID_REWIND_STATUS_BLOCKED_MULTIPLAYER) {
			HUD_init_message_literal(HM_DEFAULT, "Rewind is unavailable in current multiplayer state");
			return 1;
		}
		if (rewind_result == ANDROID_REWIND_STATUS_DISABLED) {
			HUD_init_message_literal(HM_DEFAULT, "Rewind is disabled");
			return 1;
		}
		if (rewind_result == ANDROID_REWIND_STATUS_NO_POINT) {
			HUD_init_message_literal(HM_DEFAULT, "No rewind point yet");
			return 1;
		}
		if (rewind_result == ANDROID_REWIND_STATUS_FAILED) {
			HUD_init_message_literal(HM_DEFAULT, "Rewind failed");
			return 1;
		}
		return 1;
	}

	if (android_coop_restart_level_pending) {
		android_coop_restart_level_pending = 0;
		if (!coop_level_restart_request())
			HUD_init_message_literal(HM_DEFAULT, "Level-start checkpoint unavailable");
		else
			HUD_init_message_literal(HM_DEFAULT, "Restarting level from checkpoint");
		return 1;
	}

	if (android_matcen_mode_apply_pending())
		return 1;

	if (android_reactor_pause_toggle_pending) {
		android_reactor_pause_toggle_pending = 0;
		if (!PlayerCfg.MapCheatsAccessible || !reactor_countdown_is_active())
			return 0;
#ifdef NETWORK
		if (Game_mode & GM_MULTI) {
			multi_request_reactor_pause_toggle();
			return 1;
		}
#endif
		if (reactor_countdown_set_paused(!Reactor_countdown_paused, Countdown_timer))
			HUD_init_message_literal(HM_DEFAULT, Reactor_countdown_paused ? "Reactor countdown paused" : "Reactor countdown resumed");
		return 1;
	}

	return 0;
}

int android_matcen_mode_apply_pending(void)
{
	int mode;

	if (!android_matcen_mode_cycle_pending)
		return 0;
	android_matcen_mode_cycle_pending = 0;
	if (!PlayerCfg.MapCheatsAccessible)
		return 0;
	mode = (matcen_mode_get() + 1) % MATCEN_MODE_COUNT;
#ifdef NETWORK
	if (Game_mode & GM_MULTI) {
		multi_request_matcen_mode(mode);
		return 1;
	}
#endif
	matcen_set_mode(mode);
	return 1;
}

int meta_action_dispatch(int action_id, int pressed)
{
	const meta_action_entry_t *entry = NULL;
	int i;

	/* Special case: release guide-bot control (no key equivalent).
	 * Set a flag for the game thread to consume in gamecntl.c. */
	if (action_id == META_GUIDE_RELEASE_CONTROL) {
		if (pressed)
			android_escort_release_pending = 1;
		return 0;
	}

	if (action_id == META_GUIDE_SPAWN) {
		if (pressed)
			android_escort_spawn_pending = 1;
		return 0;
	}

	if (action_id == META_GUIDE_FIND_SECRET) {
		if (pressed)
			android_escort_find_secret_pending = 1;
		return 0;
	}

	if (action_id == META_GUIDE_FIND_UNEXPLORED) {
		if (pressed)
			android_escort_find_unexplored_pending = 1;
		return 0;
	}

	if (action_id == META_GUIDE_NEXT_GOAL) {
		if (pressed)
			android_escort_next_goal_pending = 1;
		return 0;
	}

	if (action_id == META_GUIDE_WARP_TO_ME) {
		if (pressed)
			android_escort_warp_to_me_pending = 1;
		return 0;
	}

	/* Special case: toggle Android quick input-demo recording.
	 * This must run on the game thread because it starts/stops newdemo state. */
	if (action_id == META_DEMO_RECORD_TOGGLE) {
		if (pressed)
			android_demo_record_toggle_pending = 1;
		return 0;
	}

	/* Special case: rewind must run on the game thread because it will
	 * eventually restore game state and input-demo recorder state. */
	if (action_id == META_REWIND) {
		if (pressed)
			android_rewind_pending = 1;
		return 0;
	}

	if (action_id == META_COOP_RESTART_LEVEL) {
		if (pressed)
			android_coop_restart_level_pending = 1;
		return 0;
	}

	/* Special case: return to launcher.  Queue an autosave first only when
	 * the front window has Android handling for that request.  Other screens
	 * such as movies and multiplayer waits need an immediate SDL_QUIT. */
	if (action_id == META_RETURN_TO_LAUNCHER) {
		if (pressed) {
			if (android_front_window_can_consume_exit_autosave()) {
				if (g_android_autosave_request_kind < ANDROID_SAVE_META_KIND_AUTO_EXIT)
					g_android_autosave_request_kind = ANDROID_SAVE_META_KIND_AUTO_EXIT;
				LOGI("meta_action_dispatch: queued exit autosave request");
			} else {
				android_push_force_quit_event("autosave unavailable");
			}
		}
		return 0;
	}

	for (i = 0; i < (int) DISPATCH_TABLE_SIZE; i++) {
		if (dispatch_table[i].action_id == action_id) {
			entry = &dispatch_table[i];
			break;
		}
	}

	if (!entry) {
		LOGI("meta_action_dispatch: unknown action %d", action_id);
		return -1;
	}

	if (entry->flags & META_FLAG_INSTANT) {
		/* Instant: inject full press+release cycle on button down only */
		if (!pressed)
			return 0;
		/* Press all keys in order (modifiers first) */
		for (i = 0; i < entry->key_count; i++)
			inject_sdl_key(entry->keys[i], 1);
		/* Release in reverse order */
		for (i = entry->key_count - 1; i >= 0; i--)
			inject_sdl_key(entry->keys[i], 0);
	} else {
		/* Hold: track press/release state */
		if (pressed) {
			for (i = 0; i < entry->key_count; i++)
				inject_sdl_key(entry->keys[i], 1);
		} else {
			for (i = entry->key_count - 1; i >= 0; i--)
				inject_sdl_key(entry->keys[i], 0);
		}
	}

	return 0;
}

int android_weapon_select_apply_pending(void)
{
	int request = android_weapon_select_pending;
	int secondary_flag;
	int weapon_index;
	int weapon_status;

	if (!request)
		return 0;
	android_weapon_select_pending = 0;
	secondary_flag = (request >> 8) & 1;
	weapon_index = (request & 0xff) - 1;
	if (!Game_wind || Screen_mode != SCREEN_GAME || Player_is_dead)
		return 1;
	if (weapon_index < 0 ||
	    (!secondary_flag && weapon_index >= MAX_PRIMARY_WEAPONS) ||
	    (secondary_flag && weapon_index >= MAX_SECONDARY_WEAPONS))
		return 1;
	weapon_status = player_has_weapon(Player_num, weapon_index, secondary_flag);
	if (!(weapon_status & HAS_WEAPON_FLAG) ||
	    (secondary_flag && !(weapon_status & HAS_AMMO_FLAG)))
		return 1;
	input_demo_record_direct_command_select_weapon_exact(secondary_flag,
	                                                     weapon_index);
	select_weapon(weapon_index, secondary_flag, 1, 1);
	return 1;
}

/* JNI entry point */
JNIEXPORT jint JNICALL
Java_com_dxxredux_app_NativeMetaActions_nativeMetaAction(
    JNIEnv *env, jclass clazz, jint actionId, jint pressed)
{
	return meta_action_dispatch(actionId, pressed);
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_NativeMetaActions_nativeSelectWeaponExact(
    JNIEnv *env, jclass clazz, jint weaponClass, jint weaponIndex)
{
	if ((weaponClass != 0 && weaponClass != 1) || weaponIndex < 0 || weaponIndex > 9)
		return -1;
	android_weapon_select_pending = (weaponClass << 8) | (weaponIndex + 1);
	return 0;
}

#endif /* ANDROID */
