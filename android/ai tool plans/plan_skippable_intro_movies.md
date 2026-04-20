# Skippable intro/movies and "disable intro movie" launcher preference

## Status

- Completed: engine-side touch/controller skip for D2 movies and D1/D2 intro splash/title screens
- Completed: launcher `skip_intro_movie` preference, native sync in MainActivity, and intro-only large overlay button
- Completed: intro introspection support with `intro_active` and `intro_skip_applied`
- Completed: automation support with a new `skip_intro` step, dedicated intro input regression coverage, and intro auto-skip pref coverage folded into `test_engine_prefs_unified.json5`
- Verified: `./android/run-code-quality.ps1 -Fix`
- Verified: `android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
- Verified: `test_intro_skip_inputs_unified.json5` passed for D1 and D2
- Verified: `test_engine_prefs_unified.json5` passed for D1 and D2 with intro auto-skip pref coverage folded in
- Attempted but environment-blocked: existing host `buildd1` / `buildd2` CMake build dirs fail before compilation with missing system headers (`stdlib.h`, `winsock2.h`, `string.h`)

## Goals

1. Any movie or briefing screen can be skipped by a tap anywhere on the screen
   or by controller A/B buttons (in addition to the existing ESC and circular
   SKIP overlay).
2. Launcher Game Preferences gets a new toggle: "Skip intro movie on launch"
   (default off). Applies to both D1 and D2. Only the intro (D2: intro.mve +
   titles fallback + logo PCX chain; D1: iplogo1/logo/descent PCX splash
   sequence from `show_titles()`) is auto-skipped. Per-level briefings, robot
   movies, end-level movies still play normally (with tap-to-skip).
3. While the intro is playing, the existing circular overlay button becomes a
   larger pill-shaped button labeled "Skip every launch". Tapping it writes the
   launcher pref to `true` (so next launch auto-skips) AND aborts the current
   intro playback. During other movies / briefings, the overlay keeps the
   current small round "SKIP" label and behavior.

## Existing infrastructure we reuse

- `volatile int g_skippable_active` in
  [android/app/src/main/cpp/android_input.c](android/app/src/main/cpp/android_input.c#L50),
  already set in [d2/main/movie.c](d2/main/movie.c#L464),
  [d2/main/titles.c](d2/main/titles.c#L1508), and
  [d1/main/titles.c](d1/main/titles.c#L1193).
- [SkipButtonView.kt](android/app/src/main/java/com/dxxredux/app/SkipButtonView.kt)
  circular overlay with `label` property and `keyCallback` that injects ESC.
- [MainActivity.kt](android/app/src/main/java/com/dxxredux/app/MainActivity.kt)
  polling loop that updates `skipButton.visibility` and `skipButton.label`
  based on `nativeIsSkippableScreen()` and other volatiles.
- `nativeKeyEvent` / `nativeTouchEvent` JNI bridge in
  [android_input.c](android/app/src/main/cpp/android_input.c).
- [EnginePreferencesPage.kt](android/app/src/main/java/com/dxxredux/app/EnginePreferencesPage.kt)
  with `dxx_prefs` SharedPreferences and `PREF_GUIDEBOT_HELPER_LINE`-style
  boolean toggles.

## Shared constants (document duplicated values)

- `PREF_SKIP_INTRO_MOVIE = "skip_intro_movie"` (Kotlin String).
  - Read/written by EnginePreferencesPage and MainActivity (applies to both
    variants, one shared key).
  - Pushed into the native layer via a new
    `nativeSetSkipIntroMovie(boolean)` JNI.

## Phase 1: engine side input - make movies/briefings/splashes skip on tap and gamepad

All edits guarded with `#ifdef __ANDROID__` / `#ifdef ANDROID`, duplicated in
d1 and d2 as appropriate. Match surrounding style.

### 1a. D2 movies: `MovieHandler` in [d2/main/movie.c](d2/main/movie.c#L300)
Add cases alongside the existing `KEY_ESC` branch:
- `EVENT_MOUSE_BUTTON_DOWN` (any button): set `m->result = m->aborted = 1;
  window_close(wind); return 1;`
