# Proposed GQR-0305: verify retained route work across launcher/game handoff

Diagnosis only, 2026-10-14. Proposed GQF-0319 OPEN / GQR-0305 TODO; not admitted or implemented. Direct PRIMARY47MEDIUM H/M/B/C/R23/0/4/10/10. Scoped missing integration oracle, not an observed production failure.

The original priority/handoff plan requires resuming launcher-generated visibility chunks and integration evidence of retained progress to next_ready. Frozen/current test_route_metadata_import_handoff instead waits for a diagnostic last_finished_mission name and later checks a usable Guidebot route. The monitor updates that name after any individual level attempt, including failure. Recomputing the same route from zero can satisfy every assertion. The complete generic runner adds correlated terminal PASS, process/transport health and timing grace, with no cache-reuse verdict.

Repair the integration owner using native observations for one exact pinned KCXF2RM game/content/mission/level/native-cache-key generation. Establish an actual published launcher artifact for the chosen level before launch. For a controlled partial case, require nonempty validated retained chunks and an explicit flush/retirement boundary, then require the in-game worker to accept that generation and make monotonic progress to useful readiness. Do not substitute display names, elapsed sleeps, file existence or in-memory visibility-cache hits for disk-checkpoint reuse.

Use a separately selected complete-cache control to require a validated matching native cache hit/adoption without fresh analysis of that completed level. Do not demand partial resume when the selected case legitimately completed first. Existing filename/generation/topology/hit/adoption fields support the completed case; add only minimum read-only native accepted-chunk/sample/identity evidence for the partial case. Keep cache formats, keys and validation in the engine; no Kotlin binary reader or new cache scheme.

Preserve current import wakeups, source/profile identity, native validation, safe adoption boundaries, run-ID/FAIL guards, functional Guidebot assertions and worker-generation teardown. A changed source must invalidate retained work correctly rather than reuse stale bytes. No Android compatibility scaffolding or planner rewrite is proposed.

Future acceptance must pass matching retained partial work and a matching completed-cache hit, but fail dropped chunks/recomputation from zero, disabled checkpoint loading, wrong source/level/generation, a stale diagnostic mission name, failed launcher work and unrelated already-usable routes. Missing-reuse controls must fail even if a route eventually becomes usable. Keep an explicitly cold control that may compute fresh work, plus changed-source rejection/recovery. Use deterministic production observation seams and correlated worker identity, not fixed timing races. No tests or mutation controls executed here.

Distinct from BR0484 worker ownership/cancellation, BR0588 stale introspection, BR0593 staged bytes, BR0285 numeric comparison, BR0233 output publication, GQR0102 source transactions and GQR0212 work admission. The original plan explicitly scopes the separate Uneasy4 case to liveness and leaves frame-time tuning to hardware QA; no numeric-performance root is added.

H23 integration-verdict integrity, M0 original-file payoff, B4 one scenario, C10 exact script/runner/monitor/native producer and original contract, R10 localized test/read-only observation repair. No pooled native/JNI/resource severity or inherited1996saving.


## Canonical admission, 2026-10-14

GQF-0319 OPEN / GQR-0305 TODO admitted through chunk0444 exactnineoperationpublication and independentproposed/publishedreplayPASS4571. The earlier proposed wording is historical prepublication state. Implementation and actual acceptance remain TODO; no production failure/mutation was executed. PRIMARY47MEDIUM23/0/4/10/10 unchanged. See primary0444 terminal handoff for immutable report/snapshot, mutation/replay and mountedDrive receipt.
