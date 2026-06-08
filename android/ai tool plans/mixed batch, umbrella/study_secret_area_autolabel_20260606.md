# Secret area auto-label study

## Goal
- Study whether levels can be scanned at load time to identify optional secret areas, track which have been visited, and optionally reveal them in the automap.

## Plan
- [x] Inspect level geometry, wall, trigger, object, and automap data structures in D1 and D2.
- [x] Identify reliable signals for hidden doors, optional reachable regions, keys, hostages, robots, and ordinary progress.
- [x] Sketch candidate algorithms, cache/state ownership, save-game behavior, and automap integration.
- [x] Sketch shared D1/D2 ownership, adapter boundaries, and committed base-game JSON regression coverage.
- [x] Summarize risks, expected false positives/negatives, and a staged implementation plan.

## Implementation Progress
- [x] Added shared scanner core in `android/app/src/main/cpp/shared/secret_area_scan.c` and `secret_area_scan.h`.
- [x] Added D1/D2 adapters in `d1/main/secretarea.c` and `d2/main/secretarea.c`.
- [x] Deduplicated the D1/D2 adapters into `android/app/src/main/cpp/shared/secret_area_game_adapter.c`.
- [x] Wired level-load rescans through D1/D2 `LoadLevel()`.
- [x] Wired per-frame segment entry marking through D1/D2 `GameProcessFrame()`, guarded against demo playback.
- [x] Added `secret_areas` to Android introspection output for level-loaded game state.
- [x] Added shared headless base-game JSON dump entrypoint in `android/app/src/main/cpp/headless/secret_area_dump_main.cpp`.
- [x] Added `dxx-redux-d1-secretareas` and `dxx-redux-d2-secretareas` host target declarations.
- [x] Added PowerShell compare/update harness in `android/tests/test_secret_area_baseline.ps1` and `update_secret_area_baseline.ps1`.
- [x] Build the new host dump targets and commit `android/test_fixtures/secret_area_base_game_baseline.json`.
- [x] Added per-secret item summaries to runtime state, introspection, and the base-game regression JSON.
- [x] Add first-entry HUD popup text.
- [x] Add save/restore found bits.
- [x] Add automap found labels and reveal-cheat labels.
- [x] Split secret-area reveal from normal cheat state and add an automap-only touch settings toggle that resets on level change.
- [x] Updated first-entry popup wording to `found secret N (total: X/Y)` and made found/revealed secret segments draw full automap edges without mutating normal automap visited state.
- [x] Expanded D2 candidate boundaries to include no-key switch-opened doors, open-wall links, and illusory-wall links when the switch source is ordinary reachable.
- [x] Added `secrets: N/M` to the HUD robot/hostage count block when generated secrets are active.
- [x] Add a D2 Guide-Bot `find secret` command that targets the nearest reachable unfound secret entrance.
- [x] Add the Android Guide wheel/meta-action wiring for `find secret`.
- [x] Add introspection/test visibility for the selected Guide-Bot secret target.
- [x] Build and validate the Guide-Bot secret command.
- [x] Fixed automap reveal/found secret edges being immediately hidden by the segment distance-limit pass; revealed secret segments now bypass `EF_TOO_FAR` without mutating normal `Automap_visited`.
- [x] Added secret-area reveal/drawable counts to introspection so future automap reveal tests can assert the toggle changes drawable secret state without screenshots.
- [x] Fixed automap secret labels being invisible by drawing them after the 3D automap frame, while keeping edge reveal inside the normal 3D pass.
- [x] Added `test_secret_reveal_automap_d2.json5` to launch D2 level 1, enable reveal in automation, and assert the automap sees/draws secret edges and labels.
- [x] Make secret-area automap edges draw yellow only while the Reveal Secrets cheat is active; found-only secret edges keep normal automap coloring.

## Findings
- The engine already has a strong hidden-door signal: `WallAnims[wall.clip_num].flags & WCF_HIDDEN`.
- D1 and D2 automap already use this signal to make hidden doors look like normal walls, and mark the edge as `EF_SECRET`.
- The editor segment `special` field is not a secret marker. D1 defines fuel, repair, control center, and robotmaker values; D2 adds blue/red goal values.
- The segment graph already exposes candidate "behind this wall" regions through `Segments[seg].children[side]`.
- Existing automap visited state is per segment and saved/restored, but it means "seen/rendered", not "player entered".
- A secret-area counter should therefore track player segment entry, not reuse `Automap_visited` as the source of truth.
- Generated secret lists should be recomputed from pristine level data at load time. Save files should store only found bits.
- Base-game validation currently produces conservative totals of 86 D1 secrets and 175 D2 secrets. D1 level 1 produces 2 generated secret areas, matching the corrected expectation that it only has about 1-2 real secret areas. D2 level 1 produces 10 generated secret areas after adding switch-opened optional areas.
- Secret entries now track aggregate powerup summaries with id, readable name, total count, direct count, and contained count. Names prefer the engine `Powerup_names` table where available, with a shared fallback table for narrow host tools.

