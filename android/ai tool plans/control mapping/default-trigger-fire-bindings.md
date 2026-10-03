# Default trigger fire bindings

- Add RT/GAS/R2 primary and LT/BRAKE/L2 secondary defaults
- Route trigger button actions through the existing OR input mixer so native two-column bindings do not discard an axis and overlapping reports release correctly
- Verify config compilation for both games, overlapping trigger sources, scoped code quality, and Android build

## Verification

- Implemented defaults and trigger OR mixing, excluding proportional half-axis bindings from digital actions
- Controller config version 5 uses the existing reset-to-defaults policy on launcher startup
- Scoped code quality passed
- All 56 controller and input mixer unit tests passed, including D1/D2 default compilation and overlapping trigger release
- Debug APK assembly and CMake targets for both games passed for arm64-v8a, armeabi-v7a, and x86_64
