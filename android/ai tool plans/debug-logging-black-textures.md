# Debug Logging and Black Texture Diagnostics

## Problem
All 3D textures are black when using highres KTX2/ETC2 texture pack on both phone and emulator.

## Hypotheses
1. .dxa mount failure after rename (low probability - issue predates rename)
2. Silent glCompressedTexImage2D failure (most likely)
3. Shader sampling issue (texture enable state, alpha channel)
4. Texture format mismatch (RGB8 vs RGBA8 ETC2)
5. Texture coordinate or UV issue

## Implementation

### Phase 1: Extensible Debug Logging Facility [DONE]
- Created `debug_log_categories.h` (shared C/Kotlin constants: NETWORK=0, GRAPHICS=1, TEXTURE=2)
- Created `debug_log.h` + `debug_log.c` (C-side per-category volatile flags, JNI bridge)
- Created `DebugLogCategory.kt` (Kotlin mirror of C constants)
- Created `DebugLog.kt` (multi-category logger, migrates old net_logging_enabled pref)
- Refactored `NetLog.kt` to thin delegation wrapper
- Updated `CMakeLists.txt`, `jni_main.c`, `MainActivity.kt`, `SetupActivity.kt`

### Phase 2: glGetError Diagnostics [DONE]
- D2 ogl.c: glGetError check after glCompressedTexImage2D + debug_log(DLOG_TEXTURE)
- D1 ogl.c: same, inside mip upload loop (logs level number too)
- Errors go to con_printf(CON_URGENT) + logcat + debug_log

### Phase 3: GLES3 Shim Debug Modes [DONE]
- Added `volatile int gles3_shim_debug_mode` (settable via JNI nativeSetDebugFlag "gfx_mode")
- Fragment shader modes: 0=normal, 1=show alpha, 2=vertex color only, 3=texcoord viz
- Uniform `uDebugMode` uploaded in flush_state()

### Phase 4: UI [DONE]
- Replaced NetworkLoggingSection with DebugLoggingSection in AdvancedSettingsPage.kt
- Per-category toggle switches (Network, Graphics, Texture)
- Same file list/export/delete functionality
- Added `debuglog_exports` to FileProvider paths.xml

### Phase 5: Build and Verify [DONE]
- Build passes clean
- ktlint auto-fix applied (only pre-existing SetupActivity max-line-length remains)
- clang-format applied to all C files

## Next Steps
- Deploy to emulator/device and enable Texture logging
- Check logcat for "ETC2 upload FAILED" or "DXX-TEX" errors
- Try shim debug modes (1=alpha, 2=no-tex, 3=texcoord) to narrow down