- `EVENT_JOYSTICK_BUTTON_DOWN` (any button): same as above.
Both wrapped in `#ifdef ANDROID`.

### 1b. D1 splash screens: `title_handler` in [d1/main/titles.c](d1/main/titles.c#L85)
Already handles `EVENT_MOUSE_BUTTON_DOWN` button 0 and, under ANDROID,
`EVENT_JOYSTICK_BUTTON_DOWN`. Relax `EVENT_MOUSE_BUTTON_DOWN` under ANDROID to
accept any button (a touch down gets mapped to mouse button 0 anyway, so this
is mostly a no-op; verify and leave as-is if button 0 is sufficient).

### 1c. D2 splash screens: `title_handler` in [d2/main/titles.c](d2/main/titles.c#L92)
Same structure as D1. Already supports mouse button 0 + ANDROID joystick. No
change expected; confirm during implementation.

### 1d. D1/D2 briefings: already handle mouse + joystick (ESC to fully exit,
other buttons advance). No change; document the existing behavior.

### 1e. Wrap `show_title_screen` event loops with `g_skippable_active`
Currently only `do_briefing_screens()` and `RunMovie()` set the flag. Splash
screens are skippable too (they accept any tap) so wrap the
`while (window_exists(wind)) event_process();` loops in both
[d1/main/titles.c `show_title_screen`](d1/main/titles.c#L170) and
[d2/main/titles.c `show_title_screen`](d2/main/titles.c#L183) with
`#ifdef ANDROID` `g_skippable_active = 1; ... = 0;`. This also makes the D1
"intro" splash sequence show the overlay button.

## Phase 2: intro-specific flag and auto-skip of the intro on launch

### 2a. New volatile pair in android_input.c
```c
volatile int g_intro_active = 0;   /* 1 while the launch-intro sequence runs */
volatile int g_skip_intro_pref = 0; /* set by launcher via JNI */
```
Expose two JNI calls in android_input.c (next to `nativeIsSkippableScreen`):
- `Java_com_dxxredux_app_MainActivity_nativeIsIntroActive()` -> `jboolean`
- `Java_com_dxxredux_app_MainActivity_nativeSetSkipIntroMovie(jboolean v)`
  (stores into `g_skip_intro_pref`).

### 2b. Gate the intro sequence in `show_titles()`
Both D1 and D2 `show_titles()` are the launch-intro entry points.
- Wrap the body with `g_intro_active = 1; ... g_intro_active = 0;` under
  `#ifdef __ANDROID__`.
- At the very top, under `#ifdef __ANDROID__`: `if (g_skip_intro_pref) return;`
  - For D2 this skips pre_i, intro.mve, titles.mve, and the logo PCX chain.
  - For D1 this skips iplogo1/logo/descent PCX chain.

This keeps all per-level / end-level movies and briefings untouched; only the
launch intro is gated.

### 2c. MainActivity reads pref on startup and pushes to native
In [MainActivity.kt](android/app/src/main/java/com/dxxredux/app/MainActivity.kt)
before `nativeStart`/game thread spawn:
```kotlin
val skipIntro = getSharedPreferences("dxx_prefs", MODE_PRIVATE)
    .getBoolean(PREF_SKIP_INTRO_MOVIE, false)
nativeSetSkipIntroMovie(skipIntro)
```
Add `external fun nativeSetSkipIntroMovie(v: Boolean)` and
`external fun nativeIsIntroActive(): Boolean` next to the other externals.

## Phase 3: overlay variant for intro + pref-writing tap

### 3a. Extend SkipButtonView
- Add a `bigLabel: Boolean` property that toggles layout between the current
  small circle and a larger pill covering ~25% of screen width at the top.
- Keep the existing drawing path for circle mode; add a rounded-rect path for
  pill mode.
- Keep `keyCallback` for ESC injection.
- Add a second callback, e.g. `skipEveryLaunchCallback: (() -> Unit)?`. When
  `bigLabel = true`, on tap call `skipEveryLaunchCallback?.invoke()` first,
  then inject ESC. Hook from MainActivity writes the SharedPreferences and
  calls `nativeSetSkipIntroMovie(true)`.

### 3b. MainActivity polling loop
In the block starting at
[MainActivity.kt line 1361](android/app/src/main/java/com/dxxredux/app/MainActivity.kt#L1361),
add an early branch: if `nativeIsIntroActive()` returns true, set
```kotlin
skipButton.label = "Skip every launch"
skipButton.bigLabel = true
skipButton.visibility = View.VISIBLE
```
Reset `bigLabel = false` for all other branches so the existing save/load,
level-complete, death, skip cases keep their current appearance.

### 3c. Wire the "Skip every launch" callback
In MainActivity where `skipButton = SkipButtonView(this).apply { ... }`
(line ~743), assign:
```kotlin
skipEveryLaunchCallback = {
    getSharedPreferences("dxx_prefs", MODE_PRIVATE).edit()
        .putBoolean(PREF_SKIP_INTRO_MOVIE, true).apply()
    nativeSetSkipIntroMovie(true)
}
```

## Phase 4: Launcher Game Preferences UI

In [EnginePreferencesPage.kt](android/app/src/main/java/com/dxxredux/app/EnginePreferencesPage.kt):
- Add an `internal const val PREF_SKIP_INTRO_MOVIE = "skip_intro_movie"` near
  the other PREF_* constants.
- Add a `var skipIntroMovie by remember { mutableStateOf(prefs.getBoolean(
  PREF_SKIP_INTRO_MOVIE, false)) }` alongside `showGuidebotLine`.
- Add a Switch row "Skip intro movie on launch" mirroring the
  `showGuidebotLine`/`showNearestPlayerLine` rows, persisting to the shared
  `dxx_prefs` store on change.
- This is a pure app-level pref (not a pilot pref) so it does not touch
  `NativePilotPreferences` / `android_pilot_prefs.cpp`.

## Phase 5: Tests

Add to `android/game_scripts/`:
1. `test_intro_skip_by_tap.json5` - launch game, verify
   `introspect.json screen_mode == "movie"` or a new `intro_active` field,
   send a tap, verify we advance past the intro and land on the main menu.
2. Fold intro auto-skip pref coverage into an existing launcher engine prefs
  regression so the app-level Game Preferences toggle is verified alongside
  the rest of that screen's persistence path.
3. Extend `game_introspect.c` to add a top-level `intro_active` boolean
   (reads `g_intro_active`). Needed for (1) to assert reliably and to track
   regressions without logcat scraping. Do this as the first phase-5 change.

Run both with `run_test.ps1 -Game d2` and `-Game d1` per the repo convention.

## Phase 6: lint + build verification

- `./android/run-code-quality.ps1 -Fix`
- `./gradlew assembleDebug`
- `cmake --build buildd1 --target dxx-redux-d1` and same for d2 (host build)
  to confirm the `__ANDROID__` guards are correct and don't regress desktop.
- Run `android/run_all_tests.ps1` for the new scripts.

## Notes / pitfalls

- `SkipButtonView.onVisibilityChanged` arms a 500ms debounce when shown.
  Switching between pill and circle on the same view must not trigger a
  visibility change; just redraw. Flip `bigLabel` while `visibility` stays
  `VISIBLE` to avoid re-arming.
- The intro sometimes plays a song via `songs_play_song(SONG_TITLE, ...)`
  as a fallback when movies are missing. When `g_skip_intro_pref`, we return
  before that song starts. Verify the main menu's own song logic handles
  "no song currently playing" gracefully (it should, since that is the
  normal path on systems without movies).
- `MOVIE_REQUIRED` returns `MOVIE_ABORTED` on user abort; the fallback logic
  in `show_titles` only triggers on `MOVIE_NOT_PLAYED`. Tap-skipping the
  intro.mve therefore does NOT fall through to titles.mve or logo PCXs,
  matching the ESC-skip behavior today. Good.
- Keep all d1/d2 source changes minimal and `#ifdef` guarded so upstreaming
  stays easy. No refactoring of existing handlers; add cases only.
