/*
 * android_input.c — Inject Android touch, key & joystick events into SDL 1.2's event queue.
 *
 * Touch events are converted to SDL_MOUSEMOTION / SDL_MOUSEBUTTONDOWN / UP.
 * Key events are converted to SDL_KEYDOWN / SDL_KEYUP.
 * Joystick events are converted to SDL_JOYAXISMOTION / SDL_JOYBUTTONDOWN / UP.
 *
 * All injection uses SDL_PushEvent() so the existing event_poll() → key_handler()
 * / mouse_button_handler() / mouse_motion_handler() / joy_*_handler() pipeline
 * works unchanged.
 */

#ifdef ANDROID

#include <jni.h>
#include <android/log.h>
#include <SDL.h>
#include <string.h>
#include <android/keycodes.h>
#include "fix.h"
#include "gr.h"

/* Automap_active is defined in automap.c; we only need the extern. */
extern int Automap_active;

#define LOG_TAG "DXX-Input"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

/* ── Automap touch input accumulator ────────────────────────
 *
 * Written from the UI thread (nativeAutomapInput), read + zeroed by the
 * game thread in automap_apply_input().  Simple volatile is fine here:
 * both sides are single-writer and a lost frame of input is negligible.
 */
volatile fix g_automap_heading  = 0;
volatile fix g_automap_pitch    = 0;
volatile fix g_automap_thrust   = 0;
volatile fix g_automap_bank     = 0;
volatile fix g_automap_vertical = 0;
volatile fix g_automap_sideways = 0;
volatile int g_automap_center   = 0;

/* ── Skippable-screen flag (movies, briefings) ──────────────
 * Set to 1 by the game thread while inside a skippable event loop
 * (movie playback, briefing screens).  Read by the Kotlin UI thread
 * to show/hide the Skip overlay button.
 */
volatile int g_skippable_active = 0;

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
static int g_touch_active = 0;   /* is a finger currently down? */
static int g_input_count  = 0;   /* debug counter */

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

