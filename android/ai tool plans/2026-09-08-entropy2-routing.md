# Entropy 2 routing

Add Entropy2 to the shared targeted mission manifest, reproduce a failing level, implement a general routing or simulation correction in shared source where practical, and verify repeat completion plus the expanded regression corpus. Keep implementation out of headers and leave outstanding_bugs.md untouched. Do not pursue the deferred TEW 9 separately.

## Findings and changes

- Entropy2 adds six levels, bringing the manifest to nine missions and 209 levels. Baseline levels 1, 2, 3, and 6 pass; levels 4 and 5 time out
- Selected level 4, Vermillion Caverns. After destroying the reactor, the actor attempted to cross wall 40 from segment 263 to 387 and timed out at 5,344 frames. This reactor-linked wall is WALL_CLOSED, which wall_toggle does not open in either engine
- The shared engine metadata adapter now exposes only reactor links that operate on doors or blastable walls. This also corrects the live path certifier's passability assumptions
- With the grate excluded, planning exposed a second mismatch: a locked fly-through trigger door was predicted closed after reactor destruction. Engine diagnostics confirmed the real linked doors were opening, while planner diagnostics showed hypothetical reactor destruction left their progress states closed
- The shared dependency planner now records both faces of linked door openings and blastable-wall destruction when predicting reactor or boss completion
- Level 4 physically completes twice at 3,132 frames with the same objectives: switch 0, blastable wall, blue key, switch 2, reactor, pass-through trigger 12, exit
- Added a repeat integration test checking physical completion, objective order, deterministic results, and agreement with regenerated metadata

## Validation

Scoped formatting, manifest discovery tests, and master regeneration tests pass. Both Windows engines build; all 49 D2 native tests pass (D1 has no registered CTest tests). Existing D1 weapon.c return-path warnings remain unrelated to this change.

All nine missions regenerate metadata successfully. The repeated level 4 integration test passes at 3,132 frames with byte-equivalent parsed results and metadata agreement. The full 209-level corpus improves from 187 to 188 passing levels, with no previously passing level lost. Entropy2 level 5 remains a timeout; the other unresolved statuses are unchanged. Updated generated regression JSON from the verified run.

Counterstrike 17 retains its objectives and 4,407-frame completion but has a changed RNG endpoint. Two focused repeats produce identical complete results and match the full corpus, so the generated RNG update is retained.

Artifacts: `temp/entropy2_final_build.log`, `temp/entropy2_final_d2_ctest.log`, `temp/entropy2_final_integration.log`, `temp/entropy2_final_metadata.log`, and `android/temp/core_nine_entropy2_verified`.
