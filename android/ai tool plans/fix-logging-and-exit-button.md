# Fix MP Logging Guards and Exit Button on Select Players Screen

## Problem 1: Dead Logging Code
All ~46 new `net_log_comment()` debug logging blocks in D1 and D2 `net_udp.c`
use `#ifdef __android__` (lowercase). The Android NDK only defines `__ANDROID__`
(uppercase), so none of this logging ever compiles on Android.

### Fix
Bulk replace `#ifdef __android__` with `#ifdef __ANDROID__` in both files.
Pre-existing code already uses the correct uppercase form.

## Problem 2: EXIT Button Not Working on Select Players Screen
The EXIT button (ExitButtonView) fires META_RETURN_TO_LAUNCHER which pushes
SDL_QUIT. The Quitting mechanism in standard_handler should cascade-close
windows, but the user reports it "does nothing" on the select players screen.

### Root Cause Analysis
The SDL_QUIT -> Quitting -> window_close cascade requires `call_default_handler`
to be invoked, which only happens when `event_send` sends an event that no
window handles. The newmenu handler returns 0 for EVENT_IDLE (via
`newmenu_mouse(wind, event, menu, -1)` which hits no switch case), so idle
frames SHOULD trigger the cascade. However, if continuous touch/input events
arrive, the system may never reach an idle frame, preventing the Quitting
mechanism from firing.

### Fix
In `meta_action_dispatch`, inject an ESC key event BEFORE the SDL_QUIT push.
The ESC event will be processed in the same `event_poll` pass:
- In menus: ESC immediately closes the current menu window via
  newmenu_key_command, which is processed inline. The SDL_QUIT then handles
  remaining cleanup.
- During gameplay: ESC opens the game menu, then SDL_QUIT cascades through
  the Quitting mechanism as before.

This approach is minimally invasive and ensures the current menu always
closes immediately when EXIT is tapped.

## Files Modified
- `d1/main/net_udp.c` - fix ~18 `#ifdef __android__` instances
- `d2/main/net_udp.c` - fix ~28 `#ifdef __android__` instances
- `android/app/src/main/cpp/shared/android_meta_actions.c` - add ESC injection
