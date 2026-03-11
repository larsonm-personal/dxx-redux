# Plan: Integrate D1 into Android Port (Shared Code Architecture)

## TL;DR
Port D2 Android changes to D1 while keeping both d1/ and d2/ as close to upstream as possible. Centralize all *new* Android-specific files under `android/app/src/main/cpp/shared/` so they're compiled into both game .so files. Build two game .so files (`libdxx-redux-d2.so` and `libdxx-redux-d1.so`) plus shared dependency .so files (`libSDL12.so`, `libphysfs.so`, etc.). Add game selection to the launcher.

## Design Principles

1. **Minimize d1/ and d2/ changes** — these track upstream. Only add small `#ifdef ANDROID` hooks that call into shared helpers.
2. **Centralize new code** — any new C/C++ files go under `android/app/src/main/cpp/shared/` and are compiled into both .so files.
3. **No file consolidation** — files that already exist in upstream d1/ and d2/ stay separate, even if they're similar. Duplicates are acceptable because that's the upstream project's design.
4. **Shared constants OK** — shared header files for interfaces between Kotlin and C, or between the shared android C code and the game engines, are fine.
5. **Two game .so files** — `libdxx-redux-d2.so` (D2) and `libdxx-redux-d1.so` (D1). Only one is loaded per game session.
6. **Rename D2 target** — current target is `d2x-redux` (produces `libd2x-redux.so`). Rename to `dxx-redux-d2` (produces `libdxx-redux-d2.so`). D1 target is `dxx-redux-d1` (produces `libdxx-redux-d1.so`).
7. **Shared dependency .so files** — SDL, PhysFS, SDL_mixer, and LZMA are built as shared libraries, linked by both game .so files. This avoids duplicating ~3MB of dependency code per ABI (see analysis below). Header-only deps (TinySoundFont, nlohmann/json) are compiled into each game .so as before.
8. **DXX_TARGET_PREFIX for CMake** — avoid target name collisions between D1 and D2 static libraries (both define `arch_sdl`, `2d`, `3d`, etc.) by using a `${DXX_TARGET_PREFIX}` variable. This is a mechanical change to CMakeLists files and has zero effect on non-Android builds (prefix defaults to empty).
9. cleanliness: in cases where both d1+d2 are listed in a source file or other file, organize things so that d1 is first. for example, in the android cmake file, some places have d1+d2 on two successive lines, the d1 line should be first, and so on

---

## Shared Dependency Analysis

### Current state (D2 only, all static)
Stripped release .so is ~3 MB per ABI. Debug unstripped is ~10-11 MB.

### Static library sizes (per ABI)
| Library | arm64-v8a | armeabi-v7a | x86_64 | Shareable? |
|---------|-----------|-------------|--------|------------|
| libSDL12.a | ~1.5 MB | ~1.1 MB | ~1.5 MB | Yes — SHARED |
| libphysfs.a | ~1.3 MB | ~1.3 MB | ~1.3 MB | Yes — SHARED |
| SDL_mixer | ~80-150 KB | ~80-150 KB | ~80-150 KB | Yes — SHARED |
| LZMA SDK | ~50-100 KB | ~50-100 KB | ~50-100 KB | Yes — SHARED |
| TinySoundFont | ~200-300 KB | ~150-200 KB | ~200-300 KB | Yes — build as SHARED .so (clean C API) |
| nlohmann/json | ~2-3 MB per consumer (.o) | ~1.5-2 MB per consumer (.o) | ~2-3 MB per consumer (.o) | No — use LTO to shrink; leave as header-only |
| zlib | system NDK lib | system NDK lib | system NDK lib | Already shared |

Note on header-only sizes: TinySoundFont and nlohmann/json are "header-only" but compile to significant object code. TSF instantiates via `#define TSF_IMPLEMENTATION` in digi_tsf_music.c (~200-300 KB). nlohmann/json is template-heavy C++ — **every file that `#include <json.hpp>` gets ~2-3 MB of .o code**: game_introspect.cpp (~3 MB .o), game_automate.cpp (~3.5 MB .o), android_gamepad_config.cpp (~3 MB .o). However, most of this is duplicate COMDAT template instantiations that the linker (especially with LTO) strips aggressively. The .o sizes are misleading — actual contribution to the final linked .so is much smaller.

### LTO for nlohmann/json

