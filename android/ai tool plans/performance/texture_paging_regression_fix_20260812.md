# Texture paging regression fix

- [x] Reproduce and decompose the slow texture-paging phase
- [x] Identify the regression by comparing the current path with recent history
- [x] Implement the smallest paired D1/D2 or shared Android fix
- [x] Add or extend regression coverage for the corrected behavior
- [x] Run scoped formatting, paired host builds/tests, Android build, and emulator timing verification

## Cause and fix

- The first replacement-texture lookup recursively indexed the complete PhysicsFS
  namespace to provide case-insensitive matching. App-private imported sets, route
  caches, save/support data, and other unrelated trees therefore became part of
  synchronous level loading.
- Texture lookup requests only address bare replacement-archive entries or paths
  beneath `textures/`. The index now scans root entries but descends only into the
  case-insensitive `textures` directory.
- The Profiling category now records the bounded index time and visited/indexed
  counts as `loadprof_v=1 type=texture_index`.

## Validation

- Scoped code-quality formatting and the replacement-texture limit regression
  test pass.
- Windows D1 and D2 builds pass; host tests pass for D1 (32/32) and D2 (38/38).
- The Android debug APK builds for arm64-v8a, armeabi-v7a, and x86_64.
- Emulator launcher automation passes for a fresh D2 level and the unified D2
  save/load dispatch flow.
- On the same emulator, the cold D2 level load improved from 1,800,099 us to
  1,413,546 us (21.5%), and the texture phase improved from 1,694,518 us to
  1,301,551 us (23.2%). The replacement-texture index itself is now bounded at
  24,289 us while visiting 887 root entries and no unrelated directories.
