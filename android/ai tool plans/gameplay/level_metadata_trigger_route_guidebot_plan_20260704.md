# Trigger-Aware Level Pathing and Guidebot Planning

## Goal
Plan support for trigger-aware mission path analysis, metadata UI documentation, and guidebot routing through objective chains such as keys, reactors, bosses, exit triggers, hidden doors, and switch-opened passages.

## Planning Checklist
- [x] Create this plan before further work.
- [x] Inspect current level metadata scan data flow and JSON/UI consumers.
- [x] Inspect guidebot path target selection and passability decisions.
- [x] Design a route/objective model that can express keys, triggers, bosses, reactors, hidden doors, and exits.
- [x] Design metadata submenu output for per-level route chains.
- [x] Design guidebot next-objective integration using live trigger/open state.
- [x] Split implementation into safe phases with tests.

## Implementation Checklist
- [x] Begin Phase 1 and route-step JSON implementation.
- [x] Add route step data structures and callback surface to `level_metadata_scan.h`.
- [x] Teach `level_metadata_scan.c` to produce a trigger-aware route chain without regressing current travel distance/status fields.
- [x] Extend C synthetic tests in `android/tests/test_level_metadata_scan.c`.
- [x] Serialize route steps from JNI and headless metadata dump paths.
- [x] Extend Kotlin metadata models and add a per-level route detail UI.
- [x] Preserve route steps in launcher automation JSON output.
- [x] Regenerate focused KCXF2 committed mission ZIP regression baseline with route arrays.
- [ ] Regenerate broader committed mission ZIP regression baselines under `game_data/mission_files/*.json` with route arrays. Partial: 106 of 107 non-tracklist JSON files now have route steps. `U3AAH.json` remains open because metadata generation succeeds but the launch sanity step stalls after difficulty selection. `ulterior_v1.0.6b.7z` did not create a normal metadata JSON because archive import timed out.
- [x] Add a live guidebot next-objective selector that can target trigger source walls.
- [x] Add introspection and automation coverage for guidebot route objectives.
- [x] Run scoped formatting, C tests, Kotlin compile, host builds, and a KCXF2 level 2 verification script.
- [x] Run focused Android metadata and guidebot automation after the KCXF2 baseline is regenerated.

## Implementation Notes 2026-07-04
- Phase 1 and the metadata/UI slice are implemented.
- The route planner now emits structured steps for start, keys, triggers, reactor/boss, and exit.
- `route_status`, `route_problem`, and `route_steps` are serialized by JNI and headless metadata paths.
- `LauncherScriptExecutor` now preserves `route_steps`, so newly generated mission metadata JSON will carry route arrays.
- The per-level metadata table now has a route details action that opens a compact path dialog.
- Headless metadata now emits failed rows for missing level files instead of aborting the whole JSON file. KCXF2RMv11 references missing secret file `kcxf2_s0.rl2`; this now appears as a missing-level problem while normal levels still serialize.
- MSVC packing from legacy engine headers can leave callers at 1-byte packing. `level_metadata_scan.h` now brackets its shared ABI structs with MSVC pack push/pop to keep C and C++ route-step layout consistent.

Validation completed:
- `.\android\helpers\stop-stale-formatters.ps1`
- `.\android\run-code-quality.ps1 -Fix -Paths ...`
- `.\run-windows-build.ps1 -Target both`
- `.\buildd1\maths\test_level_metadata_scan.exe`
- `.\buildd2\maths\test_level_metadata_scan.exe`
- `.\android\gradlew.bat :app:compileDebugKotlin`
- KCXF2 headless smoke: level 2 route status `ok`; route kinds `start -> key -> trigger -> trigger -> trigger -> trigger -> trigger -> trigger -> trigger -> exit`; trigger chain `4, 7, 8, 13, 19, 18, 17`; exit trigger `16`.

## Implementation Notes 2026-07-04 Guidebot Slice
- Focused mission ZIP automation regenerated `game_data/mission_files/KCXF2RMv11.json` with route arrays. KCXF2 level 2 now records `start -> key -> trigger -> trigger -> trigger -> trigger -> trigger -> trigger -> trigger -> exit`, with trigger chain `4, 7, 8, 13, 19, 18, 17` and exit trigger `16`.
- Android D2 guidebot default "next" now checks `level_metadata_get_state()->route_steps` before falling back to classic key, boss, reactor, and exit selection.
- Trigger route objectives store their own target segment, side, wall, trigger id, and label. The guidebot menu and "Finding NEXT" text use that route label.
- Live route satisfaction checks player key flags, disabled triggers, and current wall state so already-opened trigger segments are skipped.
- Introspection now exposes `guidebot.route_goal_active`, `route_goal_label`, `route_goal_seg`, `route_goal_side`, `route_goal_wall`, and `route_goal_trigger`.
- Automation now supports `set_debug` field `player_keys`, which lets a script grant keys and forces D2 guidebot goal recomputation.
- Added `android/game_scripts/test_kcxf2_guidebot_route_next.json5`. It imports KCXF2, analyzes metadata, launches level 2, verifies the guidebot first targets the blue key, grants the blue key, and verifies the next target is open-wall trigger 4.
- Added `.7z` to `game_data/generate_game_data_index.ps1` and regenerated `game_data/game_data_index.txt` so mission ZIP dependencies can be declared directly.

