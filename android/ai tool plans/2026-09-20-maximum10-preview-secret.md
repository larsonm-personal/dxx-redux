# Maximum level 10 preview secret investigation

- [x] Confirm host metadata reports segment 187 as S2 in both Maximum versions
- [x] Identify the reported closet relative to the hostage room, including other matching item placements
- [x] Trace the rejection in the shared scanner using temporary component diagnostics
- [x] Remove temporary instrumentation and rebuild the D2 host metadata target

The host result alone does not explain the missing label in the user's preview

## Findings

- There are two shield/smart-missile closets: segment 187 (detected) and segment 147 (reported missing)
- Both hostages are in segment 274; the connection is 274 -> 188 -> hidden door 91/92 -> 147
- Segment 147 is a separate component with items, no progression objects, and hidden reachability
- Its candidate `present` flag remains zero because `collect_raw_candidates` skips source segment 188 when its `progression_distance` is -1
- Segment 188 shares a component with the hostages and the red key in segment 247; that parent component is excluded for progression objects, but never promoted to an eligible source for nested secret candidates
- The entrance-detail collector has the same progression-distance restriction and must be considered in a future fix
- The Android preview uses this shared scanner; the independent preview smoke attempt failed before launch with `no previewable levels for host-selected mission pack`, so no Android rendering verification was obtained

This investigation identifies a nested-secret detection bug. No behavior change was made. A fix should handle optional hidden compartments reached through excluded progression areas and validate against existing secret baselines

Evidence: `android/temp/maximum10_closet/components.log`

## Authorized fix

- [x] Allow reachable progression-containing components to supply candidate entrances without changing trigger reachability or labeling those components
- [x] Cover nested closets, alternate access, and disconnected progression areas
- [x] Verify both Maximum versions and review D1/D2 secret baseline differences
- [x] Invalidate stale metadata caches and run relevant builds
- [x] Run scoped formatting and final checks

## Fix validation

- Maximum fixed and original level 10 now include S3, segment 147, entrance 188/4, wall 91, with one shield and one smart missile
- Reusable `android/tests/test_maximum_nested_secret.ps1 -DataDir <D2 assets>` passes two deterministic full-engine scans and verifies the surrounding hostage/key area stays excluded
- CMake scanner classification/persistence and scan-budget tests pass
- D1 and D2 host metadata targets and Android x86_64 native targets build successfully
- Base-game secret inventories retain every prior identity and membership; D1 gains five compartments (L15, L22, L26), D2 gains eight (L2, L6, L8, L12, L14, L23) and some additional entrances
- Updated only secret inventory/count fields in the base-game fixture; unrelated pre-existing flyout/provenance metadata differences prevent a full-document baseline match
- Cache generation advanced from 41 to 42 on native and JVM sides
- Updated the two checked-in Maximum L10 secret counts; no all-mission metadata regeneration or device installation performed
