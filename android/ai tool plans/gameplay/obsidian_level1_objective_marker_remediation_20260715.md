# Obsidian Level 1 Objective Marker Remediation

## Goal

Correct bogus or misleading Obsidian level 1 route objectives and make coincident automap objective markers readable without introducing mission-specific behavior.

## Plan

- [x] Extract the exact level 1 route steps, trigger sources, opened links, and marker positions from reviewed and fresh metadata.
- [x] Determine whether the `fly through` step is a false planner dependency, an incorrect activation classification, or only a bad automap projection.
- [x] Audit coincident objective markers and decide whether equivalent steps should be deduplicated or distinct steps should share a combined label.
- [x] Fix the shared planner or shared automap overlay at the general rule responsible for each defect.
- [x] Add focused native coverage for route semantics and marker grouping, plus an Obsidian live integration assertion where practical.
- [x] Run scoped quality, native tests, Windows/Android builds, focused metadata generation, and relevant corpus regression checks.
- [x] Record the result and remaining limitations.

## Boundaries

- Do not add Obsidian-specific segment, wall, or trigger exceptions.
- Do not change classic GuideBot physical movement, aiming, firing, flares, or simulation RNG.
- Preserve genuinely distinct coincident objectives, but render them as one readable combined marker such as `5&6`.

## Result

- Fresh shared-planner metadata identifies trigger 8 at segment 180 side 5 as a real segment crossing that opens wall 114. Its opening meets the player navigator clearance requirement. The route remains `ok`; this objective was not removed as mission-specific data.
- Non-shootable trigger sources on faces without a child segment are now rejected. Closed doors between real segments remain valid pass-through sources.
- Completed planner paths now record opening triggers crossed naturally, preventing later redundant trigger objectives.
- The Android-only `GO` and `X` GuideBot labels were removed. Shoot and pass-through guidance uses the numbered from/to markers and connector line.
- Projected objective labels that overlap are grouped into one readable label. The focused level 1 emulator test observes `3&4&5` in the default automap view.
- Added merged-label diagnostics to introspection and `test_obsidian_level1_objective_markers.json5` coverage.
- Native route snapshot and metadata scan tests pass for D1 and D2. The Android debug build and focused emulator test pass. Fresh host analysis reports `ok` for all 18 Obsidian levels.

## Limitation

- Label grouping is view-dependent by design. Combined text changes as automap rotation and zoom change which projected labels overlap.

## GuideBot Crossing Follow-up

- [x] Confirm that targeting the child segment does not explain the large canonical/live marker separation; discard that approach.
- [x] Compare the numbered objective source with the live GuideBot route source for the same trigger.
- [x] Bind explicit fly-through and pass-through GuideBot goals to the selected route step source instead of a substituted path-terminal fallback.
- [x] Add introspection-driven coverage that asserts trigger, source segment, side, and GuideBot guidance destination agree.
- [x] Re-run native tests, D1/D2 host builds, Android build, focused Obsidian metadata, and emulator coverage.

The live Obsidian level 3 regression now selects fly-through trigger 6 and reports objective, GuideBot target, and guidance segment 379 side 2 consistently. Its classic GuideBot path parity probe passes. The level 1 regression also confirms that naturally crossing trigger 8 removes the redundant crossing objective and advances the GuideBot to the reactor.
