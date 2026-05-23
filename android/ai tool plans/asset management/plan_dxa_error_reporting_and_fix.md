# Plan: DXA Error Reporting and Processing Fix

## Context
User reports .dxa mod files silently failing on phone with build 1072.
Also, black texture issue to be addressed by reverting ogl_etc2_broken approach.

## Phase 1: Add comprehensive error reporting to all file processing paths
Goal: Every unprocessed file that isn't handled by "select game files" workflow
should show a user-visible message when it fails, no matter the bug.

### Changes needed in SetupActivity.kt:
1. **DXA import**: wrap importMod calls with error reporting (Toast + importStatus)
2. **ZIP/7z extraction**: show error on failure, not just log
3. **Game file copy**: show per-file errors, not just count
4. **Download failures**: show Toast on download failure (current just shows -1 progress)
5. **General exception handler**: improve the outer catch -- show Toast in addition to importStatus

### Changes needed in ModManager.kt:
6. **importMod**: return a Result type or throw with context, so caller can report

## Phase 2: Find and fix the .dxa processing bug
- Check logcat on the phone for DXX-Mods or DXX-Setup messages
- Look for: SAF URI access issues, storage permissions, file copy failures
- Check if modsDir creation works, if .active_mod_paths is written correctly
- If the file DOES get copied but isn't mounted, check PhysFS mounting logic

## Phase 3: Revert ogl_etc2_broken / software decode approach
- Remove etc2_decode.c / etc2_decode.h (or keep for future but don't use)
- Remove ogl_etc2_broken detection from gr.c (both d1 and d2)
- Remove ogl_etc2_broken global from ogl.c (both d1 and d2)
- Remove etc2_decode.h include from ogl.c (both d1 and d2)
- Restore simple direct glCompressedTexImage2D upload in ETC2 path
- Remove etc2_decode.c from CMakeLists.txt

## Phase 4: Investigate real cause of black textures
- Compare current ETC2 upload code with a known-working version
- Check for texture state issues: mipmap, filter, format
- Check if the issue is specific to the emulator or also affects phones
- Look for recent changes that could affect texture rendering

## Status
- [x] Phase 1: Error reporting -- DONE
  - DXA import: shows Toast + importStatus on failure, logs details
  - ZIP/7z extraction: propagates errors via ZipExtractionResult.error field, shows Toast
  - Download failures: all 3 download paths show Toast on failure
  - importMod: added 0-byte file check, better logging with URI/path details
  - writeEnabledModPaths: validates mod files exist on disk before writing paths
- [x] Phase 2: DXA processing bug -- improved logging/validation, no specific bug found yet
  - Need user to try again with new error reporting to surface the actual issue
- [x] Phase 3: Revert etc2 software decode -- DONE
  - Removed etc2_decode.h includes from d1/d2 ogl.c
  - Removed ogl_etc2_broken global from d1/d2 ogl.c
  - Removed emulator detection from d1/d2 gr.c
  - Removed etc2_decode.c from CMakeLists.txt (both targets)
  - Hardware compressed upload is now the only ETC2 path
- [x] Phase 4: Black texture investigation -- DONE
  - Root cause confirmed: emulator GLES translator broken ETC2 decoder
  - Restored ogl_etc2_broken global in d1/d2 ogl.c
  - Restored SwiftShader/Android Emulator detection in d1/d2 gr.c ogl_get_verinfo()
  - ETC2 upload path guarded: skips glCompressedTexImage2D on emulator, falls back to
    base paletted textures (DXA lacks PNG fallbacks)

## Phase 5: Fix SIGSEGV at 0x0 on phone with base GOG D2 (no texture packs)
- Root cause: ogl_loadbmtexture_f Android-only early return for BM_FLAG_PAGED_OUT
  leaves bm->gltexture NULL. ogl_bindbmtex then dereferences gltexture->handle
  (offset 0) causing SIGSEGV at address 0x0
- With texture packs the hi-res path sets gltexture before the check, masking the bug
- Fix: replaced early return with on-demand piggy_bitmap_page_in() for GameBitmaps
- Added defensive NULL guard in ogl_bindbmtex after ogl_loadbmtexture
- Applied to both d1/arch/ogl/ogl.c and d2/arch/ogl/ogl.c
- [x] Phase 5: DONE -- build passes