static void remap_touch(int *gx, int *gy)
{
    if (!g_menu_scale_active || g_menu_scale_dst_w <= 0 || g_menu_scale_dst_h <= 0)
        return;

    int tx = *gx, ty = *gy;
    /* If the touch is within the enlarged (destination) rect, map it
     * back to the original (source) rect so the menu code sees the
     * correct item coordinates. */
    if (tx >= g_menu_scale_dst_x && tx < g_menu_scale_dst_x + g_menu_scale_dst_w &&
        ty >= g_menu_scale_dst_y && ty < g_menu_scale_dst_y + g_menu_scale_dst_h) {
        *gx = g_menu_scale_src_x + (tx - g_menu_scale_dst_x) * g_menu_scale_src_w / g_menu_scale_dst_w;
        *gy = g_menu_scale_src_y + (ty - g_menu_scale_dst_y) * g_menu_scale_src_h / g_menu_scale_dst_h;
    }
    /* Touches outside the enlarged rect pass through — they'll hit
     * background area and the menu will ignore them. */
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeTouchEvent(JNIEnv *env, jobject thiz,
                                                     jint action, jfloat normX, jfloat normY)
{
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));

    /* Map normalised coordinates to the engine's actual resolution.
     * This avoids any mismatch between the Kotlin-side GAME_W/H
     * (from SharedPreferences) and the real engine resolution
     * (from descent.cfg via grd_curscreen). */
    int screenW = grd_curscreen ? grd_curscreen->sc_w : 640;
    int screenH = grd_curscreen ? grd_curscreen->sc_h : 480;
    jint gameX = (jint)(normX * screenW);
    jint gameY = (jint)(normY * screenH);
    if (gameX < 0) gameX = 0;
    if (gameX >= screenW) gameX = screenW - 1;
    if (gameY < 0) gameY = 0;
    if (gameY >= screenH) gameY = screenH - 1;

    remap_touch(&gameX, &gameY);

    switch (action) {
    case 0: /* ACTION_DOWN */
        g_last_touch_x = gameX;
        g_last_touch_y = gameY;
        g_touch_active = 1;

        /* Send a motion event to position the cursor.
         * On Android, mouse_motion_handler() uses absolute x/y when
         * xrel==0 && yrel==0 (see mouse.c). */
        ev.type = SDL_MOUSEMOTION;
        ev.motion.x      = (Uint16)gameX;
        ev.motion.y      = (Uint16)gameY;
        ev.motion.xrel    = 0;
        ev.motion.yrel    = 0;
        ev.motion.state   = 0;
        SDL_PushEvent(&ev);

        /* Then send button-down (left button = SDL_BUTTON_LEFT = 1) */
        memset(&ev, 0, sizeof(ev));
        ev.type = SDL_MOUSEBUTTONDOWN;
        ev.button.button = SDL_BUTTON_LEFT;
        ev.button.state  = SDL_PRESSED;
        ev.button.x      = (Uint16)gameX;
        ev.button.y      = (Uint16)gameY;
        SDL_PushEvent(&ev);

        if (++g_input_count <= 5)
            LOGI("touch DOWN at (%d,%d)", gameX, gameY);
        break;

    case 1: /* ACTION_MOVE */ {
        if (!g_touch_active) break;
        g_last_touch_x = gameX;
        g_last_touch_y = gameY;

        /* Send motion with xrel=0, yrel=0 so mouse_motion_handler uses
         * absolute x/y positioning (Android path). */
        ev.type = SDL_MOUSEMOTION;
        ev.motion.x      = (Uint16)gameX;
        ev.motion.y      = (Uint16)gameY;
        ev.motion.xrel    = 0;
        ev.motion.yrel    = 0;
        ev.motion.state   = SDL_BUTTON(SDL_BUTTON_LEFT);
        SDL_PushEvent(&ev);
        break;
    }

    case 2: /* ACTION_UP */
        g_touch_active = 0;

        ev.type = SDL_MOUSEBUTTONUP;
        ev.button.button = SDL_BUTTON_LEFT;
        ev.button.state  = SDL_RELEASED;
        ev.button.x      = (Uint16)gameX;
        ev.button.y      = (Uint16)gameY;
        SDL_PushEvent(&ev);

        if (g_input_count <= 5)
            LOGI("touch UP   at (%d,%d)", gameX, gameY);
        break;
    }
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
    case AKEYCODE_ENTER:      return SDLK_RETURN;
    case AKEYCODE_ESCAPE:     return SDLK_ESCAPE;
    case AKEYCODE_BACK:       return SDLK_ESCAPE;   /* Android Back = Esc */
    case AKEYCODE_DEL:        return SDLK_BACKSPACE; /* DEL is backspace on Android */
    case AKEYCODE_FORWARD_DEL:return SDLK_DELETE;
    case AKEYCODE_TAB:        return SDLK_TAB;
    case AKEYCODE_SPACE:      return SDLK_SPACE;

    /* Arrow keys */
    case AKEYCODE_DPAD_UP:    return SDLK_UP;
    case AKEYCODE_DPAD_DOWN:  return SDLK_DOWN;
    case AKEYCODE_DPAD_LEFT:  return SDLK_LEFT;
    case AKEYCODE_DPAD_RIGHT: return SDLK_RIGHT;

    /* Punctuation the game uses */
    case AKEYCODE_MINUS:        return SDLK_MINUS;
    case AKEYCODE_EQUALS:       return SDLK_EQUALS;
    case AKEYCODE_LEFT_BRACKET: return SDLK_LEFTBRACKET;
    case AKEYCODE_RIGHT_BRACKET:return SDLK_RIGHTBRACKET;
    case AKEYCODE_BACKSLASH:    return SDLK_BACKSLASH;
    case AKEYCODE_SEMICOLON:    return SDLK_SEMICOLON;
    case AKEYCODE_APOSTROPHE:   return SDLK_QUOTE;
    case AKEYCODE_GRAVE:        return SDLK_BACKQUOTE;
    case AKEYCODE_COMMA:        return SDLK_COMMA;
    case AKEYCODE_PERIOD:       return SDLK_PERIOD;
    case AKEYCODE_SLASH:        return SDLK_SLASH;

    /* Modifiers */
    case AKEYCODE_SHIFT_LEFT:   return SDLK_LSHIFT;
    case AKEYCODE_SHIFT_RIGHT:  return SDLK_RSHIFT;
    case AKEYCODE_CTRL_LEFT:    return SDLK_LCTRL;
    case AKEYCODE_CTRL_RIGHT:   return SDLK_RCTRL;
    case AKEYCODE_ALT_LEFT:     return SDLK_LALT;
    case AKEYCODE_ALT_RIGHT:    return SDLK_RALT;

    /* Function keys */
    case AKEYCODE_F1:  return SDLK_F1;
    case AKEYCODE_F2:  return SDLK_F2;
    case AKEYCODE_F3:  return SDLK_F3;
    case AKEYCODE_F4:  return SDLK_F4;
    case AKEYCODE_F5:  return SDLK_F5;
    case AKEYCODE_F6:  return SDLK_F6;
    case AKEYCODE_F7:  return SDLK_F7;
    case AKEYCODE_F8:  return SDLK_F8;
    case AKEYCODE_F9:  return SDLK_F9;
    case AKEYCODE_F10: return SDLK_F10;
    case AKEYCODE_F11: return SDLK_F11;
    case AKEYCODE_F12: return SDLK_F12;

    /* Home / End / Page */
    case AKEYCODE_MOVE_HOME:  return SDLK_HOME;
    case AKEYCODE_MOVE_END:   return SDLK_END;
    case AKEYCODE_PAGE_UP:    return SDLK_PAGEUP;
    case AKEYCODE_PAGE_DOWN:  return SDLK_PAGEDOWN;
    case AKEYCODE_INSERT:     return SDLK_INSERT;

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
    SDLKey sym = android_to_sdlk(androidKeyCode);
    if (sym == SDLK_UNKNOWN)
        return;

    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));

    ev.type = (action == 0) ? SDL_KEYDOWN : SDL_KEYUP;
    ev.key.state = (action == 0) ? SDL_PRESSED : SDL_RELEASED;
    ev.key.keysym.sym = sym;
    ev.key.keysym.mod = KMOD_NONE;
    ev.key.keysym.unicode = (Uint16)(unicodeChar & 0xFFFF);

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
#include "game.h"            /* Game_wind */
#include "screens.h"         /* SCREEN_GAME */
#include "inferno.h"         /* Screen_mode */