Additional validation completed:
- `.\android\run-code-quality.ps1 -Fix -Paths @('d2/main/escort.c','d2/main/escort.h','android/app/src/main/cpp/shared/game_introspect.cpp','android/app/src/main/cpp/shared/game_automate.cpp','android/game_scripts/test_kcxf2_guidebot_route_next.json5','game_data/generate_game_data_index.ps1','game_data/game_data_index.txt','game_data/mission_files/KCXF2RMv11.json','android/ai tool plans/gameplay/level_metadata_trigger_route_guidebot_plan_20260704.md')`
- `.\run-windows-build.ps1 -Target d2`
- `.\buildd1\maths\test_level_metadata_scan.exe`
- `.\buildd2\maths\test_level_metadata_scan.exe`
- `cd android; .\gradlew.bat :app:assembleDebug` with JDK 21
- `.\android\run-code-quality.ps1 -Fix -Paths @('android/game_scripts/test_kcxf2_guidebot_route_next.json5')`
- `.\android\helpers\run_test.ps1 test_kcxf2_guidebot_route_next.json5 -TimeoutSeconds 900`

## Implementation Notes 2026-07-04 Broader Baseline Slice
- `android/helpers/run_mission_zip_batch.ps1` now accepts multiple patterns and defaults to both `*.zip` and `*.7z`, so committed 7z mission archives can use the same metadata generation path.
- Mission-list automation now separates command entries from stock base missions. D2 skips prefixed stock entries such as `D1:` and `D2:`, plus `Counterstrike`, while custom missions with descriptive names such as `Descent 2: Enemy Vignettes` remain selectable.
- Added a sole-base fallback for mission-list automation. This keeps focused single-mission imports usable when the list legitimately contains only one selectable mission.
- Generated new route-array baseline `game_data/mission_files/diehard.json` from `diehard.7z`.
- Regenerated route arrays across four bounded `*.zip` chunks and one focused `Descent.zip` rerun. Current coverage is 46 of 106 non-tracklist mission JSON files with at least one `route_steps` array. The remaining 60 JSON files still need batch generation.
- `ewithin-versions.zip` was intentionally skipped during bounded chunks by passing `-LargeZipIncludePatterns @()` because it is the oversized archive already treated specially by the batch helper.
- `game_data/generate_game_data_index.ps1` now includes `.7z`; `game_data/game_data_index.txt` now includes `game_data/mission_files/KCXF2RMv11.7z`.

Additional validation completed:
- `.\android\helpers\run_mission_zip_batch.ps1 -Pattern 'diehard.7z' -Install -TimeoutSeconds 900`
- `.\android\helpers\run_mission_zip_batch.ps1 -Pattern 'Descent.zip' -Install -TimeoutSeconds 900`
- Four bounded runs of `.\android\helpers\run_mission_zip_batch.ps1 -Pattern '*.zip' -MaxZips 12 -LargeZipIncludePatterns @() -TimeoutSeconds 900`
- `cd android; .\gradlew.bat :app:assembleDebug` with JDK 21
- `.\game_data\generate_game_data_index.ps1`

## Implementation Notes 2026-07-04 Baseline Continuation
- Continued broad mission ZIP generation from 46 of 106 existing non-tracklist JSON files with route arrays to 105 of 106.
- Focused `tu.zip` rerun passed after the previous batch failure. Logcat showed the failed batch was an Android `levelmeta_d2` service process attach/start timeout, not a scanner timeout.
- One bounded batch hit the outer tool timeout after updating `Tyrsis`, `Vela1`, `Vertigo Missions`, `Vesta`, and `Vignettes`; `U3AAH` timed out during launch sanity and `vignett2` exposed a mission selector bug.
- Tightened D2 mission-list filtering so custom missions named like `Descent 2: Enemy Vignettes` are selectable. The automation still skips `D1:` and `D2:` prefixed stock entries and `Counterstrike`.
- Focused `vignett2.zip` passed after the selector fix.
- Regenerated the remaining existing normal ZIP baselines: `-MOON-`, `af_d1_beta`, `af-d2x`, `anachron`, `ascent`, `Bahagad`, `BelialSystemXL`, `bratmaze`, and `castaway_redux`.
- `bratmaze.zip` initially lost the emulator during the group run. A fresh emulator/install resolved it and the focused rerun passed.
- Added new route-array baseline `game_data/mission_files/nefarious.json` from `nefarious.7z`.
- Final current coverage is 106 of 107 non-tracklist mission JSON files with route arrays. The remaining missing JSON is `U3AAH.json`; its temp metadata contains route arrays, but the batch does not copy it because the game launch sanity never reaches `automation_result.json`.
- `ulterior_v1.0.6b.7z` remains open. The archive import step did not complete within 900 seconds, so no normal metadata JSON was created. `ewithin-versions.zip` remains intentionally skipped as the 892.7 MB oversized archive.

