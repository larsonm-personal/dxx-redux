# Mission Route Trigger Dependency Audit

## Objective

Characterize the remaining trigger dependency and objective-order failures, then improve playable-level coverage through general planner state search or traversal semantics without mission-specific rules or status fallbacks.

## Guardrails

- Start from the accepted 1,114 `ok`, 83 `partial`, and 77 `failed` corpus.
- Model only actions and side effects that gameplay actually performs.
- Do not prefer or preserve a route based on its corpus status.
- Do not add mission, level, trigger, wall, segment, or coordinate exceptions.
- Review all changed objective sequences and every status regression.
- Keep canonical analysis bounded enough for Android per-build caching and host corpus generation.

## Phase 1: characterize dependency failures

- [x] Inventory all 16 explicit trigger loops and related order-sensitive partial routes.
- [x] Trace planner state, chosen trigger source, blocked target, and alternate reachable actions for representative D2 cases.
- [x] Separate self-dependent source modeling from missing proactive trigger traversal and unrelated key-unreachable failures.

## Phase 2: design and focused coverage

- [x] Prototype bounded backtracking and deterministic reachability-expansion recovery.
- [x] Measure representative route gains and runtime costs.
- [x] Reject both prototypes: one Levigen level converted, while representative mission runtime rose roughly 1.4x to 2.5x for the cheaper form and over 2x corpus-wide for backtracking.

## Phase 3: implementation and corpus review

- [x] Implement and remove the experimental corrections after measurement.
- [x] Run focused native tests and representative host mission scans.
- [x] Run a no-copy partial corpus comparison: 1,199 processed levels yielded one conversion and no other route changes before the slow run was terminated.
- [x] Restore the accepted planner exactly after rejecting unreasonable runtime cost.

## Phase 4: performance acceptance and validation

- [x] Leave checked-in metadata and the corpus baseline unchanged.
- [x] Run scoped code quality and D1/D2 native planner tests for the experiment before restoring it.
- [x] Profile the accepted planner and Android read/cache path for route-neutral runtime reductions.
- [x] Remove duplicate edge probes and retain only the candidate depths required to preserve corpus route status.
- [x] Extend the existing build-numbered Android cache to valid `partial` and `failed` canonical analyses, including reuse of useful partial prefixes.
- [x] Validate the retained optimization with corpus status comparison, native tests, Windows builds, Android build, and scoped code quality.
- [x] Document the characterized subcategories, rejected recovery, retained optimization, and validation evidence.

## Performance findings

- The hot path was firing-pose enumeration, not route graph search. Each segment previously generated 152 feature probes: four depths at six faces, eight vertices, and 24 face-enumerated edges. Every physical edge was visited twice because it belongs to two faces.
- The retained sampler generates 32 feature probes: two depths at each face, one at each vertex, and one at each of the 12 unique physical edges. Collision, occupiability, connected-segment, intended-wall, and route-cost rules are unchanged.
- The complete 110-archive host generation finished in 170.1 seconds, versus roughly nine minutes for the accepted sampler. All 1,274 level statuses were preserved exactly: 1,114 `ok`, 83 `partial`, and 77 `failed`.
- The regenerated artifact changed 109 route JSONs through different equivalent firing poses or paths. Only two objective sequences changed, both by adding real traversal actions while remaining `ok`; no generated metadata was copied into the dirty checked-in corpus.
- The existing Android cache key already includes the Android build number plus topology and progression hashes. It previously rejected all non-`ok` analyses, causing expensive known-partial levels to be recomputed. Valid `partial` and `failed` results now round-trip through the same cache, and live routing can reuse their still-pending canonical prefix.
- The attempted Lagrange device timing was invalid: game introspection showed the automation stopped at the pilot-selection menu before level loading, with no planner invocation and no route-cache read or write. It is not used as performance evidence.

## Validation

- D1 and D2 `test_route_analysis_cache` and `test_level_metadata_scan`: passed.
- `run-windows-build.ps1 -Target both`: passed.
- Android `assembleDebug` for arm64-v8a, armeabi-v7a, and x86_64: passed.
- Full host corpus comparison: passed with identical status counts and no status changes.

## Phase 5: localized unresolved-action continuation

- [x] Bucket current `partial` and `failed` routes by the exact action where planning stops and whether later objectives remain structurally identifiable.
- [x] Define a general unresolved-action representation that exposes the blocker without inventing a firing pose or claiming Guidebot reachability.
- [x] Prototype continuation from the state transition associated with that action, while isolating only the unproven action from Guidebot guidance.
- [x] Measure route coverage, objective changes, and processing time against the accepted corpus, allowing reviewed regressions only when they provide a better global model.
- [x] Add focused high-level coverage for the map marker and absent Guidebot pose, and verify the existing linked-wall completion/resumption path.
- [x] Run scoped quality, native tests, D1/D2 builds, Android build, and full corpus comparison for the retained implementation.

### Phase 5 findings

