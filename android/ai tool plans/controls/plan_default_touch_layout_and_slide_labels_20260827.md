# Default touch layout and vertical-slide labels - 2026-08-27

Goal: Replace the touchscreen default preset with the supplied exported layout and correct editor direction labels for the virtual vertical-slide axis without changing runtime input behavior.

1. [done] Trace default preset selection, exported-layout import format, and axis direction conventions
2. [done] Locate and validate the supplied `touch_layout (1).json`
3. [done] Replace the bundled `Claw` preset and select it as the touchscreen default
4. [done] Correct `Slide U/D` direction labels and add regression coverage
5. [done] Run scoped code quality, focused JVM tests, the full unit suite, and debug APK assembly

## Findings

- Touchscreen devices select the bundled preset named by `DEFAULT_TOUCH_PRESET_NAME`; controller-only devices use a separate controller-menu preset
- The exact path in the request was absent, but the adjacent `touch_layout (1).json` exists, is the newest matching export, parses successfully as version 11, and is named `Claw`
- Bundled files are templates. Existing active user copies are intentionally not overwritten by a bundled preset change
- `ControllerConfigModel.HALF_AXIS_MAP` documents the engine sign convention: `Slide Up` is the positive half of `Slide U/D`, while `Slide Down` is the negative half
- This differs from ordinary screen-Y joystick axes such as Pitch, where Up is negative and Down is positive
- The editor direction-label table currently assigns vertical Slide using the ordinary Y convention, which explains why that pair alone is reversed
- The fix belongs in label semantics only. Do not change touch axis output, inversion, persistence, or native input mapping

## Verification constraints

Another task is using the emulator, so verification for this tranche must not install, launch, navigate, or otherwise depend on the emulator. Use pure JVM assertions, Android unit tests, and build verification only.

## Verification results

- Scoped code quality passed for the changed asset, Kotlin sources, tests, and plan
- Focused `TouchMouseEdgeMovementTest` and `ControllerMenuTouchOverlayDefaultTest` passed
- Full `:app:testDebugUnitTest` and `:app:assembleDebug` passed with JDK 21
- No emulator or device state was used