Android NDK supports link-time optimization via `-flto=thin` (recommended over `-flto=full` for build speed). With LTO:
- The linker sees all translation units at once and eliminates unused template instantiations
- Duplicate COMDAT sections across the 3 JSON consumer files are merged to a single copy
- Dead code from nlohmann/json (unused `json::parse()` overloads, exception paths, etc.) is stripped
- Expected to reduce nlohmann/json's contribution from ~6-9 MB .o total to a fraction of that in the final .so

LTO applies to the entire .so, so all code (not just JSON) benefits. This is the simplest approach — no wrapper library, no API changes to consumer files.

### With two games: static vs shared deps

| Scenario | Per-ABI size | 3 ABIs total |
|----------|-------------|-------------|
| **All static (both games)** | D1 ~3MB + D2 ~3MB = 6MB | ~18 MB |
| **Shared C deps only** | saves ~1.5MB per ABI | saves ~4.5 MB |
| **Shared C deps + TSF .so** | saves additional ~200KB per ABI | saves ~5.1 MB |
| **+ LTO on both game .so** | further reduces each game .so (especially JSON bloat) | TBD — measure after enabling |

### Recommendation: Shared .so for C deps + TSF, LTO for JSON

Since D1 and D2 always ship together and are always built at the same time:
- No versioning risk — both game .so files link the exact same dep .so files from the same build
- ~5 MB APK savings from shared deps + TSF dedup
- LTO handles the JSON template bloat without requiring a wrapper library
- One `System.loadLibrary("dxx-redux-d2")` call handles everything — Android auto-loads .so dependencies
- The deps (SDL 1.2, PhysFS) are mature and their API is stable

### Implementation

**C deps**: Change `add_library(SDL12 STATIC ...)` to `add_library(SDL12 SHARED ...)` (and same for PhysFS, SDL_mixer, LZMA).

**TinySoundFont**: Create a new `tsf_impl.c` that does `#define TSF_IMPLEMENTATION` + `#include "tsf.h"` and `#define TML_IMPLEMENTATION` + `#include "tml.h"`. Build as `add_library(tsf SHARED tsf_impl.c)`. Then digi_tsf_music.c includes the headers without the IMPLEMENTATION defines and links against `libtsf.so`. Clean separation since TSF is a pure C library with a stable, narrow API (`tsf_load_*`, `tsf_render_float`, `tsf_note_on/off`, etc.).

**nlohmann/json**: Leave as header-only. Enable LTO (`-flto=thin`) on the game .so targets in CMake. The linker will deduplicate and strip the template bloat. No code changes needed to the 3 consumer files.

Both game targets link against the shared libs. Android packages all .so files into the APK automatically.

---

## File Architecture

### What lives where

```
android/app/src/main/cpp/
├── shared/                          # NEW — compiled into BOTH .so files
│   ├── android_jni_overlay.c/.h     # JNI helpers: send_track_name, send_level_name to Kotlin
│   ├── game_introspect.cpp/.h       # MOVED from d2/introspect/ — 100% game-generic
│   ├── game_automate.cpp/.h         # MOVED from d2/introspect/ — 100% game-generic
│   ├── digi_tsf_music.c             # MOVED from d2/arch/sdl/ — pure MIDI synth, no game refs
│   ├── rbaudio_bin.c                # MOVED from d2/arch/sdl/ — BIN/CUE CD audio, no game refs
│   ├── messagebox.c                 # MOVED from d2/arch/android/ — pure Android log stubs
│   ├── physfs_archiver_saf.c        # MOVED from d2/arch/android/ — SAF filesystem, no game refs
│   └── tsf_impl.c                   # NEW — TSF/TML implementation instantiation → builds libtsf.so
├── jni_main.c                       # Shared — calls extern main(), same for both games
├── jni_saf.c                        # Shared — pure SAF I/O
├── jni_music_control.c              # Shared — wraps rbaudio.h (identical API in D1/D2)
├── android_surface.c                # Shared — SDL framebuffer blitting
├── android_input.c                  # Shared — touch/key injection (uses generic engine globals)
├── android_gamepad_config.cpp       # Shared — gamepad remapping
├── extract/                         # Already shared — no game refs
│   ├── cue_parser.c, iso9660_reader.c, sow_extract.c, ...
│   └── jni_disc_import.c, jni_gog_import.c
│
d2/main/track_names.c               # Stays — D2 track name table + calls shared JNI helpers
d1/main/track_names.c               # NEW — D1 track name table + calls shared JNI helpers
d2/introspect/                       # REMOVED (files move to shared/)
d2/arch/android/                     # REMOVED (files move to shared/)
d1/arch/android/                     # NOT NEEDED (shared/ handles it)
d1/introspect/                       # NOT NEEDED (shared/ handles it)
```

