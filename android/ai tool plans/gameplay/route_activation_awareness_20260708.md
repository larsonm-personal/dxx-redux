# Route Activation Awareness Study

## Goal
Study how to add route-step awareness for shootable switches, touch/fly-through triggers, hidden doors, and pass-through wall triggers so both metadata UI and Android guidebot guidance can describe and act on the player's next useful action.

## Checklist
- [x] Create this plan before further work.
- [x] Inspect the shared level metadata route analyzer and serialized route fields.
- [x] Inspect Kotlin metadata route models and display text.
- [x] Inspect guidebot route-step selection, satisfaction checks, and firing-position behavior.
- [x] Identify the smallest data model additions needed for route activation semantics.
- [x] Propose implementation and validation phases.

## Implementation Checklist
- [x] Add native route activation kind fields and classification.
- [x] Serialize and preserve activation kind through JNI/headless/Kotlin/automation JSON.
- [x] Update metadata path display text to use player-action wording.
- [x] Update guidebot route guidance and messages to use activation kind.
- [x] Extend focused C route tests.
- [x] Run focused validation and record results.

## Findings
- Current route steps know objective type, route waypoint, source wall, trigger id/type, key index, distance, and opened links. They do not carry a player action such as shoot, fly through, touch, open hidden door, or destroy.
- `level_metadata_scan.c` already has most of the hard pathing work. `metadata_route_try_trigger_firing_path()` first looks for a direct route to the trigger source side, then falls back to a visible route via `metadata_route_find_visible_path()`. `metadata_route_fire_trigger()` records the chosen route segment in the step. If the chosen segment differs from the source wall's segment, it clears `source_side` to `-1`.
- Existing generated metadata therefore already has a useful signal: trigger steps with `side == -1` are visible-from-here cases. A sweep of `game_data/mission_files/*.json` found 170 such trigger steps (`open_wall` 152, `open_door` 18). Most trigger steps still have a side, so this is not enough to classify all shootable switches.
- Engine activation paths split cleanly:
  - Flying through/touching a trigger side calls `check_trigger(..., shot=0)` from player segment-crossing logic in `object.c`.
  - Shooting a wall trigger calls `check_effect_blowup()` from weapon-wall collision in `collide.c`; only if that blow-up succeeds does it call `check_trigger(..., shot=1)`.
  - Hidden doors are not trigger steps. The scanner detects `WALL_DOOR` plus hidden wallclip (`WCF_HIDDEN`), and the game opens them through normal door wall-hit processing.
- A static "shootable switch" predicate should not call `check_effect_blowup()` because that depends on the exact UV impact point and mutates wall textures/effects. The safe approximation is to expose a read-only callback for "source side has a destroyable overlay/effect texture" and/or source wall type.
- D2 wall type gives another strong clue:
  - `WALL_OPEN` is flyable and can naturally represent a fly-through trigger source.
  - `WALL_OVERLAY` exists specifically as an overlay over a solid side for triggers, and is a good candidate for shootable switch classification when paired with a destroyable overlay/effect.
- Kotlin metadata is currently a thin route-step pass-through in `LevelMetadata.kt`; `SetupSections.kt` formats labels directly from `kind`, `trigger_type`, `seg`, `side`, `wall`, and `opens`. Adding optional JSON fields can stay backward compatible through `optString`/`optInt`.
- Android guidebot routing already has the needed modes: `reach_objective`, `reach_hidden_door`, `reach_firing_position`, and `nearest_progress_point`. Today, `escort_route_step_guidance_mode()` maps every trigger to `reach_firing_position`.
- Guidebot live guidance already recomputes the nearest reachable segment with line of sight to the route trigger wall in `escort_route_refresh_guidance_target()`. That means the metadata does not need to cache one global firing segment. It needs to say that the step is shootable and identify the objective/source wall.
- Current guidebot messages use the route label, for example `Finding NEXT: Open wall trigger 13`. To get player-facing help, either the label or a new route action phrase needs to become `shoot switch`, `fly through trigger`, `open hidden wall door`, etc.

## Suggested Data Model
- Add an integer `activation_kind` to `level_metadata_route_step`, with a string serializer. Suggested values:
  - `none`
  - `pickup_key`
  - `shoot_switch`
  - `fly_through_trigger`
  - `activate_switch`
  - `open_hidden_door`
  - `destroy_reactor`
  - `destroy_boss`
  - `enter_exit`
