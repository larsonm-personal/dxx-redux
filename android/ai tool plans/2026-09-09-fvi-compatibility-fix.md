# Restore collision compatibility and isolate routing geometry

User authorizes implementing the adversarial review recommendations while preserving demo compatibility

1. Remove the unconditional swept-edge rejection from both native collision paths
2. Restore original native occupancy functions; move route wall-aware traversal into shared implementation with declaration-only header and native face tests
3. Format changed code, build both engines, run all committed demos without changing expectations
4. Run focused occupancy/guided-shot and recovery checks plus the small mission set to measure any routing fallout
5. Address affected routing with legacy-compatible shared recovery, never global physics changes; record remaining evidence and limitations

Future enhanced/non-enhanced setting is separate work. No replay-specific compensations, fixture rebaselines, or new global collision policy

## Progress

Removed the swept-edge branch from both fvi.c files and restored the native sphere_intersects_wall/object_intersects_wall functions. Native fvi.h occupancy additions removed. New shared route_collision.c/.h uses the original per-engine check_sphere_to_face primitive with a local bounded traversal; secretarea and guided_missile_route call it. Both Windows engines build and all 15 committed input demos pass (temp/fvi_fix_demos.log)

Full 293-level sweep at android/temp/fvi_fix_routes: 281 OK / 5 timeout / 3 failed / 4 unsupported. Two formerly passing routes now time out: Plutonia L3 and TEW secret -3. All other prior statuses retained, including Obsidian and Castaway. Comparison temp/fvi_fix_route_comparison.json. Checked-in mission JSON untouched by these runs

Extended test_plutonia_narrow_funnel.ps1 to require the D1 L16 and short D2 L9 collision regression demos before checking repeated funnel completion. Scoped formatting passed. Android validation initially failed because Gradle uses Windows PowerShell 5 and clean-workspace.ps1's new progress message used Join-String; replaced with -join, scoped formatter and synthetic cleanup integration test passed. Android retry still needed after native jobs finish

Plutonia L3 diagnostics: existing sampled recovery candidates fail both long and short native sweeps. Merely dropping the long-sweep requirement would not fix it. A temporary precise-steering experiment crossed the funnel forward under native physics, then stalled on the reverse crossing at segment363. Centering on the current segment before steering also failed. All diagnostics and steering experiments removed. Current candidate adds six opening-axis projections after the existing 43 samples, requiring the same native full-radius full/short sweeps for both legs. Focused build/run session9667, logs temp/fvi_recovery_axis_build.log and temp/fvi_recovery_axis.log. This candidate is not yet accepted

The opening-axis candidate also failed and was removed. No diagnostic or steering experiments remain. Removed unused swept_edge_clearance.c/.h and test_swept_edge_clearance.c plus its CMake registration. All four native fvi.c/.h files match ca9c1b20^ exactly after newline normalization

Final validation of this stage:

- All 15 committed input demos pass again after rebuilding final sources and removing experiments: temp/fvi_fix_final_demos.log, results temp/fvi_fix_final_demos
- Android :app:externalNativeBuildDebug passes for arm64-v8a, armeabi-v7a and x86_64: temp/fvi_fix_android_final.log
- Both Windows engines build. D2 CTest passes 49 tests; D1 build has no configured CTest tests, so do not claim a D1 unit-suite pass: temp/fvi_fix_native_tests.log
- All five repeated primary-target grate tests pass: Vertigo6, Plutonia1/29, Vignettes14/17, temp/fvi_fix_primary_test.log
- Guided annotations pass for Plutonia4 and Bitesize-3/-1: temp/fvi_fix_guided_test.log. These are annotation tests, not completed missile routes
- Extended funnel test passes its two recorded demo checks, then fails on the known L3 route timeout: temp/fvi_fix_funnel_test.log
- Scoped formatting/lint and diff checks pass

An extra PowerShell5 run of test_clean_workspace.ps1 failed in the test harness's junction teardown. Automatic approval review blocked direct removal of its remaining synthetic repository at temp/cleanup-fixture-dae67fd58448482ab07f578967224859; only a generic policy-block message was provided. Do not retry removal through another mechanism. PowerShell7 cleanup integration passes, and the real Gradle PowerShell5 retention invocation succeeds after the Join-String fix