### Files that move from d2/ to shared/

| Current location | New location | Reason it's shareable |
|------------------|--------------|-----------------------|
| d2/introspect/game_introspect.cpp | shared/game_introspect.cpp | Uses only common engine globals (Players[], ConsoleObject, Game_wind, Screen_mode, etc.) — all identical in D1/D2 |
| d2/introspect/game_introspect.h | shared/game_introspect.h | Header only |
| d2/introspect/game_automate.cpp | shared/game_automate.cpp | Generic JSON script parser + SDL key injection. No game-specific logic |
| d2/introspect/game_automate.h | shared/game_automate.h | Header only |
| d2/arch/sdl/digi_tsf_music.c | shared/digi_tsf_music.c | Pure TinySoundFont MIDI synth. Uses only generic headers (hmp.h, args.h, SDL.h). Zero game globals |
| d2/arch/sdl/rbaudio_bin.c | shared/rbaudio_bin.c | BIN/CUE CD audio playback. Uses generic PhysFS + track_names.h. No game globals |
| d2/arch/android/messagebox.c | shared/messagebox.c | 12 lines: `__android_log_print()` stubs. Zero game dependencies |
| d2/arch/android/physfs_archiver_saf.c | shared/physfs_archiver_saf.c | PhysFS archiver for Android SAF. Pure filesystem code |

### New shared files to create

| File | Purpose |
|------|---------|
| shared/android_jni_overlay.c | Extract JNI call wrappers from track_names.c: `android_send_track_name()`, `android_send_level_name()`. Both d1 and d2 track_names.c call these instead of doing JNI directly |
| shared/android_jni_overlay.h | Declares the JNI overlay helpers |

### Compatibility verification

All key structs/enums needed by the shared files are **identical** between D1 and D2:

| Symbol | D1 | D2 | Compatible? |
|--------|----|----|-------------|
| `PlayerCfg.ControlType` | ✓ | ✓ | Yes |
| `PlayerCfg.AutomapFreeFlight` | ✓ | ✓ | Yes |
| `CONTROL_USING_JOYSTICK` | `= 1` | `= 1` | Yes |
| `GameCfg.MusicType`, `MUSIC_TYPE_REDBOOK` | ✓, `= 2` | ✓, `= 2` | Yes |
| `Players[].energy/shields/score/lives` | ✓ | ✓ | Yes |
| `ConsoleObject->pos`, `segnum` | ✓ | ✓ | Yes |
| `Game_wind`, `Screen_mode`, `window_get_front()` | ✓ | ✓ | Yes |
| `newmenu_handler/listbox_handler` signatures | ✓ | ✓ | Yes |
| `hmp.h` API | ✓ | ✓ | Yes |

---

## Phase 1: Reorganize Shared Files + Rename D2 Target (D2 only — no functional changes)

Move files from d2/ to shared/, rename the D2 target, update CMakeLists.txt paths, verify D2 still builds and works. This is a pure refactor — no D1 work yet.

### 1A. Rename D2 target: `d2x-redux` → `dxx-redux-d2`

The current target name `d2x-redux` produces `libd2x-redux.so`. Rename to `dxx-redux-d2` → `libdxx-redux-d2.so` for consistency with the planned `dxx-redux-d1`.

| # | File | Change |
|---|------|--------|
| 1 | d2/main/CMakeLists.txt | Change `add_library(d2x-redux SHARED ...)` → `add_library(dxx-redux-d2 SHARED ...)` and `add_executable(d2x-redux ...)` → `add_executable(dxx-redux-d2 ...)`. Update all `target_*` commands (~20 references to `d2x-redux` in this file) |
| 2 | d2/CMakeLists.txt | Update `add_dependencies(d2x-redux ...)` → `add_dependencies(dxx-redux-d2 ...)` (~6 references). Update `project(d2x-redux ...)` → `project(dxx-redux-d2 ...)` |
| 3 | android/app/src/main/cpp/CMakeLists.txt | Update all `target_*(d2x-redux ...)` → `target_*(dxx-redux-d2 ...)` (~15 references) |
| 4 | android/app/src/main/java/.../MainActivity.kt | `System.loadLibrary("d2x-redux")` → `System.loadLibrary("dxx-redux-d2")` |
| 5 | android/app/src/main/java/.../DiscImportBridge.kt | `System.loadLibrary("d2x-redux")` → `System.loadLibrary("dxx-redux-d2")` |
| 6 | android/app/src/main/java/.../GogImportBridge.kt | `System.loadLibrary("d2x-redux")` → `System.loadLibrary("dxx-redux-d2")` |
| 7 | android/app/src/main/java/.../NativePilotPatcher.kt | `System.loadLibrary("d2x-redux")` → `System.loadLibrary("dxx-redux-d2")` |