## Candidate Algorithm
1. After a level file is loaded and before save-game mutations are applied, build a topology graph over segments.
2. Build this graph conservatively. Only add an edge when both segments are valid children of each other or can be confirmed with `find_connect_side()`.
3. Treat no-wall child connections, unlocked normal doors, blastable walls, open sides, and already-passable illusion walls as ordinary traversable topology.
4. Treat `WALL_DOOR` sides whose wall clip has `WCF_HIDDEN` as hidden boundaries. Require `KEY_NONE`, not `WALL_DOOR_LOCKED`, a valid child segment, and a valid reverse side for the first pass.
5. Treat D2 switch-opened `TT_OPEN_DOOR`, `TT_OPEN_WALL`, and `TT_ILLUSORY_WALL` links as candidate boundaries only when the target wall has `KEY_NONE`, the linked side has a valid child/reverse side, and at least one wall that fires the trigger is ordinary reachable.
6. Do not assume other trigger-opened, one-way, external, malformed, or ambiguous geometry is reachable. If reachability depends on a trigger or a wall state change that the scanner cannot prove, reject the candidate for now.
7. Find connected components with secret boundaries removed.
8. Use `Player_init[Player_num].segnum` as the ordinary start component.
9. For each hidden or switch-opened boundary from the ordinary component to another component, create a candidate secret area for the far component. Merge duplicate doors into the same component so two entrances to one room count once.
10. Reject candidates containing:
   - `OBJ_HOSTAGE`
   - direct key powerups: `OBJ_POWERUP` with `POW_KEY_BLUE`, `POW_KEY_RED`, or `POW_KEY_GOLD`
   - contained key drops in robots or other objects, using `contains_type == OBJ_POWERUP` and key `contains_id`
   - reactor/control center segments or objects
   - exit or secret-exit trigger surfaces
11. Reject candidates that are not reachable from the start in a second "player-reachable with secret boundaries allowed" BFS. This pass should allow only the same conservative ordinary edges plus hidden-door and switch-opened crossings that meet the strict requirements above.
12. Record robot counts and robotmaker segments as confidence metadata, not a hard rejection initially. Some real secrets may contain enemies.
13. Compute ordering metadata for every accepted candidate:
   - `entry_distance`: shortest graph distance from `Player_init[Player_num].segnum` to the ordinary-side segment of any hidden entrance into the candidate.
   - `entry_seg` and `entry_side`: the lowest-segment, lowest-side hidden entrance among entrances at the best distance.
   - `label_pos`: a stable label anchor, preferably the average of candidate segment centers, with a fallback to the center of the closest entry-side/secret-side pair.
14. Sort accepted candidates by `entry_distance`, then `entry_seg`, then `entry_side`, then lowest member segment. Assign the visible numbers after this sort, so `S1`, `S2`, etc. roughly follow distance from the start of the mine.
15. Apply the sanity cutoff after filtering and before exposing the feature. If more than `MAX_GENERATED_SECRETS` candidates remain, disable generated secrets for this level.
16. Create `secret_area_for_segment[MAX_SEGMENTS]`, using 0 for no area and 1..N for the sorted visible secret numbers.
17. Once per gameplay frame, if `ConsoleObject->segnum` maps to an area, mark `secret_area_found[area] = 1`.

## Sanity Cutoff
- Add a named constant such as `MAX_GENERATED_SECRETS`, defaulting to 30.
- If the scanner produces more than this after all conservative filters, set a per-level state like `secret_generation_disabled = 1`.
- When disabled:
  - expose `total=0`, `found=0`, and `disabled_reason="too_many_candidates"` through introspection/debug output
  - do not show HUD secret counts or first-entry popups
  - do not draw automap labels or reveal-only secret edges
  - leave normal automap behavior unchanged
- This should be a soft runtime guard, not an assert. A user-made level using unusual hidden-door geometry should remain playable.
- Include both raw candidate count and final candidate count in the debug report so suspicious levels can be reviewed later.

## Conservative Reachability
- The first implementation should prefer false negatives over false positives.
- Use two reachability views:
  - `ordinary_reachable`: from the start segment without crossing hidden doors.
  - `secret_reachable`: from the start segment with strictly openable hidden doors allowed.
- A candidate is eligible only if:
  - it has at least one hidden-door entrance from `ordinary_reachable`
  - the hidden door has a valid child segment and valid reverse connection
  - the door is a `WALL_DOOR` with `WCF_HIDDEN`, `KEY_NONE`, and no locked flag
  - every segment in the candidate can be reached from that entrance using only conservative ordinary edges inside the candidate
  - the candidate appears in `secret_reachable`
