# Texture paging regression fix

- [x] Reproduce and decompose the slow texture-paging phase
- [x] Identify the regression by comparing the current path with recent history
- [x] Implement the smallest paired D1/D2 or shared Android fix
- [x] Add or extend regression coverage for the corrected behavior
- [ ] Run scoped formatting, paired host builds/tests, Android build, and emulator timing verification

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
