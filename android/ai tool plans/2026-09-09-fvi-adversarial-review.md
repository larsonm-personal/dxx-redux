# FVI adversarial review

Scope: review current D1/D2 FVI edits, including the swept-edge correction committed in ca9c1b20 and uncommitted wall-aware occupancy helpers. Determine ordinary gameplay impact, run committed input-demo regressions, and assess extraction for a future enhanced/non-enhanced Guide-Bot boundary. No production refactor requested in this review

1. Inspect native collision call sites, new query behavior, and shared helpers
2. Run the committed demo corpus against the current build with expected checks enabled
3. If a demo fails, compare with only the swept-edge correction disabled, then restore the exact current sources and rebuild
4. Record evidence, limitations, and extraction recommendations

Findings so far

- High: check_line_to_face applies the new IT_EDGE rejection unconditionally in both engines, for any positive-radius query against a side with wall_num<0. Those sides include ordinary solid level geometry, not merely traversable portals. Players, projectiles, and ordinary robots use this path through find_vector_intersection. No Guide-Bot actor, query flag, or enhancement setting gates it
- High: previous routing validation did not run the committed input demos. 50 native tests and 293 route simulations do not establish replay compatibility or preservation of original collision behavior
- The new object_intersects_blocking_wall is called only from shared secretarea firing-pose validation and guided_missile_route. Existing object_intersects_wall/sphere_intersects_wall still select check_wall_state=0. Static review finds no intended legacy semantic change from that refactor alone
- The shared swept-edge math uses double precision in the originally fixed-point collision decision. This needs cross-platform determinism coverage; a Windows pass alone cannot establish Android parity
- July's FQ_PASSABLE_WALL_CALLBACK hook is explicitly opt-in and clears callbacks for ordinary queries. It is a better policy boundary than the unconditional edge correction, but uses existing non-reentrant global FVI state; callbacks must not recursively invoke FVI

Current demo run: android/tests/test_input_demo_regressions.ps1 -ResultArchiveRoot temp/fvi_adversarial_demo_review, log temp/fvi_adversarial_demo_review.log, session86630. Corpus15demos, current build freshness checked by runner. First D1L15 demo fails expected score2700/actual1000 and robots_alive44/47. Attribution pending controlled comparison; do not claim all mismatches are caused by FVI

Extraction recommendation pending validation: keep geometry and policy in shared implementation files with declarations only in shared headers. Preserve the legacy native path as default. Use explicit per-query opt-in for enhanced planning and enhanced companion physics; moving the code without gating the call does not isolate the base game. Do not enable correction globally just because an enhanced buddy exists. Ordinary player physics must retain the selected compatibility policy. Any occupancy extraction must preserve real native face/portal semantics rather than substitute an unverified zero-length ray. A small native primitive hook is preferable to copying the whole original collision engine


Before cleanup pause: complete current-build corpus finished all15 demos,14failed and only d2_descent2_level9_20260511_192533 passed. Controlled A/B build with the edge correction disabled failed during CMake configuration due full disk, before compilation. Both d1/main/fvi.c and d2/main/fvi.c were restored byte-for-byte to their pre-review snapshots. No temporary review code remained. Attribution was unproven at that point. User then explicitly requested comprehensive deletion of all temporary files; raw logs/results were disposable under that instruction

## Controlled comparison after cleanup

Rebuilt both Windows engines with the normal host build guard, MSVC x86 RelWithDebInfo. Ran all 15 committed fixtures: four D1 full-game replays with drawing/presentation bypassed and eleven D2 headless replays. Expected-result checks remained enabled; no fixture expectations were changed

