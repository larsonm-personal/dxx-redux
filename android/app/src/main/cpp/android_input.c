/*
 * android_input.c - Bridge Android touch, key, and joystick input to the engine.
 *
 * Touch events are converted to SDL_MOUSEMOTION / SDL_MOUSEBUTTONDOWN / UP.
 * Key events are converted to SDL_KEYDOWN / SDL_KEYUP.
 * Continuous joystick axes use a coalesced latest-state mailbox. Buttons and
 * discrete input are converted to SDL events.
 *
 * The game thread drains axes through joy_axisbutton_handler() and
 * joy_axis_handler(); event_poll() handles the discrete SDL queue.
 */

#ifdef ANDROID

#include <jni.h>
#include <android/log.h>
#include <SDL.h>
#include <string.h>
#include <android/keycodes.h>
#include "android_log.h"
#include "android_meta_actions.h"
#include "android_profile.h"
#include "android_screen_advance.h"
#include "android_lifecycle_actions.h"
#include "android_lifecycle_diagnostics.h"
#include "android_rewind.h"
#include "coop/coop_level_restart.h"
#include "cntrlcen.h"
#include "android_save_meta.h"
#include "state_android_shared.h"
#include "android_axis_mailbox.h"
#ifdef INTROSPECT_ON
#include "game_automate.h"
#endif
#include "gr.h"
#include "input_demo_recorder.h"
#include "joy.h"
#include "mouse.h"
#include "playsave.h"
#ifdef NETWORK
#include "multi.h"
#endif
#include "secretarea.h"
#include "timer.h"
#include "window.h"

/* Automap_active is defined in automap.c; we only need the extern. */
extern int Automap_active;

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetCoopLevelRestartState(JNIEnv *env, jobject thiz)
{
	(void) env;
	(void) thiz;
	return (jint) coop_level_restart_get_state();
}

#define LOG_TAG   "DXX-Input"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

/* Requested by Kotlin when automap recenter is selected from touch UI. */
volatile int g_automap_center = 0;
volatile int g_automap_set_marker = -1;
volatile int g_automap_go_marker = -1;
volatile int g_automap_name_marker = 0;

/* Set to 1 while the launch intro/title sequence is active.
 * Kotlin uses this to swap the Skip overlay to a larger
 * "Skip every launch" button. */
volatile int g_intro_active = 0;

/* Shared launcher preference: auto-skip the launch intro/title sequence.
 * Written by JNI from Kotlin, read by d1/d2 titles.c. */
volatile int g_skip_intro_pref = 0;

/* Android QoL preference: keep the D2 headlight off when picked up.
 * Written by JNI from Kotlin, read by d2 playsave.c. */
volatile int g_headlight_off_by_default_qol = 1;

/* Shared launcher preference: include per-frame state in live input demo
 * recordings. Written by JNI from Kotlin, read by d1/d2 newdemo.c. */
volatile int g_demo_record_per_frame_state = 0;

/* Set to 1 when the current launch actually bypassed the intro/title segment
 * because the launcher preference requested it.  Exposed via introspection for
 * automation checks. */
volatile int g_intro_skip_applied = 0;

/* Set to 1 while save/load menu is open.  Kotlin shows a BACK button. */
volatile int g_saveload_menu_active = 0;

/* Set to 1 while the host is on the "select players" screen.
 * Kotlin shows a "START GAME" overlay button. */
volatile int g_host_selecting_players = 0;
volatile int g_host_start_game_requested = 0;

static int g_dpad_center_down = 0;

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeJoystickButton(JNIEnv *env, jobject thiz,
                                                        jint button, jint pressed);

/* Set by JNI on the UI thread when the admin tray requests save/load.
 * The game thread consumes these in d1/d2 gamecntl.c. */
volatile int g_android_open_save_menu = 0;
volatile int g_android_open_load_menu = 0;
volatile int g_android_open_game_menu = 0;
volatile int g_android_difficulty_request = -1;

/* Set on the UI thread when Android lifecycle or launcher actions request
 * an autosave.  The game thread consumes this in d1/d2 gamecntl.c. */
volatile int g_android_autosave_request_kind = 0;

/* Forward declaration — defined later in this file, written by the blit path. */
extern volatile int g_blit_y_offset;

/* ── Touch → Mouse ──────────────────────────────────────────
 *
 * The game tracks the mouse position via deltas (SDL_MouseMotionEvent.xrel/yrel)
 * accumulated into Mouse.x / Mouse.y.  We keep the last known touch position
 * and convert absolute touch coords to relative deltas.
 *
 * Touch coordinates from Kotlin are normalised 0.0–1.0 across
 * the SurfaceView and mapped to engine resolution here.
 */

static int g_last_touch_x = -1;
static int g_last_touch_y = -1;
static int g_touch_active = 0; /* is a finger currently down? */
static int g_input_count = 0;  /* debug counter */
static int g_intro_skip_touch_pressed = 0;
static int g_touch_down_suppressed = 0;
static unsigned char g_joy_buttons_down[256];
static unsigned char g_suppressed_joy_buttons[256];
static int g_cutscene_release_gate = 0;
static volatile fix64 g_cutscene_tap_suppress_until = 0;
volatile int g_cutscene_tap_suppress_arms = 0;
volatile int g_cutscene_tap_suppress_hits = 0;
static volatile int g_screen_advance_kind = ANDROID_SCREEN_ADVANCE_NONE;
static volatile int g_screen_advance_ready = 0;
static volatile unsigned int g_screen_advance_generation = 0;
static volatile unsigned int g_screen_advance_request_generation = 0;

#define CUTSCENE_TAP_SUPPRESS_WINDOW (F1_0 / 2)

/* Shared with d1/d2 arch/sdl/joy.c: mark touch-sourced virtual axes in the
 * high bit so the gameplay deadzone can distinguish touch from controller. */
#define ANDROID_TOUCH_AXIS_FLAG 0x80

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeKeyEvent(JNIEnv *env, jobject thiz,
                                                  jint action, jint androidKeyCode,
                                                  jint unicodeChar);
static void inject_key_tap(SDLKey sym);
static int android_cutscene_tap_suppressed(void);
static int android_any_joy_button_down(void);
static void android_update_cutscene_release_gate(void);

static void android_get_touch_screen_size(int *screen_w, int *screen_h)
{
	int w = 640;
	int h = 480;

	if (grd_curscreen) {
		w = grd_curscreen->sc_canvas.cv_bitmap.bm_w;
		h = grd_curscreen->sc_canvas.cv_bitmap.bm_h;
		if (w <= 0 || h <= 0) {
			w = grd_curscreen->sc_w;
			h = grd_curscreen->sc_h;
		}
	}
	if (screen_w)
		*screen_w = w;
	if (screen_h)
		*screen_h = h;
}

void android_automation_joystick_button(int button, int pressed);

static int android_intro_skip_touch_inside(jfloat normX, jfloat normY, int screenW, int screenH)
{
	const float h_over_w = (screenW > 0) ? ((float) screenH / (float) screenW) : 1.0f;
	const float pill_left = 1.0f - 0.245f * h_over_w;
	const float pill_top = 0.019f;
	const float pill_bottom = 0.075f;
	return normX >= pill_left && normX <= 1.0f && normY >= pill_top && normY <= pill_bottom;
}

static void android_persist_skip_intro_pref(JNIEnv *env, jobject thiz)
{
	jclass cls = (*env)->GetObjectClass(env, thiz);
	if (!cls)
		return;
	jmethodID mid = (*env)->GetMethodID(env, cls, "persistSkipIntroMovieFromNative", "()V");
	if (mid)
		(*env)->CallVoidMethod(env, thiz, mid);
}

static int android_cutscene_tap_suppressed(void)
{
	android_update_cutscene_release_gate();
	return g_cutscene_release_gate ||
	       (g_cutscene_tap_suppress_until && timer_query() < g_cutscene_tap_suppress_until);
}

static int android_any_joy_button_down(void)
{
	int i;
	for (i = 0; i < (int) sizeof(g_joy_buttons_down); i++)
		if (g_joy_buttons_down[i])
			return 1;
	return 0;
}

static void android_update_cutscene_release_gate(void)
{
	if (!g_cutscene_release_gate)
		return;
	if (g_touch_active || android_any_joy_button_down())
		return;
	g_cutscene_release_gate = 0;
	debug_log(DLOG_GAME, "[cutscene-input] release gate cleared\n");
}

void android_arm_cutscene_tap_suppress(void)
{
	g_cutscene_tap_suppress_until = timer_query() + CUTSCENE_TAP_SUPPRESS_WINDOW;
	g_cutscene_release_gate = g_touch_active || android_any_joy_button_down();
	g_touch_down_suppressed = 0;
	memset(g_suppressed_joy_buttons, 0, sizeof(g_suppressed_joy_buttons));
	g_cutscene_tap_suppress_arms++;
	g_cutscene_tap_suppress_hits = 0;
	debug_log(DLOG_GAME, "[cutscene-input] armed gate=%d touch=%d joy_down=%d\n",
	          g_cutscene_release_gate, g_touch_active, android_any_joy_button_down());
}

int android_cutscene_tap_suppress_active(void)
{
	return android_cutscene_tap_suppressed();
}

void android_screen_advance_begin(android_screen_advance_kind kind, int ready)
{
	if (kind == ANDROID_SCREEN_ADVANCE_NONE)
		return;

	g_screen_advance_ready = 0;
	g_screen_advance_generation++;
	if (!g_screen_advance_generation)
		g_screen_advance_generation++;
	g_screen_advance_request_generation = 0;
	android_arm_cutscene_tap_suppress();
	g_screen_advance_kind = kind;
	g_screen_advance_ready = ready != 0;
	debug_log(DLOG_GAME, "[screen-advance] begin kind=%d ready=%d generation=%u\n",
	          kind, g_screen_advance_ready, g_screen_advance_generation);
}

