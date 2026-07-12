# Numbered objective automap overlay plan

## Goal

Add an Android automap cheat toggle that draws the level's calculated objectives as world-space labels numbered `1..N`. Compute stable label anchors during the shared level metadata scan, retain the initial canonical objective list for the life of the level, and write each anchor to the checked-in mission JSON metadata.

## Planning status

- [x] Trace D1 and D2 automap rendering and projected text labels
- [x] Trace the Android automap menu, checkbox state, JNI, introspection, and automation paths
- [x] Trace route objective generation and mission metadata serialization
- [x] Define objective numbering, label-anchor, state lifetime, and JSON contracts
- [x] Identify focused and corpus-level verification
- [ ] Implement the metadata fields and canonical objective snapshot
- [ ] Implement the Android toggle and D1/D2 automap rendering
- [ ] Add tests, regenerate metadata, build, and run emulator verification

## Current architecture findings

- The existing objective definition is `level_metadata_state.route_steps`. Step 0 is `start`; the remaining steps are already ordered as indices 1 through N and cover keys, switches/triggers, hidden doors, reactor or boss, and exit.
- `level_metadata_route_step` already stores fixed-point `activation_pos` and `aim_pos`. The activation position is where an actor should stand and is not a suitable label anchor. The aim position is usually the objective center: object position for keys, reactor, and boss; side center for switches, hidden doors, and exit.
- Neither Android mission serialization nor headless mission serialization currently emits those positions. `LevelMetadata.kt` and `LauncherScriptExecutor.kt` would also discard an added field unless their parse/write model is extended.
- `level_metadata_get_state()` can have its route steps replaced by a live Guide-Bot route-only rescan. The overlay therefore cannot safely read that mutable route list if numbering must remain the initial level objective order.
- D1 and D2 automap code already projects world-space secret labels after the 3D automap frame. D2 also has marker-number projection. Objective labels should reuse this proven render timing and local projection pattern.
- While automap is active, the Android admin tray already exposes `Reveal Secrets` as a non-persistent checkbox backed by native state. This is the closest UI and lifecycle precedent.
- Existing automap introspection counts projected secret labels, and automation has a `set_secret_reveal` action. The new overlay can follow the same test pattern without screenshot analysis.

## Proposed behavior contract

1. Define an objective as every non-`start` step in the initial canonical end-route metadata.
2. Use the canonical route order as the visible numbering. The first non-start step is 1 and the last is N.
3. Keep this list frozen for the loaded level. Picking up a key, destroying a boss, or asking Guide-Bot to find another target must not renumber or move the overlay.
4. Show all known canonical objectives, including completed objectives. This is a level-inspection cheat, not a current-task indicator.
5. For a partial route, show every known non-start step. For a failed route with no steps, draw nothing and expose zero counts through introspection.
6. The toggle is off by default and resets on level change, matching `Reveal Secrets`. Do not store it in saves, player configuration, the normal `cheats` struct, or input demos.
7. The toggle does not reveal automap edges and does not mutate `Automap_visited`. Players may combine it with the existing full-map or secret reveal cheats when they want map context.
8. Expose the checkbox only while automap is active, with the same mode availability as the existing `Reveal Secrets` checkbox. It is local display state and does not affect multiplayer simulation. Use the dynamic text `Show Objectives` or `Hide Objectives` to match the existing automap cheat style.

## Label position contract

Add `label_pos_valid` and `label_pos[3]` to `level_metadata_route_step`. Populate them before the scan result is exposed:

1. Skip the `start` step for objective display.
2. If `aim_pos_valid`, copy `aim_pos`. This produces the physical object center or relevant wall-side center for current route step kinds.
3. Otherwise, use `side_center(seg, side)` when the step has a valid side.
4. Otherwise, use `segment_center(seg)` as a defensive fallback.
5. Leave the position invalid only for malformed data where none of those sources is available.

Keep the native representation as exact engine fixed-point integers for direct use as a `vms_vector`. Serialize the checked-in mission metadata in readable game-world units:

```json
{
  "index": 1,
  "kind": "trigger",
  "label": "Shoot switch trigger 8",
  "label_pos": {
    "x": 120.5,
    "y": -48.0,
    "z": 312.25
  }
}
```

Omit `label_pos` when invalid. The object form makes the X/Y/Z meaning explicit. Serialization divides each fixed value by `LEVEL_METADATA_FIX_SCALE`; the runtime overlay does not read JSON back and therefore does not introduce a floating-point round trip.

Do not alias `label_pos` to `activation_pos`. For an off-center shoot-switch firing pose, that would place the number at the Guide-Bot/player vantage point instead of on the objective.

## State ownership

Add a canonical level metadata snapshot in `secret_area_game_adapter.c`:

- Capture it immediately after the non-route-only `level_metadata_scan_level()` call.
- Keep the existing mutable `Level_metadata_state` behavior for Guide-Bot consumers to avoid broad behavior changes.
- Add a narrow getter such as `level_metadata_get_canonical_state()` and use it for objective rendering and metadata serialization.
- Clear/rebuild the canonical state only during a full level rescan.
- Store the per-level `show objectives` flag beside the shared metadata runtime state, with getter/setter functions declared in both D1 and D2 `secretarea.h` files.
- Reset the flag in the same level-load path that resets secret reveal.

This is a small version of the canonical/live route separation already needed by the route system and prevents Guide-Bot commands from changing the overlay.

## Implementation phases

### Phase 1: Shared objective anchors

- Extend `level_metadata_route_step` in `android/app/src/main/cpp/shared/level_metadata_scan.h` with label position and validity fields.
- Add one finalization helper in `level_metadata_scan.c` that applies the anchor fallback order after route construction. Avoid repeating center-selection logic in each route-step producer.
- Add scanner tests for key, shoot switch, fly-through trigger, hidden door, boss/reactor, and exit anchors.
- Add a test where `activation_pos` differs from the wall center and assert that `label_pos` follows the objective target.
- Add invalid-segment/side coverage to prove malformed metadata is omitted safely.

Exit gate: every normal non-start route step has a deterministic anchor and existing route order/status/distance tests are unchanged.

### Phase 2: Freeze canonical objectives

- Capture a canonical `level_metadata_state` after the full level scan.
- Add a canonical getter without changing the current getter used by live routing.
- Make JNI and headless mission serializers use the canonical route steps.
- Add a focused test or introspection assertion that a route-only rescan does not change the canonical objective count, order, or label positions.

Exit gate: Guide-Bot live routing can change its mutable route while objective metadata remains byte-for-byte stable.

### Phase 3: JSON metadata propagation

- Add a shared small serializer helper in both `jni_level_metadata.cpp` and `headless_metadata_dump_main.cpp` to emit the `label_pos` X/Y/Z object in game units.
- Add a `LevelMetadataPosition` value type and nullable `labelPosition` to `LevelMetadataRouteStep` in `LevelMetadata.kt`.
- Parse `label_pos` defensively and preserve it in `LauncherScriptExecutor.levelMetadataRouteStepsJson()` so emulator-generated metadata does not lose host-generated fields.
- Leave the launcher route-step UI unchanged initially; the position is machine metadata for rendering and future tooling, not useful primary display text.
- Regenerate all checked-in `game_data/mission_files/*.json` only after reviewing a focused Obsidian diff and confirming host/emulator parity.

Exit gate: host and emulator generation emit the same schema and every valid non-start Obsidian route step contains `label_pos`.

### Phase 4: Native toggle and Android menu

- Add native get/set/toggle functions for the per-level objective overlay flag.
- Add JNI methods beside the existing secret-area reveal methods in `android_input.c` and declarations/callback wiring in `MainActivity.kt`.
- Add a new admin action constant in `TouchOverlayView.kt` and include it in `AdminTrayPolicy` as an automap-only checkbox that stays open after activation.
- Add state-provider handling so controller and touch navigation both show the checked state.
- Do not add a typed cheat code or duplicate the route/objective definitions in Kotlin.

Exit gate: the automap admin tray toggles native state in D1 and D2, resets it on the next level, and never affects save/config state.

### Phase 5: D1 and D2 automap rendering

- In both `d1/main/automap.c` and `d2/main/automap.c`, generalize the local projected secret-label helper into a projected automap text helper.
- Read the frozen canonical objective steps through the shared getter.
- When the flag is enabled, iterate non-start steps, format the objective number as decimal text, convert the fixed `label_pos` directly to `vms_vector`, rotate/project it, and center the text.
- Draw after `g3_end_frame()`, beside the existing secret-label pass, because the current OpenGL path requires text labels outside the 3D automap frame.
- Use one high-contrast color distinct from red/green secret labels, red D2 marker labels, player colors, and key colors. Start with bright cyan or white and adjust only if emulator verification shows poor contrast.
- Render all valid labels without creating marker objects, consuming marker slots, sorting each frame, doing geometry scans, or changing edge visibility.
- Add objective enabled/candidate/projected counters to the existing `automap_view_info` introspection structure in both D1 and D2.

Exit gate: enabling the toggle makes the projected count positive in a known objective-rich level; disabling it returns the candidate/projected counts to zero.

### Phase 6: Automation and regression coverage

