/*
 * android_input.c — Inject Android touch & key events into SDL 1.2's event queue.
 *
 * Touch events are converted to SDL_MOUSEMOTION / SDL_MOUSEBUTTONDOWN / UP.
 * Key events are converted to SDL_KEYDOWN / SDL_KEYUP.
 *
 * All injection uses SDL_PushEvent() so the existing event_poll() → key_handler()
 * / mouse_button_handler() / mouse_motion_handler() pipeline works unchanged.
 */

#ifdef ANDROID

#include <jni.h>
#include <android/log.h>
#include <SDL.h>
#include <string.h>
#include <android/keycodes.h>

#define LOG_TAG "DXX-Input"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

/* ── Touch → Mouse ──────────────────────────────────────────
 *
 * The game tracks the mouse position via deltas (SDL_MouseMotionEvent.xrel/yrel)
 * accumulated into Mouse.x / Mouse.y.  We keep the last known touch position
 * and convert absolute touch coords to relative deltas.
 *
 * Touch coordinates from Kotlin are already in *game canvas* space
 * (the Kotlin side maps from SurfaceView pixels → 640×480).
 */

static int g_last_touch_x = -1;
static int g_last_touch_y = -1;
static int g_touch_active = 0;   /* is a finger currently down? */
static int g_input_count  = 0;   /* debug counter */

/*
 * nativeTouchEvent(action, gameX, gameY)
 *   action: 0 = DOWN, 1 = MOVE, 2 = UP  (matches MotionEvent.ACTION_*)
 *   gameX, gameY: coordinates already mapped to 640×480 canvas space
 */
JNIEXPORT void JNICALL
Java_com_dxxredux_app_MainActivity_nativeTouchEvent(JNIEnv *env, jobject thiz,
                                                     jint action, jint gameX, jint gameY)
{
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));

    switch (action) {
    case 0: /* ACTION_DOWN */
        g_last_touch_x = gameX;
        g_last_touch_y = gameY;
        g_touch_active = 1;

        /* Send a motion event to position the cursor first */
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
        int dx = gameX - g_last_touch_x;
        int dy = gameY - g_last_touch_y;
        g_last_touch_x = gameX;
        g_last_touch_y = gameY;

        ev.type = SDL_MOUSEMOTION;
        ev.motion.x      = (Uint16)gameX;
        ev.motion.y      = (Uint16)gameY;
        ev.motion.xrel    = (Sint16)dx;
        ev.motion.yrel    = (Sint16)dy;
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

#endif /* ANDROID */
