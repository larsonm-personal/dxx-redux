# D1/D2 Android OGL viewport extraction plan

## Goal

Remove the duplicated Android viewport scaling, cache, drawable-size, and keyboard-gap implementation from the upstream-original D1/D2 `arch/ogl/ogl.c` files while preserving menu-frame application and the engine globals used by later draw code.

## Baseline

- `d1/arch/ogl/ogl.c`: 1,895 additions, 65 deletions versus `upstream/main`
- `d2/arch/ogl/ogl.c`: 1,988 additions, 64 deletions versus `upstream/main`
- Both files contain the same roughly 80-line Android viewport/gap block
- `last_width`, `last_height`, and Android `last_kb_off` are read later in each OGL translation unit and cannot simply move into private shared state

## Work

- [x] Add `ogl_viewport_android.c/.h` with a pointer-backed viewport cache state
- [x] Move axis scaling, drawable-size policy, GL viewport cache comparison, and keyboard-gap clearing into the shared source
- [x] Retain one compact local `ogl_android_viewport` wrapper per game to supply current screen/canvas dimensions and keyboard offset
- [x] Keep the existing public wrapper name used by `OGL_VIEWPORT`
- [x] Replace direct local drawable-size and gap calls with shared operations
- [x] Preserve menu-frame viewport/gap application before `eglSwapBuffers`
- [x] Wire the source into both Android native targets
- [x] Run scoped code quality
- [x] Build all Android ABIs
- [x] Build Windows D1 and D2
- [ ] Run focused keyboard/pause viewport tests when the shared emulator is free
- [x] Record exact inherited-file reduction and update campaign documents

## Risk controls

- Keep the three engine globals authoritative through pointers in the shared state
- Preserve rounding for positive and negative coordinates and the one-pixel minimum extent rule
- Preserve the exact GL clear/scissor order for the keyboard gap
- Do not change any MSAA framebuffer selection, overlay preparation, frame begin/end, or swap ordering

## Completed result

- `d1/arch/ogl/ogl.c`: 1,895 additions to 1,827 additions versus `upstream/main`
- `d2/arch/ogl/ogl.c`: 1,988 additions to 1,920 additions versus `upstream/main`
- Exact inherited-file reduction: 136 additions
- Shared state updates the engine-owned width, height, and keyboard-offset globals through pointers while owning all private logical/physical cache fields
- Scoped code quality passed
- Android `externalNativeBuildDebug` passed for arm64-v8a, armeabi-v7a, and x86_64
- Windows D1 and D2 compiled and linked
- Focused emulator coverage remains deferred while an unrelated guidebot runner owns the sole emulator
- `git diff --check` passed
