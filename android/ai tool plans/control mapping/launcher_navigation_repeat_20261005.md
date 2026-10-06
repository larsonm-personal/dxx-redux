# Launcher navigation repeat

- Share the existing navigation repeat timing and key scheduler across launcher content, dialogs and dropdown windows
- Repeat raw directional key events so lists retain spatial navigation, sliders retain adjustment, and the Touch Editor toolbar retains its custom order
- Keep specialized vertical focus traversal, but let the window root own its repeat timer to avoid duplicate repeats
- Cancel on release, window focus loss, touch input, disconnect and disposal; repeat the controller mapping action grid while preserving its two-second hold-to-edit
- Extend device slider/navigation coverage with held directions, release, dialogs and focus-loss cancellation; run scoped formatting, Android build, JVM tests and device checks

## Implementation

- Launcher root, Material alert dialogs, custom dialogs and dropdown menus share a repeat owner that redispatches directional keys to their existing handlers
- Existing vertical traversal helpers defer timing to that owner; standalone uses keep their previous repeat behavior
- The controller mapping poll loop uses the same timing and stops navigation while its window is unfocused or a binding picker is open
- Added held-input device coverage to the existing slider navigation runner and raised its master timeout to 300 seconds

## Validation

- Android x86_64 app and instrumentation APK builds passed
- JVM tests: 1,136 passed, one skipped across 186 suites
- Emulator slider/navigation suite passed, including all new held-input checks
- Scoped Kotlin lint and both automation catalog checks passed
- Rebuilt and reran JVM and emulator navigation checks after the final disconnect and window-focus cancellation changes; all passed
