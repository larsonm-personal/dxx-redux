# Independent shoulder buttons and trigger axes

Status: Implemented and validated

The user rejects automatic GAS/BRAKE aliases: controllers may expose these axes independently of LT/RT. Replace the previous alias policy with distinct L2, R2, LT, RT, BRAKE, and GAS bindings. Preserve analog-first hold selection when a trigger reports a paired button, without merging saved identities or gameplay actions

- Add a third shoulder target per side for L2/R2, retaining L1/R1 and LT/RT
- Provide BRAKE/GAS entries with touch and controller selection alongside L2/R2 list shortcuts
- Extend setup samples, hold selection, picker settings, persistence, native registration/mailbox/thresholds, and gameplay for the independent inputs
- Preserve existing physical, gyro, D-pad and combiner IDs. Reserve new shoulder buttons 26/27, BRAKE/GAS axis buttons 28-31, and analog axes 11/12. Expand combiner capacity with axes 13-15, keeping mixer IDs 100+ reserved
- Verify save/reload and actual engine Fire Primary/Secondary press/release for both games, including alias isolation and digital L2/R2
- Run scoped mixed-language code quality, Kotlin tests, native contract tests, both Android engine builds, and serial emulator integration

The hold timer remains two seconds after reaching 80% deflection. Measure UI capture timing and show the hold requirement clearly. Picker dismissal currently retains edits in the page; the page Save button persists them

Confirmed gaps: L2/R2 were redirected to LT/RT in the editor and were absent from gameplay key dispatch; BRAKE/GAS had no independent runtime bindings. The running engine also retained its old controller config after launcher edits. Foreground lifecycle processing now reloads the config and refreshes native control bindings on the game thread

Validation completed:

- Scoped Kotlin/C/C++/PowerShell formatting and lint passed
- All 48 Controller unit tests and both native virtual-gamepad/mailbox contract tests passed
- D1 and D2 Android engines built for arm64-v8a, armeabi-v7a, and x86_64
- Independent-axis integration passed 51 steps in each game: hold selection, save/reopen, native primary/secondary fire and release, and LT/RT isolation
- Live-rebind runner passed for both games with the same engine PID before/after launcher edits: 12 launcher steps and 13 native steps. Moving fire bindings from BRAKE/GAS to L2/R2 removes the old axis actions immediately and enables the digital actions
- Hold-priority regression passed 20 steps, including button-first/axis-first paired reports and digital-only L2
- Raw diagnostics and controller-only navigation regression passed 37 steps, covering unadvertised axis 40 and an additional numbered button, plus D-pad/A access to BRAKE and L2 list entries
- Portrait and landscape graphics inspected; tapping the new L2 and R2 graphic targets opened their independent pickers

The live runner must reorder SetupActivity to the front with FLAG_ACTIVITY_REORDER_TO_FRONT; a plain adb start only raises the task with MainActivity still on top and does not test a genuine launcher/game transition

Retroid hardware verification remains with the user; emulator integration dispatches Android MotionEvent/KeyEvent through the actual activities

General arbitrary-input binding and device-specific source choices remain separate follow-up work
