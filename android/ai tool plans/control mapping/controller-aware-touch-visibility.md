# Controller-aware touch visibility

Status: implemented and validated

## Requested behavior

For touchscreen devices, including controller handhelds, enable the touch overlay by default and use the full Touch Default layout. Add a default-on setting in Touch Editor > Global: "Hide controls that are assigned to an active controller".

Here, active means currently connected and available to the game's controller input path, including an idle built-in controller. It does not mean recently moved, and a saved controller configuration alone never qualifies.

Keep menu controls available, including Guide, primary/secondary weapon selectors, More, Settings, and Music. Hide ordinary gameplay controls only when their functions are covered by connected controller inputs. Restore them on disconnect without requiring an activity restart.

Scope assumption: retain the existing Controller Menus default on genuinely touchless devices such as Android TV. The full-layout default requested here applies to devices with a touchscreen. Existing explicit overlay-off choices remain respected.

## Behavior before implementation

Investigation used the root checkout at C:/local/dxx-redux, branch cmake, rather than the temporary worktree mentioned by the IDE's active document.

- MainActivity.kt:81 defines the overlay default as `!hasTouchscreen || !hasController`: off for touchscreen plus controller, on otherwise
- MainActivity.onResume reads that default whenever `touch_overlay_enabled` is absent; this is not exclusively a one-time first-start decision
- SetupActivity's Controller section uses the same default and saves the preference only when its checkbox is changed
- TouchLayoutRepository already selects Touch Default for touchscreen devices and Controller Menus for touchless devices
- When the overlay is off and a controller is detected, effectiveTouchOverlayLayout substitutes Controller Menus in memory while preserving the saved layout and gyro configuration
- The Controller Menus affordances are hidden until their controller-operated menus open; this does not provide the requested persistent full-layout menu controls
- MainActivity.hasWorkingControllerDevice checks current Android GAMEPAD/JOYSTICK devices, not the existence of controller_config.json
- MainActivity refreshes the effective layout on resume and controller menu cycling, but has no input-device listener for immediate hotplug updates
- RemainingTouchActions already ignores configured controller actions when no controller is present, but its controller action set is just binding-name conversion. It does not provide full directional axis coverage or input capability validation
- TouchOverlayView.setLayout resets sticks and releases held controls before rebuilding draw and hit-test state

Relevant existing tests: ControllerMenuTouchOverlayDefaultTest, RemainingKeyTouchActionsTest, OverlayVisibilityPolicyTest, ControllerOverlayChecks, and android/tests/test_controller_overlay.ps1.

## Visibility rules

1. If overlay auto-hiding is off or no usable controller is connected, return the original layout without filtering
2. Preserve menu/selector controls regardless of duplicate controller bindings. This includes ordinary buttons assigned to menu-opening actions, not just radial and diagnostic control types
3. Hide an ordinary button only if its primary action and any enabled long-press action are covered
4. Hide a stick only if all meaningful directions on both axes (or its button-mode bindings) and enabled double-tap/extreme actions are covered
5. Apply the same complete-coverage rule to D-pads, sliders, and axis regions. Preserve a partially covered control intact; do not remove individual directions
6. A full controller axis covers both directions. Two discrete directional bindings can also cover a touch axis, allowing the default D-pad to replace touch banking and vertical movement. One half-axis does not cover the opposite direction
7. Compare logical game functions, not physical axis indices: touch virtual axes and physical controller axes use different mappings. Handle half-axis combiners, trigger aliases, inversion, game-specific supported actions, and unassigned actions consistently with runtime dispatch
8. Treat unknown/unresolved coverage conservatively: leave the touch control available. Menu cycling and weapon cycling do not imply complete coverage of everything inside a selector
9. Preserve existing game-state visibility rules (D1/D2 availability, guidebot ownership, disabled rewind, etc.). Keeping menus means exempting them from this new controller filter, not overriding those existing rules
10. Preserve gyro configuration. If a visible touch stick is required to activate configured gyro, retain that activation route rather than silently disabling the user's gyro behavior