- Keep existing `seg`, `side`, and `wall` semantics for now:
  - For shootable triggers, `seg` is the route/guidance segment chosen by the static scan and `wall` is the source switch wall. If `side == -1`, the static scan chose a visible firing segment rather than the source side.
  - At runtime the guidebot can continue deriving the exact source segment/side from `Walls[wall]`, as it already does.
- Consider adding `source_wall_type` or `activation_detail` later if UI/debugging needs to explain why a step was classified as shootable or fly-through.

## Classification Sketch
- Keys: `pickup_key`.
- Hidden doors: `open_hidden_door`.
- Reactor: `destroy_reactor`.
- Boss: `destroy_boss`.
- Exit: `enter_exit`.
- Trigger steps:
  - If the source side is statically shootable, classify as `shoot_switch`.
  - Else if the source wall type is `WALL_OPEN`, classify as `fly_through_trigger`.
  - Else classify conservatively as `activate_switch`.

## Implementation Phases
1. Native model and tests:
   - Add the `activation_kind` enum/function to `level_metadata_scan.h/.c`.
   - Add a read-only scan-view callback for source-side shootability.
   - Extend C unit tests:
     - direct `WALL_OPEN` trigger becomes `fly_through_trigger`;
     - destroyable trigger visible from another segment becomes `shoot_switch`;
     - destroyable trigger in the same segment becomes `shoot_switch`;
     - hidden door becomes `open_hidden_door`.
2. Serialization and Kotlin:
   - Serialize `activation_kind` in JNI and headless metadata output.
   - Preserve it through `LevelMetadata.kt` and `LauncherScriptExecutor.kt`.
   - Update `SetupSections.kt` wording to lead with player actions, with the opened links as the "opens/makes passable" detail.
3. Guidebot:
   - Use `activation_kind` in `escort_route_step_guidance_mode()` instead of mapping all triggers to `reach_firing_position`.
   - Only run nearest-visible-wall retargeting for `shoot_switch`.
   - Adjust route goal labels/messages so shootable steps read like "Go here and shoot switch 13" instead of "Open wall trigger 13".
   - Expose `activation_kind` in route introspection.
4. Validation:
   - Run D1 and D2 `test_level_metadata_scan`.
   - Run Kotlin compile or assemble.
   - Run focused guidebot route scripts for KCXF2 and Obsidian.
   - Regenerate focused mission metadata JSON for KCXF2, Obsidian, Counterstrike, and Descent before considering broader regeneration.

## Implementation Notes
- Added `level_metadata_route_activation_kind` to the shared route step model and serialize it as `activation_kind`.
- Added a read-only adapter predicate that mirrors the engine's shootable-effect check: a side is shootable if its overlay texture has a valid effect with a destination bitmap that is not already one-shot, or in D2 if the texture has a destroyed replacement.
- Trigger labels now start with action-oriented wording:
  - `Shoot switch trigger N`
  - `Fly-through trigger N`
  - `Activate switch trigger N`
- Kotlin route summaries prefer the activation action when present, so old metadata still falls back to existing labels.
- Guidebot route analysis and route goals now carry `activation_kind`.
- Guidebot uses `reach_firing_position` only for `shoot_switch` triggers; `fly_through_trigger` and `activate_switch` route to the objective side.
- Guidebot nearest-visible-wall retargeting now runs only for `shoot_switch`.
- Guidebot route message for a shoot switch is `Finding NEXT: go here and shoot this switch`.

## Validation Results
- `.\android\run-code-quality.ps1 -Fix -Paths ...` scoped to touched files passed.
- Rebuilt D1 and D2 `test_level_metadata_scan` targets with MSVC/CMake.
- `.\buildd1\maths\test_level_metadata_scan.exe` passed.
- `.\buildd2\maths\test_level_metadata_scan.exe` passed.
- `.\android\gradlew.bat :app:assembleDebug --no-daemon --console=plain` passed after the final shared C change.
- Did not run the KCXF2/Obsidian emulator guidebot scripts in this pass; the native and Android compile validations cover the new classifier and wiring.

## Open Questions
- Whether every `WALL_OVERLAY` trigger source should be considered shootable, or only those with a destroyable overlay/effect texture. The implementation uses the safer destroyable overlay/effect predicate.
- Whether `activate_switch` is too generic for odd source walls. It is the current fallback when a trigger source is neither shootable nor a `WALL_OPEN` fly-through source.
- D1 trigger metadata has older flag-style trigger semantics. D2 guidebot is the immediate runtime consumer, so the first implementation can make D1 best-effort and keep unknown cases as `activate_switch`.
