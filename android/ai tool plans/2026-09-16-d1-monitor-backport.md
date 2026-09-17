# D1 monitor replacement backport

Integrate upstream `86dcf9ca67c7cabde082c158bc60fb2d4ff5bf19` without replacing the local D1 animation restoration pipeline

- [x] Trace generic D1 replacements, effect restoration, and destroyed-monitor mappings
- [x] Add a loader integration fixture reproducing effect bitmap corruption
- [x] Protect effect frames and destroyed images in direct and animated-clone replacements
- [x] Verify D1 effect restoration, animation, destroyed texture selection, and repeated transitions
- [x] Run scoped quality checks, host builds/tests, and the relevant Android native build
- [x] Document coverage and merge adaptations

## Design

The generic D1 texture conversion can write into D2 effect bitmap slots. Local `d1_in_d2_apply_effects()` subsequently reloads D1 animation frames into their original effect slots, but does not separately reload destroyed-monitor images. Preserve upstream's exclusion of effect frames and destroyed images from generic replacement; leave the explicit D1 effect loader in control of its frames

Also apply the exclusion to the existing static-to-animated clone loop, which otherwise bypasses the direct-target check. Use a small fixed bitmap-slot mask and bound effect/frame/texture indices before accessing it. Keep the change in `d2/main/piggy.c`; native D1 does not use this compatibility path

Reference: [upstream monitor fix](https://github.com/dxx-redux/dxx-redux/commit/86dcf9ca67c7cabde082c158bc60fb2d4ff5bf19)

## Regression coverage

Extended `android/tests/test_upstream_compat.cpp` with a synthetic registered-format D1 PIG and palette. The fixture deliberately maps generic wall replacements onto live monitor frames and a destroyed-monitor bitmap, including collisions through the animated-clone path. The unchanged engine failed at `generic D1 replacement preserves monitor frames and destroyed image` before applying the fix (`temp/d1-monitor-baseline-test.log`)

The test subsequently invokes the actual dedicated D1 effect loader, animation update, and one-shot completion code. It checks source pixels, preserved paging metadata, ordinary wall replacement, D1 animation frame restoration, destroyed-image selection with orientation bits, D2 effect metadata restoration, and a second D1 entry

A read-only comparison of the local retail GOG D1 PIG and D2 HAM tables confirmed that the converted positive D1 destroyed-monitor texture numbers are covered by D2's effect destination set. This is table validation, not an in-game playtest

## Merge notes

Preserve upstream's effect exclusion in generic replacement. Retain the bounds checks, fixed 2620-byte mask, and clone-loop exclusion added here. No D1 effect metadata or explicit frame-restoration changes are needed. Native D1 uses a different loader and needs no engine modification for this fix

## Completed validation

- `run-windows-build.ps1 -Target both`: both Windows games and host tools built successfully
- All 107 CTest tests passed (50 D1, 57 D2), including the extended loader/animation regression
- Android `:app:externalNativeBuildDebug -Pandroid.injected.build.abi=arm64-v8a` with JDK 21: both native game libraries built successfully
- `test_dpog_d1_bitmap_mapping.py` and `test_d1_custom_rle_staging.py`: all six checks passed
- Scoped mixed-language code quality checks passed. The script excludes upstream engine files and the native fixture, so the fixture was also formatted with pinned clang-format 20. `git diff --check` passed; no new warnings were reported for changed source

Run the monitor regression through `ctest --test-dir buildd2 -R '^test_upstream_compat$' --output-on-failure`. It generates its own assets in a separate `d1-monitor-fixtures` directory within the test working directory, avoiding interference with the earlier exit-model fixture

Logs are under `temp/d1-monitor-*`. No Android device playtest or live monitor shooting/rendering session was performed; coverage exercises native loading, animation, and destroyed-texture selection directly