/* Declared in digi_tsf_music.c — pause/resume MIDI when backgrounded */
extern void mix_background_pause(void);
extern void mix_background_resume(void);

/* Declared in rbaudio_bin.c / rbaudio.c — pause/resume redbook (CD) audio */
extern void RBAPause(void);
extern int  RBAResume(void);

/* Declared in android_surface.c — prevent rendering while backgrounded */
extern void android_surface_pause(void);
extern void android_surface_resume(void);

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeOnResume(JNIEnv *env, jobject thiz)
{
    LOGI("nativeOnResume — resuming");
    android_surface_resume();
    mix_background_resume();
    RBAResume();
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeOnPause(JNIEnv *env, jobject thiz)
{
    /* Stop the rendering thread from touching ANativeWindow before the
     * surface is destroyed.  This must happen first so that by the time
     * surfaceDestroyed → nativeSetSurface(null) runs, no blit is in
     * progress or can start. */
    android_surface_pause();

    /* Pause music immediately */
    mix_background_pause();
    RBAPause();

    /* Inject Escape only when the player is in live gameplay.
     * We check Game_wind (non-NULL means a level is loaded) and
     * Screen_mode (SCREEN_GAME means we are in the 3-D view).
     * Both are simple atomic reads — we intentionally avoid
     * window_get_front() here because it traverses a linked list
     * that the game thread mutates concurrently. */
    if (!Game_wind || Screen_mode != SCREEN_GAME) {
        LOGI("nativeOnPause — not in live gameplay, skipping Escape injection");
        return;
    }

    LOGI("nativeOnPause — injecting Escape key");

    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));

    /* Key-down */
    ev.type            = SDL_KEYDOWN;
    ev.key.state       = SDL_PRESSED;
    ev.key.keysym.sym  = SDLK_ESCAPE;
    ev.key.keysym.mod  = KMOD_NONE;
    ev.key.keysym.unicode = 0;
    SDL_PushEvent(&ev);

    /* Key-up */
    memset(&ev, 0, sizeof(ev));
    ev.type            = SDL_KEYUP;
    ev.key.state       = SDL_RELEASED;
    ev.key.keysym.sym  = SDLK_ESCAPE;
    ev.key.keysym.mod  = KMOD_NONE;
    ev.key.keysym.unicode = 0;
    SDL_PushEvent(&ev);
}

/* ── In-game query ──────────────────────────────────────────
 *
 * Returns true when the game window is the front window and we're
 * in SCREEN_GAME mode.  Used by the Kotlin overlay to decide whether
 * to show the virtual stick.
 */