Note: `d2/vcpkg.json` name field, `contrib/packaging/` scripts, desktop .ini/.desktop files, and SHAREPATH can optionally keep the old `d2x-redux` name — those are desktop-only and don't affect Android.

### 1B. Move files to shared/

| # | Task | Details |
|---|------|---------|
| 8 | Create `android/app/src/main/cpp/shared/` directory | |
| 9 | Move `d2/introspect/game_introspect.cpp` + `.h` → `shared/` | Delete d2/introspect/ dir after |
| 10 | Move `d2/introspect/game_automate.cpp` + `.h` → `shared/` | |
| 11 | Move `d2/arch/sdl/digi_tsf_music.c` → `shared/` | |
| 12 | Move `d2/arch/sdl/rbaudio_bin.c` → `shared/` | |
| 13 | Move `d2/arch/android/messagebox.c` → `shared/` | Delete d2/arch/android/ dir after |
| 14 | Move `d2/arch/android/physfs_archiver_saf.c` → `shared/` | |
| 15 | Extract JNI helpers from `d2/main/track_names.c` → create `shared/android_jni_overlay.c` + `.h` | d2/main/track_names.c then calls `android_send_track_name()` / `android_send_level_name()` |
| 16 | Update `android/app/src/main/cpp/CMakeLists.txt` — change source paths from d2/ to shared/ | |

### 1C. Verify

| # | Task |
|---|------|
| 17 | Build D2, verify all 3 ABIs pass: `.\gradlew.bat assembleDebug` |
| 18 | Test D2 on device — verify overlays, introspection, music all still work |

---

## Phase 2: D1 Engine Changes (Port #ifdef ANDROID blocks)

Add the minimal `#ifdef ANDROID` blocks to D1 source files. Most of the heavy lifting is already in shared files — D1 just needs the same small hooks D2 has.

### 2A. Core Platform Files (must-have to boot on Android)

| # | D1 File | Change | Size |
|---|---------|--------|------|
| 12 | d1/main/inferno.c | Add ANDROID init hook (PhysFS Android init, logging macros) | ~5 lines |
| 13 | d1/arch/sdl/gr.c | Add 4 missing ANDROID blocks: android_surface_blit, palette cache invalidation, skip VideoModeOK, JNI screen accessors | ~15 lines total |
| 14 | d1/arch/ogl/gr.c | Add EGL surface init, GLES includes, JNI screen accessors | ~25 lines total |
| 15 | d1/arch/sdl/mouse.c | Add absolute touch positioning (1 block, ~7 lines) | ~7 lines |
| 16 | d1/arch/sdl/joy.c | Add virtual gamepad init + axis/button name tables | ~12 lines |
| 17 | d1/misc/physfsx.c | Add Android PhysFS path initialization | ~10 lines |

### 2B. UI & Menu Changes (touch support)

| # | D1 File | Change | Size |
|---|---------|--------|------|
| 18 | d1/main/newmenu.c | Add missing blocks: palette caching, deferred button toggle, drag-scroll, keyboard show/hide | D1 already has 8 ANDROID blocks; add ~2-3 more |
| 19 | d1/main/menu.c | Add android_apply_gamepad_defaults() call | ~3 lines |
| 20 | d1/main/automap.c | Add logging macros + touch automap control merge | ~25 lines |

### 2C. Audio System

| # | D1 File | Change | Size |
|---|---------|--------|------|
| 21 | d1/arch/sdl/digi_mixer.c | Add Android audio buffer/sample rate config, logging | ~12 lines |
| 22 | d1/arch/sdl/rbaudio.c | Add Android redbook audio hook | ~3 lines |

Note: `digi_tsf_music.c` and `rbaudio_bin.c` are in shared/ — no D1 copies needed.

### 2D. Input & Config

| # | D1 File | Change | Size |
|---|---------|--------|------|
| 23 | d1/main/config.c | Add playsave.h include + android_apply_initial_defaults block | ~20 lines |
| 24 | d1/main/kconfig.c + .h | Add `kconfig_fill_joy_settings`, `kconfig_fill_kb_settings` functions | ~30 lines |
| 25 | d1/main/playsave.c + .h | Add player save Android hooks | ~20 lines |

