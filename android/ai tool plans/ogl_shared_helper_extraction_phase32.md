# OGL Shared Helper Extraction Phase 32

## Goal

Extract the duplicated Android DXA mask loader and the two small Android-only
logging helpers from D1 and D2 `arch/ogl/ogl.c` into shared native code.

## Scope

- android/app/src/main/cpp/shared/ogl_texture_android.h
- android/app/src/main/cpp/shared/ogl_texture_android.c
- android/app/src/main/cpp/shared/android_texture_debug.h
- android/app/src/main/cpp/shared/android_texture_debug.c
- android/app/src/main/cpp/shared/merged_wall_debug.h
- android/app/src/main/cpp/shared/merged_wall_debug.c
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [ ] Add shared Android DXA mask helper
- [ ] Add shared Android texture-bind logging helper
- [ ] Add shared merged-wall experiment-apply logging helper
- [ ] Replace duplicated D1 helper bodies/call sites with shared calls
- [ ] Replace duplicated D2 helper bodies/call sites with shared calls
- [ ] Run validation and record the result

## Guardrails

- Keep D1 and D2 mirrored
- Do not widen the shared OGL boundary beyond public headers or explicit local
  declarations
- Preserve existing Android logging text and DXA mask behavior
- Avoid touching the larger KTX2/ETC2 upload path in this tranche