| Variant | Pass | Fail | Evidence |
| --- | --- | --- | --- |
| Current working code | 1 | 14 | temp/fvi_review_current/*.actual.json |
| Only swept-edge correction disabled in both fvi.c files | 15 | 0 | temp/fvi_review_control/edge_off.log and temp/fvi_review_edge_off/*.actual.json |
| Current code restored and rebuilt | 1 | 14 | All 15 complete actual JSON results exactly match the first current-code run |

The diagnostic changes exactly one condition in each fvi.c to `if (0 && result == IT_EDGE && rad > 0 && seg->sides[side].wall_num < 0)`. All occupancy, routing, AI and other working-tree changes remain identical. Both source snapshots are restored byte-for-byte in finally; SHA-256 hashes match. The restored-current build and repeatability corpus completed successfully: all 15 complete actual JSON results match the first current-code run exactly. The repeat runner used ReferenceResultRoot and therefore reported 15 comparison passes; against the recorded fixtures these identical results still represent 1 pass and 14 failures. Evidence: temp/fvi_review_control/restored.log, temp/fvi_review_restored/*.actual.json and temp/fvi_review_control/comparison.json

This isolates the unconditional swept-edge correction as the cause of these 14 final-state regressions on this build. It is not merely a theoretical collision risk or an unrelated old fixture mismatch. Examples:

- D1 L15: correction enabled scores 1000 with 47 robots alive; disabled matches recorded 2700 and 44
- D1 L16: enabled ends in segment 84 with score 3400 and shields -6; disabled matches segment 511, score 8900 and shields 40
- D2 L9 215654: even the short replay with only small coordinate differences passes exactly when disabled
- The only fixture unaffected in final state is D2 L9 192533

The all-pass edge-off run also shows that the current occupancy helper and July wall callback do not prevent this corpus from passing. It does not prove they are correct for every route, caller or platform. No Android replay, cross-compiler parity, or per-frame first-divergence comparison was performed in this review

## What should be done

1. Remove the unconditional correction from ordinary collision processing first. Restore the original check_line_to_face return behavior in both engines. Do not alter recorded expectations or compensate during replay to hide the change. The earlier route-only pass counts were inadequate validation for a global physics modification
2. Prefer recovering the affected funnel routes through shared steering and waypoint search whose final candidate is accepted by native collision checks. Shared swept-edge geometry may help propose candidates, but ideal geometric clearance is not proof that legacy physics can execute them. Player passage and guided-missile predictions must use the player's and missile's actual collision policy
3. If an enhanced companion truly needs different collision semantics, make that a separate, explicit per-query capability, requested only by enhanced companion planning and its matching movement path. Do not turn it on for all actors, all positive-radius queries, all Android builds, or whenever a buddy exists. The future non-enhanced option must exercise the original path. A buddy-only clearance change must not be used to certify player passage or player-fired missiles
4. Extract the route occupancy operation into a shared implementation and put its declaration in a shared header. Preserve the original sphere_intersects_wall algorithm and entry-point semantics. Both engines already export check_sphere_to_face (with different signatures), so a shared adapter can reuse native face tests, segment masks and vertex lists while owning its wall-aware traversal and bounded local visited storage. This offers a way to remove the new occupancy edits from fvi.c/.h without copying the full FVI engine or replacing occupancy with an unverified zero-length ray. Keep new implementations out of headers. The existing explicit wall callback is a reasonable narrow boundary for planned door state, but must remain opt-in and must not recursively call FVI because its active state and traversal buffers are global
5. Validate the eventual fix with all 15 committed demos plus the focused funnel, Obsidian, Castaway and guided-shot checks, then the full mission set. Route counts must not substitute for player/projectile compatibility. Any retained floating-point collision policy also needs Android/host determinism evidence

This is a review and controlled experiment, not the production extraction or future enhancement-option implementation. The known global correction is restored to its original pre-review state after the experiment so this review does not silently replace the user's current code with an unvalidated routing change


## Implementation follow-through

The earlier paragraph saying the correction was restored describes the end of the controlled experiment, not the current working tree. The compatibility implementation now removes it in both engines. The four fvi.c/.h files match ca9c1b20^ after newline normalization; this establishes restoration to the pre-correction revision, not a claim that these entire files equal the 1996 release

Route occupancy now lives in shared route_collision.c with declarations only in route_collision.h, calling the existing native per-engine face primitive. Original occupancy entry points and ordinary FVI collision semantics are restored. The unused swept-edge helper and its isolated unit test were removed. This extraction does not implement the future enhanced/non-enhanced setting

The routing-only door-approach repair restores TEW secret -3. Latest full-set evidence is 282/293 passing, one fewer than the earlier 283-pass baseline: Plutonia L3 is still unresolved. Further recovery should search for a native-physics-executable route while preserving trigger avoidance. An alternate topological path exists, but has not yet been proven by execution. Route cache generations advance together to 29 to invalidate results computed under the removed global correction


Final resumed host demo run: all 15 committed recorded-input demos pass against their unchanged expectations, temp/fvi_resume_demos.log and temp/fvi_resume_demos/*.actual.json. These are host final-state checks, not an Android replay/parity result, exhaustive gameplay proof, or a new baseline. Current scoped source formatting and diff checks pass


Final resumed validation complete: all 15 committed demos pass (temp/fvi_resume_demos.log). Android externalNativeBuildDebug passes for arm64-v8a, armeabi-v7a and x86_64, and the two targeted launcher scheduling/monitor suites pass all 19 tests (temp/fvi_resume_android.log). No Android input replay was run. Cache invalidation is complete; the remaining compatibility-repair work is Plutonia L3 routing, not FVI restoration. The latest full 293-level sweep remains 282 OK / 4 timeout / 3 failed / 4 unsupported


## Final outcome

The previously unresolved Plutonia L3 route is repaired by shared alternate-path recovery. It uses the existing path builder to avoid a persistently blocked open portal, requires the original endpoint, preserves semantic edge exclusions, and executes with unchanged native collision and full actor radius. Two identical focused runs complete in6457frames; the integration test verifies both the recovery event and the full objective sequence

Final evidence: all15 committed recorded-input demos pass without fixture changes (temp/fvi_alternate_final_demos.log); all49 D2 native tests pass; repeated Plutonia, TEW, Obsidian, Castaway and primary-target grate checks pass; guided requirement/instruction checks pass. Android native build passes all3 ABIs (temp/fvi_alternate_android_final.log). Full mission set improves to284/293 passing with every prior pass retained and AF D1 beta L6 newly passing. Published thirteen simulation files match the verified results byte-for-byte. Native/JVM route cache generation29 invalidates results from the removed collision policy

The recommendation is implemented: legacy FVI is restored, routing occupancy is extracted into shared implementation, and routing fallout is addressed through shared navigation. Future enhancement selection remains separate. These host demos and route simulations are specific evidence, not proof of every gameplay interaction or Android replay parity. Detailed requirement-by-requirement completion evidence is in 2026-09-09-fvi-compatibility-fix.md