- The accepted corpus contained 160 non-OK entries: 76 failed routes stopped at `Start`, 73 partial routes stopped after a key, and 16 reported an explicit trigger dependency loop. Key-unreachable and generic target-unreachable failures remain separate because they do not identify one safe transition to isolate.
- Normal planning now runs first and is returned unchanged whenever it succeeds. Only a non-OK end-level route receives a second pass that may continue through an unresolved trigger.
- Continuation is allowed only when the source is a known shootable trigger wall with known links. The new step marks that wall as `Locate and activate switch trigger N` and has no activation pose. Phase 7 later moved the uncertainty to that objective when downstream analysis reaches the exit.
- D2 Guidebot returns no route goal for the unresolved step. Automap numbering and labels still include it. Existing linked-wall completion detection advances past it after the player activates the switch, at which point live routing resumes from the real player state.
- The first prototype exposed downstream objectives in 29 levels but made three OK routes partial by acting inside speculative branches. The retained two-pass form removed all three regressions.
- Final corpus status counts are 1,114 `ok`, 92 `partial`, and 68 `failed`. Nine failed routes became partial, 17 existing partial routes gained downstream objectives, and 28 unresolved markers all have at least one later objective. No previously OK route changed status.
- Final full generation completed in 171.4 seconds, compared with the 170.1-second optimized baseline. Normalized checked-in metadata was regenerated from the retained planner.
- Focused Lagrange level 1 output contains 12 steps, switch 8 has a map position but no activation position, and three objectives follow it.

### Phase 5 validation

- Focused D1 and D2 metadata-scan test covers the unresolved switch marker, absent Guidebot pose, downstream exit, and continued failure for an unknown/non-shootable source. Linked-wall completion and the live-route refresh path were verified in the existing adapter flow.
- D1 and D2 `test_level_metadata_scan` and `test_route_analysis_cache`: passed.
- `run-windows-build.ps1 -Target both`: passed.
- Android `:app:assembleDebug` for arm64-v8a, armeabi-v7a, and x86_64: passed.
- Scoped code quality and `git diff --check`: passed.
- Full 110-archive host regeneration: 109 passed, one configured descriptor-less archive skipped, zero failed.

## Phase 6: unresolved-switch frontier guidance

- [x] Preserve the closest proven reachable segment when an unresolved switch continuation is created.
- [x] Route Guidebot to that frontier using nearest-progress guidance without assigning a firing position.
- [x] Add focused coverage that distinguishes the frontier from the unreachable switch segment.
- [x] Run scoped quality, D1/D2 native tests and builds, Android build, emulator integration, and mission regeneration.

### Phase 6 findings and validation

- The unresolved step now stores `partial_frontier_segment` as its navigation segment while retaining the actual switch wall, trigger, and label position as its objective identity.
- D2 Guidebot uses `nearest_progress_point` for this activation kind and explicitly prefers the step frontier over any canonical path terminal.
- Obsidian level 10 demonstrates the separation: Guidebot routes to reachable segment 172 while the objective remains trigger 0, wall 18, in switch segment 63.
- The new emulator test `test_obsidian_unresolved_switch_frontier.json5` passed and asserts the active goal, guidance mode, frontier segment, switch segment, wall, and trigger.
- D1/D2 metadata-scan and cache tests, D1/D2 Windows builds, Android all-ABI debug assembly, automation catalog validation, scoped quality, and `git diff --check` passed.
- Final regeneration completed in 174.9 seconds with unchanged status counts: 1,114 `ok`, 92 `partial`, and 68 `failed`; 109 archives passed, one configured descriptor-less archive skipped, and none failed.

## Phase 7: per-objective calculation status

- [x] Represent unresolved switch guidance as an objective-level `calculated` state rather than a route-wide partial status.
- [x] Show unresolved objectives as `Not calculated` in the metadata viewer while retaining their normal numbering and details.
- [x] Keep automap and Guidebot frontier behavior unchanged.
- [x] Update native, Kotlin, metadata, and emulator coverage for the new status semantics.
- [x] Run scoped quality, D1/D2 tests and builds, Android build, and full corpus regeneration.

### Phase 7 findings and validation

- Serialized unresolved-trigger objectives now carry `calculated: false`; other objectives remain implicitly calculated for backward-compatible metadata.
- A canonical level route that reaches the exit after isolating an unresolved switch is now `ok` with an empty route problem. Routes that remain structurally incomplete still report `partial` or `failed`.
- The metadata viewer renders the affected numbered row as `(Not calculated)`. Start remains unnumbered and all objective numbering is unchanged.
- Guidebot live routing intentionally remains `partial` while approaching the unresolved action, and the Obsidian level 10 emulator test passed with nearest-progress guidance to segment 172.
- Focused Obsidian metadata reports levels 10 and 13 as `ok` with one not-calculated switch objective each and no route problem.
- D1/D2 metadata-scan and route-cache tests, D1/D2 Windows builds, the Kotlin viewer test, Android all-ABI debug assembly, and the emulator integration test passed.
- Full regeneration completed in 164.6 seconds: 109 archives passed, one configured descriptor-less archive skipped, and none failed. Across 1,274 levels, statuses are 1,140 `ok`, 66 `partial`, and 68 `failed`; 28 objectives in 26 otherwise-complete levels are marked not calculated.
