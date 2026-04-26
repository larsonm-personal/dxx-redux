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

volatile int android_force_quit = 0;
volatile int android_escort_release_pending = 0;
volatile int android_demo_record_toggle_pending = 0;

#define LOG_TAG   "DXX-MetaAction"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

/* Maximum number of SDL keys in a single meta action sequence */
#define MAX_SEQ 4

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

	/* Special case: toggle Android quick input-demo recording.
	 * This must run on the game thread because it starts/stops newdemo state. */
	if (action_id == META_DEMO_RECORD_TOGGLE) {
		if (pressed)
			android_demo_record_toggle_pending = 1;
		return 0;
	}

	/* Special case: return to launcher.  SDL_QUIT cascades through
	 * the Quitting mechanism to close all windows and exit the game
	 * loop.  android_force_quit tells standard_handler to skip the
	 * "Abort Game?" dialog.  No ESC injection -- that would open
	 * the game menu when Game_wind is the front window. */
	if (action_id == META_RETURN_TO_LAUNCHER) {
		if (pressed) {
			SDL_Event ev;

			/* Tell standard_handler to skip the "Abort Game?" dialog */
			android_force_quit = 1;

			memset(&ev, 0, sizeof(ev));
			ev.type = SDL_QUIT;
			SDL_PushEvent(&ev);
			LOGI("meta_action_dispatch: SDL_QUIT pushed (return to launcher)");
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

/* JNI entry point */
JNIEXPORT jint JNICALL
Java_com_dxxredux_app_NativeMetaActions_nativeMetaAction(
    JNIEnv *env, jclass clazz, jint actionId, jint pressed)
{
	return meta_action_dispatch(actionId, pressed);
}

#endif /* ANDROID */