### 2E. D1-Specific New Files

| # | File | Location | Notes |
|---|------|----------|-------|
| 26 | track_names.c + .h | d1/main/ | D1-specific track name table (D1 CD has different tracks). Calls shared `android_send_track_name()` / `android_send_level_name()` |
| 27 | LoadLevel hook | d1/main/gameseq.c | Add `#include "track_names.h"`, call `level_overlay_notify()` after `Current_level_num` assignment (~line 637) |

---

## Phase 3: D1 Build System

### 3A. DXX_TARGET_PREFIX — Resolve Target Name Collisions

Both D1 and D2 define static library targets with the same names: `arch_sdl`, `arch_ogl`, `2d`, `3d`, `iff`, `maths`, `mem`, `misc`, `texmap`, `xmodel`, `ui`. CMake doesn't allow duplicate target names.

**Solution: `DXX_TARGET_PREFIX` variable.** Each CMakeLists.txt uses `${DXX_TARGET_PREFIX}` before target names. The Android CMakeLists sets the prefix before each `add_subdirectory` call.

This is the recommended approach because:
- All changes are mechanical (search-replace `add_library(2d` → `add_library(${DXX_TARGET_PREFIX}2d`)
- Non-Android builds are unaffected (prefix defaults to empty string)
- Self-maintaining: when D1 adds a new source file, it automatically gets the prefix
- No ExternalProject complexity, no CMake meta-programming tricks
- ~27 lines changed across d1/ CMakeLists files, ~30 lines across d2/ CMakeLists files

| # | File | Change |
|---|------|--------|
| 28 | d1/2d/CMakeLists.txt | `add_library(2d ...)` → `add_library(${DXX_TARGET_PREFIX}2d ...)` |
| 29 | d1/3d/CMakeLists.txt | Same pattern |
| 30 | d1/arch/sdl/CMakeLists.txt | Same pattern for `arch_sdl` target |
| 31 | d1/arch/ogl/CMakeLists.txt | Same pattern for `arch_ogl` target |
| 32 | d1/iff/CMakeLists.txt | Same pattern |
| 33 | d1/maths/CMakeLists.txt | Same pattern |
| 34 | d1/mem/CMakeLists.txt | Same pattern |
| 35 | d1/misc/CMakeLists.txt | Same pattern |
| 36 | d1/texmap/CMakeLists.txt | Same pattern |
| 37 | d1/xmodel/CMakeLists.txt | Same pattern |
| 38 | d1/ui/CMakeLists.txt | Same pattern |
| 39 | d1/CMakeLists.txt | Update `add_dependencies(d1x-redux ...)` to use `${DXX_TARGET_PREFIX}` for all dep names |
| 40 | d1/main/CMakeLists.txt | Update `target_link_libraries` to use `${DXX_TARGET_PREFIX}` for all dep names. Add `if(ANDROID) add_library(dxx-redux-d1 SHARED ...)` block; add track_names.c to sources |
| 41 | d2/2d/CMakeLists.txt through d2/xmodel/CMakeLists.txt | Same mechanical prefix changes (~14 files) |
| 42 | d2/CMakeLists.txt | Update `add_dependencies(dxx-redux-d2 ...)` to use prefixed dep names |
| 43 | d2/main/CMakeLists.txt | Update `target_link_libraries` to use prefixed dep names |

Example of a typical sub-CMakeLists change (e.g., d1/2d/CMakeLists.txt):
```cmake
# Before:
add_library(2d STATIC ${2D_SOURCES})
# After:
add_library(${DXX_TARGET_PREFIX}2d STATIC ${2D_SOURCES})
```

Non-Android builds work unchanged because `DXX_TARGET_PREFIX` is empty by default.

### 3B. D1 CMakeLists.txt Android Block

| # | File | Change |
|---|------|--------|
| 44 | d1/CMakeLists.txt | Add `if(ANDROID) ... endif()` block: define ANDROID, INTROSPECT_ON, suppress SDL_mixer |
| 45 | d1/main/CMakeLists.txt | Add `if(ANDROID) add_library(dxx-redux-d1 SHARED ...)` block; add track_names.c to sources |
| 46 | d1/arch/sdl/CMakeLists.txt | Nothing needed for source changes — digi_tsf_music.c and rbaudio_bin.c are in shared/ now |

### 3C. Top-Level Android CMakeLists.txt

