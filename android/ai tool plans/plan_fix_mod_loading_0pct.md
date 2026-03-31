# Plan: Fix test_mod_loading 0% hires textures

## Problem
test_mod_loading showed 0% high-res textures and all-yellow labels. Two root causes:

### Root Cause 1: DXA ZIP compression incompatible with PhysFS
Previous .NET ZipFile.Update operations re-compressed DXA entries from Store to
Deflate. PhysFS silently fails to mount Deflate-compressed ZIP archives, so the
texture DXA was never in the PhysFS search path despite being listed in
.active_mod_paths.

### Root Cause 2: depVars missing param option vars
In run_test.ps1, $depVars only contained top-level param values (e.g.
RESOLUTION=128) but NOT the per-option vars (e.g. DXA_SHA from the selected
RESOLUTION option). This meant ${DXA_SHA} in dep sha256 was never substituted,
SHA lookup failed, and the dep resolver silently skipped pushing newer versions.

## Fixes

### Phase 1: Fix depVars substitution in run_test.ps1
- [x] Merge per-option vars from scriptParams into depVars (same logic as
  Resolve-TestScript uses)

### Phase 2: Rebuild DXA with Store compression
- [x] Extract all entries from broken 128px DXA (7z x)
- [x] Re-create with 7-Zip Store compression (7z a -tzip -mx0)
- [x] Update SHA in test_mod_loading.json5
- [x] Regenerate game_data_index.txt

### Phase 3: Add mount error logging
- [x] Change con_printf in d2/misc/physfsx.c from CON_DEBUG to CON_NORMAL
- [x] Add PHYSFS_getLastError() to failure message
- [x] Add "No .active_mod_paths" message when file not found
- [x] Apply same changes to d1/misc/physfsx.c

### Phase 4: Verify
- [x] Full clean rebuild (delete .cxx + build, gradlew assembleDebug)
- [x] Push rebuilt 18.3MB DXA to device (Store compression, 1742 entries)
- [x] Run test_mod_loading with RESOLUTION=128
- [x] Result: PASS, 1424 hires textures loaded, replacement_pct=100%
- [x] Mount logging confirms both DXAs mounted successfully

## Key lesson
PhysFS does NOT support Deflate-compressed ZIP entries. Always use Store
(NoCompression / -mx0) when creating DXA files. The original
convert_d2xxl_textures.ps1 correctly uses CompressionLevel.NoCompression.