- Reject, at least in the first pass, if the only route requires:
  - a trigger-opened `WALL_CLOSED`
  - a `TT_OPEN_WALL`, `TT_ILLUSORY_WALL`, or similar trigger effect
  - a hidden door behind a keyed door where the key path cannot be proven without game progression analysis
  - malformed one-way child links
  - external sides or missing child segments
  - a component reachable only by crossing another generated secret, unless `nested_secret` is explicitly allowed after review
- Trigger-driven and nested secrets can be added later as opt-in heuristic layers after bundled-level reports prove the base scanner is too conservative.

## Secret Ordering
- d1 and d2 have existing path computation, leverage that
- Use graph distance, not Euclidean distance, because mine layout is a winding segment graph.
- The first implementation can use unweighted BFS over segment connections, counting each segment transition as 1.
- Hidden door crossings should not be traversed while computing distance to the entrance. The distance should stop at the normal-side entrance segment, then the secret gets the distance of that entrance.
- If a secret has multiple hidden entrances, use the minimum distance entrance.
- If a secret is only reachable through another secret, reject it in the first implementation. Keep enough introspection detail to revisit nested secrets later if the reports show real bundled-level misses.
- The visible order should be saved only indirectly through deterministic recomputation. Save files should store found bits by visible number for the generated list that was active when saved, and older/different generated lists should rebuild from `Automap_visited` if counts do not match.

## Runtime Popup
- When the player first enters a secret area's segment, show a HUD message once for that area.
- Suggested text: `Found secret S%d` or `Found secret S%d (%d/%d)`.
- Keep popup generation in the secret-area tracking module, but route display through existing HUD message functions, such as `HUD_init_message_literal()` or `HUD_init_message()`.
- Do not show the popup during demo playback, level initialization, or non-player camera movement unless existing HUD conventions say otherwise.
- In multiplayer, initially make this local-only for the player who enters the area. Coop synchronization can be added later if desired.

## Cache And Save State
- Keep generated data in memory. The scanner itself should live in shared Android native code, with only small D1/D2 adapters.
- Do not persist the generated component list. It is deterministic, cheap, and should be regenerated from the original level data.
- Persist found bits in save state after bumping the D1/D2 save versions.
- For older saves, initialize found bits approximately from `Automap_visited`: mark an area found if any of its segments are already visited.
- For secret-level return saves in D2, the generated list should be rebuilt when `StartNewLevelSub()` reloads the level, then found bits restored from the save.

## Shared Ownership And Duplication Control
- Put the actual scanner in `android/app/src/main/cpp/shared`, following the existing pattern used by shared input-demo and save helpers already compiled into both D1 and D2.
- Keep the scanner C-compatible unless the JSON baseline writer has a strong reason to be C++. The game-side scanner needs to be cheap to compile into D1, D2, Android, and host test targets.
- Suggested shared files:
  - `android/app/src/main/cpp/shared/secret_area_scan.h`
  - `android/app/src/main/cpp/shared/secret_area_scan.c`
  - `android/app/src/main/cpp/shared/secret_area_json.cpp` only if the baseline writer uses `nlohmann::ordered_json`
- Keep tiny game adapters in D1 and D2:
  - `d1/main/secretarea.c`
  - `d1/main/secretarea.h`
  - `d2/main/secretarea.c`
  - `d2/main/secretarea.h`
- The adapters should only translate current engine globals into a shared `secret_area_scan_view`, own one per-level runtime state object, and provide local hooks for HUD, savegame, and automap code.
- Avoid putting D1/D2 struct knowledge directly in the scanner. Prefer callbacks or copied scalar facts over including many engine headers in shared code.
- A good shared view shape:

```c
typedef struct secret_area_scan_view {
	int game_id;
	int num_segments;
	int num_walls;
	int num_objects;
	int start_segment;
	void *user;
	int (*segment_child)(void *user, int seg, int side);
	int (*reverse_side)(void *user, int seg, int child);
	int (*wall_num)(void *user, int seg, int side);
	int (*wall_type)(void *user, int wall_num);
	int (*wall_flags)(void *user, int wall_num);
	int (*wall_keys)(void *user, int wall_num);
	int (*wall_clip_flags)(void *user, int wall_num);
	int (*segment_special)(void *user, int seg);
	int (*segment_center)(void *user, int seg, int xyz[3]);
	int (*object_count)(void *user);
	int (*object_segment)(void *user, int objnum);
	int (*object_type)(void *user, int objnum);
	int (*object_id)(void *user, int objnum);
	int (*object_contains_type)(void *user, int objnum);
	int (*object_contains_id)(void *user, int objnum);
	int (*side_has_exit_trigger)(void *user, int seg, int side);
} secret_area_scan_view;
```

- The scanner result should be fixed-size and caller-owned to fit the engine style:

```c
#define SECRET_AREA_MAX_GENERATED 30

typedef struct secret_area_entry {
	int display_index;
	int entry_distance;
	int entry_seg;
	int entry_side;
	int label_pos[3];
	int segment_count;
	int segments[MAX_SEGMENTS];
	int entrance_count;
	int entrance_seg[16];
	int entrance_side[16];
	int robot_count;
	int robotmaker_count;
} secret_area_entry;

typedef struct secret_area_state {
	int enabled;
	int disabled_reason;
	int raw_candidate_count;
	int final_candidate_count;
	int found_count;
	int segment_to_secret[MAX_SEGMENTS];
	unsigned char found[SECRET_AREA_MAX_GENERATED];
	secret_area_entry secrets[SECRET_AREA_MAX_GENERATED];
} secret_area_state;
```

- If `MAX_SEGMENTS` is awkward in the shared header, pass capacities in the state initializer rather than hardcoding the engine maximum in new shared code.
- The public shared API should stay narrow:
  - `secret_area_scan_level(&view, &state, max_generated)`
  - `secret_area_mark_segment_entered(&state, seg)`
  - `secret_area_get_total(&state)`
  - `secret_area_get_found(&state)`
  - `secret_area_get_segment_secret(&state, seg)`
  - `secret_area_get_entry(&state, index)`
  - `secret_area_write_baseline_json(...)`
- Keep found tracking in shared state, but keep side effects in adapters. The shared function can return "newly found S3", then D1/D2 adapter code decides whether to call `HUD_init_message()` and whether current mode permits the popup.
- Do not create marker objects, mutate `Automap_visited`, or store generated secrets in save files. Save files should store only found bits plus enough count/version metadata to reject stale found bits after an algorithm change.
- Use the same shared scanner for runtime, introspection, baseline generation, and automap queries. This is the main guard against four subtly different definitions of "secret".

## Existing Code To Reuse
- Reuse `find_connect_side()` or the local equivalent through the adapter for reverse-side validation.
- Reuse existing segment child links, wall structs, wall animation flags, key constants, object constants, segment special constants, and trigger inspection. Do not write an independent level parser for the scanner.
- Reuse existing `LoadLevel(level_num, 0)` and mission arrays (`Level_names`, `Secret_level_names`, `Last_level`, `Last_secret_level`) in the regression dumper. This keeps the test aligned with runtime loading, including shareware, full-game, and mission-specific level lists.
- Reuse the existing headless-console initialization style instead of building a new renderer path. D2 already has `dxx-redux-d2-headless`; D1 can get an analogous narrow target for this dump tool.
- Reuse the existing PowerShell JSON comparison helpers from the input-demo scripts where practical. A small shared `Compare-JsonDiff` helper can be factored into `android/tests/json_compare_helpers.ps1` if the secret baseline test needs the same semantic diff.
- Reuse `game_introspect.cpp` only as an optional display path. The scanner should not depend on introspection, but introspection can serialize the already-generated `secret_area_state` for debugging.
- Do not reuse `Automap_visited` as found-state and do not reuse automap depth generation as reachability. Automap traversal answers "what can be drawn or has been seen"; the secret scanner needs "what can the player physically enter under conservative wall rules".
- Avoid AI or robot path helpers unless they are genuinely shared and neutral. Robot path code often encodes robot-specific door permissions and D2-only assumptions.

## Regression Baseline Test
- Add a committed JSON baseline for the bundled base games so scanner changes are visible in review.
- Suggested committed file:
  - `android/test_fixtures/secret_area_base_game_baseline.json`
- Suggested update script:
  - `android/tests/update_secret_area_baseline.ps1`
- Suggested verification script:
  - `android/tests/test_secret_area_baseline.ps1`
- The test should generate actual output under `temp/secret_area_baseline/actual.json` and compare it to the committed fixture.
- The scripts should accept:
  - `-Game d1|d2|both`, default `both`
  - `-DataDir <path>`, defaulting through the existing game-data helper conventions
  - `-BuildBeforeRun`
  - `-Update`, only on the update script
  - `-RequireData`, so local broad test runs can skip cleanly when base game assets are absent, but CI or manual regression runs can fail if assets are missing
- The baseline should include both base game campaigns:
  - D1 "First Strike", loaded by the D1 target
  - D2 "Counterstrike!", loaded by the D2 target
- Include normal levels and secret levels. Store D1/D2 secret levels with negative `level_num` to match the engine.
- Prefer a host console dumper over emulator automation for this test. It will be faster, deterministic, and easier to review.
- Suggested targets:
  - `dxx-redux-d1-secretareas`
  - `dxx-redux-d2-secretareas`
- The D2 target can share most of the existing `dxx-redux-d2-headless` setup. The D1 target should be the same kind of narrow headless host tool, not a large new game path.
- Suggested command shape:

```powershell
buildd1\main\dxx-redux-d1-secretareas.exe -hogdir C:\path\to\data -secretarea-json-out temp\secret_area_baseline\d1.json
buildd2\main\dxx-redux-d2-secretareas.exe -hogdir C:\path\to\data -secretarea-json-out temp\secret_area_baseline\d2.json
```