Additional validation completed:
- `.\android\gradlew.bat -p android :app:assembleDebug` with JDK 21
- Focused rerun: `.\android\helpers\run_mission_zip_batch.ps1 -Pattern 'vignett2.zip' -Install -TimeoutSeconds 900`
- Focused rerun: `.\android\helpers\run_mission_zip_batch.ps1 -Pattern 'U3AAH.zip' -Install -TimeoutSeconds 240` failed during launch sanity after metadata generation
- Three focused groups covering the remaining ZIP baselines
- Focused reruns: `bratmaze.zip`, `castaway_redux.zip`, `nefarious.7z`
- Focused `ulterior_v1.0.6b.7z` attempt failed by import timeout

## Investigation Notes 2026-07-04 Obsidian Advanced Pathing
- Obsidian is now the focused regression target for advanced route semantics beyond basic keys, reactor, and exit. Its levels are completable, but the current route-chain solver marks some rows partial or failed.
- Git history for `game_data/mission_files/Obsidian.json` currently has `697d5f7d initial level zip intake` and `7c4231a9 rework guideboth pathing and metadata`. Compare those versions before changing scanner behavior, because the older flat metadata may show what was considered reachable before ordered route chains were introduced.
- The investigation should separate two signals:
  - `travel_*` reachability from the older metadata pass.
  - `route_status`, `route_problem`, and `route_steps` from the newer dependency chain solver.
- Obsidian specifically needs study for shootable switches as well as hidden wall triggers. Trigger source walls that are not ordinary fly-through doors may still be valid player objectives if the player can reach a segment with line-of-fire to the switch face.
- First implementation target: identify why levels with complete `travel_targets_reached` still report `trigger route dependency loop`, then add the smallest scanner semantics or test fixture needed to model that pattern.

Implementation update:
- The scanner now treats reachable line-of-fire to a trigger source wall as a valid trigger objective. This fixes shootable switches whose source wall segment is not physically reachable.
- Route generation now records visible reactor/boss objectives when the control center or boss can be attacked from a reachable firing segment.
- Trigger dependency failures can make the optimistic search temporarily avoid the failing trigger and retry alternate paths to the same target.
- Hidden doors are now modeled as route dependencies using `wall_flags`, `wall_clip_flags`, `WCF_HIDDEN`, and `WALL_DOOR_LOCKED` from the game adapter. Opening one appends a `hidden_door` step and marks both sides of that wall connection opened.
- Trigger-linked doors are considered trigger dependencies before ordinary door passability. Generic keyless doors keep the previous passability behavior; using `WALL_DOOR_LOCKED` as a universal hard blocker produced false Obsidian failures.

Obsidian result after focused regeneration:
- `game_data/mission_files/Obsidian.json` now contains trigger-heavy and hidden-door route arrays. Examples include hidden-door steps in levels 1, 4, and 8, and trigger chains in levels 3, 5, 7, 9, and 14.
- The focused batch now leaves only two non-ok route rows:
  - Level 10, `Aquarius Falls`: `partial`, `route target unreachable`, chain `start -> key`.
  - Level 12, `Lusus Flagship`: `failed`, `red key unreachable`, chain `start`.
- This is a major improvement over the initial Obsidian ordered-route pass, where levels 1, 3, 9, 10, 12, 13, 14, and secret -1 were failed or partial. It also avoids the over-strict locked-door experiment that made levels 3, 5, 6, and 14 look worse despite known completability.

Validation completed for this Obsidian slice:
- `.\android\gradlew.bat -p android :app:assembleDebug` with JDK 21.
- `.\run-windows-build.ps1 -Target both`.
- `.\buildd1\maths\test_level_metadata_scan.exe`.
- `.\buildd2\maths\test_level_metadata_scan.exe`.
- `.\android\helpers\run_mission_zip_batch.ps1 -Pattern 'Obsidian.zip' -Install -TimeoutSeconds 900`.
- `.\android\helpers\stop-stale-formatters.ps1`.
- `.\android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/cpp/shared/level_metadata_scan.c','android/app/src/main/cpp/shared/level_metadata_scan.h','android/app/src/main/cpp/shared/secret_area_game_adapter.c','android/tests/test_level_metadata_scan.c','game_data/mission_files/Obsidian.json','android/ai tool plans/gameplay/level_metadata_trigger_route_guidebot_plan_20260704.md')`.

## Current Code Map
- Shared metadata scanner:
  - `android/app/src/main/cpp/shared/level_metadata_scan.h`
  - `android/app/src/main/cpp/shared/level_metadata_scan.c`
  - This code already computes `travel_distance`, `travel_time_seconds`, `travel_status`, key detours, and target counts.
  - It collects hostages, keys, reactor/control-center segment, and exits.
  - It does not persist an ordered route chain. It treats a trigger-opened side as immediately passable via `edge_has_trigger_opener`.
