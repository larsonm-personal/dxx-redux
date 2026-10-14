# Reconcile upper stack with current cmake

- Merge PR25-35 while preserving current level-start metadata, numeric cheats, slow frames, D1 mission selection and safe logging
- Use one ordered input dispatch boundary before simulation and assign unique direct command identifiers
- Build both engines and run focused tests; record remaining corpus re-recording or platform qualification separately
- Keep existing recordings as evidence until fresh recordings are qualified; no legacy compatibility layers

## Reconciliation and focused verification

- Numeric cheat operations have their own shared command ID17; upper delete/enable/profile-grant commands retain IDs14-16
- Both games dispatch recorded input before simulation exactly once; preserve current fresh-level state, runtime/cheat metadata and frame times through INT32_MAX
- Replay helper retains D1Mission and safe logging while incorporating upper fixture picker and FPS detection
- Fresh Debug -O1 -g1 strict-FP builds pass for both engines; D1 CTest 70/70 and D2 CTest 79/79 pass
- Ordered, secondary, all-weapons, conversion and guidebot Python checks pass 56/56; effect-phase checks pass 18 with one skipped
- Replay menu and automation catalog validation pass; every one of 28 tracked demos is below 100KB (largest 94,354 bytes)
- Full replay corpus re-recording/immutable runtime qualification remains separate; this merge does not claim to fix prior diagnostic oracle mismatch
- Aggregate run-all catalog cannot complete on this kernel because required /proc task children interface is absent; scoped formatter reports missing clang-format
