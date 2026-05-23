# D1 render crash alignment investigation

## background

- The supplied xCrash tombstone for the D1 startup crash reports `SIGBUS/BUS_ADRALN`
- The faulting frame is `g3_draw_tmap`
- Address resolution maps `pc 0x001b3a9e` to `d1/arch/ogl/ogl.c:1253`
- The exact failing load is `texcoord_array[index2] = f2glf(uvl_list[c].u)`
- The fault address ends in `...0e`, which is not 4-byte aligned

## local hypothesis

- D1 is passing `g3_draw_tmap()` a `g3s_uvl *` that points into raw polymodel bytecode
- `d1/3d/interp.c` and `d2/3d/interp.c` both cast `p+30+((nv&~1)+1)*2` directly to `g3s_uvl *`
- On ARM, that buffer is only guaranteed to be 2-byte aligned, so reading or writing `fix` members through `g3s_uvl` can trap

## plan

### phase 1 - prove the faulting field [done]

- Resolve the tombstone PC against the unstripped Android `libdxx-redux-d1.so`
- Confirm the crash is the `uvl_list[c].u` load inside `g3_draw_tmap`

### phase 2 - remove unaligned UVL access [done]

- In both `d1/3d/interp.c` and `d2/3d/interp.c`, copy model UVLs into aligned local storage before setting `l` or passing them to draw helpers
- Keep the change local to the polymodel interpreter instead of widening `WORDS_NEED_ALIGNMENT` across the whole codebase
- Use the aligned triangle-local UVL buffer in morphing draws instead of the raw model-data pointer

### phase 3 - validate [done, repro pending]

- Done. `./gradlew.bat :app:externalNativeBuildDebug --no-daemon` passed after the interp fix
- Done. Windows host validation via `run-windows-build.ps1 -Target both` passed after the interp fix
- Done. Scoped code-quality pass was rerun on the touched sources
- Pending. Repro the same D1 startup flow on ARM and confirm the BUS_ADRALN crash is gone or moves to a new site
