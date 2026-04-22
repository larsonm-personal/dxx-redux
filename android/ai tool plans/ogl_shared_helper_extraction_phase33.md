# OGL Shared Helper Extraction Phase 33

## Goal

Extract the duplicated Android TexFilt live-apply loop from D1 and D2
`arch/ogl/ogl.c` into shared native code.

## Scope

- android/app/src/main/cpp/shared/ogl_texture_android.h
- android/app/src/main/cpp/shared/ogl_texture_android.c
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [ ] Add shared Android TexFilt live-apply state and helper
- [ ] Replace duplicated D1 TexFilt live-apply block with a shared call
- [ ] Replace duplicated D2 TexFilt live-apply block with a shared call
- [ ] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored
- Reuse the existing shared texture-list helper surface instead of creating a
  new OGL subsystem
- Preserve in-place mip generation, font/NOCOLOR skip, and bind-cache reset
- Keep the helper Android-only and limited to the texture-list iteration