- Game adapter:
  - `android/app/src/main/cpp/shared/secret_area_game_adapter.c`
  - `level_metadata_rescan_current_level()` builds `level_metadata_scan_view`.
  - It already has trigger source discovery helpers through `secret_area_side_opener_source_wall_at`.
  - Metadata only receives `triggered_side_opener_count`, not trigger source wall, trigger id, trigger type, trigger flags, or trigger links.
- Metadata serializers:
  - `android/app/src/main/cpp/jni_level_metadata.cpp`
  - `android/app/src/main/cpp/headless/headless_metadata_dump_main.cpp`
  - Both serialize the current flat travel fields. Both need the same optional `route_steps` array.
- Mission ZIP regression JSON:
  - `android/app/src/main/java/com/dxxredux/app/LauncherScriptExecutor.kt`
  - `android/helpers/run_mission_zip_batch.ps1`
  - The launcher writes `files/level_metadata_automation_<label>.json`.
  - The batch runner normalizes that JSON and copies passing runs into `game_data/mission_files/*.json`, for example `game_data/mission_files/KCXF2RMv11.json`.
  - These committed mission JSON baselines should include route path arrays for every level row.
- Kotlin metadata UI:
  - `android/app/src/main/java/com/dxxredux/app/LevelMetadata.kt`
  - `android/app/src/main/java/com/dxxredux/app/SetupSections.kt`
  - The current dialog is a flat table. A route chain should be an optional per-level detail, not another cramped table column.
- Guidebot:
  - `d2/main/escort.c`
  - `d2/main/ai2.c`
  - `d2/main/ai.h`
  - The default goal order is missing keys, then boss/reactor, then exit.
  - Android has a nearest-reachable-point fallback, but that fallback is geometric. It does not know that an exit route may require firing trigger walls first.

## KCXF2 Level 2 Target Behavior
For `KCXF2RMv11.7z`, level 2 `kcxf2_n2.rl2`, "Aquabed Borehole":

- There is no reactor object and no external `child == -2` exit side.
- The only exit is D2 trigger 16 on wall 135 at segment 896 side 4.
- The only real key is the blue key in segment 152.
- The success route is not just start -> blue key -> exit. The exit route depends on a chain of `TT_OPEN_WALL` triggers.

Expected metadata route chain, roughly:

```text
Start
Blue Key, segment 152
Open Wall Trigger 4 or 5, source segment 727 or 728, opens walls 76 and 77
Open Wall Trigger 7, source segment 854, opens walls 63, 64, 65, 66, 82, 83
Open Wall Trigger 8, source segment 851, opens walls 67, 68, 78, 79, 80, 81
Open Wall Trigger 13, source segment 104, opens blue-key chamber walls
Open Wall Trigger 19, source segment 182
Open Wall Trigger 18, source segment 177
Open Wall Trigger 17, source segment 179, opens final passage near wall 123
Exit Trigger 16, segment 896 side 4
```

The exact chain should come from the planner, not a hard-coded mission special case.

## Route Step Model
Add a structured route chain to `level_metadata_state` or to a nested struct reachable from it.

Suggested constants:

```c
#define LEVEL_METADATA_MAX_ROUTE_STEPS 96
#define LEVEL_METADATA_ROUTE_LABEL_LEN 64
#define LEVEL_METADATA_ROUTE_OPEN_LINKS 10
```

Suggested step fields:

```c
typedef enum level_metadata_route_step_kind {
	LEVEL_METADATA_ROUTE_START,
	LEVEL_METADATA_ROUTE_KEY,
	LEVEL_METADATA_ROUTE_TRIGGER,
	LEVEL_METADATA_ROUTE_REACTOR,
	LEVEL_METADATA_ROUTE_BOSS,
	LEVEL_METADATA_ROUTE_EXIT,
	LEVEL_METADATA_ROUTE_HIDDEN_DOOR,
	LEVEL_METADATA_ROUTE_HOSTAGE
} level_metadata_route_step_kind;

typedef struct level_metadata_route_step {
	int kind;
	int seg;
	int side;
	int wall_num;
	int trigger_num;
	int trigger_type;
	int key_index;
	double distance_from_previous;
	char label[LEVEL_METADATA_ROUTE_LABEL_LEN];
	int opened_link_count;
	int opened_link_seg[LEVEL_METADATA_ROUTE_OPEN_LINKS];
	int opened_link_side[LEVEL_METADATA_ROUTE_OPEN_LINKS];
} level_metadata_route_step;
```

Keep labels as display helpers only. The JSON should still carry kind, seg, side, wall, trigger, and key fields so later UI and guidebot code do not parse strings.

## Scanner Callback Additions
Extend `level_metadata_scan_view` instead of reaching into D2 globals from `level_metadata_scan.c`.

Recommended callback additions:

```c
int (*wall_flags)(void *user, int wall_num);
int (*wall_state)(void *user, int wall_num);
int (*wall_trigger)(void *user, int wall_num);
int (*wall_controlling_trigger)(void *user, int wall_num);
int (*wall_clip_flags)(void *user, int wall_num);
int (*trigger_count)(void *user);
int (*trigger_type)(void *user, int trigger_num);
int (*trigger_flags)(void *user, int trigger_num);
int (*trigger_link_count)(void *user, int trigger_num);
int (*trigger_link_segment)(void *user, int trigger_num, int link_index);
int (*trigger_link_side)(void *user, int trigger_num, int link_index);
int (*trigger_source_wall_at)(void *user, int trigger_num, int index);
int (*object_is_boss)(void *user, int objnum);
```

The adapter can implement these with `Walls`, `Triggers`, and `Robot_info`. In D1 or other contexts, callbacks can be null and the scanner should degrade to the current simple route behavior.

## Planner Design
The scanner needs two related modes:

- Static metadata mode: compute the intended route from level start with no triggers fired and no keys owned.
- Live guidebot mode: compute the next unmet objective using current player keys, current wall states, and disabled trigger flags.

The first implementation should do static metadata mode inside `level_metadata_scan.c`. The guidebot can reuse the model later, either by calling a live variant or by sharing a small new route planner module.

### Passability States
Keep the existing route pathfinder, but add a richer edge classification:

- `open`: normal passable edge.
- `missing_key`: blocked until a key objective is complete.
- `trigger_closed`: blocked until a known trigger source is fired.
- `hidden_door`: visible/secret door or illusion state that may need explicit handling.
- `exit_side`: terminal only, not a transit edge.
- `unsupported`: blocked with no known way forward.

Do not let `edge_has_trigger_opener` silently return passable for metadata route chain generation. It can remain available for legacy travel status until the new planner is stable.

### Greedy Dependency Resolution
Use a deterministic greedy dependency solver rather than full state-space Dijkstra over all triggers. D2 levels can have up to 100 triggers, and most route chains are linear or nearly linear.

Algorithm sketch:

1. Collect canonical objectives:
   - Start.
   - Needed keys discovered from objects or contained items.
   - Boss if a boss robot exists and is the level completion gate.
   - Reactor/control center if present.
   - Exit trigger or external exit.
2. Pick the current canonical destination:
   - Basic D2: key(s) needed by blocking doors, then reactor/boss, then exit.
   - Reactorless level with exit: keys and triggers needed to reach exit, then exit.
3. Run shortest path with the current state.
4. If the path reaches the destination, append that destination step and update state.
5. If blocked by a missing-key edge, append a route to the nearest reachable copy of that key, update key state, and retry.
6. If blocked by a trigger-openable edge, find source wall candidates for the trigger.
7. Select the reachable source wall with the lowest route cost. Append a trigger step with the trigger id and links it opens.
8. Simulate trigger effects in planner state, then retry the blocked destination.
9. Stop with `partial` and a useful `travel_problem` if no key or trigger source is reachable.

The route planner should cap iterations at something like 128 and mark partial results rather than looping.

### Trigger Effect Simulation
For static analysis, firing a trigger should update planner-local state:

- `TT_OPEN_DOOR`: linked door sides become open/passable.
- `TT_OPEN_WALL`: linked walls become open/passable.
- `TT_ILLUSORY_WALL`: linked walls become illusion/passable.
- `TT_ILLUSION_OFF`: linked illusion walls become passable when flags imply collision is removed.
- `TT_UNLOCK_DOOR`: linked doors lose key/locked requirements.

At first, ignore or mark as non-progress triggers:

- Close door, close wall, lock door, light on/off, matcen.

For one-shot triggers, metadata can treat firing as complete. Live mode should use `TF_DISABLED` plus current wall state to decide whether the effect is already satisfied or no longer available.

## Metadata JSON
Add route fields to both JNI and headless serializers:

```json
"route_status": "ok",
"route_problem": "",
"route_steps": [
  {
    "index": 0,
    "kind": "start",
    "label": "Start",
    "seg": 0
  },
  {
    "index": 1,
    "kind": "key",
    "key": "blue",
    "label": "Blue key",
    "seg": 152,
    "distance": 1234.5
  },
  {
    "index": 2,
    "kind": "trigger",
    "label": "Open wall trigger 4",
    "seg": 727,
    "side": 4,
    "wall": 70,
    "trigger": 4,
    "trigger_type": "open_wall",
    "opens": [
      { "seg": 703, "side": 1, "wall": 76 },
      { "seg": 704, "side": 3, "wall": 77 }
    ]
  }
]
```

Keep `travel_*` fields exactly as they are for compatibility with current tables and automation summaries.

## Mission JSON Baselines
The generated mission metadata files under `game_data/mission_files/*.json` should carry the same route fields as the live launcher metadata result. This is the durable regression artifact the tests generate and commit.

Current shape, using `game_data/mission_files/KCXF2RMv11.json` as the example:

```json
[
  {
    "status": "ok",
    "mission_name": "KCXF2RM",
    "levels": [
      {
        "level_num": 2,
        "level_name": "Aquabed Borehole",
        "travel_distance": 6060.21774899593
      }
    ]
  }
]
```