#include "screens.h"         /* SCREEN_GAME */
#include "inferno.h"         /* Screen_mode */
#include "playsave.h"        /* PlayerCfg */
#include "kconfig.h"         /* CONTROL_USING_JOYSTICK */

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsInGame(JNIEnv *env, jobject thiz)
{
    return (Game_wind != NULL && Screen_mode == SCREEN_GAME
            && Game_wind == window_get_front()) ? JNI_TRUE : JNI_FALSE;
}

/*
 * Returns true while a skippable screen (movie, briefing) is active.
 * The Kotlin layer uses this to show the circular Skip button.
 */
extern volatile int g_skippable_active;

JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsSkippableScreen(JNIEnv *env, jobject thiz)
{
    return g_skippable_active ? JNI_TRUE : JNI_FALSE;
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
static int   g_overlay_joystick  = 0;

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeSetJoystickEnabled(JNIEnv *env, jobject thiz,
                                                             jboolean enabled)
{
    if (enabled && !g_overlay_joystick) {
        g_saved_control_type = PlayerCfg.ControlType;
        PlayerCfg.ControlType = CONTROL_USING_JOYSTICK; /* joystick only */
        g_overlay_joystick = 1;

        /* Release any stuck mouse-button state so fire stops immediately */
        SDL_Event ev;
        memset(&ev, 0, sizeof(ev));
        ev.type         = SDL_MOUSEBUTTONUP;
        ev.button.button = SDL_BUTTON_LEFT;
        ev.button.state  = SDL_RELEASED;
        SDL_PushEvent(&ev);
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
        sym = (SDLKey)unicodeChar;
    else
        sym = SDLK_SPACE;   /* fallback; unicode carries the real char */

    SDL_Event ev;

    /* Key-down with unicode */
    memset(&ev, 0, sizeof(ev));
    ev.type                = SDL_KEYDOWN;
    ev.key.state           = SDL_PRESSED;
    ev.key.keysym.sym      = sym;
    ev.key.keysym.mod      = KMOD_NONE;
    ev.key.keysym.unicode  = (Uint16)(unicodeChar & 0xFFFF);
    SDL_PushEvent(&ev);

    /* Key-up */
    memset(&ev, 0, sizeof(ev));
    ev.type                = SDL_KEYUP;
    ev.key.state           = SDL_RELEASED;
    ev.key.keysym.sym      = sym;
    ev.key.keysym.mod      = KMOD_NONE;
    ev.key.keysym.unicode  = 0;
    SDL_PushEvent(&ev);
}

/* ── Joystick (Android gamepad) ─────────────────────────────
 *
 * The Kotlin side detects a gamepad via InputDevice.SOURCE_JOYSTICK and
 * forwards axis / button events here.  We inject the matching SDL 1.2
 * joystick events, which event_poll() dispatches to joy_axis_handler()
 * and joy_button_handler() in arch/sdl/joy.c.
 *
 * joy_init() registers a virtual joystick (index 0) on Android so the
 * axis_map / button_map lookups work correctly.
 *
 * Axis IDs (matches Android → virtual joystick registration order):
 *   0 = left stick X   1 = left stick Y
 *   2 = right stick X  3 = right stick Y
 *   4 = L trigger       5 = R trigger
 *
 * Button IDs: 0=A 1=B 2=X 3=Y 4=L1 5=R1 6=Select 7=Start 8=L3 9=R3
 */

static int g_joy_axis_count = 0; /* debug counter */

/*
 * nativeJoystickAxis(axis, value)
 *   axis:  axis index 0-5
 *   value: -1.0 .. 1.0 (Android MotionEvent range)
 */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeJoystickAxis(JNIEnv *env, jobject thiz,
                                                       jint axis, jfloat value)
{
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));

    ev.type        = SDL_JOYAXISMOTION;
    ev.jaxis.which = 0;           /* virtual joystick 0 */
    ev.jaxis.axis  = (Uint8)axis;
    ev.jaxis.value = (Sint16)(value * 32767.0f);

    SDL_PushEvent(&ev);

    if (++g_joy_axis_count <= 5)
        LOGI("joystick axis %d = %.3f (sdl %d)", axis, value, ev.jaxis.value);
}

/*
 * nativeJoystickButton(button, pressed)
 *   button:  button index 0-9
 *   pressed: 1 = down, 0 = up
 */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeJoystickButton(JNIEnv *env, jobject thiz,
                                                         jint button, jint pressed)
{
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));

    ev.type          = pressed ? SDL_JOYBUTTONDOWN : SDL_JOYBUTTONUP;
    ev.jbutton.which = 0;
    ev.jbutton.button = (Uint8)button;
    ev.jbutton.state  = pressed ? SDL_PRESSED : SDL_RELEASED;

    SDL_PushEvent(&ev);

    LOGI("joystick button %d %s", button, pressed ? "DOWN" : "UP");
}