- The dump tool should:
  - initialize PhysFS and game data using the same approach as the existing headless runner
  - load the built-in mission for the selected game
  - loop `1..Last_level`
  - loop `-1` down to `Last_secret_level`
  - call `LoadLevel(level_num, 0)`
  - call the same `secret_area_rescan_current_level()` hook used by runtime
  - append the level's generated scanner result to the JSON document
- If normal `LoadLevel()` pulls in rendering or audio setup that is too heavy, keep the workaround in the headless runtime only. Do not add a separate HOG/RDL/RL2 parser for tests.

### Baseline JSON Shape
- Use stable integers, sorted arrays, and fixed-point coordinates. Avoid floats and wall-clock/build-machine fields.
- Suggested top-level shape:

```json
{
  "schema": 1,
  "algorithm_version": 1,
  "max_generated_secrets": 30,
  "games": [
    {
      "game": "d1",
      "campaign": "first_strike",
      "levels": []
    },
    {
      "game": "d2",
      "campaign": "counterstrike",
      "levels": []
    }
  ]
}
```

- Suggested level shape:

```json
{
  "level_num": 1,
  "level_file": "level01.rdl",
  "scanner_enabled": true,
  "disabled_reason": null,
  "raw_candidate_count": 3,
  "final_candidate_count": 2,
  "secrets": []
}
```

- Suggested secret shape:

```json
{
  "id": "S1",
  "entry_distance": 42,
  "entry_seg": 117,
  "entry_side": 3,
  "label_pos": [123456, -23456, 34567],
  "segments": [118, 119, 120],
  "entrances": [
    { "seg": 117, "side": 3, "secret_seg": 118, "wall_num": 44 }
  ],
  "robot_count": 0,
  "robotmaker_count": 0
}
```

- Do not include found bits in this baseline. Found state is save/runtime behavior, not scanner behavior.
- Do include disabled levels with `scanner_enabled=false`, `disabled_reason`, `raw_candidate_count`, and `final_candidate_count`. That makes the 30-secret sanity cutoff observable.
- The generator should sort:
  - games by `game`
  - levels by engine level order
  - secrets by display index
  - segment lists ascending
  - entrances by `seg`, then `side`, then `secret_seg`, then `wall_num`
- The test compare can be semantic JSON compare, but the committed fixture should still be pretty-printed in stable key order to make code review useful.
- On mismatch, report:
  - first changed level
  - expected and actual secret count
  - first changed secret id
  - changed field path
  - paths to expected and actual JSON
- The update script should refuse to overwrite the fixture unless both D1 and D2 data are present, unless the caller explicitly chooses `-Game d1` or `-Game d2`.
- Add a short README next to the fixture explaining that the file is generated from base game assets and should be updated only after reviewing scanner algorithm changes.

## Automap Integration
- Normal automap can remain unchanged for the first counter implementation.
- A reveal toggle should not mutate `Automap_visited`, found bits, or the normal cheat state.
- The Android touch settings tray owns this reveal toggle while automap is open. It should remain active until tapped again, but `secret_area_rescan_current_level()` resets it on level load.
- The cleanest reveal path is to let automap include segments where `secret_area_for_segment[s] != 0` when the new reveal flag is active, rendering them as special revealed edges.
- Existing automap `EF_SECRET` handling is useful for secret-door boundaries, but full secret-room reveal probably needs adding the generated component's segment edges to the edge list.
- Add a `draw_secret_area_labels()` pass after `draw_all_edges()` and before player/object markers.
- For each generated secret:
  - Draw `S%d` if the secret has been found.
  - Draw `S%d` in a different color if it has not been found and the reveal cheat is active.
  - Draw nothing for not-yet-found secrets when the reveal cheat is inactive.
- Use two distinct label colors. Example: found secrets in bright green/cyan, reveal-only secrets in amber or magenta. Keep the colors readable against the existing automap background and different from key-door colors.
- In D2, follow the existing marker label pattern around `DrawMarkerTextLabel()`: rotate a world-space anchor with `g3_rotate_point()`, project it, center the `S%d` text with `gr_get_string_size()`, then render with `gr_printf()`.
- In D1, add a small equivalent helper because D1 automap does not currently have the D2 marker text-label helper.
- Label anchors should come from generated `label_pos`, not from player marker objects. The secret system should not consume marker slots or create marker objects.
- Rebuild the automap edge list when the reveal cheat toggles, since reveal changes which secret component edges are included.

## Expected Errors
- False negatives:
  - illusion-wall secrets
  - blastable wall secrets that do not use `WCF_HIDDEN`
  - switch-opened secrets stored as `WALL_CLOSED` or trigger-only geometry
  - secrets that require trigger logic or nested-secret traversal
  - levels disabled by the `MAX_GENERATED_SECRETS` sanity cutoff
  - secrets that intentionally contain hostages or keys