| # | Task |
|---|------|
| 47 | Add `D1_SRC` path: `set(D1_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../d1")` |
| 48 | Set prefix and add D2: `set(DXX_TARGET_PREFIX "d2_")` then `add_subdirectory("${D2_SRC}" ...)` |
| 49 | Set prefix and add D1: `set(DXX_TARGET_PREFIX "d1_")` then `add_subdirectory("${D1_SRC}" ...)` |
| 50 | Add shared sources to `dxx-redux-d1` target — same source files and include dirs as `dxx-redux-d2` but with D1_SRC paths |
| 51 | Link `dxx-redux-d1` against shared deps: SDL12, physfs, android, log, EGL, GLESv1_CM, nlohmann_json, lzma_sdk, z |
| 52 | Add INTROSPECT_ON compile definition for `dxx-redux-d1` and its `d1_arch_sdl` |

### 3D. Convert Dependencies to Shared Libraries

Change the dependency builds from STATIC to SHARED so both game .so files share them:

| # | Task |
|---|------|
| 53 | Change SDL12 from STATIC to SHARED: `add_library(SDL12 SHARED ...)` |
| 54 | Change PhysFS from STATIC to SHARED: set `PHYSFS_BUILD_SHARED ON` and `PHYSFS_BUILD_STATIC OFF` |
| 55 | Change SDL_mixer from STATIC to SHARED |
| 56 | Change LZMA SDK from STATIC to SHARED |
| 57 | Create `shared/tsf_impl.c` — contains only `#define TSF_IMPLEMENTATION` + `#include "tsf.h"` + `#define TML_IMPLEMENTATION` + `#include "tml.h"`. Build as `add_library(tsf SHARED shared/tsf_impl.c)`. Add `target_include_directories(tsf PRIVATE ${_TSF_DIR})` |
| 58 | Update `shared/digi_tsf_music.c` — remove the `#define TSF_IMPLEMENTATION` and `#define TML_IMPLEMENTATION` lines (keep only the `#include` lines). Link digi_tsf_music consumers against `tsf` shared lib |
| 59 | Enable LTO on both game .so targets: `set_property(TARGET dxx-redux-d2 PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)` (and same for d1). CMake translates this to `-flto=thin` for Clang/NDK. This deduplicates nlohmann/json template instantiations across the 3 consumer TUs and strips dead code |
| 60 | Verify Android packages all .so files into APK — Gradle does this automatically for .so outputs |
| 61 | Verify `System.loadLibrary("dxx-redux-d2")` auto-loads dependency .so files — Android's linker resolves .so deps from the APK's lib directory |

Note: if shared deps cause unexpected issues (e.g., symbol visibility problems, build system complexity), falling back to static linking is trivial — just revert the STATIC/SHARED flag. If LTO causes build time or correctness issues, it can be disabled independently — just remove the `INTERPROCEDURAL_OPTIMIZATION` property.

### 3E. Gradle Changes

| # | Task |
|---|------|
| 62 | No Gradle changes expected — the NDK will find all .so files automatically if CMake builds them |

---

## Phase 4: Launcher UI Changes

### 4A. Game Selection Logic (SetupActivity.kt)

| # | Task |
|---|------|
| 63 | Add `selectedGame: String` state to SetupActivity. Persist in SharedPreferences |
| 64 | Auto-selection: only D2 ready → D2; only D1 ready → D1; both → show chooser, remember last |
| 65 | Add D1/D2 toggle UI in the launch area (tabs, radio buttons, or segmented control) |

### 4B. Launch Flow

| # | Task |
|---|------|
| 66 | Pass selected game as Intent extra to MainActivity: `intent.putExtra("game", "d1")` |
| 67 | MainActivity reads extra, loads correct .so: `System.loadLibrary(if (game == "d1") "dxx-redux-d1" else "dxx-redux-d2")` |
| 68 | **Config isolation**: Each game uses its own write directory via PhysFS. Set write dir to `files/d1/` or `files/d2/` based on which .so is loaded. This keeps descent.cfg, player files, and save games separate |

---

## Phase 5: Regression Tests

### 5A. Current State

- 8 D1 regression specs already exist
- Test infrastructure already handles D1 (title screen Escape key difference, D1 file routing)
- D1 tests currently fail at "crash on launch" because D1 .so doesn't exist — Phase 2+3 fixes this

### 5B. Changes Needed

| # | Task |
|---|------|
| 69 | Update `run_extract_test.ps1` to pass game selection to the app (Intent extra or config file) |
| 70 | Verify introspect.sh works for D1 (JSON schema should be identical since game_introspect is shared) |
| 71 | Run all 8 D1 regression specs — expect them to pass once D1 boots |
| 72 | Add D1-specific game scripts to `android/game_scripts/` for level navigation tests |