void android_screen_advance_set_ready(android_screen_advance_kind kind, int ready)
{
	if (g_screen_advance_kind != kind)
		return;
	g_screen_advance_ready = ready != 0;
}

void android_screen_advance_end(android_screen_advance_kind kind)
{
	if (g_screen_advance_kind != kind)
		return;
	debug_log(DLOG_GAME, "[screen-advance] end kind=%d generation=%u\n",
	          kind, g_screen_advance_generation);
	g_screen_advance_ready = 0;
	g_screen_advance_kind = ANDROID_SCREEN_ADVANCE_NONE;
	g_screen_advance_request_generation = 0;
}

int android_screen_advance_can_activate(android_screen_advance_kind kind)
{
	return g_screen_advance_kind == kind && g_screen_advance_ready &&
	       !android_cutscene_tap_suppressed();
}

int android_screen_advance_accept_event(android_screen_advance_kind kind, d_event *event)
{
	if (!event || !android_screen_advance_can_activate(kind))
		return 0;

	if (event->type == EVENT_MOUSE_BUTTON_DOWN)
		return event_mouse_get_button(event) == MBTN_LEFT;
	if (event->type == EVENT_JOYSTICK_BUTTON_DOWN)
		return event_joystick_get_button(event) == 0;
	return 0;
}

int android_screen_advance_request(unsigned int generation)
{
	const android_screen_advance_kind kind =
	    (android_screen_advance_kind) g_screen_advance_kind;

	if (!generation || generation != g_screen_advance_generation ||
	    !android_screen_advance_can_activate(kind)) {
		debug_log(DLOG_GAME,
		          "[screen-advance] request rejected generation=%u current=%u kind=%d\n",
		          generation, g_screen_advance_generation, kind);
		return 0;
	}

	g_screen_advance_request_generation = generation;
	debug_log(DLOG_GAME, "[screen-advance] request accepted generation=%u kind=%d\n",
	          generation, kind);
	return 1;
}

int android_screen_advance_take_request(android_screen_advance_kind kind)
{
	if (g_screen_advance_kind != kind ||
	    g_screen_advance_request_generation != g_screen_advance_generation)
		return 0;

	g_screen_advance_request_generation = 0;
	return 1;
}

int android_screen_advance_get_kind(void)
{
	return g_screen_advance_kind;
}

int android_screen_advance_get_ready(void)
{
	return g_screen_advance_ready;
}

unsigned int android_screen_advance_get_generation(void)
{
	return g_screen_advance_generation;
}

/*
 * nativeTouchEvent(action, normX, normY)
 *   action: 0 = DOWN, 1 = MOVE, 2 = UP  (matches MotionEvent.ACTION_*)
 *   normX, normY: touch position normalised to 0.0-1.0 across the SurfaceView.
 *                 The native side maps to engine resolution via grd_curscreen.
 */

/* Menu scale-blit rect globals (defined in newmenu.c via game thread).
 * src = original menu rect in 640x480 canvas space.
 * dst = enlarged rect where the scale-blit placed the magnified menu. */
int g_menu_scale_active = 0;
int g_menu_scale_src_x = 0, g_menu_scale_src_y = 0;
int g_menu_scale_src_w = 0, g_menu_scale_src_h = 0;
int g_menu_scale_dst_x = 0, g_menu_scale_dst_y = 0;
int g_menu_scale_dst_w = 0, g_menu_scale_dst_h = 0;

static int g_menu_scale_touch_locked = 0;
static int g_menu_scale_touch_src_x = 0, g_menu_scale_touch_src_y = 0;
static int g_menu_scale_touch_src_w = 0, g_menu_scale_touch_src_h = 0;
static int g_menu_scale_touch_dst_x = 0, g_menu_scale_touch_dst_y = 0;
static int g_menu_scale_touch_dst_w = 0, g_menu_scale_touch_dst_h = 0;
static int g_touch_diag_seq = 0;
static int g_touch_diag_count = 0;
static int g_touch_push_diag_count = 0;

static const char *touch_diag_action_name(int action)
{
	switch (action) {
		case 0: return "down";
		case 1: return "move";
		case 2: return "up";
		default: return "unknown";
	}
}

static int menu_scale_rect_contains(int x, int y, int rx, int ry, int rw, int rh)
{
	return rw > 0 && rh > 0 && x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static void menu_scale_touch_store_rect(void)
{
	g_menu_scale_touch_src_x = g_menu_scale_src_x;
	g_menu_scale_touch_src_y = g_menu_scale_src_y;
	g_menu_scale_touch_src_w = g_menu_scale_src_w;
	g_menu_scale_touch_src_h = g_menu_scale_src_h;
	g_menu_scale_touch_dst_x = g_menu_scale_dst_x;
	g_menu_scale_touch_dst_y = g_menu_scale_dst_y;
	g_menu_scale_touch_dst_w = g_menu_scale_dst_w;
	g_menu_scale_touch_dst_h = g_menu_scale_dst_h;
}

static int remap_touch_from_rect(int *gx, int *gy,
                                 int sx, int sy, int sw, int sh,
                                 int dx, int dy, int dw, int dh)
{
	int tx;
	int ty;
	if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
		return 0;

	tx = *gx;
	ty = *gy;
	*gx = sx + (tx - dx) * sw / dw;
	*gy = sy + (ty - dy) * sh / dh;
	return 1;
}

static int remap_touch(int action, int *gx, int *gy)
{
	int remapped = 0;

	if (action == 0)
		g_menu_scale_touch_locked = 0;

	/* Keep one touch sequence in one coordinate space.  A scaled menu can
	 * redraw or a finger can drift just past the destination edge between
	 * DOWN/MOVE/UP; switching back to raw screen coords mid-sequence makes
	 * the release hit-test land above or below the visible menu row. */
	if (g_menu_scale_touch_locked) {
		remapped = remap_touch_from_rect(gx, gy,
		                                 g_menu_scale_touch_src_x,
		                                 g_menu_scale_touch_src_y,
		                                 g_menu_scale_touch_src_w,
		                                 g_menu_scale_touch_src_h,
		                                 g_menu_scale_touch_dst_x,
		                                 g_menu_scale_touch_dst_y,
		                                 g_menu_scale_touch_dst_w,
		                                 g_menu_scale_touch_dst_h);
	} else if (g_menu_scale_active &&
	           menu_scale_rect_contains(*gx, *gy,
	                                    g_menu_scale_dst_x,
	                                    g_menu_scale_dst_y,
	                                    g_menu_scale_dst_w,
	                                    g_menu_scale_dst_h)) {
		menu_scale_touch_store_rect();
		g_menu_scale_touch_locked = 1;
		remapped = remap_touch_from_rect(gx, gy,
		                                 g_menu_scale_touch_src_x,
		                                 g_menu_scale_touch_src_y,
		                                 g_menu_scale_touch_src_w,
		                                 g_menu_scale_touch_src_h,
		                                 g_menu_scale_touch_dst_x,
		                                 g_menu_scale_touch_dst_y,
		                                 g_menu_scale_touch_dst_w,
		                                 g_menu_scale_touch_dst_h);
	}

	/* Touches outside the enlarged rect pass through. They'll hit
	 * background area and the menu will ignore them. */
	if (action == 2)
		g_menu_scale_touch_locked = 0;
	return remapped;
}

static void android_push_touch_action(int action, int gameX, int gameY)
{
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));

	switch (action) {
		case 0: /* ACTION_DOWN */
			if (android_cutscene_tap_suppressed()) {
				if (g_touch_push_diag_count < 160) {
					g_touch_push_diag_count++;
					debug_log(DLOG_GAME,
					          "[touch-push] seq=%d action=down suppressed=cutscene x=%d y=%d\n",
					          g_touch_diag_seq, gameX, gameY);
				}
				g_last_touch_x = gameX;
				g_last_touch_y = gameY;
				g_touch_active = 1;
				g_touch_down_suppressed = 1;
				g_cutscene_tap_suppress_hits++;
				return;
			}

			g_last_touch_x = gameX;
			g_last_touch_y = gameY;
			g_touch_active = 1;

			/* Send a motion event to position the cursor.
			 * On Android, mouse_motion_handler() uses absolute x/y when
			 * xrel==0 && yrel==0 (see mouse.c). */
			ev.type = SDL_MOUSEMOTION;
			ev.motion.x = (Uint16) gameX;
			ev.motion.y = (Uint16) gameY;
			ev.motion.xrel = 0;
			ev.motion.yrel = 0;
			ev.motion.state = 0;
			SDL_PushEvent(&ev);

			/* Then send button-down (left button = SDL_BUTTON_LEFT = 1) */
			memset(&ev, 0, sizeof(ev));
			ev.type = SDL_MOUSEBUTTONDOWN;
			ev.button.button = SDL_BUTTON_LEFT;
			ev.button.state = SDL_PRESSED;
			ev.button.x = (Uint16) gameX;
			ev.button.y = (Uint16) gameY;
			SDL_PushEvent(&ev);

			if (++g_input_count <= 5)
				LOGI("touch DOWN at (%d,%d)", gameX, gameY);
			if (g_touch_push_diag_count < 160) {
				g_touch_push_diag_count++;
				debug_log(DLOG_GAME,
				          "[touch-push] seq=%d action=down motion=%d,%d button=%d,%d active=%d\n",
				          g_touch_diag_seq, gameX, gameY, gameX, gameY, g_touch_active);
			}
			break;

		case 1: /* ACTION_MOVE */ {
			if (g_touch_down_suppressed) break;
			if (!g_touch_active) break;
			g_last_touch_x = gameX;
			g_last_touch_y = gameY;

			/* Send motion with xrel=0, yrel=0 so mouse_motion_handler uses
			 * absolute x/y positioning (Android path). */
			ev.type = SDL_MOUSEMOTION;
			ev.motion.x = (Uint16) gameX;
			ev.motion.y = (Uint16) gameY;
			ev.motion.xrel = 0;
			ev.motion.yrel = 0;
			ev.motion.state = SDL_BUTTON(SDL_BUTTON_LEFT);
			SDL_PushEvent(&ev);
			break;
		}

		case 2: /* ACTION_UP */
			if (g_touch_down_suppressed) {
				if (g_touch_push_diag_count < 160) {
					g_touch_push_diag_count++;
					debug_log(DLOG_GAME,
					          "[touch-push] seq=%d action=up suppressed=cutscene x=%d y=%d\n",
					          g_touch_diag_seq, gameX, gameY);
				}
				g_touch_down_suppressed = 0;
				g_touch_active = 0;
				android_update_cutscene_release_gate();
				return;
			}

			g_touch_active = 0;
			android_update_cutscene_release_gate();

			/* The SDL mouse button handler uses Mouse.x/y, not the x/y stored
			 * on the button event, so position the cursor at the release point
			 * before menu hit testing runs. */
			ev.type = SDL_MOUSEMOTION;
			ev.motion.x = (Uint16) gameX;
			ev.motion.y = (Uint16) gameY;
			ev.motion.xrel = 0;
			ev.motion.yrel = 0;
			ev.motion.state = SDL_BUTTON(SDL_BUTTON_LEFT);
			SDL_PushEvent(&ev);

			memset(&ev, 0, sizeof(ev));
			ev.type = SDL_MOUSEBUTTONUP;
			ev.button.button = SDL_BUTTON_LEFT;
			ev.button.state = SDL_RELEASED;
			ev.button.x = (Uint16) gameX;
			ev.button.y = (Uint16) gameY;
			SDL_PushEvent(&ev);

			if (g_input_count <= 5)
				LOGI("touch UP   at (%d,%d)", gameX, gameY);
			if (g_touch_push_diag_count < 160) {
				g_touch_push_diag_count++;
				debug_log(DLOG_GAME,
				          "[touch-push] seq=%d action=up motion=%d,%d button=%d,%d active=%d\n",
				          g_touch_diag_seq, gameX, gameY, gameX, gameY, g_touch_active);
			}
			break;
	}
}

