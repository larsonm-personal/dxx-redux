# Four bug study and logging plan

## Scope

1. D2 preview movie audio skips
2. Select button does not open settings tray reliably
3. Controller axis picker can freeze on stale values in the launcher
4. Touch mouse mode has an in game deadband or clamp at low motion

## Hypotheses

### Bug 1: D2 preview movie audio skips
- Likely in D2 MVE queue timing, conversion, or output rate mismatch rather than launcher level audio setup
- Best low cost discriminator is queue depth, underrun, overrun, and SDL_mixer output logging during movie playback
- Current intro-movie logs point to SDL 1.2 `SDL_BuildAudioCVT()` only applying the easy 2x step for 22050 to 48000 output and leaving the final rate gap to drift into underruns
- Follow-up sync issue likely comes from startup lead in the queued movie audio rather than steady-state decode, so the next narrow fix trims initial queued audio to a small target instead of changing the ongoing queue behavior
- Resolved Android startup sync uses a 150 ms queued-audio target based on phone testing on the current device and the temporary `[mve-audio]` diagnostics have been removed after verification

### Bug 2: Select button routing
- The failing controller path may arrive as BACK rather than BUTTON_SELECT on some devices
- Best low cost discriminator is logging of Select, Start, and Back routing decisions with tray policy state

### Bug 3: Axis picker stale values
- Motion events may stop reaching the controller config flow while a picker dialog is open
- Best low cost discriminator is logging both SetupActivity motion updates and picker side live axis generation while the dialog is open

### Bug 4: Touch mouse deadband
- The touch overlay may emit small values that are then zeroed by the native joystick deadzone path
- Best low cost discriminator is comparing touch mouse output, launcher threshold to PlayerCfg deadzone mapping, and post deadzone engine values

## Work plan

- [x] Gather code anchors for all four bugs
- [x] Mine older redbook audio work for relevant clues
- [x] Add low cost exploratory logging for all four bugs in one pass
- [x] Validate the logging patch with an Android debug compile
- [x] Apply initial audio skip mitigations: larger movie audio ring and cached SDL_mixer CVT setup
- [x] Apply layered axis-picker fix: dialog-window motion bridge plus fresh sample on open
- [ ] Run one combined manual repro pass and export launcher and game logs
- [x] Review collected logs and narrow each bug to one controlling path
- [x] Apply combined movie-audio diagnostic plus fix pass for the SDL 1.2 non power of two resample gap
- [x] Rebuild and rerun focused validation after fixes
- [x] Apply initial Android movie AV-sync trim at audio start

## Logging added

- `d2/libmve/mveplay.c`: temporary `[mve-audio]` queue diagnostics were used during the movie-audio investigation and removed after the Android fix was verified
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`: Select, Start, and Back routing logs tagged `[select-route]`
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`: launcher joystick motion logs tagged `[ctrl-picker]`
- `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt`: picker open, trigger, and live axis logs tagged `[ctrl-picker]`
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt`: touch mouse drag and drain logs tagged `[touch-mouse]`
- `android/app/src/main/cpp/android_gamepad_config.cpp`: threshold to deadzone mapping logs for axis button and PlayerCfg deadzones
- `d1/main/kconfig.c` and `d2/main/kconfig.c`: post deadzone axis logs tagged `[joy-dz]`

## Next test pass

- Enable launcher and game debug logging in the app
- Reproduce the launcher axis picker issue before launching the game
- Launch D2, let the preview movie audio play long enough to capture skips
- Test Select and Start while the touch overlay is enabled
- Enter gameplay and test touch mouse mode with slow drags, gyro off, and no controller stick movement
- Export the debug logs after the run and correlate the tagged lines