---

## Summary of d1/ and d2/ Changes

### Changes to d2/ (rename + prefix + moving files out)

| Action | Files |
|--------|-------|
| Rename target | d2/main/CMakeLists.txt — `d2x-redux` → `dxx-redux-d2` (~20 references) |
| Rename target | d2/CMakeLists.txt — `d2x-redux` → `dxx-redux-d2` (~6 references), project name |
| Add prefix | d2/2d/, d2/3d/, d2/arch/sdl/, d2/arch/ogl/, d2/iff/, d2/maths/, d2/mem/, d2/misc/, d2/texmap/, d2/xmodel/, d2/ui/, d2/libmve/, d2/editor/ CMakeLists.txt — add `${DXX_TARGET_PREFIX}` to target names (~14 files, 1 line each) |
| Update deps | d2/CMakeLists.txt, d2/main/CMakeLists.txt — use `${DXX_TARGET_PREFIX}` in add_dependencies/target_link_libraries |
| Delete | d2/introspect/ (game_introspect.cpp, game_introspect.h, game_automate.cpp, game_automate.h) |
| Delete | d2/arch/android/ (messagebox.c, physfs_archiver_saf.c) |
| Delete | d2/arch/sdl/digi_tsf_music.c, d2/arch/sdl/rbaudio_bin.c |
| Modify | d2/main/track_names.c — replace inline JNI code with calls to shared/android_jni_overlay.h helpers |

### Changes to d1/ (prefix + new Android hooks)

| Action | Files | Lines added |
|--------|-------|-------------|
| Add prefix | d1/2d/, d1/3d/, d1/arch/sdl/, d1/arch/ogl/, d1/iff/, d1/maths/, d1/mem/, d1/misc/, d1/texmap/, d1/xmodel/, d1/ui/ CMakeLists.txt — add `${DXX_TARGET_PREFIX}` to target names (~11 files, 1 line each) | ~11 |
| Update deps | d1/CMakeLists.txt, d1/main/CMakeLists.txt — use `${DXX_TARGET_PREFIX}` in add_dependencies/target_link_libraries | ~20 |
| Modify | d1/CMakeLists.txt — add `if(ANDROID)` block | ~15 |
| Modify | d1/main/CMakeLists.txt — add `if(ANDROID) add_library(dxx-redux-d1 SHARED ...)` | ~10 |
| Modify | d1/main/inferno.c | ~5 |
| Modify | d1/arch/sdl/gr.c | ~15 |
| Modify | d1/arch/ogl/gr.c | ~25 |
| Modify | d1/arch/sdl/mouse.c | ~7 |
| Modify | d1/arch/sdl/joy.c | ~12 |
| Modify | d1/misc/physfsx.c | ~10 |
| Modify | d1/main/newmenu.c | ~15 |
| Modify | d1/main/menu.c | ~3 |
| Modify | d1/main/automap.c | ~25 |
| Modify | d1/arch/sdl/digi_mixer.c | ~12 |
| Modify | d1/arch/sdl/rbaudio.c | ~3 |
| Modify | d1/main/config.c | ~20 |
| Modify | d1/main/kconfig.c + .h | ~30 |
| Modify | d1/main/playsave.c + .h | ~20 |
| Modify | d1/main/gameseq.c | ~3 |
| Create | d1/main/track_names.c + .h | ~80 (D1 track table + overlay notify) |
| **Total** | ~30 files modified, 2 files created | ~360 lines |

Note: The prefix changes are mechanical (1 line per CMakeLists file) and have zero effect on non-Android builds.

Note: these D1 changes are all small `#ifdef ANDROID` blocks that call into shared helpers. The D1 track_names files are the only substantial new files, and they're small (track name table + a few wrapper functions).

### Kotlin loadLibrary calls (Phase 1A rename)

4 Kotlin files currently call `System.loadLibrary("d2x-redux")`:
- MainActivity.kt
- DiscImportBridge.kt
- GogImportBridge.kt
- NativePilotPatcher.kt

All change to `System.loadLibrary("dxx-redux-d2")`. In Phase 4, MainActivity.kt changes further to select between `"dxx-redux-d1"` and `"dxx-redux-d2"`. The other 3 (extraction bridges, pilot patcher) use D2-only functionality, but may need D1 variants or a shared approach later.

### Shared files (new under android/)

