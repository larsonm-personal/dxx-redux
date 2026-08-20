# Guide-Bot Switch and Grate Routing Plan

## Scope

- Analyze the supplied Guide-Bot trace for the first switch and final grate obstruction
- Make firing-position guidance approach switches more closely when safely reachable
- Prevent Guide-Bot paths from treating grates as traversable passages
- Add focused regression coverage and verify host and Android builds

## Work

- [x] Correlate trace records with route goals, path endpoints, and level geometry
- [x] Identify the switch standoff and grate traversal policy defects
- [x] Implement minimal routing corrections
- [x] Extend automated coverage for both reported locations
- [x] Run scoped formatting, D2 build/tests, Android build, and device validation

## Findings

- The first reported switch is Obsidian level 6 route step 3, trigger 15 in segment 270. The firing planner preferred shorter travel over a substantially closer firing point, leaving Guide-Bot in segment 274 for a long, high-angle shot
- The final reported switch is route step 5, trigger 14 in segment 506. The generated path repeatedly began with the 540 to 607 edge, while Guide-Bot remained in segment 540 and spun against that portal
- The switch route already ends in segment 270. The regression was the legacy short-path fallback replacing that valid one-point semantic path with a random wander as soon as Guide-Bot arrived
- One-point semantic paths are now retained, while legacy non-semantic goals keep their existing fallback behavior
- Semantic route movement now detects a stalled inter-segment edge and retries the same goal while excluding the blocked destination segment

## Verification

- Scoped code-quality pass completed successfully
- D2 Windows host build and all 40 native CTest tests passed
- Android debug APK built successfully for arm64-v8a, armeabi-v7a, and x86_64
- `test_obsidian_level6_guidebot_switch_grate.json5` passed all 40 steps on the Android emulator
