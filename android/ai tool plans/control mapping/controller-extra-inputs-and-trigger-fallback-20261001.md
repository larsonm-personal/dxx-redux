# Controller trigger fallback and additional input binding

The automatic alias/shared-binding policy below is superseded by `independent-trigger-sources-20261001.md`, following the user's request to bind GAS/BRAKE independently of standard trigger axes and L2/R2

Date: 2026-10-01
Status: Raw input diagnostics verified; BRAKE/GAS trigger aliases and analog hold priority implemented; remaining binding/fallback work pending

## Intended behavior

On the Retroid Pocket 4 Pro, users can select and bind the reported L1/L2/R1/R2 inputs using touch or the controller alone. Inputs outside the controller drawing are discoverable and editable. Holding an input for two seconds opens its own binding picker, and a completed assignment leaves an editable entry in the additional-input list

Use adaptive trigger positions rather than always drawing a third shoulder control. Keep L1/R1 as bumpers where reported that way. Keep physical names stable when known: Retroid L2/R2 should remain L2/R2, with a Button or Analog indicator; use L2/LT and R2/RT as generic labels when physical naming is unknown. Treat the usual button and axis reports for one trigger as alternate sources for one logical binding. Independently bindable additional controls belong in the list

The user reports three Retroid shoulder-input settings: Digital, Analog, and Both. Initial testing was in Both, with the app readout showing shoulder-button activation but no visible trigger-axis movement. This does not establish that analog events were absent: the current readout samples only fixed known axes. Support users starting in any of these three modes and switching between them after assigning controls. Verify which physical controls change reporting in each mode rather than assuming the setting affects only L2/R2

These are proposed defaults, not an assertion about the Retroid's actual event stream. Capture device capabilities and events before implementing a device-specific assumption

Follow-up hardware evidence from the user: in Both mode the Retroid reports `AXIS_BRAKE`/`AXIS_GAS` alongside L2/R2 buttons. The user wants the analog hold selection to win over its mirrored button, including simultaneous reports. This is user-reported evidence; Digital/Analog captures and device capability details are still pending

## Findings in the current code

- `ControllerConfigPage.kt` already draws L1/R1 and their touch bounds. The reported absence of L1 needs a layout/hit-target check on the Retroid; it is not a missing model entry
- `SetupActivity.kt` recognizes L2/R2 key events in its pressed-button readout, but reads only six fixed stick/trigger axes
- Both controller-mapping and launcher input readouts use these same fixed axes and recognized button names. BRAKE/GAS, other controller axes, and unknown delivered keys are currently invisible; the readout cannot establish that no analog report exists
- `ControllerLongPressDetector.kt` already implements a two-second hold, with 80% selection and 30% interference thresholds. It examines six axes and recognized button names, not arbitrary inputs
- `ControllerConfigPage.kt` maps a held L2 to LT and R2 to RT. It maps either held stick axis to the entire stick picker, losing the selected direction for editing
- `ControllerConfigModel.kt` has L1/R1 but no distinct L2/R2 entries; LT/RT bindings refer to analog axes or axis-derived buttons
- `MainActivity.kt` recognizes ten physical gamepad keys, excluding L2/R2, and reads only the standard trigger axes. Merely adding an editor entry would not make these buttons work in gameplay
- Controller-only navigation on the main editor currently selects a six-button action grid. It does not focus the controller drawing or an input list
- A short A action is deferred until release, with special suppression for an A hold. Picker input routing allows D-pad/A, but consumes B rather than providing conventional picker cancellation
- The button picker applies an assignment immediately and closes. Its Save/Cancel labels need consistent meaning before adding more entry paths
- Human-readable config parsing accepts arbitrary binding keys, but runtime compilation, threshold/exponent loading, assigned-function summaries, and conflict handling depend on fixed control maps. Unknown keys can therefore survive a save without working
- Android virtual input registration currently provides 11 axes and 26 button slots. Additional axes must be supported through the native registration and axis mailbox, not just displayed in Kotlin. Mixer button IDs begin at 100 and must remain reserved

