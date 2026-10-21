# Store screenshot progress variants

Completed on 2026-10-21

- Extended shared debug introspection for both Android engines with authored hostage positions, real level kill credit, live robot radii/liveness, explosion positions and secret-area segment centers
- Capture profiles fire native primary weapons, collect hostages through normal movement/collision and discover secrets by entering authored areas. Progress counters are never assigned
- Kept the earlier gallery intact and generated eight PNGs plus a native-progress summary table in android/store-assets/capture_20261021_progress/index.html
- Added profile/actual-state comparisons and kill/explosion evidence checks to the existing store integration validator

## Verified captures

| File | Robots destroyed | Hostages | Secrets |
| --- | --- | --- | --- |
| 03-d1-level5.png | 0 | 0/3 | 0/5 |
| 04-d1-level5.png | 2 | 2/3 | 1/5 |
| 05-d1-level18.png | 3 | 3/3 | 2/10 |
| 06-d2-level9.png | 3 | 1/3 | 1/13 |
| 07-d1-level7-boss.png | 2 | 0/3 | 1/4 |

## Validation

- Android debug x86_64 build: PASS, both D1 and D2 shared native libraries; final build 1m 35s with no new compiler warnings
- test_store_asset_pipeline.ps1 -ScreenshotsOnly -OutputDirectory android/store-assets/capture_20261021_progress: PASS, all eight PNGs, hashes, native state, graphics, progress and combat evidence
- Scoped mixed-language code quality: PASS
- Visual QA: reviewed combat frames and final eight-image contact sheet; the gallery reports exact native counts

The first D2 combat attempt exhausted energy because collision resolution displaced the camera from its requested pose. Diagnostics were retained in wait-timeout.json. Framing now re-aims from the actual settled native player/robot positions, combat selects larger authored targets, and D2 receives native weapon provisioning. The completed D1 frames were preserved while D2 and the boss/fly-out were recaptured successfully

Artifacts: live-stills.json and selections/ record profiles/counts; states/ records engine state; events/ records native weapon kill credit and explosion state. Camera protection, level warp and robot hold are capture staging, not qualified input replay demos. No edits in d1/ or d2/, progress-counter assignments, demo-oracle changes, store uploads or physical-device writes
