# Uneasy4 startup timeout and missing liquid secrets

## Scope

Investigate the September 10 20:53:22 regression run: Uneasy4 hit the 360-second process limit. User authorized native builds and tests after the initial source-only restriction; only this task is running.

## Evidence

- Actual artifacts are under `android/temp/guidebot_simulation_regression/20260910_205322`; the pasted underscores were rendered as path separators
- Both 20:02:11 and 20:53:22 logs stop at `phase=route-start`, before `phase=simulation`
- Metadata generation passed; the batch finishing does not establish that the timed-out level was near its exit
- Instrumented replay shows repeated target/route visibility preparation. Desktop level loading builds a canonical route; confirmation then adds its actor and rebuilds that route before live objective selection
- Obsidian 13's generated metadata reports four secrets and complete liquid metadata. Its opaque water pairs 144/154 and 203/282 pass material, animation opacity, flyability, and clearance checks
- Both engines store wall triggers as signed bytes. Comparing an absent trigger with 255 rejected every ordinary liquid wall; the sentinel is -1

## Work and validation

- [x] Add opt-in liquid diagnostics and startup phase diagnostics
- [x] Correct the trigger sentinel
- [x] Reuse available authored canonical metadata, including useful partial plans at confirmation startup; preserve preparation for restored worlds and missing metadata
- [x] Run real scanner/serialization regression tests
- [x] Confirm Obsidian's opaque positives and transparent negatives in native execution
- [x] Finish the Uneasy4 regression with its unchanged 360-second process budget (known route limitation preserved)
- [x] Check neighboring route cases, paired builds, formatting, and source contracts

Do not increase the timeout or accept unrelated metadata changes as a fix. Record final measured results here.

## Confirmed results

- First focused Uneasy4 run completed through the normal regression harness in about 145 seconds, without changing the 360-second watchdog
- It matches commit `4f0cda59` exactly for status, objectives, 5768 frames, final RNG state/call count, and problem. The known gold-key-unreachable route limitation remains; the infrastructure timeout is removed, not reclassified as a successful route
- Obsidian 13 now emits six secrets with exact positive membership {154}, {282}; the prior four secrets remain. An explicit MSVC packing guard also fixes the detail dumper crash exposed during validation
- Both final native game builds succeeded without compiler warnings in the build log
- Ten selected CTests passed across D1/D2 (scanner/identity serialization under engine packing, route snapshots, analysis caches, route decisions, and route certification); 28 source contracts also passed
- `test_secret_area_liquid_obsidian.py` passed against the built D2 metadata executable and real mission assets
- Final normal regression-harness run selected Uneasy4 and Obsidian 13: exit 0, no infrastructure failures, total elapsed 144.7 seconds. Obsidian 13's route is OK; Uneasy4 again matches the historical 5768-frame gold-key-unreachable result exactly
- The normal host metadata worker regenerated the entire Obsidian mission successfully. Only level 10 secrets (4 -> 5) and level 13 secrets (4 -> 6) changed; published the reviewed Obsidian metadata and Uneasy4 simulation record
- Scoped formatting/lint and `git diff --check` passed

Artifacts: `android/temp/liquid_route_final/summary.json`, `android/temp/liquid_metadata_final/metadata/Obsidian.json`, and `android/temp/uneasy4_liquid_timeout_probe/obsidian13.verified.json`. This was focused native validation, not a full archive-corpus regeneration or emulator run