- Extend game automation with `set_objective_overlay`, mirroring `set_secret_reveal`.
- Add a D2 Obsidian automap script that asserts:
  - overlay is initially disabled
  - the canonical objective count matches the level metadata
  - enabling produces the expected candidate count and at least one projected label
  - disabling returns both draw counts to zero
- Add a D1 base-level smoke script to cover the duplicated D1 automap implementation.
- Extend Kotlin tests for automap-only visibility, checkbox behavior, state labels/providers, and tray close policy.
- Extend level metadata parser/writer tests to prove `label_pos` survives Android JSON round trips.
- Add a corpus assertion that route-step indices remain contiguous and every non-start step with a valid segment has a label position.

Exit gate: focused C/C++ tests, Kotlin unit tests, D1/D2 emulator scripts, and metadata corpus tests all pass without screenshot/OCR checks.

## Proposed files

Shared scan and ownership:

- `android/app/src/main/cpp/shared/level_metadata_scan.h`
- `android/app/src/main/cpp/shared/level_metadata_scan.c`
- `android/app/src/main/cpp/shared/secret_area_game_adapter.c`
- `d1/main/secretarea.h`
- `d2/main/secretarea.h`

Metadata serialization and Android preservation:

- `android/app/src/main/cpp/jni_level_metadata.cpp`
- `android/app/src/main/cpp/headless/headless_metadata_dump_main.cpp`
- `android/app/src/main/java/com/dxxredux/app/LevelMetadata.kt`
- `android/app/src/main/java/com/dxxredux/app/LauncherScriptExecutor.kt`
- `game_data/mission_files/*.json` through the normal regeneration workflow

Toggle, rendering, and diagnostics:

- `android/app/src/main/cpp/android_input.c`
- `android/app/src/main/cpp/shared/game_introspect.cpp`
- `android/app/src/main/cpp/shared/game_automate.cpp`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt`
- `android/app/src/main/java/com/dxxredux/app/AdminTrayPolicy.kt`
- `d1/main/automap.c` and `d1/main/automap.h`
- `d2/main/automap.c` and `d2/main/automap.h`

Tests:

- `android/tests/test_level_metadata_scan.c`
- relevant metadata generation/parser tests
- `android/app/src/test/java/com/dxxredux/app/AutomapTouchPolicyTest.kt`
- `android/app/src/test/java/com/dxxredux/app/AdminTrayUiTest.kt`
- new D1 and D2 scripts under `android/game_scripts/`

## Verification sequence

1. Run focused scanner and metadata serializer tests.
2. Run scoped code quality over the touched C, C++, Kotlin, headers, scripts, and plan file.
3. Run Android JVM unit tests with JDK 21.
4. Run `run-windows-build.ps1` for D1 and D2 host compatibility.
5. Run fast host mission metadata regeneration and review Obsidian plus base-campaign diffs.
6. Run route corpus and base mission route-status checks to prove objective semantics did not change.
7. Run the authoritative emulator metadata regeneration to validate the JNI/Kotlin path and update checked-in JSON.
8. Run the D1 and D2 automap automation scripts sequentially with cleared logcat and output redirected to files.
9. Perform one manual emulator readability check for label color, centering, overlap, and interaction with marker/secret labels. Use introspection for correctness assertions.

## Risks and mitigations

| Risk | Mitigation |
| --- | --- |
| Live Guide-Bot rescans renumber labels | Render from a frozen canonical metadata snapshot |
| Labels appear at a firing vantage point | Derive from aim/side/object center, never activation position |
| Host and emulator JSON differ | Extend both native serializers and the Kotlin parse/write bridge, then run both regeneration paths |
| Objective labels disappear behind the automap pass | Draw after `g3_end_frame()`, matching the proven secret-label path |
| Objective numbers collide with D2 marker numbers | Use a distinct color; retain pure `1..N` text as requested |
| Route is partial or malformed | Draw valid known steps only and expose exact candidate/projected counts |
| Toggle affects gameplay or saves | Keep it as isolated per-level debug state and never touch normal cheat, visited, save, or config data |
| Large metadata diff hides semantic regressions | First inspect focused Obsidian output, then gate full regeneration with route corpus/status tests |

## Definition of done

- Every valid canonical non-start route step has a deterministic label anchor.
- Checked-in mission JSON contains readable X/Y/Z `label_pos` metadata for those objectives.
- Objective numbering remains 1 through N for the loaded level and is unaffected by live Guide-Bot routing or objective completion.
- The Android automap menu exposes an off-by-default, per-level objective overlay checkbox in D1 and D2.
- D1 and D2 automaps render centered numeric world-space labels without mutating automap visibility or marker state.
- Introspection and automation verify enabled state, candidate count, and projected count without image analysis.
- Focused tests, Kotlin tests, D1/D2 builds, route corpus checks, metadata regeneration, and emulator scripts pass.
