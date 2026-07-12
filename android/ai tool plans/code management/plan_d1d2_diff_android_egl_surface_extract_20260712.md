# Plan: D1/D2 Android EGL Surface Extraction 2026-07-12

## Goal

- Move the duplicated Android EGL initialization, surface recreation, pause-aware swap, and GLES capability query from both inherited `arch/ogl/gr.c` files into one Android-owned implementation
- Preserve the current EGL call order, counters, logging, failure flow, and GLES 1 recreation policy exactly
- Leave the inherited desktop, RPi, and `ogles_destroy` behavior unchanged

## Scope

- `d1/arch/ogl/gr.c`
- `d2/arch/ogl/gr.c`
- `android/app/src/main/cpp/shared/android_egl_surface.c`
- `android/app/src/main/cpp/shared/android_egl_surface.h`
- `android/app/src/main/cpp/CMakeLists.txt`
- This plan

## Existing work to preserve

- Preserve unrelated dirty-worktree changes and concurrent campaign tranches
- Do not edit the campaign catalog or the C03/C04 plans
- Keep the four inherited EGL handle globals in each `gr.c`
- Keep `ogles_destroy` local because it is shared with non-Android OGLES builds

## Implementation

- [x] Record the live target-file metrics and exact duplicated block boundaries
- [x] Add a pointer-backed EGL state for display, config, surface, and context handles
- [x] Store the existing texture-smash and texture-cache callbacks in that state
- [x] Move the exact Android EGL initialization body to the shared implementation
- [x] Move surface recreation and pause-aware swap behavior without changing ordering or failure flow
- [x] Preserve initial GLES 3 and recreation GLES 1 as explicit private policy constants
- [x] Move the identical Android GLES renderer, anisotropy, MSAA, and GPU-timer capability query
- [x] Wire the shared implementation into both prefixed Android OGL targets
- [x] Reduce both inherited `gr.c` files to mirrored state initialization and calls

## Behavior guardrails

- Check pause before consuming the stale-surface flag
- Preserve the swap breadcrumb cadence and swap after a recreation attempt
- Preserve detach, destroy, geometry, create, make-current, and texture-recache order
- Do not reset the recreation counter during initialization or destruction
- Do not add EGL failure early returns to the initial setup path
- Do not change the GLES 1 context-loss recreation request in this extraction
- Do not add surface locking, native-window reference changes, or context-loss corrections
- Keep `gles3_shim_init`, shader initialization, initial texture caching, and `gl_initialized` ordering unchanged

## Validation

- [x] Run scoped code quality on the new shared files and Android CMake wiring
- [x] Run static searches for stale local EGL bodies and handwritten Android surface declarations
- [x] Build and link D1 and D2 for every configured Android ABI when build trees are free
- [ ] Run Windows D1/D2 builds to verify the guarded inherited changes when practical
- [x] Run the D1 and D2 background/resume automation sequentially when an emulator is available
- [x] Run `git diff --check`
- [x] Record exact before/after inherited metrics and any residual block larger than 40 lines

## Baseline

- `d1/arch/ogl/gr.c`: `+243/-1` against `upstream/main`
- `d2/arch/ogl/gr.c`: `+244/-1` against `upstream/main`
- Core duplicated EGL lifecycle bodies: 60-line recreation, 24-line swap gate, and 71-line initialization body per game
- Additional identical Android capability query: 28 lines per game

## Result

- Complete for the extraction scope.
- Added the Android-owned `android_egl_surface.c` and `.h` implementation and compiled it independently into both prefixed OGL archives. The state points at each game's four inherited EGL handles and carries the existing texture-smash and texture-cache callbacks, logical surface dimensions, and persistent counters.
- Preserved initial GLES 3 and context-loss recreation GLES 1 as separate private constants. Static order checks confirm pause precedes stale-surface consumption, recreation precedes the final swap, and detach/destroy/geometry/create/context-recache ordering is unchanged.
- Kept renderer logging at its original location and kept anisotropy, MSAA, and GPU-timer queries at their original later location in `ogl_get_verinfo`.
- Inherited diff against `upstream/main`:
  - `d1/arch/ogl/gr.c`: `+243/-1` before, `+80/-1` after
  - `d2/arch/ogl/gr.c`: `+244/-1` before, `+81/-1` after
  - Combined inherited additions fell from 487 to 161, a reduction of 326 lines. The working patch removes 187 lines and adds 24 lines in each inherited file.
- Residual audit: neither inherited file has an added hunk larger than 40 lines. The largest remaining hunk is the mirrored 21-line pointer-state/getter block; all lifecycle and capability call-site additions are 8 lines or smaller.
- Direct Ninja builds and final shared-library links passed for D1 and D2 on `arm64-v8a`, `armeabi-v7a`, and `x86_64` using the cached `FETCHCONTENT_UPDATES_DISCONNECTED=ON` trees. The new helper emitted no warnings. Existing `gr.c` warnings remain for `gl_draw_buffer` and the local `min` helper.
- Scoped NDK clang-format and cmake-format checks passed, as did the static stale-body/order assertions and `git diff --check`. The aggregate code-quality wrapper could not execute its configured `C:\local\clang-format-20` binary under the sandbox ACL, so the directly executable NDK clang-format was used for the equivalent scoped format check.
- The signed, 16 KiB-aligned APK passed the 53-step
  `test_launch_to_automap.json5` flow sequentially in D1 and D2. Both games
  backgrounded, resumed, observed `egl_recreate_count=2`, reopened rendering,
  and completed the remaining assertions.
- Windows configuration remains environment-blocked by uncached FetchContent
  dependencies and denied network access; the Android-only shared source is not
  part of desktop targets.