## Expected bundled-default result

With the complete default controller mapping available on a touchscreen handheld:

| Touch control                                       | Expected result                | Coverage                                          |
| --------------------------------------------------- | ------------------------------ | ------------------------------------------------- |
| Bank/vertical stick                                 | Hidden                         | D-pad directions plus R3 Drop Bomb                |
| Strafe/throttle stick                               | Hidden                         | Left stick plus secondary fire and L3 Afterburner |
| Mouse-look region                                   | Hidden                         | Right stick plus primary fire                     |
| Fire Flare button                                   | Hidden                         | A                                                 |
| Energy-to-Shield button                             | Hidden in D2                   | X                                                 |
| Automap button                                      | Visible                        | No default controller assignment                  |
| Rewind button                                       | Visible when rewind is enabled | No default controller assignment                  |
| Quick Save / Quick Load                             | Visible                        | No default controller assignments                 |
| Guide / primary weapon / secondary weapon selectors | Visible                        | Menu controls are preserved                       |
| Settings / More / Music                             | Visible                        | Menu controls are preserved                       |

This is derived from the bundled JSON presets, not an on-device Retroid verification. Custom bindings or unavailable physical inputs can leave more controls visible.

## Implementation sequence

- [x] Add a per-layout `hideControllerBoundControls` boolean and the Global settings checkbox, with concise help explaining that menus remain and controls return on disconnect
  - Default on for the full default layout and new layouts
  - Persist through TouchLayout JSON, HumanReadableConfig, slots, presets, single-layout import/export, and all-config export/import
  - Follow the repository's current strict format policy if a version change is needed; update bundled assets and fixtures together, without adding legacy migration machinery
  - Keep the editor canvas complete and editable regardless of runtime filtering
- [x] Change the shared overlay preference fallback to enabled for touchscreen devices with controllers
  - Update both launcher and MainActivity callers and default tests
  - Continue selecting Touch Default on touchscreen devices; do not overwrite saved custom layouts or explicit preferences
  - Do not migrate a saved false preference: it is indistinguishable from a deliberate user choice and must remain respected
- [x] Introduce a small shared controller-coverage policy near the existing controller models
  - Build coverage from the active controller slot/bindings actually used by dispatch and currently connected devices
  - Use Android-reported keys, hat axes, and motion ranges where reliable so a configured but absent physical input does not falsely suppress touch
  - Account for split device reports and trigger alternatives; count a function once if any usable connected input supplies it
  - Reuse existing function/half-axis tables and dispatch mappings; do not duplicate engine file-format knowledge in Kotlin
  - Share connected-controller coverage with RemainingTouchActions so the two policies agree
- [x] Derive an effective runtime layout from the saved layout and coverage
  - Keep filtering pure and independently testable; never save the filtered layout
  - Preserve the existing overlay-off/controller-menu fallback and touchless-device menu behavior
  - Feed the same effective controls to drawing and hit testing, so hidden controls cannot capture touches or floating-region gestures
  - Reconcile RemainingTouchActions with visible controls and connected controller coverage, retaining saved-layout configuration for candidates such as gyro
- [x] Add lifecycle-managed InputManager.InputDeviceListener handling in MainActivity
  - Recompute on add/remove/change, resume, controller mapping/slot reload, touch layout/slot reload, and setting changes
  - Refresh immediately on the UI thread; avoid replacing an unchanged effective layout
  - Release affected held touch and controller input on transitions, including meta-actions and axis state; ensure removal cannot leave firing or movement stuck
  - Do not close an open guidebot/settings menu or lose controller navigation simply because availability changes
- [x] Extend introspection/automation with connected coverage and effective visible control IDs as needed
  - Prefer structured state over screenshots for assertions
  - Keep test injection explicit and reuse the existing controller overlay integration runner
- [x] Update android_features.md after implementation and successful validation