static void android_test_touch_point(int *gameX, int *gameY)
{
	int screenW, screenH;

	android_get_touch_screen_size(&screenW, &screenH);
	*gameX = screenW > 2 ? screenW / 2 : 1;
	*gameY = screenH > 2 ? screenH / 2 : 1;

	if (*gameX < 0) *gameX = 0;
	if (*gameX >= screenW) *gameX = screenW - 1;
	if (*gameY < 0) *gameY = 0;
	if (*gameY >= screenH) *gameY = screenH - 1;
}

void android_test_inject_touch_action(int action)
{
	int gameX, gameY;

	android_test_touch_point(&gameX, &gameY);
	android_push_touch_action(action, gameX, gameY);
}

void android_test_inject_touch_tap(void)
{
	android_test_inject_touch_action(0);
	android_test_inject_touch_action(2);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeTouchEvent(JNIEnv *env, jobject thiz,
                                                    jint action, jfloat normX, jfloat normY)
{
	/* Map normalised coordinates to the engine's actual resolution.
	 * This avoids any mismatch between the Kotlin-side GAME_W/H
	 * (from SharedPreferences) and the real engine resolution
	 * (from descent.cfg via grd_curscreen). */
	int screenW, screenH;
	android_get_touch_screen_size(&screenW, &screenH);
	if (g_intro_active) {
		const int inside_intro_skip = android_intro_skip_touch_inside(normX, normY, screenW, screenH);
		switch (action) {
			case 0: /* ACTION_DOWN */
				if (inside_intro_skip) {
					g_intro_skip_touch_pressed = 1;
					return;
				}
				break;

			case 1: /* ACTION_MOVE */
				if (g_intro_skip_touch_pressed)
					return;
				break;

			case 2: /* ACTION_UP */
				if (g_intro_skip_touch_pressed) {
					g_intro_skip_touch_pressed = 0;
					if (inside_intro_skip) {
						g_skip_intro_pref = 1;
						android_persist_skip_intro_pref(env, thiz);
						Java_com_dxxredux_app_MainActivity_nativeKeyEvent(env, thiz, 0, AKEYCODE_ESCAPE, 0);
						Java_com_dxxredux_app_MainActivity_nativeKeyEvent(env, thiz, 1, AKEYCODE_ESCAPE, 0);
						return;
					}
					return;
				}
				break;
		}
	}

	jint gameX = (jint) (normX * screenW);
	jint gameY = (jint) (normY * screenH);
	jint rawGameX;
	jint rawGameY;
	jint keyboardGameY;
	int remapped;

	if (gameX < 0) gameX = 0;
	if (gameX >= screenW) gameX = screenW - 1;
	if (gameY < 0) gameY = 0;
	if (gameY >= screenH) gameY = screenH - 1;
	rawGameX = gameX;
	rawGameY = gameY;

	/* Compensate for keyboard blit offset: the rendered canvas is
	 * shifted upward by g_blit_y_offset pixels, so screen-space
	 * touches need to be mapped back to canvas-space. */
	gameY += g_blit_y_offset;
	if (gameY >= screenH) gameY = screenH - 1;
	keyboardGameY = gameY;

	if (action == 0)
		g_touch_diag_seq++;
	remapped = remap_touch(action, &gameX, &gameY);
	if (g_touch_diag_count < 160 && action != 1) {
		g_touch_diag_count++;
		debug_log(DLOG_GAME,
		          "[touch-native] seq=%d action=%s norm=%.4f,%.4f screen=%dx%d raw=%d,%d kb_y=%d kb_off=%d final=%d,%d remap=%d scale=%d lock=%d src=(%d,%d %dx%d) dst=(%d,%d %dx%d)\n",
		          g_touch_diag_seq, touch_diag_action_name(action), (double) normX,
		          (double) normY, screenW, screenH, rawGameX, rawGameY,
		          keyboardGameY, (int) g_blit_y_offset, gameX, gameY, remapped,
		          g_menu_scale_active, g_menu_scale_touch_locked,
		          g_menu_scale_src_x, g_menu_scale_src_y, g_menu_scale_src_w,
		          g_menu_scale_src_h, g_menu_scale_dst_x, g_menu_scale_dst_y,
		          g_menu_scale_dst_w, g_menu_scale_dst_h);
	}

	android_push_touch_action(action, gameX, gameY);
}

/* ── Keyboard ───────────────────────────────────────────────
 *
 * Android AKEYCODE_* → SDL 1.2 SDLK_*.
 * We only map the keys the game actually uses.
 */

static SDLKey android_to_sdlk(int akeycode)
{
	switch (akeycode) {
		/* Letters */
		case AKEYCODE_A: return SDLK_a;
		case AKEYCODE_B: return SDLK_b;
		case AKEYCODE_C: return SDLK_c;
		case AKEYCODE_D: return SDLK_d;
		case AKEYCODE_E: return SDLK_e;
		case AKEYCODE_F: return SDLK_f;
		case AKEYCODE_G: return SDLK_g;
		case AKEYCODE_H: return SDLK_h;
		case AKEYCODE_I: return SDLK_i;
		case AKEYCODE_J: return SDLK_j;
		case AKEYCODE_K: return SDLK_k;
		case AKEYCODE_L: return SDLK_l;
		case AKEYCODE_M: return SDLK_m;
		case AKEYCODE_N: return SDLK_n;
		case AKEYCODE_O: return SDLK_o;
		case AKEYCODE_P: return SDLK_p;
		case AKEYCODE_Q: return SDLK_q;
		case AKEYCODE_R: return SDLK_r;
		case AKEYCODE_S: return SDLK_s;
		case AKEYCODE_T: return SDLK_t;
		case AKEYCODE_U: return SDLK_u;
		case AKEYCODE_V: return SDLK_v;
		case AKEYCODE_W: return SDLK_w;
		case AKEYCODE_X: return SDLK_x;
		case AKEYCODE_Y: return SDLK_y;
		case AKEYCODE_Z: return SDLK_z;

		/* Digits */
		case AKEYCODE_0: return SDLK_0;
		case AKEYCODE_1: return SDLK_1;
		case AKEYCODE_2: return SDLK_2;
		case AKEYCODE_3: return SDLK_3;
		case AKEYCODE_4: return SDLK_4;
		case AKEYCODE_5: return SDLK_5;
		case AKEYCODE_6: return SDLK_6;
		case AKEYCODE_7: return SDLK_7;
		case AKEYCODE_8: return SDLK_8;
		case AKEYCODE_9: return SDLK_9;

		/* Navigation / control */
		case AKEYCODE_ENTER: return SDLK_RETURN;
		case AKEYCODE_DPAD_CENTER: return SDLK_RETURN;
		case AKEYCODE_ESCAPE: return SDLK_ESCAPE;
		case AKEYCODE_BACK: return SDLK_ESCAPE;   /* Android Back = Esc */
		case AKEYCODE_DEL: return SDLK_BACKSPACE; /* DEL is backspace on Android */
		case AKEYCODE_FORWARD_DEL: return SDLK_DELETE;
		case AKEYCODE_TAB: return SDLK_TAB;
		case AKEYCODE_SPACE: return SDLK_SPACE;

		/* Arrow keys */
		case AKEYCODE_DPAD_UP: return SDLK_UP;
		case AKEYCODE_DPAD_DOWN: return SDLK_DOWN;
		case AKEYCODE_DPAD_LEFT: return SDLK_LEFT;
		case AKEYCODE_DPAD_RIGHT: return SDLK_RIGHT;

		/* Punctuation the game uses */
		case AKEYCODE_MINUS: return SDLK_MINUS;
		case AKEYCODE_EQUALS: return SDLK_EQUALS;
		case AKEYCODE_LEFT_BRACKET: return SDLK_LEFTBRACKET;
		case AKEYCODE_RIGHT_BRACKET: return SDLK_RIGHTBRACKET;
		case AKEYCODE_BACKSLASH: return SDLK_BACKSLASH;
		case AKEYCODE_SEMICOLON: return SDLK_SEMICOLON;
		case AKEYCODE_APOSTROPHE: return SDLK_QUOTE;
		case AKEYCODE_GRAVE: return SDLK_BACKQUOTE;
		case AKEYCODE_COMMA: return SDLK_COMMA;
		case AKEYCODE_PERIOD: return SDLK_PERIOD;
		case AKEYCODE_SLASH: return SDLK_SLASH;

		/* Modifiers */
		case AKEYCODE_SHIFT_LEFT: return SDLK_LSHIFT;
		case AKEYCODE_SHIFT_RIGHT: return SDLK_RSHIFT;
		case AKEYCODE_CTRL_LEFT: return SDLK_LCTRL;
		case AKEYCODE_CTRL_RIGHT: return SDLK_RCTRL;
		case AKEYCODE_ALT_LEFT: return SDLK_LALT;
		case AKEYCODE_ALT_RIGHT: return SDLK_RALT;

		/* Function keys */
		case AKEYCODE_F1: return SDLK_F1;
		case AKEYCODE_F2: return SDLK_F2;
		case AKEYCODE_F3: return SDLK_F3;
		case AKEYCODE_F4: return SDLK_F4;
		case AKEYCODE_F5: return SDLK_F5;
		case AKEYCODE_F6: return SDLK_F6;
		case AKEYCODE_F7: return SDLK_F7;
		case AKEYCODE_F8: return SDLK_F8;
		case AKEYCODE_F9: return SDLK_F9;
		case AKEYCODE_F10: return SDLK_F10;
		case AKEYCODE_F11: return SDLK_F11;
		case AKEYCODE_F12: return SDLK_F12;

		/* Home / End / Page */
		case AKEYCODE_MOVE_HOME: return SDLK_HOME;
		case AKEYCODE_MOVE_END: return SDLK_END;
		case AKEYCODE_PAGE_UP: return SDLK_PAGEUP;
		case AKEYCODE_PAGE_DOWN: return SDLK_PAGEDOWN;
		case AKEYCODE_INSERT: return SDLK_INSERT;

		default: return SDLK_UNKNOWN;
	}
}

/*
 * nativeKeyEvent(action, androidKeyCode, unicodeChar)
 *   action: 0 = KEY_DOWN, 1 = KEY_UP
 *   androidKeyCode: AKEYCODE_* value
 *   unicodeChar: the Unicode character produced, or 0
 */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeKeyEvent(JNIEnv *env, jobject thiz,
                                                  jint action, jint androidKeyCode,
                                                  jint unicodeChar)
{
	if (androidKeyCode == AKEYCODE_DPAD_CENTER) {
		if (action == 0) {
			if (!g_dpad_center_down) {
				g_dpad_center_down = 1;
				Java_com_dxxredux_app_MainActivity_nativeJoystickButton(env, thiz, 0, 1);
			}
		} else if (g_dpad_center_down) {
			g_dpad_center_down = 0;
			Java_com_dxxredux_app_MainActivity_nativeJoystickButton(env, thiz, 0, 0);
		}
		return;
	}

	SDLKey sym = android_to_sdlk(androidKeyCode);
	if (androidKeyCode == AKEYCODE_DPAD_CENTER ||
	    androidKeyCode == AKEYCODE_ENTER ||
	    androidKeyCode == AKEYCODE_BACK ||
	    androidKeyCode == AKEYCODE_DPAD_UP ||
	    androidKeyCode == AKEYCODE_DPAD_DOWN ||
	    androidKeyCode == AKEYCODE_DPAD_LEFT ||
	    androidKeyCode == AKEYCODE_DPAD_RIGHT) {
		debug_log(DLOG_GAME,
		          "[AKEY] action=%d akey=%d sdl=%d unicode=%d",
		          action,
		          androidKeyCode,
		          sym,
		          unicodeChar);
	}
	if (sym == SDLK_UNKNOWN)
		return;

	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));

	ev.type = (action == 0) ? SDL_KEYDOWN : SDL_KEYUP;
	ev.key.state = (action == 0) ? SDL_PRESSED : SDL_RELEASED;
	ev.key.keysym.sym = sym;
	ev.key.keysym.mod = KMOD_NONE;
	ev.key.keysym.unicode = (Uint16) (unicodeChar & 0xFFFF);

	SDL_PushEvent(&ev);

	if (g_input_count <= 10)
		LOGI("key %s  akeycode=%d  sdlk=%d  unicode=%d",
		     action == 0 ? "DOWN" : "UP", androidKeyCode, sym, unicodeChar);
}

