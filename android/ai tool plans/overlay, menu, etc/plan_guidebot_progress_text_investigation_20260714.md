# Guide-Bot Progress Text Investigation

## Scope

Determine why the upper-right robots, hostages, and secrets progress text disappears while Guide-Bot HUD text is displayed, then implement an actual drawn-area collision fix while preserving unrelated in-progress Guide-Bot and multiplayer changes.

## Plan

- [done] Trace progress-text drawing, Guide-Bot message drawing, and their shared HUD state
- [done] Compare relevant D1 and D2 paths and identify the precise interaction
- [done] Record the cause and smallest safe fix direction; do not edit engine code

## Finding

- `HUD_render_message_frame()` sets the legacy global `HUD_toolong` when the first displayed HUD message has a raw `strlen()` greater than 38
- `hud_show_robot_hostage_counts()` returns immediately while `HUD_toolong` is set, so robots, hostages, and secrets all disappear together
- D2 Guide-Bot speech uses the ordinary HUD message queue and adds four invisible color-control bytes, making its raw length longer than its visible text and causing long Guide-Bot lines to cross the threshold easily
- The count draw runs before message rendering, so the suppression begins and ends one frame after the message state changes
- D1 has the same inherited HUD flag and count guard, although D1 has no Guide-Bot speech path

The smallest targeted fix is to stop treating `HUD_toolong` as a reason to hide the lower progress rows. A more general fix would replace the raw character-count heuristic with actual pixel and row collision handling for all top HUD elements.

## Drawn-Area Design Phase

- [done] Confirm font measurement, centered-string bounds, message stacking, and HUD call order
- [done] Define a small rectangle-based collision API and same-frame lifecycle
- [done] Map the legacy `HUD_toolong` consumers to measured bounds in D1 and D2
- [done] Record implementation and validation recommendations

### Recommended Design

Use text-layout bounding rectangles in the active HUD canvas coordinate space. Do not scan rendered pixels and do not reserve the whole top of the screen.

1. Split message update/layout from message drawing
   - Add `HUD_prepare_message_frame()` and call it once near the start of `draw_hud()`, before score, timer, progress, or lives drawing and before the observer early return
   - Move timer decrement and expired-message removal into the prepare call
   - Select the last `HUD_MAX_NUM_DISP` messages and calculate their rectangles using the same base Y, line spacing, observer offset, and guided-missile offset used by the renderer
   - Keep `HUD_render_message_frame()` as the later draw operation so existing visual layering remains stable

2. Measure rendered text rather than raw bytes
   - The current centered-string path skips embedded color controls, but `gr_get_string_size()` counts them as spacing
   - Add a control-aware rendered-size helper beside the font renderer and share its line-width parser with `get_centered_x()`
   - Preserve the existing `gr_get_string_size()` behavior unless a wider audit shows it is safe to change; the new helper keeps this fix focused
   - A bounding rectangle per displayed HUD message is sufficient. Multiline messages can use the union of their centered line rectangles

3. Expose one collision query
   - Add `HUD_message_area_intersects(x, y, w, h)` in `hudmsg.h`
   - Use half-open rectangle intersection and expand message bounds by one scaled HUD pixel so adjacent glyphs do not visually touch
   - The query reads the rectangles prepared for the current frame, eliminating the current one-frame lag

4. Replace every `HUD_toolong` consumer
   - `hud_show_score()`: format and measure the actual score string, then skip only if its top-right rectangle intersects a message
   - `hud_show_timer_count()`: test the timer rectangle at its real X and Y
   - `hud_show_robot_hostage_counts()`: remove the all-or-nothing guard; have the shared count drawing helpers test robots, hostages, and secrets independently after calculating each row's real rectangle
   - `hud_show_lives()`: test the union of the left-side ship bitmap and lives text, or the multiplayer deaths string
   - `hud_show_score_added()`: add the same query even though it currently lacks a `HUD_toolong` guard, making top-right behavior consistent
   - Delete `HUD_toolong` after its D1 and D2 consumers are gone

### Collision Policy

Messages keep priority, matching existing behavior. Only a corner element whose rectangle intersects a visible message rectangle is omitted for that frame. The whole progress group is not hidden, and it is not shifted into other HUD regions.

For the reported case, a long Guide-Bot line occupies the centered first text row. Robots, hostages, and secrets begin on lower rows, so their vertical bounds do not intersect and they remain visible. If several queued messages actually reach those rows, only the count rows with both horizontal and vertical intersection are omitted.

### Scope and Validation

- Apply the message preparation, rectangle query, font measurement helper, and consumer changes to both D1 and D2
- Keep the shared robots/hostages/secrets row decisions in `hud_counts_shared.c`
- Add focused tests for rectangle edge cases and control-aware width, including Guide-Bot color controls, short and long messages, multiple queued rows, and scaled fonts
- Extend debug introspection with prepared message rectangles and per-progress-row visible/blocked state, then add a D2 automation case that emits a long Guide-Bot message and asserts that non-intersecting progress rows remain drawn
- Run scoped code quality, Windows D1/D2 builds, Android debug build/tests, and the focused D2 emulator automation

## Implementation Phase

- [done] Add shared rectangle helpers and focused host tests
- [done] Add control-aware rendered text measurement in D1 and D2
- [done] Split HUD message preparation from drawing and expose collision queries in D1 and D2
- [done] Replace every `HUD_toolong` consumer, including per-row shared progress checks
- [done] Add introspection and focused automation coverage without modifying active Guide-Bot test files
- [done] Run scoped code quality, tests, D1/D2 build coverage, Android builds, and emulator validation

## Validation Results

- [done] Scoped `android/run-code-quality.ps1 -Fix -Paths ...` checks pass
- [done] `test_hud_layout` passes as a direct MSVC host test and through both D1 and D2 CMake test targets
- [done] Android external native debug build and debug APK assembly pass for the implemented changes
- [done] The focused D2 emulator automation passes with one visible Guide-Bot message rectangle and independently drawn robots, hostages, and secrets rows below it
- [done] The changed D1 and D2 font, HUD, gauges, shared count, introspection, and automation sources compile in their platform builds
- [noted] Full Windows helper-target completion is currently blocked by unrelated, concurrently edited level-metadata code: D1 reports `level_metadata_scan.c` compile errors, while D2 links its main executables before `test_level_metadata_scan.exe` fails on unresolved symbols
- [noted] A later Android rebuild also reached and compiled the implemented files before unrelated concurrent `route_planner.cpp` metadata-shadow type errors stopped the aggregate build
