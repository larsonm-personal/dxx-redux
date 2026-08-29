# Obsidian level 13 blue-key nested switch investigation

## Goal

Determine why GuideBot routing mishandles the blue key behind an electrified shoot-through grate, where the required switch is behind both the grate and a shoot-open hidden door, and propose a general fix that preserves other route behavior.

## Plan

- [x] Correlate the before-switch, after-switch, and blue-key pickup states in the supplied log
- [x] Inspect level-13 metadata for the key, switch, hidden door, grate, and trigger links
- [x] Trace how the planner represents nested shootable prerequisites and where the dependency chain is lost
- [x] Design the smallest general correction and identify regression coverage

## Implementation

- [x] Add conditional shot tracing that reports the first actionable wall without weakening ordinary visibility
- [x] Teach trigger planning to resolve an unlock trigger and shoot-open hidden door before retrying the blocked switch
- [x] Add native positive and negative route-planner coverage
- [x] Build, run focused tests, regenerate Obsidian level 13, and review the resulting objective sequence and cost

## Findings

- The canonical level-13 route knows that trigger 13 opens wall 57 and exposes the blue key in segment 343, but emits trigger 13 as `unresolved_trigger` at the wall-57 frontier in segment 342.
- Trigger 13's source is shootable overlay wall 60 on segment 346 side 0. The supplied log shows GuideBot reaching segment 342, parking there until the player activates trigger 13, advancing to the blue key, and then advancing beyond the key normally.
- A traced host analysis identifies the actual shot blocker. Rays from segment 342 through the shoot-through wall 57 toward switch wall 60 first hit wall 55 on segment 343 side 0. Wall 55 is a keyless hidden door (`WALL_DOOR`, `WALL_DOOR_LOCKED`, hidden wall clip) separating segments 343 and 346.
- Trigger 12 is a shoot switch on wall 59 in segment 341. It is an `unlock_door` trigger linked to wall 55. Therefore the real action chain is trigger 12, shoot-open hidden door 55, trigger 13, blue key.
- Movement dependencies already recurse through switch-controlled and hidden doors. Shot visibility does not: its FVI callback returns only pass/fail and only permits a live, already-unlocked keyless door. It cannot report wall 55 as the actionable blocker or evaluate visibility against the planner's hypothetical fired-trigger/opened-door state.
- The general fix should add a structured shot trace that preserves the first blocking wall and evaluates door passability against `route_progress_state`. Trigger planning can then recurse through an opener trigger, select a reachable firing pose for the newly unlocked hidden door, append the existing `open_hidden_door` semantic action, mark that wall hypothetically open, and retry the original switch. Dependency-loop and depth guards must remain in force.
- Keep projectile traversal distinct from route traversal. A shot may cross only geometry that the engine says is projectile-transparent, such as a transparent grate, or an actionable wall that the same firing sequence can actually shoot open or destroy. It must not pass through an ordinary opaque wall or a still-closed keyed door.
- A keyed door is nevertheless a valid route prerequisite when the planner's ordered progress state already contains the key, or contains an earlier reachable key objective. In that case the planner may route the player and GuideBot through the door and search for firing positions beyond it. The existing player-owned-key handoff behavior remains applicable at runtime.
- Conditional shot acceptance is limited to a complete validated chain: acquire an available required key, traverse its door, activate an opener or unlock trigger, shoot open or destroy an actionable wall, and cross projectile-transparent surfaces as needed. Reactor and boss exclusions remain separate policy and are not relaxed by this rule.
- Conditional visibility accepts exactly one unresolved shoot-open hidden door or blastable wall per firing ray. This covers a concrete two-shot action while rejecting an apparent line of fire that crosses several still-closed walls. Ordinary transparent-surface and already-unlocked keyless-door handling remains unchanged.
- Remote wall actions retain the physical wall and opened-link identities but use the firing segment, activation position, and no side. This matches the existing representation of remote switch shots and avoids inventing a segment/side pair that does not exist.
- Runtime guidance detects a hidden-door action whose physical wall is in another segment and uses its compiled firing position. Adjacent hidden doors retain the classic guide-to-door behavior.
- Focused coverage should reproduce wall 57 -> wall 55 -> wall 60 and assert the canonical order `trigger 12`, `hidden door 55`, `trigger 13`, `blue key`. Controls should cover a transparent grate, a shoot-open wall, a keyed door with an earlier planned key, the same keyed door without an obtainable key, an opaque grate, and an unopenable locked door.

## Validation

- Reproduced the checked-in unresolved route with the current D2 headless metadata analyzer.
- Ran a focused `DXX_SECRET_AREA_DUMP_TRACE=1` host analysis for Obsidian level 13. It completed successfully and identified wall 55 as the blocker for the locally relevant wall-60 firing samples.
- The native route snapshot test now covers the positive unlock -> hidden door -> switch chain and rejects an otherwise identical opaque blocker.
- A focused host regeneration for `Obsidian.zip` reports level 13 as `ok` with the canonical sequence `trigger 12`, `hidden door 55`, `trigger 13`, `blue key`. Travel distance changes from 9559.8 to 9751.5 because the formerly unresolved dependency now has two explicit actions.
- The full generated archive output is preserved under `android/temp/mission_zip_host_metadata/20260828_142816`. Only the related level-13 delta was retained in the checked-in mission metadata; unrelated pre-existing level-2 and music-inventory drift was excluded.
- Scoped code quality passed for all changed planner, scan, test, plan, and metadata files.
- Full Windows D1 and D2 builds passed. The D2 CTest suite passed all 44 tests, including route snapshots, route certification, argument defaults, slowdown detection, and the GuideBot calculation benchmark. The D1 build does not register CTest tests.
- The Android debug APK built successfully with JDK 21 for arm64-v8a, armeabi-v7a, and x86_64. Only pre-existing warnings in D2 AI and sound code were emitted.
- The benchmark remains bounded: the worst sliced case was the unreachable-switch frontier at 26 slices and 1,474 callbacks in its largest slice; compiled action selection remained effectively constant-time.
