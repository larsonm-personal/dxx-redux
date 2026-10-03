# Controller response implementation

## Design

Implement normalized Betaflight Actual response with center sensitivity and expo, full output fixed at 100%, and a live physical-input versus command-output plot. Apply deadzone before shaping and preserve controller precision and provenance through native delivery

Use mailbox channels 16-31 for normalized controller contributions to logical axes 0-15. Existing channels retain touch/gyro/legacy behavior. Combine the contributions only when constructing each engine action, where pitch and modifier limits are known. This avoids changing touch behavior or confusing processed controller input with raw legacy input

Digital controller actions use raw physical thresholds through the existing action-index button mixer. Analog settings do not affect digital thresholds

## Work

- [x] Implement Actual curve, settings serialization, and raw/analog controller dispatch
- [x] Preserve controller contribution and precision through both native engines
- [x] Add shared curve editor with live graph to stick and trigger dialogs
- [x] Extend meaningful unit and end-to-end response coverage
- [x] Build Android and Windows, run D1/D2 integration checks, and format scoped changes

## Result

For physical magnitude x, deadzone d, center sensitivity c and expo e:

```
u = clamp((abs(x) - d) / (1 - d), 0, 1)
output = sign(x) * (c*u + (1-c)*((1-e)*u^2 + e*u^6))
```

All parameters use normalized units. This is Betaflight Actual with maximum rate fixed at 1 and center rate constrained to 0..1. It is odd, monotone, bounded and reaches full output only at full physical throw (subject to final engine quantization). At c=1 the curve is linear after deadzone and expo has no effect

Presets: Linear (100% center, 0% expo), Gentle (50%, 0%, the default), Fine (25%, 50%). The shared graph includes physical deadzone, signed input/output axes, a linear reference, and a live sample/readout. Trigger graphs use the positive half. Output denotes the shaped command before binding inversion; it is not degrees per second or measured ship angular velocity

The native path normalizes pitch to FrameTime/2 and other destinations to their own command limits. Controller input bypasses legacy joystick deadzone, sensitivity and undercalibration gain. Touch/gyro contributions retain their existing path and are combined at the destination. Digital stick directions and triggers use raw physical thresholds

Configuration uses axis_responses objects containing center and expo. Compiled configuration version is 6, synchronized between Kotlin and native (the previous tree wrote version 5 but the native reader accepted only 4). Old compiled configuration is regenerated using the existing pre-release defaults policy; no exponent migration is included

## Validation

- Android x86_64 debug APK builds successfully
- Windows D1 and D2 builds succeed
- 21 selected Kotlin tests pass: response properties/reference values, serialization, slots, trigger bindings and mixer behavior
- Rebuilt native mailbox/command precision tests pass in both Windows builds, including sub-256 raw input and full endpoints across frame times
- test_controller_response.jsonc passes all 78 steps in each game on isolated emulator-5558, through Android MotionEvents to final normalized engine commands: deadzone boundary, both signs, diagonal input, 99% versus 100%, zero deadzone small input, trigger cancellation, raw digital thresholds, slide/bank modifiers, touch mixing and release
- test_independent_trigger_axes.jsonc passes all 63 steps in each game. Its setup now explicitly unbinds LT/RT/L2/R2 before testing BRAKE/GAS, because the default preset maps all three reports per trigger to fire
- Scoped mixed-language formatting/lint checks and git diff --check pass
- Launcher UI automation passes and rendered graphs were inspected at phone and tablet densities, including live signed input/output markers and tick labels. Short landscape windows use a shorter graph. Added a launcher scroll action to exercise off-screen graph content through the automation API

Reusable response runner: android/tests/test_controller_response.ps1 -Serial emulator-5558. Test logs are under temp/controller-response-*.log and temp/controller-independent-*.log