| File | Lines (est.) | Source |
|------|-------------|--------|
| shared/game_introspect.cpp + .h | ~400 | Moved from d2/introspect/ |
| shared/game_automate.cpp + .h | ~350 | Moved from d2/introspect/ |
| shared/digi_tsf_music.c | ~600 | Moved from d2/arch/sdl/ |
| shared/rbaudio_bin.c | ~500 | Moved from d2/arch/sdl/ |
| shared/messagebox.c | ~12 | Moved from d2/arch/android/ |
| shared/physfs_archiver_saf.c | ~300 | Moved from d2/arch/android/ |
| shared/android_jni_overlay.c + .h | ~40 | Extracted from d2/main/track_names.c |

---

## Inline Block Analysis (d1/d2 changes that must stay inline)

The following `#ifdef ANDROID` blocks CANNOT be extracted to shared files — they modify control flow or use file-local variables. These are the changes that must exist as small blocks in both d1/ and d2/ upstream-tracking files:

| Block | Reason it stays inline | Size |
|-------|----------------------|------|
| inferno.c logging macros | 1-line #include + macro definition | 3 lines |
| gr.c `android_surface_blit(canvas)` call | Injected into `gr_flip()` | 1 line |
| gr.c palette cache invalidation | Modifies local `gr_palette_step_up` | 3 lines |
| gr.c skip VideoModeOK | Guards SDL call | 3 lines |
| ogl/gr.c EGL surface init | Uses local ANativeWindow setup | 16 lines |
| mouse.c absolute positioning | Modifies local mouse state | 7 lines |
| joy.c virtual gamepad init | Replaces SDL joystick init block | 12 lines |
| physfsx.c Android paths | Replaces ~/.d2x-redux path logic | 10 lines |
| config.c initial defaults | Sets PlayerCfg/GameCfg fields | 16 lines |
| newmenu.c palette caching | SDL lifecycle workaround | 15 lines |
| newmenu.c deferred toggle | Touch drag detection | 21 lines |
| newmenu.c keyboard show/hide | JNI call inline | 10 lines |
| automap.c touch control merge | Merges volatile touch input | 15 lines |
| digi_mixer.c buffer config | Android audio params | 12 lines |
| kconfig.c fill_joy_settings | Standalone function behind #ifdef | 15 lines |
| playsave.c patch_keysettings | Binary format manipulation | 19 lines |

Total inline Android code per game: ~180 lines spread across ~16 files. This is the irreducible minimum — these blocks touch file-local variables or modify control flow that can't be factored into a called function.

---

## Execution Order

1. **Phase 1A** (rename D2 target) — rename `d2x-redux` → `dxx-redux-d2` everywhere, verify build
2. **Phase 1B** (reorganize shared files) — move files to shared/, verify D2 still works
3. **Phase 3A** (DXX_TARGET_PREFIX) — mechanical prefix changes in d1/ and d2/ CMakeLists
4. **Phase 3B-3E** (D1 build system) — get D1 CMake building alongside D2, convert deps to shared
5. **Phase 2** (D1 engine changes) — port #ifdef blocks, iterating with builds to catch errors
6. **Phase 4** (launcher UI) — game selection
7. **Phase 5** (tests) — verify everything works

Phases 1 and 3 can be verified with builds alone. Phase 2 requires device testing. Phase 4 needs UI testing. Phase 5 validates the full pipeline.

---

## Risks

| Risk | Mitigation |
|------|------------|
| Shared dep .so complexity | If switching SDL/PhysFS to SHARED causes symbol visibility or linker issues, revert to STATIC — trivial one-word change, costs ~5 MB APK |
| LTO build issues | LTO can increase build time and occasionally expose ODR violations. If it causes problems, disable it — the JSON bloat remains but the game works fine |
| DXX_TARGET_PREFIX breaks desktop builds | Prefix defaults to empty — zero behavior change on non-Android. Test desktop build after the mechanical changes |
| Target rename breaks references | Grep for `d2x-redux` after rename; there are exactly 53 known references (documented in Phase 1A) |
| D1 has subtle header differences not caught above | Build early, fix as discovered. The struct compatibility check covers the main shared code paths |
| Moving files from d2/ breaks git blame | Use `git mv` for history tracking; the files are all new to this project anyway (not from upstream) |
| Shared game_introspect.cpp may need D2-specific fields later | Use `#ifdef D2` for any future D2-only fields; the shared file compiles against whichever game's headers are on the include path |
| Android linker doesn't auto-load shared dep .so | Unlikely for .so files packaged in the APK's lib dir; if it fails, add explicit `System.loadLibrary()` calls for each dep before the game lib |