That malformed synthetic .dximdemo exposed a separate replay-runner bug: run_input_demo_replay.ps1 enumerated/parses unrelated demo candidates even when DemoPath was explicitly supplied. It now discovers only for interactive selection or ListOnly. New test_input_demo_explicit_path.ps1 passes using a deliberately malformed unrelated recording and an explicit committed demo. Full 15-demo rerun passes with malformed unrelated files still present

Remaining: resolve Plutonia3 and TEW-3 routing fallout without global collision changes; invalidate route caches computed under the removed collision semantics (native/JVM generation is currently28, bump both once final behavior is settled); repeat affected routing and necessary final validation. Goal remains active

Next TEW-3 lead: step8 aims to open hidden door250 in585. A flare is logged from actor545, but the step does not complete; the actor wanders, crosses incidental trigger8 and eventually loops on frontier765. Inspect actual/segmented native flare visibility, not only the logged shot intent. route_confirmation.cpp set_visible_flare_target uses a single full-length FVI for ordinary center shots, while level_metadata_door_shot_aim_from_position also requires segmented visibility. This is a hypothesis, not an established cause. No live task processes remain at end of this stage


## Resumed verification and routing findings

TEW secret -3 is now fixed without changing collision semantics. Segmented flare diagnostics proved the shot reached the intended door. The real defect was switching the path target to the far side immediately after opening a remote door, before reaching its source segment. OPEN_HIDDEN_DOOR now preserves the source-side approach until the actor reaches that segment. The existing remote shot-only action remains unchanged. Both repeated simulations complete identically in 6095 frames. New test_tew_hidden_door_approach.ps1 covers the complete repeated objective sequence

The subsequent full 293-level sweep at android/temp/fvi_door_approach_sweep reports 282 OK / 4 timeout / 3 failed / 4 unsupported. All previously passing levels except Plutonia L3 retain their passes. Do not describe this as complete route preservation or proof of live co-op behavior

Plutonia L3 remains unresolved. A temporary graph dump demonstrated an alternate currently passable route around the blocked 365-364 portal: 365,357,356,359,358,354,353,352,350,337,336,335,329,318,315,314,204. This is topology evidence only, not an executed route. The graph diagnostic has been removed. The next candidate is bounded shared alternate-path recovery preserving semantic avoided edges, not a global collision change. No speculative alternate-path implementation is installed

Native and JVM route cache generations advanced together from 28 to 29 so persisted results computed under the removed collision policy are not reused. Final demo/native/Android verification is being repeated after removing the graph diagnostic and invalidating caches


Resumed host checks: D1 build passes (no configured CTest suite). Initial D2 rebuild caught an incomplete temporary-diagnostic removal; restored the saved pre-graph source including the accepted TEW fix and reran scoped formatting. D2 rebuild then passes and CTest passes all 49 tests (temp/fvi_resume_native_d2_retry.log). The new TEW integration test passes with two identical 6095-frame completions (temp/fvi_resume_tew_test.log). Explicit demo-path selection test passes (temp/fvi_resume_explicit_test.log)


Final resumed validation complete: all 15 committed demos pass (temp/fvi_resume_demos.log). Android externalNativeBuildDebug passes for arm64-v8a, armeabi-v7a and x86_64, and the two targeted launcher scheduling/monitor suites pass all 19 tests (temp/fvi_resume_android.log). No Android input replay was run. Cache invalidation is complete; the remaining compatibility-repair work is Plutonia L3 routing, not FVI restoration. The latest full 293-level sweep remains 282 OK / 4 timeout / 3 failed / 4 unsupported


## Plutonia alternate-path recovery

Previous goal turn made progress: finalized host demo and Android validation, removed the temporary graph dump, and invalidated route caches. Remaining lost route was Plutonia L3

Added a bounded shared recovery candidate in guidebot_path_recovery.c. After one second without progress toward a blocked waypoint, with local approach recovery exhausted, it may replan around the next currently flyable adjacent portal. The existing native path builder handles all actual path geometry and movement. The requested path must reach the original path endpoint and retain both existing semantic edge exclusions. Rejected attempts restore AI state, path allocation and RNG state. A conservative path-pool capacity guard keeps safety insertion and garbage collection from destroying the original path during the attempt. No native source or collision policy change

