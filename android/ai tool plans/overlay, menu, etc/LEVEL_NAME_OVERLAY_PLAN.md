# Plan: Multi-line Overlay for Level & Track Names

## TL;DR
Show the level name as a temporary overlay when a new level starts, using the same fade style as the existing track name overlay. Generalize the single-line track overlay into a multi-line overlay container so both level name and track name (which often fire simultaneously) can coexist, with items independently fading and remaining items bumping up.

## Background / Key Findings

- `Current_level_name` **is** populated during normal gameplay — it's embedded in the .rl2/.rdl level files and read in `load_game_data()` (gamesave.c:784). No lookup table needed.
- `LoadLevel()` (gameseq.c:723) calls `load_level()` which fills `Current_level_name`, then at the end calls `songs_play_level_song()` which triggers the track overlay. Both events happen in quick succession — confirming the multi-line need.
- The track overlay is: C (`track_names.c:send_track_name_to_java`) → JNI → Kotlin (`MainActivity.showTrackName`) → a single `TextView` with ObjectAnimator fade-in/hold/fade-out.
- The view hierarchy is a `FrameLayout` with: GameSurfaceView → TouchOverlayView → trackNameView.

## Steps

### Phase 1: Kotlin — Multi-line overlay container

1. **Replace `trackNameView` with a vertical `LinearLayout` container** positioned top-left.
   - Enable `LayoutTransition` with `CHANGE_DISAPPEARING` so items slide up when predecessors are removed.
   - File: `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`

2. **Add `showOverlayLine(text: String)` method** on `MainActivity`.
   - Creates a new `TextView` styled identically to the current `trackNameView` (green, monospace, 16sp).
   - Adds it to the container.
   - Runs the same animation chain: fade in 0.5s → hold 3s → fade out 0.5s → remove from container.
   - Each line manages its own animator independently.

3. **Migrate `showTrackName()`** to delegate to `showOverlayLine()`. Keep JNI method name unchanged.

4. **Add `showLevelName(name: String)` method** — called from JNI. Delegates to `showOverlayLine()`.

5. **Remove old `trackNameView`/`trackNameAnimator` fields** — replaced by the container.

### Phase 2: C — Level name notification via JNI

6. **Add `level_overlay_notify()`** in `track_names.c` (reuse existing JNI pattern).
   - Format: `"Level N: <name>"` or `"Secret Level: <name>"`.
   - Guard with `#ifdef ANDROID`.

7. **Hook into `LoadLevel()`** in `gameseq.c`, after `load_level()` succeeds and `Current_level_name` is populated (~line 755).
   - Only notify when `Current_level_name[0] != '\0'`.

8. **Declare in header** — add `level_overlay_notify()` to `track_names.h`.

### Phase 3: D1 support

9. **Add same hook in D1's `LoadLevel()`** if it has the same `Current_level_name` mechanism.

## Relevant Files

- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` — overlay container, `showOverlayLine()`, `showLevelName()`
- `d2/main/track_names.c` — `level_overlay_notify()`, JNI bridge
- `d2/include/track_names.h` — declaration
- `d2/main/gameseq.c` — hook after `load_level()` at ~line 755
- `d2/main/gamesave.c` — reference only; `load_game_data()` at line 784 populates `Current_level_name`
- `d1/main/gameseq.c` — D1 equivalent hook point

## Verification

1. Build the Android APK — no compile errors
2. Launch D2, skip briefing, verify both level name and track name overlays appear stacked
3. Verify when one fades, the remaining line bumps up
4. Advance to level 2 — verify new overlays appear correctly
5. Verify empty `Current_level_name` produces no overlay
6. Verify Windows/Linux builds still compile (C changes are `#ifdef ANDROID`-guarded or no-ops)

## Decisions

- **Format**: `"Level 1: Vertigo Cavern"` / `"Secret Level: <name>"`
- **Timing**: Matches track overlay — 0.5s fade in, 3s hold, 0.5s fade out
- **Container**: Vertical `LinearLayout` with `LayoutTransition` — simplest bump-up behavior
- **Positioning**: Top-left corner with 8dp padding (same as current track overlay)
- **Scope**: D2 first; D1 as follow-on
