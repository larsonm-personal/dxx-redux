# Guidebot Nearest-Point Navigation Plan

## Goal
- Add an Android advanced gameplay option: "Guidebot navigate to nearest point"
- When enabled, a guidebot goal with no normal escort path should route the guidebot toward the nearest reachable point along the intended progress path instead of refusing to move
- Study Castaway level 1 (`skorrpyo.rl2`) to confirm whether an exit-adjacent disappearing wall blocks normal guidebot routing

## Phase 1: Study
- [done] Locate launcher advanced gameplay preference plumbing
- [done] Locate D2 escort path creation and failure behavior
- [done] Inspect Castaway level 1 data for exit-side wall, trigger, and route metadata behavior

## Study Notes
- `EnginePreferencesPage.kt` already has a `Gameplay` section with persisted `SharedPreferences` toggles
- Metadata travel routing treats trigger-opened walls as passable through `triggered_side_opener_count`
- Guidebot pathing uses `create_path_points`, which only traverses currently flyable/openable sides from `WALL_IS_DOORWAY` or `ai_door_is_openable`
- `escort_create_path_to_goal` rejects a partial guidebot path when the final path segment does not equal the requested goal segment, then switches to "can't reach" / scram behavior
- Castaway metadata for level 1 reports travel status ok: 5/5 travel targets reached, 3786.876 route units, 1 key detour
- Added temporary decoder at `android/temp/analyze_castaway_l1.ps1` and decoded `skorrpyo.rl2` from `android/temp/castaway_level_analysis`
- Binary sanity checks:
  - compiled mine version 0, 1912 vertices, 669 segments
  - game version 32, 114 objects, 156 walls, 27 triggers
  - wall records match their claimed segment sides: 0 bad wall references
  - player object starts in segment 203
  - likely guidebot object is robot id 33 in segment 649
  - exit segment is 647, side 2, with no wall on the outside exit side
- Route findings:
  - From player start segment 203 to exit segment 647, no-key guide/progress route is unavailable
  - With all keys modeled as acquired, guide route and progress route both reach the exit in 75 edges
  - From likely guidebot segment 649 to exit segment 647, no-key guide route is unavailable, but all-key guide/progress routes reach the exit in 74 edges
  - For the player/guidebot-start-to-exit route, the first all-key progress edge blocked in the no-key state is a blue-key door at segment 288 side 5 into segment 296, wall 64, type door, keys 2
  - The final exit tunnel to segment 647 has no wall-bearing edges: 630 -> 631 -> ... -> 646 -> 647
- Reactor-to-exit correction:
  - Reactor/control-center object is type 9, object 8, in segment 582
  - Raw child connectivity from reactor segment 582 to exit segment 647 exists in 55 edges when walls are ignored
  - A naive one-sided wall approximation incorrectly reports no reactor-to-exit route because it sees wall 144 at 517:5 -> 503 and wall 6 at 1:4 -> 10 as untriggered `WALL_CLOSED`
  - D2 trigger wall changes update both sides of a connection, so the opposite sides matter: wall 145 at 503:4 is opened by triggers 2/24, and wall 7 at 10:5 is opened by triggers 2/25
  - With paired trigger-opened walls modeled as passable, reactor-to-exit reaches the exit in 55 edges when all keys are available
  - With paired trigger-opened walls modeled but no keys, the first reactor-to-exit blocker is the red/keyed door at 127:2 -> 2, wall 33, keys 4
  - The reactor-to-exit path therefore is not physically impossible in the level data; it depends on trigger/key state that normal guidebot pathing may not model
- Castaway level 1 contains multiple trigger-opened disappearing/closed walls, including wall 153 at 667:0 controlled by trigger 24/trigger 25, and wall 122 at 107:2 controlled by trigger 17
- Working conclusion: Castaway L1 still supports the "navigate to nearest point" feature, but the static analysis needs paired wall-trigger modeling to match the mine. The reactor-to-exit path is possible with all keys and trigger-opened wall pairs, while normal guidebot pathing can still refuse when its live `WALL_IS_DOORWAY` / `ai_door_is_openable` view cannot reach the final goal segment. The nearest-point fallback should therefore be goal/path based rather than special-casing one specific wall

## Phase 2: Implementation Design
- [done] Add a persisted launcher preference under Game Preferences > Gameplay
- [done] Bridge the preference into native runtime alongside existing Android helper options
- [done] Add D2 escort fallback logic that preserves default behavior when disabled
- [done] Avoid changing D1 except for shared Android preference plumbing where needed

## Refined Generic Best-Effort Design
- The initial implementation preserves a partial path returned by `create_path_to_segment`, but that path is whatever frontier the existing BFS happened to discover
- [done] Improve this by choosing a deliberate reachable fallback segment when the requested goal segment cannot be reached normally
- [done] Keep default behavior unchanged when the Android preference is disabled
- When enabled and a concrete goal segment exists:
  - [done] Build a live reachable set from the guidebot's current segment using the same traversal rule as normal guidebot pathing: `WALL_IS_DOORWAY(... ) & WID_FLY_FLAG` or `ai_door_is_openable(...)`
  - [done] Build an optimistic reverse distance map from the goal segment using structural mine connectivity plus passable-later edges:
    - no wall, open wall, illusion, and currently flyable sides
    - doors the player can plausibly open with current key state
    - `WALL_CLOSED` or wall-changing sides whose own wall or opposite-side wall is targeted by an open-wall or illusory-wall trigger
  - [done] Select the reachable segment with the smallest optimistic distance to the goal, with tie-breakers that prefer longer progress from the guidebot start and fewer special-wall assumptions
  - [done] Re-run `create_path_to_segment` to that selected reachable segment, then polish the path normally
  - [done] Only use the fallback if it produces a path ending at the selected segment and moves at least one segment away from the guidebot
- This makes the behavior generic across missions: the guidebot still follows only live-safe movement, but the target is chosen from a broader map of likely future/progress connectivity
- Message behavior when fallback activates:
  - [done] For normal goals, say `Can't reach %s, navigating as close as possible.`
  - [done] For the exit, say `Can't reach exit, navigating as close as possible.`
  - [done] For secrets with no known secret target, keep the existing `Can't reach any secrets.` behavior because there is no concrete goal segment
- Logging should distinguish:
  - [done] normal full path
  - [done] unreachable with no fallback candidate
  - [done] nearest-point fallback candidate selected
  - [done] fallback path creation failed

## Phase 3: Verification
- [not-run] Add or extend an automated script that launches Castaway level 1, releases/spawns guidebot if needed, asks for exit navigation, and introspects guidebot route state
- [done] Run scoped code quality on changed files with `android/run-code-quality.ps1 -Fix -Paths ...` after the refined fallback implementation
- [done] Build Android debug APK with `JAVA_HOME=C:\local\jdk-21` and `./gradlew.bat :app:assembleDebug` after the refined fallback implementation
- [done] Run `git diff --check` on changed implementation files
