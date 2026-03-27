# Plan: Input Mixing Layer - Move Multi-Binding from Game Engine to Kotlin

## Summary
Replace the current design where multiple bindings per action (touch primary/secondary, controller col1/col2, gyro) each send separate SDL events with distinct state bits to the C game engine, with a Kotlin-side mixing layer that combines ALL input sources into a single button/axis state per game action, feeding the engine one unified virtual joystick.

Removes: TOUCH_BTN_OFFSET/OFFSET_2, STATE_BIT6/BIT7, JOY_BTN_MATCHES macro, half-axis combiner virtual axes, col2 alternate bindings, MAX_TOUCH_BINDINGS_PER_ACTION=2 limit, binding limit enforcement UI.
Replaces with: unlimited bindings through simple OR (buttons) and additive-clamped mixing (axes).

## Design Decisions
- Buttons: Simple OR mixing (any source pressed = active, release when all release)
- Axes: Additive with clamp to [-1, 1] (gyro fine-tunes on top of stick)
- Controller col2: Absorbed into mixer (all controller bindings handled in Kotlin)
- Count-based actions: Forward individual press events (mixer tracks count vs state actions)
- Thread safety: @Synchronized on mutation methods

## Architecture

### Current flow
```
Touch btn       -> offset by 128/256 -> nativeJoystickButton(128+idx) -> C matches via JOY_BTN_MATCHES
Controller btn  -> nativeJoystickButton(btn_id)                       -> C matches via value==btn
Gyro axis       -> nativeJoystickAxis(6/7)                            -> C reads joy_axis[6/7]
Controller axis -> nativeJoystickAxis(0-5) + halfAxisCombiners        -> C reads joy_axis[8+] (virtual combiners)
Touch stick     -> nativeJoystickAxis(axisX/axisY)                    -> C reads joy_axis[N]
```

### New flow
```
All sources -> InputMixer.setButton(actionId, sourceTag, pressed) -> OR -> nativeJoystickButton(actionId, state)
All sources -> InputMixer.setAxis(axisId, sourceTag, value)       -> sum+clamp -> nativeJoystickAxis(axisId, mixed)
```

C engine sees simple 1:1: Button N = action N, Axis N = axis N.

---

## Phase 1: Create InputMixer.kt [x]
- New file: android/app/src/main/java/com/dxxredux/app/InputMixer.kt
- setButton(actionId, sourceTag, pressed) with OR mixing
- setAxis(axisId, sourceTag, value) with additive-clamped mixing
- clearSources(prefix) for cleanup
- @Synchronized thread safety, JNI callback references

## Phase 2: Wire touch input through mixer [x]
- TouchOverlayView.kt: Replaced dispatchTouchButton with mixer calls, removed touchPrimaryRefs/buttonCallback
- Stick/mouse axis dispatch through mixer
- MainActivity.kt: Created InputMixer, wired callbacks

## Phase 3: Wire controller input through mixer [x]
- MainActivity.kt: onKeyDown/onKeyUp -> mixer.setButton with mixerButtonMap lookup
- onGenericMotionEvent -> mixer.setAxis with source tags
- D-pad routing through mixer via dispatchDpad
- ControllerConfigPage.kt: Identity mapping (MIXER_BTN_BASE + kc_index) + mixer_button_map

## Phase 4: Wire gyro through mixer [x]
- GyroInputManager.kt: axisCallback routes through mixer.setAxis("gyro")

## Phase 5: Simplify C engine [x]
- kconfig.c (d1+d2): Removed TOUCH_BTN_OFFSET/OFFSET_2, simplified JOY_BTN_MATCHES, removed STATE_BIT6/7 and state-bit remapping block
- joy.c (d1+d2): Updated button bypass to use MIXER_BTN_BASE threshold, kept VC axes (still needed for half-axis combiners)
- joy.h (d1+d2): Added MIXER_BTN_BASE = 100 shared constant

## Phase 6: Simplify Kotlin binding system [x]
- TouchBindings.kt: Removed TOUCH_BTN_OFFSET/OFFSET_2, MAX_TOUCH_BINDINGS_PER_ACTION
- TouchEditorPage.kt: Removed binding limit enforcement from all 3 pickers, removed bindingCounts parameter chain
- TouchControl.kt: Removed bindingUsageCounts()

## Phase 7: Update tests and verify [x]
- Updated test_fire_primary.json5: button 128 -> 100 (MIXER_BTN_BASE + kc_index 0)
- Updated test_dpad_triggers.json5: Phase 1 buttons 22-25 -> 108/109/106/107, Phase 3 expected values to MIXER_BTN_BASE + kc_index
- assembleDebug: BUILD SUCCESSFUL
- run-code-quality.ps1: All checks pass (except pre-existing BuildInfo.kt auto-generated file)
- Desktop C changes are all #ifdef ANDROID guarded (cmake not available locally but safe by inspection)

## Notes
- Col2 on desktop: Keep for non-Android platforms via #ifdef ANDROID
- Menu button handling: kconfig menu handler must still work with new button numbers
- fire_count actions: fire_flare, cycle_primary/secondary, drop_bomb, headlight, toggle_bomb, rear_view, automap use ci_count_ptr