/* ── Lifecycle ──────────────────────────────────────────────
 *
 * Called when the Activity goes to background (onStop).
 * Injects an Escape key press so the engine opens its pause/game menu —
 * but ONLY when the game window is at the front (live gameplay).
 *
 * If a menu / dialog is already on top of the game window, injecting
 * Escape would *close* it (toggle behaviour) and unpause the game,
 * which is the opposite of what we want.  So we skip the injection
 * in that case — the game is already effectively paused because a
 * menu is modal over it.
 */

/* Engine symbols we read (but never modify) from the UI thread.
 * Only read simple globals (pointer / int) — do NOT traverse linked
 * lists like window_get_front() because the game thread mutates them. */
#include "game.h"    /* Game_wind */
#include "screens.h" /* SCREEN_GAME */
#include "inferno.h" /* Screen_mode */

/* Declared in digi_tsf_music.c — pause/resume MIDI when backgrounded */
extern void mix_background_pause(void);
extern void mix_background_resume(void);

/* Declared in rbaudio_bin.c - suspend redbook production without changing user pause */
extern void RBABackgroundPause(void);
extern void RBABackgroundResume(void);

/* Declared in android_surface.c — prevent rendering while backgrounded */
extern void android_surface_pause(void);
extern void android_surface_resume(void);

/* window_get_callback() is defined for all builds, but its declaration is
 * currently hidden behind INTROSPECT_ON in window.h. */
extern int (*window_get_callback(window *wind))(window *, d_event *, void *);
extern int pause_handler(window *wind, d_event *event, char *msg);

static void inject_key_tap(SDLKey sym)
{
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_KEYDOWN;
	ev.key.state = SDL_PRESSED;
	ev.key.keysym.sym = sym;
	ev.key.keysym.mod = KMOD_NONE;
	ev.key.keysym.unicode = 0;
	SDL_PushEvent(&ev);

	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_KEYUP;
	ev.key.state = SDL_RELEASED;
	ev.key.keysym.sym = sym;
	ev.key.keysym.mod = KMOD_NONE;
	ev.key.keysym.unicode = 0;
	SDL_PushEvent(&ev);
}

static int is_pause_window_front(void)
{
	window *front;
	int (*callback)(window *, d_event *, void *);

	front = window_get_front();
	if (!front || front == Game_wind)
		return 0;

	callback = window_get_callback(front);
	return callback == (int (*)(window *, d_event *, void *)) pause_handler;
}

static volatile int g_android_overlay_time_paused = 0;

int android_queue_saveload_request(int save_request)
{
	window *front;

	if (!Game_wind) {
		LOGI("nativeOpen%sMenuIfSafe: not in gameplay", save_request ? "Save" : "Load");
		return 0;
	}

	front = window_get_front();
	if (front != Game_wind && !is_pause_window_front()) {
		LOGI("nativeOpen%sMenuIfSafe: unsupported front window", save_request ? "Save" : "Load");
		return 0;
	}

	g_android_open_save_menu = save_request ? 1 : 0;
	g_android_open_load_menu = save_request ? 0 : 1;
	/* The save/load menu owns its own engine pause; transfer the Kotlin
	 * overlay's pause instead of leaving an unmatched stop_time() behind */
	if (g_android_overlay_time_paused) {
		start_time();
		g_android_overlay_time_paused = 0;
	}
	return 1;
}

