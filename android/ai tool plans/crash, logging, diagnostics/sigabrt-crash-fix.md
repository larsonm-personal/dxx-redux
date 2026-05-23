# SIGABRT crash fix - piggy bitmap cache overflow

## Status: COMPLETE

## Problem
Game crashes with `SIGABRT` during `ogl_cache_level_textures()` when entering a level from the main menu.

### Root cause
`piggy_bitmap_page_in()` at d2/main/piggy.c:1198 has a fatal `Assert()` that fires when the bitmap paging cache (2400 KB) is full. The `Assert` calls `abort()` -> SIGABRT before the graceful recovery code (piggy_bitmap_page_out_all + goto ReDoIt) can run.

On desktop, `NDEBUG` is defined in release builds so `assert()` is a no-op. On Android debug builds, assert is active.

### Crash signature
```
Signal: SIGABRT (6)
Abort message: d2/main/piggy.c:1198: assertion
  "Piggy_bitmap_cache_next+(bmp->bm_h*bmp->bm_w) < Piggy_bitmap_cache_size" failed
```

## Fix (applied)
- d2/main/piggy.c: Wrapped the uncompressed-case Assert in `#ifndef ANDROID` (line 1198)
  - Note: the RLE-compressed Assert was already commented out upstream
- d1/main/piggy.c: Same fix for both Assert calls (lines 658, 680)
- The graceful recovery path (piggy_bitmap_page_out_all + goto ReDoIt) handles the overflow properly

## Verification
- `test_launch_to_automap.json5` PASS (37/36 steps, 16183ms)
- Previously crashed consistently at ogl_cache with 2590 bitmaps

## Debug log zero output (separate finding)
The debug logging system has only ONE callsite for DLOG_TEXTURE in the entire D2 codebase (d2/arch/ogl/ogl.c:1947, ETC2 upload path). There are ZERO DLOG_GRAPHICS callsites. On the emulator, ETC2 is disabled entirely ("ETC2 disabled (emulator detected)"). Zero output is expected -- the categories exist but have no log instrumentation yet during normal operation.
