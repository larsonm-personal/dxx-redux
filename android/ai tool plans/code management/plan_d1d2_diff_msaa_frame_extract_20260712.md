# D1/D2 Android MSAA frame lifecycle extraction plan

## Goal

Extend the existing shared Android MSAA module to own the duplicated frame bind, nesting, resolve, backing-target, and overlay-target mechanics currently embedded in both upstream-original OGL files.

## Baseline

- After C15, `d1/arch/ogl/ogl.c` has 1,827 additions and `d2/arch/ogl/ogl.c` has 1,920 additions versus `upstream/main`
- `ogl_msaa_android.c/.h` already owns FBO allocation, format selection, validation, and destruction
- Both OGL files still duplicate outer-frame creation/binding, depth balancing, resolve blit/error timing, window-backing target selection, and overlay target selection

## Work

- [x] Add shared begin-frame and end-frame operations using the existing state plus bound/depth pointers
- [x] Move resolve blit, error reporting, and timing into the shared module
- [x] Move backing and overlay framebuffer target selection into shared operations
- [x] Preserve local render context, viewport, orthographic shader restoration, and overlay GL blend/depth policy
- [x] Remove the now-redundant local create wrapper if the shared begin operation can call the existing allocator directly
- [x] Apply identical compact calls in D1 and D2
- [x] Run scoped code quality
- [x] Build all Android ABIs
- [x] Build Windows D1 and D2
- [ ] Run focused MSAA/pause/keyboard tests when the shared emulator is free
- [x] Record exact inherited-file reduction and update campaign documents

## Risk controls

- Keep `g_msaa_fbo_bound` externally visible for introspection and keep frame depth local through pointers
- Preserve outermost-only allocation/bind and first-bind color-clear semantics
- Preserve read/draw framebuffer bind order, nearest blit, error drain, unbind, and timing boundaries
- Do not move viewport, shader matrix, blend, alpha, cull, or render-context policy into the MSAA module

## Completed result

- `d1/arch/ogl/ogl.c`: 1,827 additions to 1,794 additions versus `upstream/main`
- `d2/arch/ogl/ogl.c`: 1,920 additions to 1,887 additions versus `upstream/main`
- Exact inherited-file reduction: 66 additions
- The earlier 200-250 estimate included viewport and overlay/shader policy that belongs to separate seams or must remain local; this tranche moved only the mechanically shared framebuffer lifecycle
- Create-request and resolve-error diagnostics, bind order, nesting, clear signal, unbind, and microsecond timing are preserved
- Scoped code quality passed
- Android `externalNativeBuildDebug` passed for arm64-v8a, armeabi-v7a, and x86_64
- Windows D1 and D2 compiled and linked
- Focused emulator coverage remains deferred while an unrelated guidebot runner owns the sole emulator
- `git diff --check` passed
