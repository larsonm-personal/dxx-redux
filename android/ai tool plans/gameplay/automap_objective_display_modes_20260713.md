# Automap Objective Display Modes

## Goal

Replace the Show Objectives boolean with a four-state control that cycles `Off`, `All`, `Remaining`, and `Next`, while keeping objective selection consistent with the shared C++ route plan.

## Plan

- [x] Trace the touch control, shared objective state, automap rendering, introspection, and automation contracts.
- [x] Add one shared objective display mode and implement the ordered touch-control cycle with `Off` as the level-load default.
- [x] Filter automap objective labels as all, first pending onward, or first pending only; keep the upper-left objective text consistent with the selected mode.
- [x] Extend introspection and focused automation coverage for every mode in both game variants where applicable.
- [x] Run scoped quality checks, D1/D2 builds, native tests, Android tests/build, and focused emulator validation.

## Constraints

- Objective mode changes are presentation-only and must not alter Guide-Bot route planning or classic movement.
- D1 and D2 automap hooks must remain equivalent.
- New state and filtering logic should remain in shared C++ code.

## Validation

- Scoped C/C++ and Kotlin quality checks passed.
- D1 and D2 Windows builds and both variants' native route snapshot and metadata scan tests passed.
- Android JVM tests and the debug APK build passed.
- Emulator coverage passed for D1's generic automap overlay, the KCXF2 level 6 `Off -> All -> Remaining -> Next -> Off` cycle, and KCXF2 level 5's progressed `All`/`Remaining` distinction.