- False positives:
  - hidden doors used for ordinary progression
  - hidden shortcuts between two ordinary-reachable areas
  - optional combat rooms behind hidden doors if robots are not filtered
  - mission-specific gimmicks where the level designer used hidden door clips for non-secret mechanics

## Suggested Implementation Stages
1. Add the scanner and introspection-only output: total, found, candidate segments, entry walls, and rejection reasons.
2. Add strict reachability filtering and include raw candidates, rejected candidates, final candidates, and rejection reasons in introspection.
3. Add `MAX_GENERATED_SECRETS` and disable generation for levels exceeding the cutoff.
4. Add deterministic distance ordering and include `display_index`, `entry_distance`, `entry_seg`, `entry_side`, `label_pos`, and `nested_secret` in introspection.
5. Add shared D1/D2 scanner ownership under `android/app/src/main/cpp/shared`, with thin D1/D2 adapters.
6. Add the host dump tool and write `temp/secret_area_baseline/actual.json`.
7. Review bundled D1/D2 output manually and tune filters before exposing UI.
8. Commit `android/test_fixtures/secret_area_base_game_baseline.json` and add `android/tests/test_secret_area_baseline.ps1`.
9. Add runtime found tracking, first-entry popup, and save/restore found bits.
10. Add HUD count using the sorted visible numbering.
11. Add found-secret automap labels.
12. Add automap reveal cheat labels and secret component edge reveal, keeping reveal separate from found state.

## Follow-Up Automap Polish
- [x] Hide all `S%d` automap labels unless the Reveal Secrets cheat is active.
- [x] Use red labels for unfound secrets and green labels for found secrets while Reveal Secrets is active.
- [x] Draw secret edges yellow only while Reveal Secrets is active, with found secret edges using a brighter yellow than unfound secret edges.

## Follow-Up Missed Secret Investigation
- [x] Increase found-secret reveal edge yellow another halfway step toward white.
- [x] Investigate why D2 level 1 `door45#0 (162/4/0)` is not generated as a secret candidate: segment 162 is behind ordinary progression/key-door reachability, so the current scanner never sees it as an ordinary-reachable entrance segment.
- [x] Decide whether the missed D2 level 1 case represents a broader false-negative pattern: yes, secrets behind key-door progression are currently missed; a broad experiment allowing all key-door traversal found the target but increased D2 base-game secrets from 175 to 333, so this needs a narrower heuristic before landing.
- [x] Re-run scanner/automap regression checks after changes.

## Follow-Up Progression-Gated Secrets
- [x] Exclude generated secret areas with no powerup items. This removes thief-only and empty hidden pockets from the player-facing count.
- [x] Treat key/progression doors as reachable for secret generation, while still keeping hidden-door and triggered-secret boundaries out of the normal traversal graph.
- [x] Keep guidebot selection live-reachability-gated. The generated list may include later progression secrets, but `find secret` still chooses only an unfound secret entrance that the guidebot can currently reach via `create_bfs_list()` and later validates with `create_path_to_segment()`.
- [x] Regenerate and review the base-game regression JSON after the scanner update. New base-game totals are D1 172 and D2 265, with no zero-item generated secrets. D2 level 1 now includes the `162/4` hidden-door room as `S9`.

## Follow-Up Guidebot Secret Cycling
- [x] When `find secret` is run while the guidebot is already targeting a secret, skip the current target and choose the next nearest reachable unfound secret.
- [x] If no other reachable unfound secret exists, keep the current reachable target instead of clearing the task.
- [x] Verify with formatting, a focused D2 Windows build, and Android native build.

## Follow-Up Guide Wheel Reveal Gating
- [x] Hide the Guide wheel `Secret` slice unless the Reveal Secrets cheat is active.
- [x] Reflow the remaining Guide wheel slices when `Secret` is hidden, matching the old slice positions.
- [x] Verify with scoped Kotlin quality checks and Android build.

## Follow-Up Marginal Trigger-Revealed Secrets Study
- [x] Examine D2 level 3 current `S10` as the exemplar for hidden walls that are revealed by required progression. The current generated `S10` is the cloak pocket at `124:3 -> 118`, wall `61`, and no trigger links to either the entry side or reverse side. The blue-key-coincident trigger evidence points instead at current `S6`/`S7`: blue key segment `200` has trigger source walls `96` and `135`, which open candidate entries `89:4` and `199:4`.
- [x] Compare with the D2 level 2 reactor-path cases where required triggers reveal the last generated secrets. Current D2 level 2 `S10` is a clean example: entry `57:1 -> 131` is opened by triggers `4`, `5`, and `6` sourced from ordinary illusion trigger walls on segments `54`, `53`, and `57`. `S8` is also trigger-opened from overlay wall `66` at `302:3`; `S9` has no direct trigger link.
- [x] Propose a conservative filter that removes mandatory/revealed-by-progression pockets without dropping genuine optional switch secrets.
  - Best first filter: mark a triggered candidate as marginal if at least one opener trigger source segment contains a key powerup, or the trigger source side itself is part of the ordinary progression traversal rather than behind another secret boundary. This catches the D2 level 3 blue-key-triggered `S6`/`S7` class and should not affect untriggered hidden-door pockets like current `S10`.
  - Safer extension for D2 level 2 reactor-path cases: add a scan callback that exposes trigger source walls and all linked target sides, then suppress candidates when the same opener trigger is reachable in the ordinary graph and has no evidence of being an optional secret switch. Ordinary, pass-through illusion trigger surfaces on the main path are a stronger mandatory signal than shootable overlay switches.
  - Avoid a broad "triggered from reachable segment" filter. The scanner intentionally added optional shootable-switch secrets using reachable trigger sources, so that broad rule would delete many real secrets.
  - Keep the filter D2-only at first, because D1 trigger data is flag-based and the current added triggered-secret heuristic only applies to D2.