## Validation and completion criteria

- Unit coverage: default-on behavior; setting off returns original layout; configured-but-disconnected controller causes no filtering; partial mappings; both-direction coverage through buttons and axes; half-axis/trigger alternatives; compound button/stick actions; menu exemptions; game variants; gyro activation; no mutation of the saved layout
- Serialization coverage: setting survives slot selection, duplication, presets, and both import/export routes under the current format policy
- High-level D1 and D2 integration: connected default controller produces the table above; disconnect restores controls live; reconnect hides duplicates; remapping updates visibility; controls work after each transition
- Exercise hotplug while holding fire, a touch stick, and a selector; verify no stuck input, invisible hit regions, or lost menu access
- Exercise Guide and weapon selectors through touch with a controller connected, and retain controller menu-cycle/navigation behavior
- Re-run the existing explicit overlay-off/controller-menu cases and touchless-device cases
- Verify actual Retroid Pocket 4 Pro input reporting and the visual result when hardware is available; emulator coverage alone cannot certify its key/axis capabilities
- Run scoped mixed-language code quality, targeted unit tests, Android assembleDebug/CMake builds for both engines, and the controller overlay integration runner to completion
- Register any new top-level automation runner in test_suite_coverage.ps1; mark subordinate JSON scripts with their owner; adjust master timeout when necessary; run both automation catalog checks before committing test additions

## Implementation and validation results

- Added the per-layout setting to internal storage, human-readable import/export, the bundled Touch Default asset, and Touch Editor > Global. The additive field uses the existing layout format and default-value convention; no migrations or format-version change were needed
- Added ControllerTouchCoverage.kt, using connected device key/axis capabilities and the existing controller dispatch builders. Physical key mappings are shared with MainActivity
- MainActivity now refreshes coverage on device add/change/remove, config reload, and resume. Filtered layouts stay in memory; saved layouts and explicit overlay-off choices are retained
- Menus retain their state across filtering updates. Held, toggled, and double-tap-latched controls are released before replacing their control state
- Extra-action availability uses effective touch controls and live controller coverage. Automation exposes visible control IDs and connected-controller coverage
- Added a registered test_controller_touch_hotplug.ps1 runner and owned startup script, with a virtual USB controller using the same axis families reported by the connected Retroid Pocket 4 Pro

Validation completed:

- 56 targeted unit tests passed (coverage policy, defaults, layout/slot serialization, remaining actions, overlay visibility, controller dispatch, and trigger bindings)
- Android assembleDebug and assembleDebugAndroidTest passed, including D1/D2 CMake builds for arm64-v8a, armeabi-v7a, and x86_64. Native builds reported existing warnings in unchanged engine/dependency sources; no new Kotlin compiler warnings
- test_controller_overlay.ps1 passed on emulator-5580, including both games' default filtered layouts, ordinary touch restoration, hidden hit targets, held inputs, toggles, double-tap latches, Guide selection across disconnect, and persistent More/controller menus
- test_controller_touch_hotplug.ps1 passed in D1 and D2 with actual Android input-device add/remove events while each engine remained running. It verified expected visible controls, disconnect while firing (including native fire_primary_state returning to zero), reconnect, restoration, and unchanged saved controller config
- Scoped mixed-language code quality passed
- test_validate_automation_catalog.ps1 and test_run_all_tests_catalog.ps1 passed
- git diff --check passed

Read-only hardware inspection confirmed the attached Retroid Pocket 4 Pro reports X/Y/Z/RZ, HAT_X/HAT_Y, BRAKE/GAS, and the expected gamepad keys. Its app/configuration was not changed; visual verification of the new APK on that physical device remains a manual follow-up

Evidence: android/temp/controller_touch_build.log, controller_touch_device.log, controller_touch_hotplug_run.log, controller_touch_hotplug/, controller_touch_quality.log, controller_touch_catalog_validate.log, and controller_touch_catalog_suite.log
