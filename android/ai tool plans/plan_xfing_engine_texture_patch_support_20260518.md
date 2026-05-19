# Xfing Engine Texture Patch Support 20260518

## Goal
Add the small engine support needed for the plain-texture DXAs, and replace custom metadata delta shapes with a preexisting generic patch format where that remains useful.

## Tasks
- [x] Trace D2 PIG/palette selection and texture lookup inputs
- [x] Choose the metadata patch format with engine and launcher constraints in mind
- [x] Update converter/verifier outputs if the patch format changes
- [x] Add D2 palette-specific texture lookup with minimal engine changes
- [x] Regenerate and verify the plain-texture DXAs
- [x] Run scoped code quality and build/test checks

## Initial Direction
- D1 root-level PNG replacements should continue to work with existing lookup
- D2 should search `textures/d2/sets/<set>/` before root-level texture names when the active PIG texture set is known
- Prefer RFC 6902 JSON Patch for metadata if the patch remains JSON-semantic; avoid inventing a custom patch language unless the engine side needs something much smaller

## Results
- Added `piggy_current_pigfile()` in D2 so rendering code can query the active PIG without exposing the mutable global
- D2 texture loading now searches `textures/d2/sets/<set>/<bitmap>.{png,jpg,tga}` before the old root-level lookup, and super-transparent masks use the same resolved basename
- D2 HAM metadata patches are applied from `patches/d2/ham_patch.rfc6902.json`, targeting `com.dxxredux.d2-ham-sections.v1`
- Added virtual bitmap slot registration for extra HAM bitmap indices, using generic `idxNNNN` names
- D1 metadata now emits `patches/d1/level_surface_patch.rfc6902.json`, an RFC 6902 JSON Patch document targeting `com.dxxredux.d1-level-surfaces.v1`
- Summaries remain in `patches/d1/level_surface_patch_summary.json` and `patches/d2/ham_patch_summary.json` for human audit, but the patch format is no longer custom
- Regenerated default DXAs in `game_data/mods/xfing/dxx_tp/tmp/plain_texture_dxa/`: UUD1 has 120 texture PNGs and 144 level-surface patch ops; UUD2 has 908 texture PNGs and 31 HAM patch ops

## Validation
- `./android/run-code-quality.ps1 -Fix -Paths game_data/mods/xfing/xfing_minimal_dxa_lib.ps1,game_data/mods/xfing/convert-xfing-minimal-dxa.ps1,game_data/mods/xfing/verify-xfing-minimal-dxa.ps1,android/ai tool plans/plan_xfing_engine_texture_patch_support_20260518.md,android/ai tool plans/plan_xfing_plain_texture_dxa_conversion_20260518.md`
- `./game_data/mods/xfing/convert-xfing-minimal-dxa.ps1 -Game both`
- `./game_data/mods/xfing/verify-xfing-minimal-dxa.ps1`
- `./run-windows-build.ps1`: completed for D1 and D2 with existing warning noise
- `ctest --test-dir buildd1 --output-on-failure` and `ctest --test-dir buildd2 --output-on-failure`: no tests registered, exit code 0
- `./android/gradlew.bat ':app:buildCMakeDebug[arm64-v8a]-2'`: completed for the Android D2 native CMake target with existing warning noise