## Follow-Up Marginal Trigger Filtering Implementation
- [x] Extend the shared scan view with D2-backed trigger source callbacks for opener segment, side, wall, and marginal-source classification.
- [x] Filter candidates whose entrances are only accepted through marginal trigger sources. The landed heuristic treats key-segment trigger sources as marginal, and treats pass-through `WALL_OPEN` sources as marginal only when at least two reachable pass-through openers target the same entrance.
- [x] Regenerate and inspect the base-game secret JSON. D1 remains 172 secrets; D2 drops from 265 to 252. D2 level 2 old `S10` is removed, D2 level 3 old `S6`/`S7` are removed, and the cloak pocket formerly `S10` remains as the new `S8`.
- [x] Run scoped code quality and the secret-area regression command after updating the baseline.

## Follow-Up Required-Route Trigger Filtering Study
- [x] Replace or augment the current marginal trigger heuristic with a deterministic required-route mask.
- [x] Build route masks from spawn to each required key, each key to matching locked doors, and spawn to the reactor with locked doors passable.
- [x] Mark a trigger source as progression pass-through only when the source side/edge is intersected by one of those route masks.
- [x] Prefer a scanner-local deterministic segment path helper over calling live AI path APIs directly, because `create_path_points()` depends on object state, random side order, and global `Point_segs`.

### Study Notes
- The live pathing helpers are useful references, but should not be called by secret generation at level load:
  - D1/D2 `create_path_points()` traverses with `WALL_IS_DOORWAY()` or `ai_door_is_openable()`, can randomize side order, consumes global path buffers, and uses object/player state.
  - D2 `create_bfs_list()` is also live-AI-oriented. It asks `segment_is_reachable()`, which delegates to `ai_door_is_openable(NULL, ...)`, so the answer can depend on current door/key/player state.
  - The scanner should remain deterministic and asset-only, so the route study should be implemented in `android/app/src/main/cpp/shared/secret_area_scan.c` as a small segment BFS and path reconstruction helper.
- The filter should operate on edges, not just segments:
  - D2 trigger sources are wall sides. The source is effectively `(Walls[source_wall].segnum, Walls[source_wall].sidenum)`.
  - A source segment that contains a key is too broad. It can mark optional key-room switches as mandatory even when the required route never crosses that side.
  - The route mask should therefore be `required_route_side[seg][side]`, with the reverse side marked too when the route crosses into a child segment.
- Required-route families requested for this first pass:
  - Spawn to each key segment, but only for key colors that have at least one matching keyed door in the level. A key without a matching locked door is not required progress.
  - Each key color to its matching door side. If multiple same-color keys exist, choose the shortest successful path from any same-color key to each same-color keyed door, then explicitly mark the keyed door side.
  - Spawn to reactor, with all keyed doors considered passable. This catches mandatory late-game travel through blue/red/gold doors without needing to solve full key order.
- Reactor target discovery:
  - Prefer an `OBJ_CNTRLCEN` object segment, matching how guidebot/reactor logic reasons about the control center.
  - If no object is present, fall back to segment special `SEGMENT_IS_CONTROLCEN` through the scan view if that is already available.
  - If no reactor/control-center target is present, skip the reactor route mask rather than disabling secret generation. Boss/variant levels may not have a normal reactor object.
- Key and door discovery:
  - Key segments come from live level objects with `OBJ_POWERUP` ids `POW_KEY_BLUE`, `POW_KEY_RED`, and `POW_KEY_GOLD`, ignoring dead objects.
  - Keyed door sides come from walls whose key mask includes `KEY_BLUE`, `KEY_RED`, or `KEY_GOLD`. Use bit tests, not strict equality, because these constants are flag values even though ordinary levels usually store one key.
  - Door route marking should mark the wall side itself and, if the reverse side has a wall, the reverse wall side too. Otherwise a trigger mounted on either face can be recognized as route-intersected.

