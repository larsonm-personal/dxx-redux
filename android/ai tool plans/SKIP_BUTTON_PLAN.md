# Skip Button Overlay for Cutscenes/Briefings/Movies

## Problem
There is no touch interface to skip movies, briefings, or cutscenes on Android.
On PC, ESC skips these screens, but there is no on-screen button to tap.

## Solution
Add a circular "Skip" button in the upper-right corner that appears only during
skippable screens (movies, briefings). Tapping it injects an ESC keypress.

## Architecture

### C-side: volatile flag + JNI

- `volatile int g_skippable_active` in android_input.c (same pattern as automap volatiles)
- Wrap the synchronous `while (window_exists(wind)) event_process()` loops in:
  - d2/main/movie.c RunMovie() -- movies (D2 only)
  - d2/main/titles.c do_briefing_screens() -- briefings
  - d1/main/titles.c do_briefing_screens() -- briefings
- All wrapped with `#ifdef ANDROID` to keep desktop builds untouched
- New JNI function `nativeIsSkippableScreen()` returns the flag state

### Kotlin-side: SkipButtonView

- New SkipButtonView.kt: simple custom View
  - Semi-transparent circular button, ~10% of screen width
  - "SKIP" text centered
  - Upper-right corner placement
  - On tap: injects ESC key down+up via nativeKeyEvent()
- MainActivity.kt changes:
  - Add nativeIsSkippableScreen() external declaration
  - Create and add SkipButtonView to the FrameLayout
  - Update overlay polling to show/hide skip button (mutually exclusive with game overlay)

## Exclusions

- Credits: already close on any touch/key
- Title screens: already close on any touch/key
- These do not need a skip button

## Files Modified

- android/app/src/main/cpp/android_input.c (volatile + JNI function)
- d2/main/movie.c (wrap event loop, #ifdef ANDROID)
- d2/main/titles.c (wrap event loop, #ifdef ANDROID)
- d1/main/titles.c (wrap event loop, #ifdef ANDROID)
- android/app/src/main/java/com/dxxredux/app/SkipButtonView.kt (new)
- android/app/src/main/java/com/dxxredux/app/MainActivity.kt (add skip button view + polling)

## Shared Constants / Duplicated Items

- g_skippable_active is defined in android_input.c, extern-declared in d1/d2 source files
  with #ifdef ANDROID guards. Both d1 and d2 set the same global since only one game
  library is loaded at a time on Android.
