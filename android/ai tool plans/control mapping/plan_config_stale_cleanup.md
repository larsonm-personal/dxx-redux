# Plan: Config staleness fix + legacy cleanup

## Context
After the input mixer redesign, several issues remain:
1. Empty `#ifdef ANDROID ... #endif` block in kconfig.c (d1+d2) with only a legacy comment
2. D-pad triggers test fails because controller_config.json on emulator has old values
3. Controller compare shows old SDL button values (0, 1, 2...) not identity-mapped values (100, 101...)
4. Controller compare shows gyro mismatch: Slide U/D (255 vs 7), Bank L/R (255 vs 6)

## Root cause
- `writeDefaultControllerConfig()` only writes if file doesn't exist
- Old config persists across APK updates with stale button values
- Config doesn't include gyro axis defaults that android_apply_gamepad_defaults applies

## Changes

### Phase 1: kconfig.c cleanup (d1 + d2)
- [x] Remove empty `#ifdef ANDROID ... #endif` block (lines 59-64 in both files)

### Phase 2: ControllerConfigPage.kt
- [x] Add `internal const val CONTROLLER_CONFIG_VERSION = 2` near CONFIG_FILENAME
- [x] Remove dead D-pad skip loop in buildJoyPairs (does nothing but `continue`)
- [x] Add gyro axis defaults (19->7, 21->6) after normal binding loop, before poisoned filter
- [x] Use CONTROLLER_CONFIG_VERSION in saveConfig instead of hardcoded 1

### Phase 3: SetupActivity.kt
- [x] writeDefaultControllerConfig: check version before skipping regeneration

### Phase 4: android_gamepad_config.cpp
- [x] Update header comment: "128+ touch overlay" -> "100+ mixer (MIXER_BTN_BASE)"

### Phase 5: Rebuild and verify
- [x] Build APK
- [x] Run code quality checks (only pre-existing BuildInfo.kt issues remain)
- [ ] Run tests on emulator