Focused Plutonia L3 repeats both complete at 6457 frames, with identical results. Log shows alternate recovery at segment363 avoiding364, goal297,27points on the return trip. Evidence temp/fvi_alternate_route.log and android/temp/fvi_alternate_route. Full293 sweep is in progress at android/temp/fvi_alternate_sweep. Final compatibility and integration gates remain pending for this candidate


Full alternate-recovery sweep completed: 284 OK / 2 timeout / 3 failed / 4 unsupported across293 levels. No previous pass lost. AF D1 beta L6 additionally improves from timeout to confirmed (13124 frames), demonstrating use outside Plutonia. Comparison temp/fvi_fix_route_comparison.json and raw evidence android/temp/fvi_alternate_sweep. Deferred Counterstrike secret-5 and TEW L9 remain timeouts

Dedicated Plutonia funnel test passes its two unchanged collision demos and identical repeated complete runs. Added an assertion that alternate-path recovery actually ran, to distinguish this coverage from incidental route success; final execution of that extended assertion pending. Repeated Obsidian L14 and Castaway secret-1 integration tests pass, including expected objective sequences and the Castaway alternate exit evidence. D2 CTest49/49 passes. Final15-demo run and additional focused tests are in progress


## Completion audit

The Plutonia routing fallout is resolved. The alternate-path implementation remains entirely in shared guidebot_path_recovery.c and is reached only by the existing active enhanced-route companion hook. It leaves ordinary collision behavior unchanged. A replacement must reach the original endpoint; rejected candidates preserve the original path and do not discard semantic trigger exclusions. The full-size actor completes the route through normal movement, with no teleportation, radius reduction or forced objective success

All five planned requirements are now verified against the current tree:

1. Both fvi.c files restore the original check_line_to_face return and occupancy routines. All four fvi.c/.h files match ca9c1b20^ after newline normalization. This is the pre-correction revision, not a claim of byte identity with the 1996 release
2. Shared route_collision.c/.h owns route occupancy, reusing native face primitives. The header contains declarations only; existing ordinary native occupancy entry points retain their semantics. Unused swept-edge implementation and isolated test removed
3. Both Windows engines build. Final complete committed demo corpus passes15/15 against unchanged fixture expectations (temp/fvi_alternate_final_demos.log). Git confirms no committed demo fixture changes. Scoped formatting and diff checks pass. D2 native CTest49/49 passes (temp/fvi_alternate_ctest.log); D1 has no configured CTest suite
4. Final focused tests pass: repeated Plutonia L3 with both collision demos and explicit alternate-recovery log assertion (temp/fvi_alternate_final_funnel_test.log); TEW secret-3 (temp/fvi_alternate_tew_test.log); Obsidian L14 (temp/fvi_alternate_obsidian_test.log); Castaway secret-1 including real exit closure and alternative exit selection (temp/fvi_alternate_castaway_test.log); all five primary-target grate cases (temp/fvi_alternate_primary_test.log); guided annotations/instructions for Plutonia4 and Bitesize-3/-1 (temp/fvi_alternate_guided_test.log). Annotation coverage does not claim complete guided missile flight verification
5. Full293-level final sweep gives284 OK /2 timeout /3 failed /4 unsupported. Every previously passing identity still passes. AF D1 beta L6 is an additional pass. The thirteen checked-in simulation files were refreshed byte-for-byte from that verified sweep and their published totals verified. Raw evidence android/temp/fvi_alternate_sweep; pre-publication snapshots temp/fvi_before_simulation_publish. Simulation schema tests pass (temp/fvi_final_schema_test.log). No outstanding_bugs.md changes

Final Android externalNativeBuildDebug succeeds for arm64-v8a, armeabi-v7a and x86_64 (temp/fvi_alternate_android_final.log). Native/JVM cache generation29 remains synchronized and invalidates pre-restoration caches; the preceding19 scheduling/monitor tests pass with these same constants (temp/fvi_resume_android.log). No further JVM changes were made

No production diagnostic probes remain. The enhanced/non-enhanced UI/behavior option remains separate future work as requested. No Android input replay or exhaustive co-op validation is claimed. The preexisting deferred failures are unchanged; resolving them is outside this collision-compatibility goal. The compatibility restoration, extraction, affected-route repair, cache invalidation, regression publication and required validation are complete
