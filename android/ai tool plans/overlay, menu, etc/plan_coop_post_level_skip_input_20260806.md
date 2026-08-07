# Coop post-level skip input plan

## Goal

Allow touch taps and the controller confirm button to skip the timed coop post-level screen while preserving protection against input held during mine exit.

## Phases

- [x] Trace the coop post-level screen input path in D1 and D2
- [x] Inventory comparable timed or acknowledgement screens and their touch/controller handling
- [x] Identify the existing held-input debounce or timeout contract used after mine exit
- [x] Implement the narrowest reusable fix in both games where applicable
- [x] Add or extend high-level regression coverage
- [x] Run scoped formatting, relevant tests, and Windows CMake build verification

## Notes

- Preserve unrelated working-tree changes
- Prefer a shared input category fix only if the affected screens have the same dismissal semantics
- A fresh press may dismiss the screen; input already held when it opens must not

## Findings

- Standard `newmenu` and `listbox` menus already handle touch and controller A. The coop summary is a custom `kmatrix` window and bypasses that support.
- Other custom windows do not form one safe dismissal category. Credits and high scores already accept touch, pause uses B/Esc semantics, movies and briefings have their own skip rules, and interactive custom screens need navigation rather than dismissal.
- A global unhandled-touch or controller fallback would make background taps activate ordinary menus and could assign the wrong action to pause, abort, or interactive windows.
- The shared Android cutscene guard combines a 500 ms deadline with a release gate for any touch or joystick button held when the screen opens.
- The post-level handler uses input-down only. A carried input produces a release on the new screen, so accepting release would bypass the guard and recreate the accidental-skip bug.
- Skipping is enabled only after all players reach the summary and its final seven-second timer begins.

## Validation

- Scoped `android/run-code-quality.ps1 -Fix` passed for all touched files
- Android `:app:assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64
- `test_levelcomplete_touch_skip.json5` passed all 59 emulator steps, including held/fresh touch and held/fresh controller A cases
- `run-windows-build.ps1` passed for D1 and D2
- `git diff --check` passed
