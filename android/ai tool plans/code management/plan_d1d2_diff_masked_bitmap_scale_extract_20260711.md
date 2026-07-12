# Plan: D1/D2 Masked Bitmap Scale Extraction, 2026-07-11

## Goal
- Remove the paired Android menu-only masked scaling kernel from inherited `bitblt.c` files and their inherited public graphics headers
- Make `android_menu_scale.c`, the only caller, own the private implementation

## Baseline
- `d1/2d/bitblt.c`: `+51/-9`
- `d2/2d/bitblt.c`: `+57/-9`
- `d1/include/gr.h`: `+1`; `d2/include/gr.h`: `+1`
- D1 local implementation: 46 added lines
- D2 local implementation and explanatory comment: 49 added lines
- Expected inherited-file reduction: 97 additions

## Steps
- [x] Copy the exact masked horizontal and vertical scaling behavior into private helpers in `android_menu_scale.c`
- [x] Route both masked menu-scale call sites to the private helper
- [x] Remove both `bitblt.c` copies and both inherited `gr.h` declarations
- [x] Confirm no remaining caller or declaration exists outside the Android-owned helper
- [x] Run scoped quality and diff checks
- [x] Build and link both Android games for every configured ABI
- [x] Run focused menu-scale and readable-tiny runtime coverage
- [x] Record exact metrics and update the campaign catalog

## Guardrails
- Preserve nearest-neighbor accumulation, transparency value 255, row strides, destination preservation, and allocation ownership exactly
- Keep ordinary unmasked `gr_bitmap_scale_to` in the engine graphics layer
- Do not alter crop, destination, direct-render, OGL overlay, or non-OGL fallback behavior
- Keep the new implementation private because it has no caller outside `android_menu_scale.c`
- Do not broaden this tranche into kconfig or newmenu render orchestration

## Outcome
- Moved the exact nearest-neighbor masked row and bitmap scaling loops into private `android_menu_scale.c` helpers
- Replaced both Android-owned masked call sites and removed the public engine declaration entirely
- D1 `bitblt.c` moved from `+51/-9` to `+5/-9`; D2 moved from `+57/-9` to `+8/-9`
- Both inherited `gr.h` files returned to zero additions for this API
- Exact inherited-file reduction: 97 additions
- Search confirms one private implementation and two Android-owned call sites, with no D1/D2 caller or declaration
- `git diff --check` passed with only line-ending normalization warnings
- Both Android games rebuilt and linked for arm64-v8a, armeabi-v7a, and x86_64
- The Windows wrapper remains unavailable in this managed session because vcpkg cannot write its external buildtree and fresh configure cannot discover the compiler/toolchain after retry
- Isolated runtime validation used a dedicated ADB server on port 5041 and `emulator-5560`, avoiding concurrent default-emulator work
- D2 `test_menu_scale_d2.json5` passed 16 of 16 steps
- D2 `test_readable_tiny_help_d2.json5` passed 24 of 24 steps
- D1 `test_autoselect_crash_unified.json5` passed 74 of 74 steps
