# Mine exit cutscene auto-skip on Android

## Symptom
On Android, when leaving a mine in D2, the end-level cutscene (`esa.mve` etc.) is sometimes skipped instantly. Hypothesis: a touch tap is registering at the moment the movie window opens, and the movie's `EVENT_MOUSE_BUTTON_DOWN` handler aborts immediately. Players are usually holding the throttle slider when they hit the exit, so the lift-off (or any reposition tap) lands right when the movie starts.

## Root cause analysis

### D2 movie abort path
[d2/main/movie.c#L296-L320](d2/main/movie.c#L296-L320) `MovieHandler` aborts on any of:

```c
#ifdef ANDROID
case EVENT_MOUSE_BUTTON_DOWN:
case EVENT_JOYSTICK_BUTTON_DOWN:
    m->result = m->aborted = 1;
    window_close(wind);
    return 1;
#endif
case EVENT_KEY_COMMAND:
    if (key == KEY_ESC) { ... aborted = 1; ... }
```

The Android-only mouse/joy abort is the offender. Touch input arrives via [android/app/src/main/cpp/android_input.c#L218-L246](android/app/src/main/cpp/android_input.c#L218-L246) which `SDL_PushEvent`s `SDL_MOUSEBUTTONDOWN` for every `ACTION_DOWN`. Two separate ways the abort can fire instantly:

1. A touch that is already `down` when the movie opens does not generate a fresh `DOWN`, but if the player taps to skip the score / fires off a control tap during the transition into the movie, the freshly queued `SDL_MOUSEBUTTONDOWN` is dispatched on the very first event_process pass inside the movie loop.
2. The throttle slider release path is fine on its own (only emits `BUTTON_UP`), but any UI reposition tap (e.g. opening the auto map or the next finger contact while the engine is loading the .mve) lands as `BUTTON_DOWN`.

### D2 endlevel path
[d2/main/endlevel.c#L334-L341](d2/main/endlevel.c#L334-L341) calls `start_endlevel_movie()` with no input flush:

```c
window_set_visible(Game_wind, 0);
if (PLAYING_BUILTIN_MISSION)
    if (!(Game_mode & GM_MULTI))
        endlevel_movie_played = start_endlevel_movie();
window_set_visible(Game_wind, 1);
```

There is also a playback-replay variant at [d2/main/endlevel.c#L307-L311](d2/main/endlevel.c#L307-L311) that calls `start_endlevel_movie()` for builtin missions during demo playback.

### D1
D1 has no `movie.c` and no `PlayMovie`. The exit sequence is a rendered 3D flythrough in [d1/main/endlevel.c](d1/main/endlevel.c), driven by `do_endlevel_frame()` and terminated by game state, not by any "press any key to skip" handler. So D1 does not have the same auto-skip bug, and no D1 code change is needed for this fix.

### Existing flush helper
[d2/main/game.c#L851-L860](d2/main/game.c#L851-L860) already provides `game_flush_inputs()` (drains SDL queue + key/joy/mouse buffers + zeros `Controls`). Same helper exists in d1.

### Existing skippable flag
`g_skippable_active` is set by [d2/main/movie.c#L472-L478](d2/main/movie.c#L472-L478) around the `event_process()` loop and is currently consumed only by Kotlin to show the skip overlay. We can also use it to gate touch injection during the very first part of playback.

## Fix design

Two complementary layers, both `#ifdef __ANDROID__`-gated, no behavior change on desktop.

### Layer 1: flush before movie starts
Call `game_flush_inputs()` immediately before `start_endlevel_movie()` so any tap queued during the window-visibility flip and movie load cannot leak into `MovieHandler`.

D2 hooks (both call sites in `start_endlevel_sequence`):
- [d2/main/endlevel.c#L307](d2/main/endlevel.c#L307) (demo playback branch)
- [d2/main/endlevel.c#L339](d2/main/endlevel.c#L339) (normal branch)

To avoid duplicating the `#ifdef __ANDROID__` snippet across files, add a tiny helper `android_flush_pre_cutscene_input()` in a new shared TU (or extend an existing android shim) that:
1. calls `game_flush_inputs()`
2. arms the suppress-touch timer (Layer 2).

Place the helper in `android/app/src/main/cpp/android_input.c` (already has the touch state) and forward-declare in `android_log.h` or a small new header `android_movie_skip.h` to keep includes minimal. Each call site becomes one line under `#ifdef __ANDROID__`.

### Layer 2: 500 ms touch-down suppress window
In `android_input.c`:

```c
static volatile fix64 g_movie_tap_suppress_until = 0;  /* timer_query() compatible */
#define MOVIE_TAP_SUPPRESS_MS 500

void android_arm_cutscene_tap_suppress(void) {
    g_movie_tap_suppress_until = timer_query() + (F1_0 / 1000) * MOVIE_TAP_SUPPRESS_MS;
}
```

In `nativeTouchEvent`, before pushing `SDL_MOUSEBUTTONDOWN`:

```c
case 0: /* ACTION_DOWN */
    if (g_movie_tap_suppress_until && timer_query() < g_movie_tap_suppress_until) {
        /* swallow this tap; still update last position so MOVE/UP behave */
        g_last_touch_x = gameX; g_last_touch_y = gameY; g_touch_active = 1;
        return;
    }
    ...
```

ACTION_MOVE / ACTION_UP pass through normally (they cannot abort the movie - only `BUTTON_DOWN` does).

Suppress the joystick virtual-button path the same way: at the corresponding `SDL_JOYBUTTONDOWN` injection site in `android_input.c`, drop the event during the window. This prevents the same `EVENT_JOYSTICK_BUTTON_DOWN` abort branch from firing if the user is on a controller mapping that emits a button on release. (Verify exact location during impl; if no synthetic joybutton is currently emitted on Android touch path, this part is a no-op safeguard.)

### Layer 3: optional - require deliberate skip
We deliberately do NOT remove the existing `EVENT_MOUSE_BUTTON_DOWN` / `EVENT_JOYSTICK_BUTTON_DOWN` abort handler. Keeping it allows the player to skip end-of-level cutscenes intentionally with a tap. We rely solely on the flush + 500 ms gate to filter accidental taps.

If post-fix testing shows the tap-to-skip is still firing in non-deliberate cases, follow up with:
- raise the gate to 750 ms, and / or
- require the abort tap to be inside an on-screen "Skip" button rect (already shown via `g_skippable_active`).

## Implementation phases

### Phase 1 - shared helper + d2 hookup
- [x] Add `android_arm_cutscene_tap_suppress()` and suppress-window state to [android/app/src/main/cpp/android_input.c](android/app/src/main/cpp/android_input.c).
- [x] Route touch injection through a helper that swallows the initial `ACTION_DOWN` inside the suppress window while still allowing the later intentional tap-to-skip path.
- [x] Extend [android/app/src/main/cpp/shared/android_log.h](android/app/src/main/cpp/shared/android_log.h) with the public declaration used by d2.
- [x] In [d2/main/endlevel.c](d2/main/endlevel.c), add the Android pre-movie helper before both `start_endlevel_movie()` call sites so the engine flushes inputs and arms the suppress window immediately before movie startup.

### Phase 2 - d1 parity
- [x] Research complete: no D1 code change needed because D1 does not enter the same movie abort path.

### Phase 3 - joybutton-down safety
- [x] Apply the same suppress window to `nativeJoystickButton()` so controller button-down events cannot immediately abort the cutscene during the arming window.

### Phase 4 - integration test
- [x] Add [android/game_scripts/test_mine_exit_movie_touch_skip.json5](android/game_scripts/test_mine_exit_movie_touch_skip.json5) to trigger the D2 endlevel path, inject a touch tap, and validate the suppress window through introspection.
- [x] Run `android\run_test.ps1 -ScriptName test_mine_exit_movie_touch_skip.json5 -Game d2 -Install` and iterate until green.
- [x] Adjust the assertion target after confirming the local emulator data set does not include the D2 `.mve` files. The committed regression now verifies the suppress window is armed and that the injected tap is swallowed instead of asserting on visible movie playback.

### Phase 5 - lint + build
- [x] `android\gradlew.bat assembleDebug`
- [x] `android\run-code-quality.ps1 -Fix`
- [x] `run-windows-build.ps1` to confirm no regression on the desktop build.

## Files touched
- [android/app/src/main/cpp/android_input.c](android/app/src/main/cpp/android_input.c) - new state + arm function + ACTION_DOWN gate.
- [android/app/src/main/cpp/android_log.h](android/app/src/main/cpp/android_log.h) (or new tiny header) - declare `android_arm_cutscene_tap_suppress`.
- [android/app/src/main/cpp/shared/game_automate.cpp](android/app/src/main/cpp/shared/game_automate.cpp) - add automation actions to trigger endlevel and inject a touch tap.
- [android/app/src/main/cpp/shared/game_introspect.cpp](android/app/src/main/cpp/shared/game_introspect.cpp) - expose suppress-window state and hit count to automation.
- [d2/main/endlevel.c](d2/main/endlevel.c) - two-line guard at two call sites.
- [android/game_scripts/test_mine_exit_movie_touch_skip.json5](android/game_scripts/test_mine_exit_movie_touch_skip.json5) - regression script.

## Validation results
- Android debug build passed with `android\gradlew.bat assembleDebug`.
- The automation regression passed after the test was adapted to the local asset set.
- Windows host builds for both d1 and d2 passed with `run-windows-build.ps1`.
- The local emulator data set is missing the D2 movie files such as `esa.mve`, so direct "movie stayed open" validation could not be used in this workspace.

## Notes
- The existing `g_skippable_active` flag is left as-is (Kotlin still uses it for the Skip overlay).
- `timer_query()` is the right clock here - it advances even when game windows are suspended, and `start_endlevel_movie` runs after `window_set_visible(Game_wind, 0)`.
- Do NOT remove the existing `EVENT_MOUSE_BUTTON_DOWN` abort. It is the intentional touch-to-skip path. We are only filtering taps within the first 500 ms.
- The only engine-side gameplay change is in D2. D1 was left untouched after confirming it does not use the same movie-skip path.