### Route Traversal Policy
- Add a dedicated route traversal predicate instead of reusing live AI helpers:
  - Require a valid child and a valid reverse side.
  - Do not traverse a side that the scanner already considers a secret boundary. A required-route search that walks through secret doors would make optional secrets look mandatory.
  - No wall is passable.
  - `WALL_OPEN` is passable.
  - `WALL_ILLUSION` follows the current scanner behavior: passable when it is not `WALL_ILLUSION_OFF`.
  - `WALL_DOOR` is passable when `wall_keys == KEY_NONE`, or when `(wall_keys & allowed_key_mask) != 0`.
  - `WALL_BLASTABLE` should initially follow the existing scanner's ordinary traversal behavior for consistency. This can over-accept ordinary shootable walls as routeable, but changing that now would also alter the existing reachable-secret graph.
  - Exit triggers are not passable.
- Use three allowed-key modes:
  - Spawn to key: no keys allowed.
  - Key to matching door: the matching key color allowed.
  - Spawn to reactor: blue, red, and gold allowed.

### Implementation Detail
- Add scanner-local arrays in `secret_area_scan.c`:
  - `required_route_side[SECRET_AREA_MAX_SEGMENTS][SECRET_AREA_MAX_SIDES]`
  - `route_parent_seg[SECRET_AREA_MAX_SEGMENTS]`
  - `route_parent_side[SECRET_AREA_MAX_SEGMENTS]`
  - `route_distance[SECRET_AREA_MAX_SEGMENTS]`
  - `route_queue[SECRET_AREA_MAX_SEGMENTS]`
- Add `find_required_route_and_mark(view, start_seg, goal_seg, allowed_key_mask)`:
  - Run deterministic BFS with side order `0..SECRET_AREA_MAX_SIDES-1`.
  - Store the parent segment and parent side used to enter each child.
  - Stop at the first goal hit for deterministic shortest-path behavior.
  - Walk backward from goal to start and mark each parent side in `required_route_side`.
  - Mark the reverse side by calling `view->reverse_side(user, parent, child)`.
- Add `mark_required_routes(view)` after `progression_distance` has been built, because the secret-boundary check depends on progression reachability:
  - Clear the route mask.
  - Collect key segments by color.
  - Collect keyed door sides by color.
  - For each key color with at least one key and at least one matching door, mark spawn-to-key paths.
  - For each matching keyed door, try all same-color key segments and mark only the shortest successful key-to-door path. Then mark the keyed door side itself.
  - Find the reactor segment and mark spawn-to-reactor with all keys allowed.
- Replace the current broad marginal checks with side-intersection checks:
  - `source_wall_is_progression_pass_through()` should become a simple lookup of `required_route_side[source_seg][source_side]`, after validating the trigger source wall/side.
  - Remove or stop using the segment-level `segment_contains_key_powerup()` criterion for marginal trigger filtering. The route mask is the conservative replacement.
  - Keep the existing "all reachable trigger openers are marginal" candidate suppression shape. The difference is that a marginal opener now means "this exact trigger source side lies on a required route".

### Expected Effects
- D2 level 2 reactor-path hidden walls should be filtered only when their opening triggers are on the spawn-to-reactor route.
- D2 level 3 blue-key-adjacent cases should be filtered only if the trigger side is crossed by spawn-to-key or key-to-door routing, not simply because the source segment contains the blue key.
- Optional shootable-switch secrets should survive when the switch is in a reachable but non-required side pocket.
- Progression-gated secrets remain eligible as secrets. The route mask is only used to reject secret candidates opened by mandatory pass-through triggers; it does not require all generated secrets to be reachable before collecting every key.

### Validation Plan
- Run the base-game secret regression with baseline output to a temp directory first:
  - `.\android\tests\test_secret_area_baseline.ps1 -Game both -BuildBeforeRun -RequireAssets -OutputDir temp\secret_area_required_route_probe`
- Inspect D2 level deltas against the current baseline:
  - D2 level 2: confirm reactor-path mandatory trigger pockets are removed.
  - D2 level 3: confirm the exemplar blue-key/blue-route case is removed, while the cloak/optional hidden pocket remains.
  - D2 level 1: confirm the known shootable-switch secret near `rock313 (24/4/0)` and `door45#0 (162/4/0)` still appear if they contain items.
- in general, the total secret counts should go down by maybe 1-2 per level, if that. it shouldn't be half as many, based on my play testing
- If the deltas look right, update the committed baseline:
  - `.\android\tests\test_secret_area_baseline.ps1 -Game both -BuildBeforeRun -RequireAssets -UpdateBaseline -OutputDir temp\secret_area_required_route_update`
- Run scoped code quality:
  - `.\android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/cpp/shared/secret_area_scan.c', 'android/app/src/main/cpp/shared/secret_area_scan.h', 'android/app/src/main/cpp/shared/secret_area_game_adapter.c', 'android/test_fixtures/secret_area_base_game_baseline.json', 'android/ai tool plans/mixed batch, umbrella/study_secret_area_autolabel_20260606.md')`

