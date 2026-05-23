# Input demo camera wake engine fix

## Goal
- Re-evaluate the D2 guided-missile camera wake desync as a game-engine nondeterminism issue
- Remove input-demo special event handling if a deterministic engine path can replace it
- Preserve the held secondary fire replay timing fix, which matches live engine behavior
- Validate with focused builds, tests, and the affected level 9 demos

## Steps
1. Check current diffs and changed files before editing
2. Inspect D2 camera wake/render paths and replay/headless execution paths
3. Replace special demo event handling with a deterministic engine-side fix if practical
4. Keep D1/D2 held-secondary replay timing fix intact
5. Run scoped quality checks, host build, shared tests, and affected demo replays

## Status
- [x] Plan created
- [x] Current diffs checked
- [x] Engine path reviewed
- [x] Special event handling removed or justified
- [x] Validation run

## Notes
- User guidance: demos are tools to expose engine nondeterminism, not artifacts to preserve; avoid special demo codec handling when a game-engine fix is available
- The post-frame held secondary fire fix remains in scope as a legitimate engine timing alignment
- Removed the replay/recording `ai_camera_wake` event path and render-time calls to mutate AI from `wake_up_rendered_objects()`
- Added a D2 simulation-side missile camera wake pass near the end of `GameProcessFrame()` so live and headless replay use the same engine path
- The final predicate wakes close line-of-sight robots, and requires a forward cone for farther robots to avoid broad render-list over-wakes
- Final validation passed: scoped code quality, `run-windows-build.ps1 -Target both`, D1/D2 `test_input_demo_replay.exe`, and both supplied D2 level 9 demos