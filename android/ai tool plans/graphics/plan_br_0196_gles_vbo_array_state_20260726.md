# BR-0196 preserve VBO-backed arrays through the GLES compatibility shim

## Goal

Preserve valid vertex-buffer-backed array state across the GLES compatibility
shim so buffered draws use the intended offsets and buffer bindings without
regressing client-memory arrays.

## Plan

- [x] Read repository instructions and the complete BR-0196 finding
- [x] Compare the frozen and live shim, VBO binding, array capture, and draw
      paths
- [x] Define explicit client-memory and VBO-backed array-state semantics
- [x] Implement the smallest shared fix and add focused VBO/client-array
      regressions
- [x] Run scoped code quality, focused graphics tests, native suites, and
      Android ABI builds
- [x] Finalize BR-0196 and move its complete finding and disposition entry to
      the done ledger

## Verification

- Frozen-to-live trace: PASS; the live shim still treated legal VBO offsets as
  client-memory pointers
- Focused `gles3_shim_array_source_tests`: PASS for client arrays, same-VBO
  interleaved offsets, inactive stale state, mixed sources, and differing VBOs
- `test_gles3_shim_vbo_arrays.json5`: PASS on D1 and D2 with real interleaved
  VBO draws at position offset zero, texture offset 12, `first=0`, and
  `first=3`; both produced the expected opaque white framebuffer sample with
  no GL error
- Existing D1 unified newmenu/game client-array regression: PASS
- Existing D2 merged-wall snapshot external four-attribute regression: PASS
- Automation catalog validation: PASS
- Scoped code quality: PASS
- Native support suite: PASS, all 14 tests
- `:app:assembleDebug`: PASS for arm64-v8a, armeabi-v7a, and x86_64 with no new
  compiler warnings