/* ── Automap touch controls ─────────────────────────────────
 *
 * nativeIsAutomapActive() — returns true when the 3-D automap is displayed.
 * nativeAutomapInput()    — accumulates heading/pitch/thrust deltas that
 *                           automap_apply_input() reads each frame.
 */
JNIEXPORT jboolean JNICALL
Java_com_dxxredux_app_MainActivity_nativeIsAutomapActive(JNIEnv *env, jobject thiz)
{
    return Automap_active ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeAutomapInput(JNIEnv *env, jobject thiz,
                                                       jfloat heading, jfloat pitch, jfloat thrust,
                                                       jfloat bank, jfloat vertical, jfloat sideways)
{
    /* Values are fractions of screen dimension.  Convert to fix-point
     * and accumulate so nothing is lost between game frames.
     * The scale factors are tuned so a full-screen drag ≈ 90° rotation
     * and a full-screen pinch gives rapid traversal. */
    g_automap_heading  += (fix)(heading  * 80000.0f);
    g_automap_pitch    += (fix)(pitch    * 80000.0f);
    g_automap_thrust   += (fix)(thrust   * 600.0f);
    g_automap_bank     += (fix)(bank     * 80000.0f);
    g_automap_vertical += (fix)(vertical * 80000.0f);
    g_automap_sideways += (fix)(sideways * 80000.0f);
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
 * nativeAutomapSelectMarker(idx) — inject key 1-9 to highlight a marker
 */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeAutomapSelectMarker(JNIEnv *env, jobject thiz,
                                                             jint idx)
{
    if (idx < 0 || idx > 9) return;
    SDLKey sym = (idx == 9) ? SDLK_0 : (SDLKey)(SDLK_1 + idx);

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
    SDL_PushEvent(&ev);
}

/*
 * nativeGetMarkerCount() — returns number of placed markers (0-9 in single player)
 * D2 only — MarkerObject doesn't exist in D1.
 */
#ifdef DXX_BUILD_DESCENT_II
extern int MarkerObject[];
JNIEXPORT jint JNICALL
Java_com_dxxredux_app_MainActivity_nativeGetMarkerCount(JNIEnv *env, jobject thiz)
{
    int count = 0;
    for (int i = 0; i < 10; i++) {
        if (MarkerObject[i] != -1) count++;
        else break;  /* markers are placed sequentially */
    }
    return count;
}
#endif

/* ── C→Java keyboard callbacks ──────────────────────────────
 *
 * The engine calls these from the game thread when a menu with a text
 * input field is activated or closed.  We call back to Java via JNI
 * to show/hide the Android soft keyboard.
 */
extern JavaVM  *g_jvm;
extern jobject  g_activity;

void android_show_keyboard(int numeric)
{
    if (!g_jvm || !g_activity) return;

    JNIEnv *env;
    int attached = 0;
    if ((*g_jvm)->GetEnv(g_jvm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
        attached = 1;
    }

    jclass cls = (*env)->GetObjectClass(env, g_activity);
    jmethodID mid = (*env)->GetMethodID(env, cls, "showKeyboard", "(I)V");
    if (mid) {
        /* Android InputType: TYPE_CLASS_TEXT=1, TYPE_CLASS_NUMBER=2 */
        (*env)->CallVoidMethod(env, g_activity, mid, numeric ? 2 : 1);
    }

    if (attached) (*g_jvm)->DetachCurrentThread(g_jvm);
    LOGI("android_show_keyboard(numeric=%d)", numeric);
}

void android_hide_keyboard(void)
{
    if (!g_jvm || !g_activity) return;

    JNIEnv *env;
    int attached = 0;
    if ((*g_jvm)->GetEnv(g_jvm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
        attached = 1;
    }

    jclass cls = (*env)->GetObjectClass(env, g_activity);
    jmethodID mid = (*env)->GetMethodID(env, cls, "hideKeyboard", "()V");
    if (mid) {
        (*env)->CallVoidMethod(env, g_activity, mid);
    }

    if (attached) (*g_jvm)->DetachCurrentThread(g_jvm);
    LOGI("android_hide_keyboard()");
}

#endif /* ANDROID */
