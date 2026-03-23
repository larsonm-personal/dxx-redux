# Plan: run_emulator rebuild, Gyro Absolute Mode, Single-Axis Region Control

## Phase 1: run_emulator.sh auto-rebuild
- [x] When emulator is already running, still build APK and install it (don't skip)
- File: android/run_emulator.sh

## Phase 2: Fix gyro ABSOLUTE mode (4 bugs)
- [x] 2.1: Change default mode RATE -> ABSOLUTE in GyroConfig (TouchControl.kt)
- [x] 2.2: Add mode + maxAngle field parsing in parseGyro() (HumanReadableConfig.kt)
- [x] 2.3: Add Rate/Absolute radio buttons + maxAngle slider to GyroSettingsDialog (TouchEditorPage.kt)
- [x] 2.4: Fix GyroInputManager -- don't overwrite reference when inactive in ABSOLUTE mode; fire diagnosticCallback when inactive

## Phase 3: Single-Axis Region control
- [x] 3.1: AxisRegionControl data class in TouchControl.kt
- [x] 3.2: Parsing/serialization in HumanReadableConfig.kt, add axisRegions to TouchLayout
- [x] 3.3: AxisRegionState + geometry in TouchOverlayView.kt
- [x] 3.4: Touch handling: check regions BEFORE sticks, pointer stealing, return-to-stick
- [x] 3.5: Drawing in TouchOverlayView.kt
- [x] 3.6: Editor support in TouchEditorPage.kt (preview + properties panel + AddControlDialog)
- [x] 3.7: Default layout entry in advanced.json

## Phase 4: Build, lint, test
- [x] Build (gradlew assembleDebug)
- [x] Lint (run-code-quality.ps1 --fix) -- only pre-existing BuildInfo.kt issue remains
- [ ] Deploy + manual verify

## Key decisions
- Default gyro mode: ABSOLUTE
- Axis regions checked BEFORE sticks in touch dispatch
- -1..1 bidirectional, spring to 0
- Pointer steal from joystick to region sets a new zero point
- Pointer steal BACK to joystick reuses the original joystick drag center (or the stick's fixed center if drag originated in the region)
- Expose maxAngle as "Tilt Range" slider in gyro settings
- Default axis: AXIS_SLIDE_UD, orientation: VERTICAL