Planned shape:

```json
[
  {
    "status": "ok",
    "mission_name": "KCXF2RM",
    "levels": [
      {
        "level_num": 2,
        "level_name": "Aquabed Borehole",
        "travel_distance": 6060.21774899593,
        "route_status": "ok",
        "route_problem": "",
        "route_steps": [
          { "index": 0, "kind": "start", "label": "Start", "seg": 0 },
          { "index": 1, "kind": "key", "key": "blue", "label": "Blue key", "seg": 152 },
          { "index": 2, "kind": "trigger", "trigger": 4, "trigger_type": "open_wall", "label": "Open wall trigger 4", "seg": 727 },
          { "index": 3, "kind": "exit", "trigger": 16, "trigger_type": "exit", "label": "Exit trigger", "seg": 896, "side": 4 }
        ]
      }
    ]
  }
]
```

Implementation notes:

- `jni_level_metadata.cpp` should put `route_status`, `route_problem`, and `route_steps` into every level row returned to Kotlin.
- `LevelMetadata.kt` should parse and retain those fields in `LevelMetadataLevelRow`.
- `LauncherScriptExecutor.levelMetadataResultJson()` must write `route_steps` back out. This is the path that produces the app-private `level_metadata_automation_<label>.json` file.
- `run_mission_zip_batch.ps1` should not need semantic changes if the app JSON already contains the fields, but the plan should include a verification step that its normalized local copy preserves the arrays.
- The committed baselines in `game_data/mission_files/*.json` should become the cross-mission regression record for route analysis, not just the launcher UI payload.
- If a route cannot be solved, keep `route_steps` as the partial chain and use `route_status: "partial"` plus `route_problem`. Do not omit the array unless the scanner cannot run at all.

## Kotlin UI
Extend `LevelMetadata.kt`:

- Add `LevelMetadataRouteStep`.
- Add `routeStatus`, `routeProblem`, and `routeSteps` to `LevelMetadataLevelRow`.
- Parse `route_steps` with `optJSONArray`, so old outputs still load.

Extend `SetupSections.kt`:

- Keep the table as the compact overview.
- Add a per-level details affordance. The simplest option is making each level row clickable and showing a detail dialog or expanded section.
- The detail view should show:
  - Level name/file.
  - Current travel summary.
  - Route chain as a compact ordered list.
  - Problems/notes if route status is partial or failed.

Example display text:

```text
Path
1. Start
2. Blue key, segment 152
3. Open wall trigger 4, segment 727
4. Open wall trigger 7, segment 854
5. Exit trigger, segment 896 side 4
```

Avoid putting the full chain in the main table. Long trigger chains will wrap badly there.

## Guidebot Integration
Do this only after metadata route chains are stable.

### New Live Route Objective
Add an Android-specific live route objective layer in `d2/main/escort.c`, guarded with `#ifdef __ANDROID__` where practical.

Recommended approach:

- Add a new internal goal type rather than overloading `ESCORT_GOAL_EXIT`.
- Store route goal fields separately:
  - objective kind
  - target segment
  - target side
  - target wall
  - trigger id
  - display label
- Keep existing key/reactor/boss/exit goals working as before.

Potential additions:

```c
#define ESCORT_GOAL_ROUTE_NEXT 26
#define MAX_ESCORT_GOALS       27
```

But this touches shared goal arrays and save/checkpoint assumptions. A lower-risk alternative is to keep `ESCORT_GOAL_EXIT` for classic behavior and use a private `Escort_route_goal_kind` when Android asks for the default next goal. That needs fewer changes in `ai.h`.

### Next Target Selection
Before `escort_set_goal_object()` returns the classic default, ask the live route planner for the first unmet route step:

- If blue/yellow/red key is not owned and exists, return the existing key goal.
- If a trigger step is unsatisfied, set private route-goal fields and path to the source wall segment.
- If boss/reactor remains, return existing boss/reactor goal.
- If exit is now reachable or all prerequisites are complete, return existing exit goal.

For a trigger source wall, path to a reachable segment from which the player/guidebot can hit or fly through the trigger side. If the source wall's own segment is blocked, try the linked side or nearest visible segment.

### Completion and Messaging
The guidebot should say the semantic next step, not just "Finding EXIT".

Examples:

```text
Finding NEXT: blue key
Finding NEXT: open wall trigger 7
Finding NEXT: exit trigger
```

The escort menu currently builds `goal_str` from `escort_set_goal_object()`. Add route-aware wording there so the menu can show something like:

```text
Find next: open wall trigger
```

Route objective satisfaction rules:

- Key step: player or coop team owns key.
- Trigger step: linked edges are now passable, or the trigger is disabled after firing and its target wall state matches the expected open state.
- Reactor step: control center destroyed.
- Boss step: boss dead or no live boss robot remains.
- Exit step: exit reached or still the terminal target.

## Tests and Verification
### C Unit Tests
Extend `android/tests/test_level_metadata_scan.c` with small synthetic mines:

- Reactorless exit still reports ok and a route chain of start -> exit.
- Keyed door route emits start -> blue key -> exit.
- Trigger-opened wall route emits start -> trigger -> exit.
- Trigger source behind a key emits start -> key -> trigger -> exit.
- Missing trigger source returns partial with a useful problem.
- Boss route emits boss instead of reactor when `object_is_boss` is present.

Run through the existing CMake targets in both D1 and D2 maths builds, because the shared scanner is compiled in both.

### Headless Metadata
Add route serialization checks for:

- Built-in D2 simple level, expecting a basic key/reactor/exit route shape.
- KCXF2 level 2, expecting trigger steps before the exit.

The KCXF2 check can start as a local automation script under `android/game_scripts/` or a temp verification helper if the archive is not committed.

### Mission ZIP Baseline JSON
Add or extend a mission ZIP regression check so the generated files under `game_data/mission_files/*.json` contain path arrays:

- Run `android/helpers/run_mission_zip_batch.ps1` for a focused KCXF2 pattern after route serialization lands.
- Verify the local metadata artifact and copied regression JSON both include `levels[].route_steps`.
- For `KCXF2RMv11.json`, assert level 2 has at least one `key` step, multiple `trigger` steps, and a final `exit` step.
- For a basic D2 mission baseline, assert the path is the simple shape, for example start, optional keys, reactor or boss, exit.
- Keep JSON output pretty-printed and normalized at the producer/write site, matching the existing test JSON normalization rule.

### Android UI
Run metadata analysis from the launcher and verify:

- Old rows still show robots, secrets, travel, and notes.
- Clicking or expanding a row shows route steps.
- Long trigger-heavy route chains remain readable on a phone-size viewport.

### Guidebot
Add introspection fields before doing final behavior tests:

- current escort goal
- route objective kind
- route objective label
- route target segment/side/wall/trigger
- path endpoint segment

Then automate KCXF2 level 2 checks:

1. At level start, next route objective should be blue key.
2. After blue key pickup, next route objective should be the first required open-wall trigger.
3. After firing early triggers, next route objective advances.
4. After final trigger, next route objective is the exit trigger.

Use introspection and automation scripts, not screenshots.

## Phased Implementation Plan
### Phase 1: Scanner Route Chain
Scope:

- Add structured route steps and scanner callbacks.
- Keep current `travel_*` outputs stable.
- Implement key and trigger dependency route generation.
- Add synthetic C tests.

Exit criteria:

- `test_level_metadata_scan` passes for D1 and D2 maths builds.
- Headless output includes a plausible `route_steps` array.
- Existing flat travel fields do not regress on simple fixtures.

### Phase 2: Metadata Serialization and UI
Scope:

- Serialize route steps from JNI and headless paths.
- Parse route steps in Kotlin.
- Preserve route steps in `LauncherScriptExecutor.levelMetadataResultJson()` so launcher automation output includes them.
- Confirm `run_mission_zip_batch.ps1` copies the route arrays into `game_data/mission_files/*.json`.
- Add per-level route detail UI in the metadata dialog.
- Update automation JSON writer to preserve route steps.

Exit criteria:

- Mission metadata analysis still works for existing mods.
- Per-level route details render for basic D2 levels and KCXF2 level 2.
- Focused mission ZIP baseline generation produces `game_data/mission_files/KCXF2RMv11.json` with `levels[].route_steps`, including KCXF2 level 2 trigger steps.

### Phase 3: Live Guidebot Next Objective
Scope:

- Add live route objective selection for Android guidebot default "next".
- Target trigger source walls when route prerequisites are unmet.
- Add route objective messaging and menu text.
- Add introspection for current route objective.

Exit criteria:

- Existing guidebot special goals still work.
- KCXF2 level 2 guidebot targets the next trigger objective instead of repeatedly targeting the blocked exit.
- If a trigger has already opened its path, the guidebot advances to the next objective.

### Phase 4: Broaden Semantics and Regression Coverage
Scope:

- Improve boss-only and reactorless levels.
- Add hidden doors and illusion-off/on semantics where needed.
- Batch scan mission zips and record route-status anomalies for follow-up.

Exit criteria:

- A batch report identifies unsupported route patterns without crashing or producing misleading "ok" route chains.
- The remaining guidebot behavior changes are backed by automation or input-demo coverage.

## Risks and Guardrails
- Do not hard-code KCXF2 level names or segments in production code. Use it as a regression case only.
- Do not move trigger semantics into Kotlin. Keep mine semantics in C/C++ and serialize structured results.
- Do not replace guidebot behavior in one large change. Metadata route generation should prove the model first.
- Be careful with D1/D2 shared code. The shared scanner must degrade cleanly when D2 trigger callbacks are absent.
- Treat `TT_CLOSE_*`, `TT_LOCK_DOOR`, matcen triggers, and damage triggers as non-progress until there is a clear route use case.
- Keep maximum route steps and trigger iterations bounded. Partial output is better than an infinite or exponential search.

## Recommended First Code Tranche
Start with Phase 1 only:

1. Extend `level_metadata_scan_view` with trigger source/link callbacks.
2. Add route step storage to `level_metadata_state`.
3. Add helper functions to append start, key, trigger, reactor/boss, and exit steps.
4. Add edge classification for blocked-by-key and blocked-by-trigger.
5. Add C tests for simple key and trigger routes.
6. Serialize `route_steps` only after the scanner tests are passing.

That gives a narrow, testable base before the guidebot starts making live decisions from the new model.

## Guidebot Vantage Pathing Addendum
Current code already has an Android route-goal bridge in `d2/main/escort.c`, and it is doing useful work:

- `escort_route_next_goal()` reads `level_metadata_get_state()->route_steps`.
- Key route steps map to the existing blue/yellow/red guidebot goals.
- Trigger route steps become an active `Escort_route_goal`, with the guidebot menu showing `next: <label>`.
- `escort_create_path_to_goal()` can fall back to `escort_create_path_to_nearest_point()` when the route target segment is not currently reachable.

The remaining design gap is that a route step has one segment field, but shootable objectives need two concepts:

- Objective: the wall, trigger, boss, reactor, hidden door, or exit side that must be acted on.
- Guidance target: the reachable segment or point where the player and guidebot should go next.

For ordinary keys and reachable doors these are often the same. For shootable switches, visible reactors, bosses, and some hidden doors, they are often different.

### Recommended Live Guidebot Model
Extend the Android route-goal state so it can distinguish objective identity from movement guidance:

- `objective_kind`: route step kind such as trigger, hidden door, reactor, boss, exit.
- `objective_seg`, `objective_side`, `objective_wall`, `objective_trigger`: the thing that must be opened, shot, or reached.
- `guidance_seg`: the currently reachable segment to path the guidebot toward.
- `guidance_pos`: optional fixed-point point within that segment, useful for line-of-fire positions found by the metadata scanner.
- `guidance_mode`: `reach_objective`, `reach_hidden_door`, `reach_firing_position`, or `nearest_progress_point`.

The guidebot should continue to create ordinary paths only through live passable edges. The broader route model should select a better live destination, not make the guidebot fly through closed walls.

### Shootable Switch Behavior
For trigger route steps:

1. Check whether the trigger step is already satisfied using current trigger flags and linked wall passability.
2. If not satisfied, first try to path to the source wall's own segment and side.
3. If the source wall segment is not reachable, compute a live reachable set from the guidebot or player and choose the nearest reachable segment with line of fire to the trigger source wall.
4. Path to that firing segment and label the goal as the semantic objective, for example `next: Open wall trigger 4`.
5. Once there, the guidebot should initially point or wait rather than automatically firing. Automatic shooting can come later after tests prove it does not solve puzzle triggers out of order.

This is the same idea as the scanner's `metadata_route_find_visible_path()`, but evaluated against live wall state. The current `escort_find_nearest_reachable_goal_segment()` is a good fallback building block, but shootables need "nearest reachable segment that can see the target", not just "nearest reachable segment on an optimistic path to the target segment".

### Hidden Door Behavior
Hidden door route steps should become guidebot route goals instead of falling back to classic behavior:

1. Skip the step if the side is already passable.
2. Path to the nearest live-reachable side of the hidden door pair.
3. Show `next: Hidden door` or a more specific route label.
4. After the wall becomes passable, advance to the next route step.

### Reactor and Boss Visibility
If a reactor or boss objective is not directly reachable but the scanner found a valid visible attack position, guidebot "next" should path to that firing position and retain the semantic label:

- `next: Reactor`
- `next: Boss`

This avoids saying "can't reach reactor" when the real success path is to shoot it from a window, shaft, or neighboring chamber.

### Implementation Slices
- [done] Add guidance fields to `level_metadata_route_step` or derive them live into `escort_route_goal`.
- [pending] Preserve scanner terminal firing positions for trigger, reactor, boss, and exit-visible paths if the live guidebot should reuse static metadata.
- [done] Add a live line-of-fire helper in D2 escort code, preferably matching the adapter's `FQ_TRANSWALL` `find_vector_intersection()` behavior.
- [done] Route `LEVEL_METADATA_ROUTE_HIDDEN_DOOR` through the Android guidebot route-goal bridge.
- [done] Add introspection for objective kind, objective segment, guidance segment, path endpoint segment, and the live route chain.
- [done] Extend `test_kcxf2_guidebot_route_next.json5` to assert that the active route goal paths to a reachable trigger guidance target.
- [pending] Add an Obsidian-focused automation or headless check for shootable switch route steps so this does not only fit KCXF2.

### Implementation Notes
The first guidebot tranche derives guidance live in `escort_route_goal` rather than changing the route-step ABI. For KCXF2 level 2, current generated metadata is `Start -> Open wall trigger 13 -> Open wall trigger 19 -> Open wall trigger 18 -> Open wall trigger 17 -> Exit`; the focused automation now asserts the first `next` target as trigger 13 with `reach_firing_position` guidance and a reachable path endpoint.