## Trigger policy

1. Resolve each side independently, per runtime controller device. Identify analog sources through joystick motion ranges and digital sources through device key capabilities plus observed events
2. Accept the standard trigger axis and BRAKE/GAS as known alternate trigger reports. The implemented reader takes the larger value of each side's two analog reports so alias-only devices work and duplicated values never sum. Per-device source overrides remain future work
3. Where there is no usable analog source, feed L2/R2 press/release into the logical trigger as 1/0. This makes default fire bindings and trigger-to-throttle/slide assignments usable with digital resolution
4. Keep one logical left/right trigger binding shared by drawing, list, hold selection, editor readout, and gameplay. Retain internal LT/RT IDs if they remain the simplest fit. Input type is a runtime capability, not a binding identity; preserve the physical name and show a Button or Analog indicator
5. When both analog and button events are reported, prefer analog and suppress the usual mirrored digital report. One squeeze must not fire twice or block its own hold detection
6. Do not infer missing analog support from a zero/resting value or a short period with no motion. For contradictory capabilities, add an explicit Auto/Axis/Button source choice in the trigger picker. If an observed key arrives for a device with incomplete capabilities, retain it as discovery evidence and allow selection
7. Hide analog sensitivity/threshold controls for digital sources. Directional movement actions remain available and run at full digital magnitude
8. Provide separate raw-source binding only when the user explicitly chooses to expose a source that is actually independent. Do not manufacture a third shoulder target from mirrored trigger reports
9. Rebuild capabilities on device added/changed/removed notifications, reconnect, page entry, and Activity resume after visiting device settings. A mode change may or may not create a new runtime device ID; support both. A device or reporting-mode switch must not rewrite saved assignments

## Raw input diagnostics first

Extend both the controller-mapping live readout and launcher input diagnostics before deciding what the Retroid reports or choosing automatic alias rules

- Keep the familiar stick/trigger summary and add a controller-focusable, scrollable raw-input view backed by the same sample collector in both screens
- Enumerate every controller/joystick motion range for the event device, including source, axis identifier/name, min/max, flat/fuzz, and current raw value. Include supported idle axes so users can distinguish advertised-but-idle input from an axis that is absent
- Sample additional valid Android axis IDs to reveal changing/nonzero values omitted from declared ranges. Identify these as observed-only; a zero value by itself cannot establish that an undeclared axis is supported
- Explicitly include LTRIGGER/RTRIGGER, BRAKE/GAS, and generic/vendor-used axes. In raw diagnostics retain separate reports even where the binding UI later folds them into one logical control
- Record delivered controller keys before the recognized-name mapping. Show symbolic key names and numeric codes, pressed/released state, device, and source, including unknown keys without consuming or remapping them merely to observe them
- Track state by runtime device and source so button and axis reports from different input interfaces remain distinguishable. Capture at the Activity dispatch boundary before navigation synthesis, aliases, dead zones, response curves, and picker routing
- Show changed-input highlighting plus a small recent-event history so a release or short peak remains inspectable. Use stable row ordering; never interpret every nonzero idle/rest value as an active control
- Separate raw physical reports from effective logical trigger values and synthesized navigation. A digital-to-trigger fallback must not make the raw display appear to have received an analog axis
- Export the same normalized, stable diagnostic structure through introspection and existing debug export, with enough timestamps to compare button/axis reports from one squeeze. Avoid per-frame unchanged-value log spam
- Reset stale held/value state on disconnect, focus loss, and mode refresh. Retain recent observations as clearly historical entries, not live pressed controls

Compare slow squeezes, full holds, and releases of each shoulder input in Digital, Analog, and Both. Record whether the app receives motion events at all, which axes move, whether values are proportional, which keys change, and whether capabilities/device IDs change. Only then conclude whether the remaining problem is a hidden axis, input-source filtering, device configuration, or genuinely digital-only delivery