static void android_log_autosave_gate(const char *event)
{
	debug_log(DLOG_GAME, "autosave lifecycle %s: game_wind=%d screen_mode=%d game_mode=%d request_kind=%d",
	          event, Game_wind ? 1 : 0, Screen_mode, Game_mode, g_android_autosave_request_kind);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeQueueMinimizeAutosave(JNIEnv *env, jobject thiz)
{
	(void) env;
	(void) thiz;
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeOnResume(JNIEnv *env, jobject thiz)
{
	(void) env;
	(void) thiz;
	android_lifecycle_diagnostics_request_visibility(
	    ANDROID_LIFECYCLE_VISIBILITY_FOREGROUND,
	    ANDROID_LIFECYCLE_REASON_ACTIVITY_RESUME);
	LOGI("nativeOnResume - foreground requested");
	android_surface_resume();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeOnPause(JNIEnv *env, jobject thiz)
{
	(void) env;
	(void) thiz;
	android_lifecycle_diagnostics_request_visibility(
	    ANDROID_LIFECYCLE_VISIBILITY_BACKGROUND,
	    ANDROID_LIFECYCLE_REASON_ACTIVITY_STOP);
	/* Stop the rendering thread from touching ANativeWindow before the
	 * surface is destroyed.  This must happen first so that by the time
	 * surfaceDestroyed → nativeSetSurface(null) runs, no blit is in
	 * progress or can start. */
	android_surface_pause();
	LOGI("nativeOnPause - background requested");
}

static int g_android_multiplayer_dormancy_timeout_requested;
static int g_android_multiplayer_dormancy_disconnect_pending;

extern JavaVM *g_jvm;
extern jobject g_activity;

static void android_notify_multiplayer_dormancy_disconnected(void)
{
	JNIEnv *env = NULL;
	jclass cls;
	jmethodID method;
	int attached = 0;

	if (!g_jvm || !g_activity)
		return;
	if ((*g_jvm)->GetEnv(g_jvm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
		if ((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK)
			return;
		attached = 1;
	}
	cls = (*env)->GetObjectClass(env, g_activity);
	method = cls ? (*env)->GetMethodID(
	                   env, cls, "onNativeMultiplayerDormancyDisconnected", "()V")
	             : NULL;
	if (method)
		(*env)->CallVoidMethod(env, g_activity, method);
	if (cls)
		(*env)->DeleteLocalRef(env, cls);
	if (attached)
		(*g_jvm)->DetachCurrentThread(g_jvm);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeRequestMultiplayerDormancyTimeout(
    JNIEnv *env, jobject thiz)
{
	(void) env;
	(void) thiz;
	__atomic_store_n(&g_android_multiplayer_dormancy_timeout_requested, 1,
	                 __ATOMIC_RELEASE);
}

void android_lifecycle_actions_game_tick(int screen_is_game, int has_game_window,
                                         int game_window_is_front, int multiplayer_active)
{
#ifdef NETWORK
	if (__atomic_exchange_n(&g_android_multiplayer_dormancy_timeout_requested, 0,
	                        __ATOMIC_ACQ_REL)) {
		g_android_multiplayer_dormancy_disconnect_pending = 1;
		if (multiplayer_active) {
			debug_log(DLOG_DORMANCY,
			          "multiplayer background timeout: requesting normal engine disconnect");
			multi_quit_game = 1;
		}
	}
	if (g_android_multiplayer_dormancy_disconnect_pending &&
	    !multiplayer_active) {
		g_android_multiplayer_dormancy_disconnect_pending = 0;
		debug_log(DLOG_DORMANCY,
		          "multiplayer background timeout: engine disconnected");
		android_notify_multiplayer_dormancy_disconnected();
		if (android_lifecycle_diagnostics_requested_visibility() ==
		    ANDROID_LIFECYCLE_VISIBILITY_BACKGROUND)
			android_lifecycle_diagnostics_request_visibility(
			    ANDROID_LIFECYCLE_VISIBILITY_BACKGROUND,
			    ANDROID_LIFECYCLE_REASON_MULTIPLAYER_TIMEOUT);
	}
#endif
	for (;;) {
		int visibility;
		uint64_t generation;

		if (!android_lifecycle_diagnostics_take_pending(&visibility, NULL,
		                                                &generation))
			return;

		if (visibility == ANDROID_LIFECYCLE_VISIBILITY_FOREGROUND) {
			android_profile_resume();
			mix_background_resume();
			RBABackgroundResume();
			androidaud_background_resume();
			android_lifecycle_diagnostics_set_suspend_state(
			    ANDROID_LIFECYCLE_SUSPEND_RUNNING);
			android_lifecycle_diagnostics_acknowledge(generation, visibility);
			return;
		}

		android_lifecycle_diagnostics_set_suspend_state(
		    ANDROID_LIFECYCLE_SUSPEND_QUIESCING);
		androidaud_background_pause();
		mix_background_pause();
		RBABackgroundPause();
		if (screen_is_game && has_game_window && !multiplayer_active) {
			int checkpoint_result;

			android_lifecycle_diagnostics_checkpoint_request(
			    generation, ANDROID_SAVE_META_SLOT_AUTO_MINIMIZE);
			android_lifecycle_diagnostics_checkpoint_writing();
			checkpoint_result = state_android_save_lifecycle_checkpoint(
			    ANDROID_SAVE_META_SLOT_AUTO_MINIMIZE, ANDROID_SAVE_DESC_AUTO_MINIMIZE,
			    ANDROID_SAVE_META_KIND_AUTO_MINIMIZE);
			android_lifecycle_diagnostics_checkpoint_finish(
			    checkpoint_result > 0
			        ? ANDROID_LIFECYCLE_CHECKPOINT_COMMITTED
			        : (checkpoint_result == 0
			               ? ANDROID_LIFECYCLE_CHECKPOINT_SKIPPED
			               : ANDROID_LIFECYCLE_CHECKPOINT_FAILED));
			if (game_window_is_front) {
				android_log_autosave_gate("pause-inject-escape");
				inject_key_tap(SDLK_ESCAPE);
			} else {
				android_log_autosave_gate("pause-window-already-open");
			}
		} else if (multiplayer_active) {
			android_lifecycle_diagnostics_checkpoint_request(generation, -1);
			android_lifecycle_diagnostics_checkpoint_finish(
			    ANDROID_LIFECYCLE_CHECKPOINT_SKIPPED);
			android_log_autosave_gate("pause-skipped-multiplayer");
		} else {
			android_lifecycle_diagnostics_checkpoint_request(generation, -1);
			android_lifecycle_diagnostics_checkpoint_finish(
			    ANDROID_LIFECYCLE_CHECKPOINT_SKIPPED);
			android_log_autosave_gate("pause-skipped-not-gameplay");
		}
		if (multiplayer_active) {
			android_lifecycle_diagnostics_set_suspend_state(
			    ANDROID_LIFECYCLE_SUSPEND_RUNNING);
			android_lifecycle_diagnostics_acknowledge(generation, visibility);
			return;
		}

		stop_time();
		if (android_lifecycle_diagnostics_park_until_wake(generation) ==
		    ANDROID_LIFECYCLE_WAKE_EXTERNAL) {
			start_time();
			android_lifecycle_diagnostics_set_suspend_state(
			    ANDROID_LIFECYCLE_SUSPEND_RUNNING);
			return;
		}
		start_time();
	}
}

void android_lifecycle_actions_request_wake(void)
{
	android_lifecycle_diagnostics_request_external_wake();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeUpdateDormancyUiPollCounters(
    JNIEnv *env, jobject thiz, jlong central, jlong independent)
{
	(void) env;
	(void) thiz;
	android_lifecycle_diagnostics_set_ui_poll_counters(
	    (uint64_t) central, (uint64_t) independent);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeInitializeDormancyDiagnostics(
    JNIEnv *env, jobject thiz)
{
	(void) env;
	(void) thiz;
	android_lifecycle_diagnostics_request_visibility(
	    ANDROID_LIFECYCLE_VISIBILITY_FOREGROUND,
	    ANDROID_LIFECYCLE_REASON_ACTIVITY_RESUME);
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeOpenSinglePlayerPauseIfSafe(JNIEnv *env, jobject thiz)
{
	if (!Game_wind || Screen_mode != SCREEN_GAME) {
		LOGI("nativeOpenSinglePlayerPauseIfSafe: not in live gameplay");
		return JNI_FALSE;
	}
	if (Game_mode & GM_MULTI) {
		LOGI("nativeOpenSinglePlayerPauseIfSafe: multiplayer active");
		return JNI_FALSE;
	}
	if (window_get_front() != Game_wind) {
		LOGI("nativeOpenSinglePlayerPauseIfSafe: menu already open");
		return JNI_FALSE;
	}
	inject_key_tap(SDLK_PAUSE);
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeOpenOverlayPauseIfSafe(JNIEnv *env, jobject thiz)
{
	(void) env;
	(void) thiz;
	return android_open_overlay_pause_if_safe() ? JNI_TRUE : JNI_FALSE;
}

int android_open_overlay_pause_if_safe(void)
{
	if (g_android_overlay_time_paused)
		return 1;
	if (!Game_wind || Screen_mode != SCREEN_GAME) {
		LOGI("nativeOpenOverlayPauseIfSafe: not in live gameplay");
		return 0;
	}
	if (Game_mode & GM_MULTI) {
		LOGI("nativeOpenOverlayPauseIfSafe: multiplayer active");
		return 0;
	}
	if (window_get_front() != Game_wind) {
		LOGI("nativeOpenOverlayPauseIfSafe: menu already open");
		return 0;
	}
	stop_time();
	g_android_overlay_time_paused = 1;
	return 1;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeCloseOverlayPauseIfOwned(JNIEnv *env, jobject thiz)
{
	if (!g_android_overlay_time_paused)
		return JNI_FALSE;
	start_time();
	g_android_overlay_time_paused = 0;
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeClosePauseIfFront(JNIEnv *env, jobject thiz)
{
	if (!Game_wind || (Game_mode & GM_MULTI) || !is_pause_window_front())
		return JNI_FALSE;

	inject_key_tap(SDLK_PAUSE);
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsGamePaused(JNIEnv *env, jobject thiz)
{
	(void) env;
	(void) thiz;
	if (!Game_wind || (Game_mode & GM_MULTI))
		return JNI_FALSE;
	return (g_android_overlay_time_paused || is_pause_window_front()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeOpenSaveMenuIfSafe(JNIEnv *env, jobject thiz)
{
	return android_queue_saveload_request(1) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeOpenLoadMenuIfSafe(JNIEnv *env, jobject thiz)
{
	return android_queue_saveload_request(0) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeOpenGameMenuIfSafe(JNIEnv *env, jobject thiz)
{
	window *front;

	if (!Game_wind || Screen_mode != SCREEN_GAME)
		return JNI_FALSE;

	front = window_get_front();
	if (front != Game_wind && !is_pause_window_front())
		return JNI_FALSE;

	g_android_open_game_menu = 1;
	return JNI_TRUE;
}

/* ── In-game query ──────────────────────────────────────────
 *
 * Returns true when the game window is the front window and we're
 * in SCREEN_GAME mode.  Used by the Kotlin overlay to decide whether
 * to show the virtual stick.
 */
#include "screens.h"  /* SCREEN_GAME */
#include "inferno.h"  /* Screen_mode */
#include "playsave.h" /* PlayerCfg */
#include "kconfig.h"  /* CONTROL_USING_JOYSTICK */

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsInGame(JNIEnv *env, jobject thiz)
{
	return (Game_wind != NULL && Screen_mode == SCREEN_GAME && Game_wind == window_get_front()) ? JNI_TRUE : JNI_FALSE;
}

/* Packed transient-screen state shared with MainActivity.kt.
 * Bits 0..7: kind, bit 8: ready, bit 9: can activate, bits 32..63: generation. */
JNIEXPORT jlong JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetScreenAdvanceState(JNIEnv *env, jobject thiz)
{
	const android_screen_advance_kind kind =
	    (android_screen_advance_kind) android_screen_advance_get_kind();
	const unsigned int generation = android_screen_advance_get_generation();
	jlong state = ((jlong) generation) << 32;

	(void) env;
	(void) thiz;
	state |= (jlong) (kind & 0xff);
	if (android_screen_advance_get_ready())
		state |= (jlong) 1 << 8;
	if (android_screen_advance_can_activate(kind))
		state |= (jlong) 1 << 9;
	return state;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeRequestScreenAdvance(JNIEnv *env, jobject thiz,
                                                              jlong generation)
{
	(void) env;
	(void) thiz;
	return android_screen_advance_request((unsigned int) generation) ? JNI_TRUE : JNI_FALSE;
}

extern volatile int g_intro_active;

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsIntroActive(JNIEnv *env, jobject thiz)
{
	return g_intro_active ? JNI_TRUE : JNI_FALSE;
}

extern volatile int g_skip_intro_pref;

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetSkipIntroMovie(JNIEnv *env, jobject thiz, jboolean enabled)
{
	g_skip_intro_pref = enabled ? 1 : 0;
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetHeadlightOffByDefaultQol(JNIEnv *env, jobject thiz, jboolean enabled)
{
	g_headlight_off_by_default_qol = enabled ? 1 : 0;
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetDemoRecordPerFrameState(JNIEnv *env, jobject thiz, jboolean enabled)
{
	g_demo_record_per_frame_state = enabled ? 1 : 0;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsDemoRecordingActive(JNIEnv *env, jobject thiz)
{
	return input_demo_recorder_is_active() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetRewindEnabled(JNIEnv *env, jobject thiz, jboolean enabled)
{
	android_rewind_set_enabled(enabled ? 1 : 0);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetRewindTargetSeconds(JNIEnv *env, jobject thiz, jint seconds)
{
	android_rewind_set_target_seconds((int) seconds);
}

extern volatile int g_saveload_menu_active;

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsSaveLoadMenuActive(JNIEnv *env, jobject thiz)
{
	return g_saveload_menu_active ? JNI_TRUE : JNI_FALSE;
}

extern volatile int g_host_selecting_players;

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsHostSelectingPlayers(JNIEnv *env, jobject thiz)
{
	return g_host_selecting_players ? JNI_TRUE : JNI_FALSE;
}

extern volatile int g_host_start_game_requested;

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeStartSelectedPlayers(JNIEnv *env, jobject thiz)
{
	g_host_start_game_requested = 1;
}

/*
 * android port: mid-game join request state.
 * WaitForRefuseAnswer is set by net_udp_do_refuse_stuff when a player
 * wants to join a game with RefusePlayers enabled. RefusePlayerName
 * contains the requesting player's callsign. Setting RefuseThisPlayer=1
 * is equivalent to pressing F6 (accept the join request).
 */
extern char WaitForRefuseAnswer, RefuseThisPlayer, RefusePlayerName[];

JNIEXPORT jstring JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetJoinRequest(JNIEnv *env, jobject thiz)
{
	if (WaitForRefuseAnswer)
		return (*env)->NewStringUTF(env, RefusePlayerName);
	return (*env)->NewStringUTF(env, "");
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeAcceptJoinRequest(JNIEnv *env, jobject thiz)
{
	RefuseThisPlayer = 1;
}

/*
 * Enable or disable the "Use Joystick" flag in the player config.
 * Called from Kotlin when the touch overlay is activated/deactivated.
 *
 * When enabling, we also clear CONTROL_USING_MOUSE so that stray
 * SDL_MOUSEBUTTONDOWN events (from touches that reached the surface
 * before the overlay took ownership) don't trigger fire_primary.
 * The original ControlType is saved and restored on disable.
 */
static ubyte g_saved_control_type = 0;
static int g_overlay_joystick = 0;
static void android_touch_enable_joystick_mode(void)
{
	if (!g_overlay_joystick) {
		g_saved_control_type = PlayerCfg.ControlType;
		g_overlay_joystick = 1;
	}
	PlayerCfg.ControlType = CONTROL_USING_JOYSTICK; /* joystick only */
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetJoystickEnabled(JNIEnv *env, jobject thiz,
                                                            jboolean enabled)
{
	if (enabled) {
		int was_overlay_joystick = g_overlay_joystick;
		android_touch_enable_joystick_mode();

		if (!was_overlay_joystick) {
			/* Release any stuck mouse-button state so fire stops immediately */
			SDL_Event ev;
			memset(&ev, 0, sizeof(ev));
			ev.type = SDL_MOUSEBUTTONUP;
			ev.button.button = SDL_BUTTON_LEFT;
			ev.button.state = SDL_RELEASED;
			SDL_PushEvent(&ev);
		}
	} else if (!enabled && g_overlay_joystick) {
		PlayerCfg.ControlType = g_saved_control_type;
		g_overlay_joystick = 0;
	}
	LOGI("joystick %s (ControlType=%d, saved=%d)",
	     enabled ? "ENABLED" : "DISABLED",
	     PlayerCfg.ControlType, g_saved_control_type);
}

/* ── Soft-keyboard text input ───────────────────────────────
 *
 * Called by the Kotlin InputConnection when the soft keyboard commits text.
 * Injects a key-down + key-up pair for each character, with the unicode
 * value set so key_handler() buffers it for key_ascii() → newmenu text entry.
 *
 * SDL 1.2 SDLK values equal ASCII for the printable range, so we use the
 * character value directly as the sym.  key_handler() buffers the unicode
 * BEFORE checking key_properties, so even if the keycode lookup fails
 * the character still reaches the text input system.
 */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeTextInput(JNIEnv *env, jobject thiz,
                                                   jint unicodeChar)
{
	if (unicodeChar <= 0) return;

	SDLKey sym;
	if (unicodeChar >= 32 && unicodeChar < 127)
		sym = (SDLKey) unicodeChar;
	else
		sym = SDLK_SPACE; /* fallback; unicode carries the real char */

	SDL_Event ev;

	/* Key-down with unicode */
	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_KEYDOWN;
	ev.key.state = SDL_PRESSED;
	ev.key.keysym.sym = sym;
	ev.key.keysym.mod = KMOD_NONE;
	ev.key.keysym.unicode = (Uint16) (unicodeChar & 0xFFFF);
	SDL_PushEvent(&ev);

	/* Key-up */
	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_KEYUP;
	ev.key.state = SDL_RELEASED;
	ev.key.keysym.sym = sym;
	ev.key.keysym.mod = KMOD_NONE;
	ev.key.keysym.unicode = 0;
	SDL_PushEvent(&ev);
}

/* ── Joystick (Android gamepad) ─────────────────────────────
 *
 * The Kotlin side detects a gamepad via InputDevice.SOURCE_JOYSTICK and
 * forwards axis / button events here. Axes publish their latest mixed state
 * to the game-thread mailbox while buttons retain discrete SDL delivery.
 *
 * joy_init() registers a virtual joystick (index 0) on Android so the
 * axis_map / button_map lookups work correctly.
 *
 * Axis IDs (matches Android → virtual joystick registration order):
 *   0 = left stick X   1 = left stick Y
 *   2 = right stick X  3 = right stick Y
 *   4 = L trigger       5 = R trigger
 *   6-7 = gyro virtual axes
 *   8-10 = configurable controller half-axis combiners
 *
 * Button IDs: 0=A 1=B 2=X 3=Y 4=L1 5=R1 6=Select 7=Start 8=L3 9=R3
 */

static int g_joy_axis_count = 0; /* debug counter */

android_axis_generation android_joystick_axis_publish(int axis, int raw_value,
                                                      int touch_source)
{
	return android_axis_mailbox_publish(axis, raw_value, touch_source);
}

/*
 * nativeJoystickAxis(axis, value, touchActive)
 *   axis:        mixed axis index 0-10
 *   value:       -1.0 .. 1.0 (Android MotionEvent range)
 *   touchActive: nonzero when touch currently contributes to this mixed axis
 */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeJoystickAxis(JNIEnv *env, jobject thiz,
                                                      jint axis, jfloat value,
                                                      jboolean touchActive)
{
	static int joy_jni_diag_count[8];
	const int touch_source = touchActive != JNI_FALSE;
	const int raw_value = (int) (value * 32767.0f);
	if (touch_source)
		android_touch_enable_joystick_mode();
	if (axis >= 0 && axis < 8 && (axis == 2 || axis == 3) && value != 0.0f) {
		int abs_sdl = raw_value < 0 ? -raw_value : raw_value;
		int count = ++joy_jni_diag_count[axis];
		if (count <= 24 || (abs_sdl <= 4096 && (count % 8) == 0) || (count % 64) == 0)
			debug_log(DLOG_GAME, "[joy-jni] axis=%d touch=%d in=%.4f sdl=%d\n", axis, touch_source, value, raw_value);
	}

	android_joystick_axis_publish(axis, raw_value, touch_source);

	if (++g_joy_axis_count <= 5)
		LOGI("joystick axis %d = %.3f (sdl %d)", axis, value, raw_value);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeJoystickAxes(
    JNIEnv *env, jobject thiz, jintArray axis_array,
    jfloatArray value_array, jbooleanArray touch_array)
{
	(void) thiz;
	if (!axis_array || !value_array || !touch_array)
		return;
	const jsize count = (*env)->GetArrayLength(env, axis_array);
	if (count <= 0 || count > ANDROID_AXIS_MAILBOX_AXIS_COUNT ||
	    (*env)->GetArrayLength(env, value_array) != count ||
	    (*env)->GetArrayLength(env, touch_array) != count)
		return;

	jint *jni_axes = (*env)->GetIntArrayElements(env, axis_array, NULL);
	jfloat *jni_values = (*env)->GetFloatArrayElements(env, value_array, NULL);
	jboolean *jni_touches = (*env)->GetBooleanArrayElements(env, touch_array, NULL);
	if (!jni_axes || !jni_values || !jni_touches) {
		if (jni_axes)
			(*env)->ReleaseIntArrayElements(env, axis_array, jni_axes, JNI_ABORT);
		if (jni_values)
			(*env)->ReleaseFloatArrayElements(env, value_array, jni_values, JNI_ABORT);
		if (jni_touches)
			(*env)->ReleaseBooleanArrayElements(env, touch_array, jni_touches, JNI_ABORT);
		return;
	}

	int axes[ANDROID_AXIS_MAILBOX_AXIS_COUNT];
	int raw_values[ANDROID_AXIS_MAILBOX_AXIS_COUNT];
	unsigned char touch_sources[ANDROID_AXIS_MAILBOX_AXIS_COUNT];
	for (jsize index = 0; index < count; ++index) {
		axes[index] = jni_axes[index];
		raw_values[index] = (int) (jni_values[index] * 32767.0f);
		touch_sources[index] = jni_touches[index] != JNI_FALSE;
		if (touch_sources[index])
			android_touch_enable_joystick_mode();
	}
	android_axis_mailbox_publish_batch(axes, raw_values, touch_sources, count);

	(*env)->ReleaseIntArrayElements(env, axis_array, jni_axes, JNI_ABORT);
	(*env)->ReleaseFloatArrayElements(env, value_array, jni_values, JNI_ABORT);
	(*env)->ReleaseBooleanArrayElements(env, touch_array, jni_touches, JNI_ABORT);
}

/*
 * nativeJoystickButton(button, pressed)
 *   button:  button index 0-9
 *   pressed: 1 = down, 0 = up
 */
static int android_track_joystick_button(int button, int pressed)
{
	if (button < 0 || button >= (int) sizeof(g_joy_buttons_down))
		return 1;

	if (pressed) {
		g_joy_buttons_down[button] = 1;
		if (android_cutscene_tap_suppressed()) {
			g_suppressed_joy_buttons[button] = 1;
			g_cutscene_tap_suppress_hits++;
			return 0;
		}
	} else {
		g_joy_buttons_down[button] = 0;
		android_update_cutscene_release_gate();
		if (g_suppressed_joy_buttons[button]) {
			g_suppressed_joy_buttons[button] = 0;
			return 0;
		}
	}

	return 1;
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeJoystickButton(JNIEnv *env, jobject thiz,
                                                        jint button, jint pressed)
{
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));

	if (!android_track_joystick_button((int) button, pressed != 0))
		return;

	ev.type = pressed ? SDL_JOYBUTTONDOWN : SDL_JOYBUTTONUP;
	ev.jbutton.which = 0;
	ev.jbutton.button = (Uint8) button;
	ev.jbutton.state = pressed ? SDL_PRESSED : SDL_RELEASED;

	SDL_PushEvent(&ev);

	LOGI("joystick button %d %s", button, pressed ? "DOWN" : "UP");
}

void android_automation_joystick_button(int button, int pressed)
{
	SDL_JoyButtonEvent ev;
	if (!android_track_joystick_button(button, pressed != 0))
		return;
	memset(&ev, 0, sizeof(ev));
	ev.type = pressed ? SDL_JOYBUTTONDOWN : SDL_JOYBUTTONUP;
	ev.which = 0;
	ev.button = (Uint8) button;
	ev.state = pressed ? SDL_PRESSED : SDL_RELEASED;
	joy_button_handler(&ev);
}

int android_axis_mailbox_drain(int joystick_enabled)
{
	android_axis_mailbox_snapshot snapshot;
	android_axis_mailbox_transition transition;
	unsigned char transition_seen[ANDROID_AXIS_MAILBOX_AXIS_COUNT] = { 0 };
	int transition_raw[ANDROID_AXIS_MAILBOX_AXIS_COUNT] = { 0 };
	unsigned char transition_touch[ANDROID_AXIS_MAILBOX_AXIS_COUNT] = { 0 };
	int dispatch_count = 0;
	int processed_count = 0;

	if (!android_axis_mailbox_take_snapshot(&snapshot))
		return 0;

	/* Preserve discrete axis-button crossings while continuous values coalesce. */
	while (android_axis_mailbox_take_transition(snapshot.batch_generation,
	                                            &transition)) {
		SDL_JoyAxisEvent ev;

		if (!joystick_enabled)
			continue;
		memset(&ev, 0, sizeof(ev));
		ev.type = SDL_JOYAXISMOTION;
		ev.which = 0;
		ev.axis = (Uint8) (transition.axis |
		                   (transition.touch_source ? ANDROID_TOUCH_AXIS_FLAG : 0));
		ev.value = (Sint16) transition.raw_value;
#ifdef INTROSPECT_ON
		game_automate_axis_dispatch_begin(
		    transition.generation, transition.axis,
		    transition.raw_value, transition.touch_source);
#endif
		if (transition.axis < ANDROID_AXIS_MAILBOX_AXIS_BUTTON_COUNT &&
		    joy_axisbutton_handler(&ev))
			++processed_count;
		if (joy_axis_handler(&ev))
			++processed_count;
#ifdef INTROSPECT_ON
		game_automate_axis_dispatch_end();
#endif
		transition_seen[transition.axis] = 1;
		transition_raw[transition.axis] = transition.raw_value;
		transition_touch[transition.axis] = transition.touch_source != 0;
		++dispatch_count;
	}

	if (!joystick_enabled) {
		android_axis_mailbox_mark_applied(&snapshot, 0);
		return 0;
	}

	for (int axis = 0; axis < ANDROID_AXIS_MAILBOX_AXIS_COUNT; ++axis) {
		SDL_JoyAxisEvent ev;

		if (!snapshot.should_dispatch[axis])
			continue;
		if (transition_seen[axis] &&
		    transition_raw[axis] == snapshot.raw_value[axis] &&
		    transition_touch[axis] == snapshot.touch_source[axis])
			continue;
		memset(&ev, 0, sizeof(ev));
		ev.type = SDL_JOYAXISMOTION;
		ev.which = 0;
		ev.axis = (Uint8) (axis | (snapshot.touch_source[axis] ? ANDROID_TOUCH_AXIS_FLAG : 0));
		ev.value = (Sint16) snapshot.raw_value[axis];

#ifdef INTROSPECT_ON
		game_automate_axis_dispatch_begin(
		    snapshot.axis_generation[axis], axis,
		    snapshot.raw_value[axis], snapshot.touch_source[axis]);
#endif
		if (axis < ANDROID_AXIS_MAILBOX_AXIS_BUTTON_COUNT &&
		    joy_axisbutton_handler(&ev))
			++processed_count;
		if (joy_axis_handler(&ev))
			++processed_count;
#ifdef INTROSPECT_ON
		game_automate_axis_dispatch_end();
#endif
		++dispatch_count;
	}

	android_axis_mailbox_mark_applied(&snapshot, dispatch_count);
	return processed_count;
}

/* ── Automap touch controls ─────────────────────────────────
 *
 * nativeIsAutomapActive() — returns true when the 3-D automap is displayed.
 */
JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsAutomapActive(JNIEnv *env, jobject thiz)
{
	return Automap_active ? JNI_TRUE : JNI_FALSE;
}

/*
 * nativeAutomapCenter() — request automap re-center on player position
 */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeAutomapCenter(JNIEnv *env, jobject thiz)
{
	g_automap_center = 1;
}

/*
 * nativeAutomapSetMarker(idx) — request that automap sets a D2 marker slot.
 */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeAutomapSetMarker(JNIEnv *env, jobject thiz,
                                                          jint idx)
{
	if (idx < 0 || idx > 8) return;
	g_automap_set_marker = idx;
}

/*
 * nativeAutomapSelectMarker(idx) — request that automap jumps to a D2 marker slot.
 */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeAutomapSelectMarker(JNIEnv *env, jobject thiz,
                                                             jint idx)
{
	if (idx < 0 || idx > 8) return;
	g_automap_go_marker = idx;
}

/*
 * nativeAutomapNameMarker() - request the classic marker text entry flow.
 */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeAutomapNameMarker(JNIEnv *env, jobject thiz)
{
	g_automap_name_marker = 1;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeSecretAreaRevealActive(JNIEnv *env, jobject thiz)
{
	return secret_area_get_reveal_unfound() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeToggleSecretAreaReveal(JNIEnv *env, jobject thiz)
{
	if (PlayerCfg.MapCheatsAccessible)
		secret_area_set_reveal_unfound(!secret_area_get_reveal_unfound());
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeMatcenMode(JNIEnv *env, jobject thiz)
{
	return matcen_mode_get();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeCycleMatcenMode(JNIEnv *env, jobject thiz)
{
	if (PlayerCfg.MapCheatsAccessible)
		android_matcen_mode_cycle_pending = 1;
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeObjectiveOverlayMode(JNIEnv *env, jobject thiz)
{
	return level_metadata_get_objective_mode();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeCycleObjectiveOverlay(JNIEnv *env, jobject thiz)
{
	if (PlayerCfg.MapCheatsAccessible)
		level_metadata_cycle_objective_mode();
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeMapCheatsAccessible(JNIEnv *env, jobject thiz)
{
	return PlayerCfg.MapCheatsAccessible ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeReactorCountdownActive(JNIEnv *env, jobject thiz)
{
	return reactor_countdown_is_active() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeReactorCountdownPaused(JNIEnv *env, jobject thiz)
{
	return Reactor_countdown_paused ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeReactorPauseAllowed(JNIEnv *env, jobject thiz)
{
	return (!(Game_mode & GM_MULTI) || (Game_mode & GM_MULTI_COOP)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeToggleReactorCountdownPause(JNIEnv *env, jobject thiz)
{
	if (PlayerCfg.MapCheatsAccessible)
		android_reactor_pause_toggle_pending = 1;
}

/* ── C→Java keyboard callbacks ──────────────────────────────
 *
 * The engine calls these from the game thread when a menu with a text
 * input field is activated or closed.  We call back to Java via JNI
 * to show/hide the Android soft keyboard.
 */
extern JavaVM *g_jvm;
extern jobject g_activity;

/* ── Keyboard viewport offset ───────────────────────────────
 *
 * When the soft keyboard is visible it covers the bottom of the screen.
 * We shift the rendered canvas upward so the active text input field
 * is centered in the non-occluded visible area.
 *
 * g_keyboard_height_native / g_screen_height_native: set from Kotlin
 *   via nativeSetKeyboardHeight() when IME insets change.
 * g_active_input_field_y: canvas Y of the text input field, set
 *   when android_show_keyboard() is called from newmenu.c.
 * g_blit_y_offset: the computed pixel offset applied during blit,
 *   also used by nativeTouchEvent() to remap touch coordinates.
 */
static volatile int g_keyboard_height_native = 0;
static volatile int g_screen_height_native = 0;
static volatile int g_active_input_field_y = 0;
volatile int g_blit_y_offset = 0;

int android_get_keyboard_y_offset(int canvas_h)
{
	int kb_h = g_keyboard_height_native;
	int scr_h = g_screen_height_native;
	if (kb_h <= 0 || scr_h <= 0)
		return 0;

	int kb_game = kb_h * canvas_h / scr_h;
	int visible_h = canvas_h - kb_game;
	if (visible_h <= 0) {
		static int vis_warn = 0;
		if (!vis_warn) {
			LOGI("kb_y_offset: canvas_h=%d kb_game=%d visible_h=%d (<=0)", canvas_h, kb_game, visible_h);
			vis_warn = 1;
		}
		return 0;
	}

	int field_y = g_active_input_field_y;
	int field_y_orig = field_y;

	/* If scale-blit is active, remap field_y to post-scale position */
	if (g_menu_scale_active && g_menu_scale_src_h > 0 && g_menu_scale_dst_h > 0) {
		field_y = g_menu_scale_dst_y +
		          (field_y - g_menu_scale_src_y) * g_menu_scale_dst_h / g_menu_scale_src_h;
	}

	int target_center = visible_h / 2;
	int offset = field_y - target_center;
	if (offset < 0) offset = 0;
	if (offset > kb_game) offset = kb_game;

	static int last_logged_offset = -1;
	if (offset != last_logged_offset) {
		LOGI("kb_y_offset: kb_native=%d scr_native=%d canvas_h=%d kb_game=%d "
		     "visible_h=%d field_y=%d(orig=%d) scale=%d offset=%d",
		     kb_h, scr_h, canvas_h, kb_game,
		     visible_h, field_y, field_y_orig, g_menu_scale_active, offset);
		last_logged_offset = offset;
	}

	return offset;
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetKeyboardHeight(JNIEnv *env, jobject thiz,
                                                           jint heightPx, jint screenHeightPx)
{
	g_keyboard_height_native = heightPx;
	g_screen_height_native = screenHeightPx;
}

/* Expose keyboard state for introspection (statics can't be externed) */
int android_get_keyboard_state(int *kb_native, int *scr_native, int *field_y)
{
	if (kb_native) *kb_native = g_keyboard_height_native;
	if (scr_native) *scr_native = g_screen_height_native;
	if (field_y) *field_y = g_active_input_field_y;
	return 1;
}

static volatile int g_keyboard_requested = 0;

void android_update_keyboard_field_y(int field_y)
{
	g_active_input_field_y = field_y;
}

int android_is_keyboard_shown(void)
{
	return g_keyboard_requested;
}

void android_show_keyboard(int numeric, int field_y, const char *initial_text)
{
	if (!g_jvm || !g_activity) return;

	g_active_input_field_y = field_y;
	g_keyboard_requested = 1;

	JNIEnv *env;
	int attached = 0;
	if ((*g_jvm)->GetEnv(g_jvm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
		(*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
		attached = 1;
	}

	jclass cls = (*env)->GetObjectClass(env, g_activity);
	jmethodID mid = (*env)->GetMethodID(env, cls, "showKeyboard", "(ILjava/lang/String;)V");
	if (mid) {
		jstring text = (*env)->NewStringUTF(env, initial_text ? initial_text : "");
		/* Android InputType: TYPE_CLASS_TEXT=1, TYPE_CLASS_NUMBER=2 */
		(*env)->CallVoidMethod(env, g_activity, mid, numeric ? 2 : 1, text);
		(*env)->DeleteLocalRef(env, text);
	}

	if (attached) (*g_jvm)->DetachCurrentThread(g_jvm);
}

void android_hide_keyboard(void)
{
	if (!g_jvm || !g_activity) return;

	JNIEnv *env;
	int attached = 0;
	if ((*g_jvm)->GetEnv(g_jvm, (void **) &env, JNI_VERSION_1_6) != JNI_OK) {
		(*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
		attached = 1;
	}

	jclass cls = (*env)->GetObjectClass(env, g_activity);
	jmethodID mid = (*env)->GetMethodID(env, cls, "hideKeyboard", "()V");
	if (mid) {
		(*env)->CallVoidMethod(env, g_activity, mid);
	}

	if (attached) (*g_jvm)->DetachCurrentThread(g_jvm);
	g_keyboard_height_native = 0;
	g_blit_y_offset = 0;
	g_keyboard_requested = 0;
	LOGI("android_hide_keyboard()");
}

/* ── Admin tray JNI functions ───────────────────────────────
 *
 * Cockpit cycling, auto-leveling toggle, and state queries
 * for the in-game admin settings tray.
 */
#include "object.h" /* ConsoleObject, PF_LEVELLING */

extern void toggle_cockpit(void);
extern void toggle_cockpit_reverse(void);

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeCycleCockpit(JNIEnv *env, jobject thiz,
                                                      jint direction)
{
	if (direction > 0)
		toggle_cockpit();
	else
		toggle_cockpit_reverse();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeToggleAutoLeveling(JNIEnv *env, jobject thiz)
{
	PlayerCfg.AutoLeveling = !PlayerCfg.AutoLeveling;
	if (ConsoleObject) {
		if (PlayerCfg.AutoLeveling)
			ConsoleObject->mtype.phys_info.flags |= PF_LEVELLING;
		else
			ConsoleObject->mtype.phys_info.flags &= ~PF_LEVELLING;
	}
	write_player_file();
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetAutoLeveling(JNIEnv *env, jobject thiz)
{
	return PlayerCfg.AutoLeveling ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetCockpitMode(JNIEnv *env, jobject thiz)
{
	return PlayerCfg.PreferredCockpitMode;
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetDifficulty(JNIEnv *env, jobject thiz)
{
	return Difficulty_level;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeCanShowDifficultyChange(JNIEnv *env, jobject thiz)
{
	return difficulty_can_show_live() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeCanChangeDifficulty(JNIEnv *env, jobject thiz)
{
	return difficulty_can_change_live() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetDifficulty(JNIEnv *env, jobject thiz,
                                                       jint difficulty)
{
	if (difficulty < 0 || difficulty >= NDL)
		return JNI_FALSE;
	if (!difficulty_can_change_live())
		return JNI_FALSE;
	g_android_difficulty_request = difficulty;
	return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_LevelPreviewActivity_nativeKeyEvent(
    JNIEnv *env, jobject thiz, jint action, jint key_code, jint unicode_char)
{
	Java_com_dxxredux_app_MainActivity_nativeKeyEvent(
	    env, thiz, action, key_code, unicode_char);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_LevelPreviewActivity_nativeJoystickAxis(
    JNIEnv *env, jobject thiz, jint axis, jfloat value, jboolean touch_active)
{
	Java_com_dxxredux_app_MainActivity_nativeJoystickAxis(
	    env, thiz, axis, value, touch_active);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_LevelPreviewActivity_nativeJoystickAxes(
    JNIEnv *env, jobject thiz, jintArray axes, jfloatArray values,
    jbooleanArray touch_active)
{
	Java_com_dxxredux_app_MainActivity_nativeJoystickAxes(
	    env, thiz, axes, values, touch_active);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_LevelPreviewActivity_nativeJoystickButton(
    JNIEnv *env, jobject thiz, jint button, jint pressed)
{
	Java_com_dxxredux_app_MainActivity_nativeJoystickButton(
	    env, thiz, button, pressed);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_LevelPreviewActivity_nativeAutomapCenter(
    JNIEnv *env, jobject thiz)
{
	Java_com_dxxredux_app_MainActivity_nativeAutomapCenter(env, thiz);
}

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_LevelPreviewActivity_nativeSecretAreaRevealActive(
    JNIEnv *env, jobject thiz)
{
	return Java_com_dxxredux_app_MainActivity_nativeSecretAreaRevealActive(
	    env, thiz);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_LevelPreviewActivity_nativeToggleSecretAreaReveal(
    JNIEnv *env, jobject thiz)
{
	Java_com_dxxredux_app_MainActivity_nativeToggleSecretAreaReveal(env, thiz);
}

JNIEXPORT jint JNICALL
Java_com_dxxredux_app_LevelPreviewActivity_nativeObjectiveOverlayMode(
    JNIEnv *env, jobject thiz)
{
	return Java_com_dxxredux_app_MainActivity_nativeObjectiveOverlayMode(
	    env, thiz);
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_LevelPreviewActivity_nativeCycleObjectiveOverlay(
    JNIEnv *env, jobject thiz)
{
	Java_com_dxxredux_app_MainActivity_nativeCycleObjectiveOverlay(env, thiz);
}

#endif /* ANDROID */
