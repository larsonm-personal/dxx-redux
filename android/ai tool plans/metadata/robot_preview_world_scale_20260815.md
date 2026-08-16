# Robot Preview World Scale

## Objective

Make robot preview size reflect model world size for base-game and modded robots without robot-ID-specific adjustments, while fitting models that are physically too large for the ordinary view.

## Plan

- [x] Remove the D2 robot-ID scale-reference table and associated camera adjustment.
- [x] Keep one fixed camera distance for ordinary robots and fit only models that physically exceed that frame.
- [x] Update preview introspection and integration assertions to verify fixed tier distance and radius-proportional display size.
- [x] Run scoped quality checks, Android and Windows builds, CTest, and focused emulator tests.

## Result

- Ordinary robots use a fixed 6.5-unit view radius. Their loaded model geometry therefore determines their relative on-screen size, with no robot-number lookup or model-radius normalization.
- A boss or any model larger than the ordinary frame is fitted using its own radius. This is the sole zoom exception and applies automatically to oversized mod robots.
- D2 robots 5 and 19 both report model radius 5.2705, camera view radius 6.5, and display ratio 0.8108. D2 robot 65 is selected as oversized from its 15.4838 model radius rather than its number.

## Validation

- Scoped formatting and static analysis passed.
- Android debug APK built for arm64-v8a, armeabi-v7a, and x86_64.
- Emulator integration passed for D1 robot 0, D2 robots 5, 19, and 65, and the Obsidian modified-robot list.
- Windows D1 and D2 builds passed.
- CTest passed 32 of 33 D1 tests and 39 of 40 D2 tests. The unrelated, concurrently modified `test_route_snapshot` failed its route step-count assertions in both builds.