## Switching Digital, Analog, and Both modes

- Bind a logical physical control to a function, independently of the Android event source currently delivering it. A saved `left trigger -> Fire Secondary` assignment remains the same assignment in either mode
- Button mode produces released/pressed magnitudes of 0/1. Analog mode produces proportional magnitude and uses the saved activation threshold for discrete functions. Directional movement assignments preserve their function, becoming full-strength digital movement in button mode and proportional movement in analog mode
- Both mode retains raw button and axis visibility in diagnostics but normally supplies one logical trigger binding from the analog source. Prefer a usable corresponding analog source and suppress the mirrored digital activation; do not bind or fire both by default
- Preserve analog thresholds, response settings, and source-independent row identity even while their UI controls are hidden in button mode. Restore those settings when analog reporting returns
- Auto source selection is the default. Refresh advertised capabilities and observed events without treating idle zero-valued axes as proof of absence. If firmware advertises unchanged capabilities in both modes, expose the source override rather than relying on an inactivity timeout or guessing whether a button event mirrors a later analog event
- If an explicit source override becomes unavailable, preserve the preference, visibly indicate the temporary fallback, and use the available equivalent source. Restore the preferred source when it becomes available again. Do not silently rewrite an override or leave the user with an inert trigger
- Clear old source contributions and hold timers before switching input interpretation; require the replacement input to return to neutral before accepting new gameplay presses or hold capture. Switching while pressed must neither leave fire/thrust stuck nor generate a new action from the old held state
- Update the drawing, picker type/value, and additional-input row together. Keep focus and the same assignment visible rather than deleting a button row and creating an unrelated axis row
- On capability changes while a picker is open, cancel pending capture, retain the pending function choice, and refresh the input type without moving selection or applying an assignment automatically
- Raw sources deliberately exposed as independent bindings remain source-specific and show Unavailable when absent. Only known equivalent reports transfer automatically; unfamiliar axes must not inherit a shoulder binding based on position, enumeration order, or device-name guesses

