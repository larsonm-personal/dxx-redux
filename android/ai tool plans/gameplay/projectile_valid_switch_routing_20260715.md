# Projectile-Valid Switch Routing

## Objective

Replace the pending key-first/source-segment workaround with one general rule: a remote shoot-switch route is eligible only when the navigator can reach and occupy the exact firing pose and a projectile-equivalent collision query reaches the intended trigger wall through a valid connected segment chain.

## Required invariants

1. No mission, level, segment, wall, trigger, coordinate, or corpus-baseline exception.
2. No automatic key pickup merely because a route begins with a remote switch.
3. No source-segment preference. All valid firing poses compete by total reachable route distance.
4. A candidate segment being reachable is insufficient. The exact candidate pose must be occupiable for the configured navigator radius.
5. A visibility result is insufficient unless FVI reports a valid connected segment chain from the firing segment to the target wall segment.
6. The collision query must model a projectile corridor, not a zero-width mathematical ray.
7. The target wall must be the first blocking wall, or the trace must terminate at its center without an earlier collision.
8. Same-segment candidates use the same validation path as remote candidates.
9. Existing legitimate shoot-through routes remain eligible when they satisfy the same physical rule.
10. Corpus changes are reviewed to find rule defects. They are not suppressed to preserve prior metadata.

## Implementation plan

### Phase 1: Remove policy workaround

- [x] Remove `acquire_available_key`, `prefer_source_segment`, and `allow_remote_switch_key_replan`.
- [x] Restore one dependency-planner execution path with the existing radius-relaxation fallback only.
- [x] Remove the public `prefer_source_segment` parameter from trigger firing-path selection.

### Phase 2: Define physical firing-pose validation

- [x] Rename the callback contract from generic visibility to physical wall shootability.
- [x] Validate that each sampled candidate lies in its declared segment and has navigator-radius clearance.
- [x] Replace the wall visibility shortcut and permissive disconnected-chain fallback with one connected FVI result.
- [x] Use a positive projectile corridor radius derived from stable gameplay geometry, documented at the adapter boundary.
- [x] Require the intended trigger wall to be the trace endpoint or first wall hit.

### Phase 3: Candidate selection

- [x] Keep deterministic sampling but reject non-occupiable samples.
- [x] Evaluate all reachable samples and choose the minimum total route distance instead of the first visible segment/sample.
- [x] Preserve deterministic tie behavior.

### Phase 4: Focused automated coverage

- [x] Add native tests proving candidates with no physically shootable pose are rejected.
- [x] Add native tests proving a connected, occupiable remote shot remains eligible.
- [x] Update Obsidian level 2 coverage to prove the blue key is first because trigger 4 has no valid initial firing pose.
- [x] Prove trigger 4 is actionable after the key from a generally validated pose.
- [x] Exercise KCX-F2 level 5's reflective-grate switch as a non-Obsidian positive control.

### Phase 5: Broad validation

- [x] Run scoped formatting and static checks.
- [x] Run native route and metadata tests.
- [x] Build Windows D1 and D2.
- [x] Build the Android debug APK for all configured ABIs.
- [x] Run focused emulator regressions.
- [x] Regenerate the metadata corpus and classify all changed routes by the new invariant.
- [x] Retain legitimate metadata changes even when broad; investigate unexpected losses of valid routes.

## Corpus audit result

- All 109 analyzable mission archives passed; one descriptor-less archive was skipped.
- The checked-in metadata changes span 89 files. Across those files, route status changed from 969 ok / 48 partial / 46 failed to 877 ok / 98 partial / 88 failed.
- A denser face-interior sampler rescued only two additional ok routes while more than doubling scan time. This demonstrated that the broad delta comes from removing the permissive disconnected-ray fallback, not from a mission-shaped sampling gap, so the expensive sampler was not retained.
- Obsidian levels 1 and 2 and KCX-F2 level 5 remain complete. Obsidian level 2 begins with the blue key, and its post-key trigger 4 firing pose succeeds with an actual laser shot on the emulator.

## Completion evidence

- The workaround symbols no longer exist.
- Every shoot-switch firing pose is produced by the same feasibility and cost rule.
- Native negative and positive geometry tests pass.
- Obsidian level 2 and a non-Obsidian remote-shot integration test pass.
- D1, D2, Android, and corpus regeneration complete without unexplained failures.