Android documents trigger button/axis variants and duplicate trigger reports in [Handle controller actions](https://developer.android.com/games/sdk/game-controller/controller-input). Capability and device identity APIs are documented in [InputDevice](https://developer.android.com/reference/android/view/InputDevice)

## Additional inputs list

Use the title `Additional inputs`, since the list also contains saved entries that are currently disconnected or intentionally pinned

Populate it with the union of:

- Reported or observed controller buttons/axes not represented by the drawing, after known aliases are folded
- Bound inputs not currently represented or available, with an `Unavailable` status
- Inputs explicitly added by completing a hold-selected assignment, including inputs already represented by the drawing

Each row shows the input name, button/axis type, assignment or `Unassigned`, and a small live value/pressed indicator. Use readable fallback names such as `Button 13` or `Axis GENERIC_1`, with raw identifiers in diagnostic detail. Axis rows summarize the full-axis or negative/positive assignments

Touch selects a row; controller focus scrolls it into view and A opens the same picker. Keep stable ordering and focus by input identity when new inputs appear. Newly observed activity must not move the user's selection. After a hold assignment, highlight and reveal the resulting row

Keep discovered inputs for the editor session. Persist bindings and explicitly added rows in the active config slot, including rows set back to None, until the user removes the row or replaces the slot with a preset. Unpinning a row does not clear its assignment; a still-detected extra or bound unavailable input remains listed. Ordinary session discoveries do not require a separate device-history database

Use stable binding identities based on logical roles for existing controls and Android key/axis codes for extras. Keep runtime device IDs out of saved bindings. Extra rows reference the same binding as the drawing rather than storing a second assignment

The current active config slot remains the configuration boundary. Per-device automatic profile switching is outside this task; runtime capability resolution and held-state tracking still belong to individual devices

## Controller-only interaction

- Add explicit focus regions for the drawing, additional-input list, and existing slot/preset/import/export/save/cancel actions. D-pad/left-stick short navigation moves between reachable controls and regions
- A short release activates the selected item. B short release closes the current picker or leaves the page, retaining the page's existing unsaved-change behavior
- Holding A/B must take priority over short activation/back. Do not navigate, save, or dismiss on their initial press; consume the completed hold and its release
- Short directional inputs navigate immediately. A sustained isolated direction can become the existing two-second binding gesture; suppress repeat navigation once the hold gesture claims the input. Document this dual use in the page hint
- Show hold progress after a brief delay, including the target name, so users can distinguish navigation from binding selection
- Only run hold detection on the main mapping page. Gate it during every modal, including slot/preset/import dialogs, and during page transitions
- On hold completion, open the picker once and require release/neutral before picker navigation is armed. The opening A/B/D-pad/stick must not select, cancel, or scroll within the new popup
- Re-arm hold detection only after the opening input has returned to rest. Closing a picker with an input still held must not immediately reopen it
- On picker dismissal, restore focus to the original drawing/list target. Hold assignments reveal their resulting additional-input row
- Match the established controller focus policy: show focus immediately on controller-only devices, and show it after controller navigation on touch devices

Provide a focusable `Bind an input...` action that explicitly arms capture. This complements the hold shortcut and lets users capture a directional input without moving focus, or capture noisy hardware that cannot meet isolation thresholds. In this mode consume A/B as capture candidates; provide a focusable/tappable Cancel control and system Back to leave capture

## Hold and axis semantics

- Extend the existing detector rather than adding a competing detector. Use canonical, normalized input samples, not exponent-adjusted gameplay values
- Button selection requires one logical button held for two seconds with no unrelated axis above 30% and no unrelated button down
- Axis selection requires one normalized direction held at least 80% for two seconds, with no unrelated axis above 30% or unrelated button down. Fold mirrored trigger and HAT/key reports before applying isolation checks
- Use declared ranges, flat/dead-zone information, and an explicit neutral policy. Do not interpret a trigger resting at its minimum as a held negative axis; do not guess a neutral point for an unfamiliar unipolar axis silently
- Open the picker for the held axis and direction. A negative/positive direction can be assigned a button action; a full-axis option can be assigned an analog function. Changing between these modes must expose and resolve incompatible existing assignments
- Retain analog thresholds and response settings where meaningful. A physical button stays digital even when assigned a directional movement action
- Axis jitter should not select another target; brief interruptions reset selection according to a documented hysteresis policy. Test stick drift and diagonal movements
- Discovery includes delivered controller events omitted from capability reports. It cannot expose a vendor or OS-intercepted button that never reaches the app; show this limitation in diagnostic capture help if needed

## Binding and persistence requirements

- Use a small shared Android input catalogue/source resolver so setup and gameplay agree about names, aliases, normalization, and input identity. Avoid building a general controller framework
- Route extra buttons and axis directions through the existing mixer/meta-action paths. Retain press destination ownership through release, including menu transitions
- Allocate extra analog inputs to bounded virtual slots outside sticks, triggers, gyro axes, and half-axis combiners. Update native registration, thresholds, mailbox bounds/edge handling, and introspection together for both games as necessary
- Reserve existing axis-derived button IDs, D-pad IDs, and mixer IDs. Check actual capacity at compile time/config validation; never accept a saved assignment that silently drops at runtime
- Extend config state, human JSON, runtime compilation, all-slot and single-slot import/export, preset replacement, and slot switching for extra bindings, source overrides, and explicitly added rows
- Preserve the project's current binding-count/conflict policy. Extra buttons and axis directions participate in the same limits; show a reassignment warning when selecting a function displaces another binding
- Include extras in assigned/unassigned-function summaries and the controller-only missing-Menu warning
- Keep stored input identity separate from its label and transient native slot allocation. Save the deterministic runtime translation needed by the engine
- Apply current pre-release config policy directly; do not add migrations or compatibility readers unless separately requested

## Work sequence

- [ ] 1. Extend raw button/axis diagnostics in both input readouts and introspection, then capture Retroid evidence in Digital, Analog, and Both: device names/sources, key capabilities, all joystick ranges/observed axes, raw shoulder press/release events, and device-change notifications. Inspect L1/R1 drawing bounds on its resolution
- [ ] 2. Add the shared input catalogue/source resolver and fix digital trigger fallback end to end in setup and gameplay. Adapt labels and picker options; verify default fire bindings, no duplicate activation, and saved assignments across both mode-switch directions
- [ ] 3. Extend binding/config/runtime support for extra buttons and axes, including native registration/mailbox changes and meaningful validation. Complete storage roundtrips before exposing editable rows
- [ ] 4. Add the scrolling additional-input list and complete focus navigation through drawing/list/actions. Add observed-input discovery, persistent explicit rows, and unavailable-binding display
- [ ] 5. Extend hold selection to canonical arbitrary inputs and exact axis directions. Add progress, release-to-arm modal handling, A/B short-versus-hold routing, all-modal gating, and explicit capture
- [ ] 6. Add introspection and reusable automation that exercises real Activity key/motion dispatch, editor selection, save/reload, and gameplay consequences for D1 and D2
- [ ] 7. Run scoped mixed-language code quality, relevant Kotlin checks, Android CMake builds for both games, native tests when changed, and emulator integration serially. Run Windows builds/tests if shared/native engine hooks change
- [ ] 8. Retest on the Retroid starting in Digital, Analog, and Both and switching between every pair, plus an analog controller, then update the existing outstanding-bugs entries with evidence. Keep unrelated in-progress checklist edits intact

## Acceptance coverage

- Digital-only L2/R2: correct labels, touch and controller selection, short fire press/release, hold binding, defaults, and directional movement assignment
- Analog-only and analog-plus-digital triggers: proportional control retained, one logical activation per squeeze, one hold target, BRAKE/GAS aliases folded
- Raw diagnostics: unknown delivered keys, BRAKE/GAS, generic axes, observed-only changing axes, supported idle axes, recent releases/peaks, multiple interfaces, and no synthesized values mistaken for physical events
- Retroid Both mode: slow L2/R2 squeezes show the actual raw button and axis reports and source devices; verify proportional input and deduplication where both are delivered
- Mixed left/right trigger reporting and capability/event disagreements: per-side choice, manual override, no saved-binding mutation on controller change
- Button-first and analog-first setup: assign, save/reload, switch modes, and switch back without rebinding; verify firing and directional movement semantics plus restored analog settings
- Both-first setup and all six directed transitions among Digital, Analog, and Both: same saved logical assignments and pinned rows, one action per physical squeeze, no source-state leakage
- Mode changes after Activity resume, with and without a changed device ID or capability advertisement: refreshed labels/type, same list row/focus, unavailable override fallback, and no stuck input or spurious action when switched while held
- Reporting changes while the mapping page or a picker is open: pending hold reset, stable selection/function choice, and consistent drawing/list/picker state
- Extra known and generic buttons: advertised and observed-only discovery, selection, hold assignment, gameplay/meta actions, release, saved row, and disconnected display
- Extra bipolar/unipolar axes: normalization, neutral, exact held direction, full-axis assignment, directional actions, thresholds, and capacity validation
- A/B and D-pad/stick holds: opening release cannot activate/cancel/navigate the picker; every modal gates capture; closing while held cannot reopen; short actions still work
- Long-list focus scrolling, stable selection during discovery, row reveal after assignment, and full touch/controller access to page actions
- Slot switching, preset replacement, single/all-slot import/export, cleared assignments, pinned rows, conflict limits, and assigned/unassigned summaries
- Disconnect, reconnect, focus loss, and multiple devices: no stuck controls or cross-device hold; displayed capabilities agree with the controller being edited
- Both games: saved extra bindings have verified engine effects, including meta actions and a movement action, rather than only matching serialized JSON

Extend `ControllerInputAutomation.kt` with L2/R2, trigger aliases, generic inputs, and held transitions. Expose setup input catalogue, focus target, capture state, selected picker input/direction, and effective binding state through introspection. Synthetic capability profiles are needed to test advertised-versus-observed inputs; event injection alone cannot prove physical controller capability discovery

## Current validation

- Raw diagnostics are implemented in the launcher's Controller -> Test section and the controller mapper's Raw inputs popup. The mapper also summarizes additional axes and raw pressed keys alongside its existing readout
- The shared collector shows every advertised controller motion range, separately samples Android axis IDs 0 through 63 for observed-only inputs, retains raw key codes and per-device/source identity, includes historical motion samples, and keeps the latest 24 changed reports without repeat spam
- Lifecycle/device changes invalidate live samples and pressed state. Recent-event text is explicitly historical. The raw popup gates the mapper's hold detector and supports controller scrolling and B dismissal
- Setup introspection includes `controller_raw_inputs` and `controller_mapping` state. A pretty-printed `controller_raw_inputs.json` is also written during introspection; existing log export includes the current launcher snapshot or the last saved snapshot
- Extended launcher controller event automation supports L2/R2, BRAKE/GAS, and numeric key/axis identifiers. `android/game_scripts/test_controller_raw_inputs.jsonc` passed all 25 steps on emulator-5554, including hidden-axis/button discovery, release, and controller opening/dismissal of the raw popup
- `ControllerInputDiagnosticsTest` passed four cases covering Both-style simultaneous reports and export, source/device isolation, capability/focus invalidation, and bounded history without repeat spam
- Scoped mixed-language code quality passed. Debug APK assembly passed, including CMake targets for both engines across arm64-v8a, armeabi-v7a, and x86_64, with no new source warnings
- Evidence: `temp/controller-diagnostics-quality.log`, `temp/controller-diagnostics-build.log`, `temp/controller-diagnostics-integration.log`, and the Gradle unit-test results under `android/app/build/test-results/testDebugUnitTest`

Detailed Retroid captures in Digital, Analog, and Both and the reported L1 rendering problem remain open. Step 1's diagnostic implementation is complete; its physical capture portion remains open. The user has confirmed BRAKE/GAS plus L2/R2 in Both mode, leading to the scoped alias/hold-priority follow-up below. Digital trigger fallback and extra-input editing remain pending

## Analog hold priority follow-up

- The existing detector evaluates axes before buttons, permits the corresponding mirrored L2/R2 during an axis hold, and suppresses button selection while analog activity is present. This ordering is now documented and covered explicitly for simultaneous reports, axis-first arrival, button-first arrival at its hold deadline, digital-only holds, and unrelated-button interference
- Added a shared analog trigger reader used by setup and gameplay. BRAKE maps to LT and GAS to RT; standard and alternate values describe one logical trigger and are never added. This also makes assignments made through the analog hold picker receive the same alias values in gameplay
- Raw diagnostics keep all original reports separately. Setup introspection exposes the six effective trigger/stick samples and the last hold selection's axis/button identity so automation can verify which source actually opened the picker
- Added `test_controller_trigger_axis_priority.jsonc` to exercise real launcher key/motion dispatch, both trigger aliases, button-first and axis-first holds, digital-only selection, release, and popup dismissal
- Validation passed: scoped code quality; 18 targeted unit tests (12 hold-detector, two alias-reader, four diagnostics); debug APK assembly with both native engines for all three ABIs; the 20-step trigger-priority integration test and the existing 25-step raw-input regression on emulator-5554
- Evidence: `temp/controller-trigger-priority-quality.log`, `temp/controller-trigger-priority-build.log`, `temp/controller-trigger-priority-integration.log`, `temp/controller-trigger-priority-raw-regression.log`, and Gradle unit-test XML under `android/app/build/test-results/testDebugUnitTest`. Actual Retroid testing of the updated APK remains pending

Digital trigger gameplay fallback, adaptive labels, arbitrary-input bindings, and release-to-arm improvements remain planned work
