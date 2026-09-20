# Difficulty menu tap investigation

## Findings

- Both games share Android input mapping, and their menu draw loops visit background windows before the front window
- Each rendered menu overwrites the global scale rectangles, even when it is not the front menu
- The UI thread formerly read those transient globals, then locked the wrong transform for the entire gesture
- Reproduced on the D2 difficulty menu at 1920x1080: tapping screen (959,402) twice produced native (959,423) and (959,429), both hit=-1
- The visible difficulty menu used source (616,301 688x478), destination (299,81 1321x918), which maps that tap to (959,468) inside Trainee
- The missed taps instead used background source rectangles (624,151 672x778) and (442,171 1036x738)
- This explains why deeper menus can be worse: more background menus publish temporary transforms
- The four-internal-pixel drag threshold is separate and unchanged

## Changes

- Use the existing synchronized front-menu interaction snapshot for Android touch mapping, while retaining the per-gesture coordinate lock
- Log the selected front-menu scale alongside existing Android tap diagnostics
- Add Advanced > Debug Logging Categories > Show tap feedback, default off
- Observe finger-down events before overlay dispatch and draw conspicuous white/magenta rings for 500 ms in a non-interactive ViewOverlay
- Support multiple fingers, bound retained rings, and dispose pending callbacks on activity pause
- Leave outstanding_bugs.md untouched, as requested by its local note

## Validation

- Scoped Kotlin and native code quality passed
- Kotlin compilation passed
- D1 and D2 x86_64 CMake library builds passed, with no warnings in the changed native file
- Full assemble hit the native-retention guard because unrelated jobs were active; built the existing CMake directories directly without cleanup and packaged using those outputs
- Baseline D2 automation reached the difficulty menu and original APK logs demonstrated the coordinate mismatch above
- Final D2 emulator check passed: a single tap at (959,402) mapped to (959,468), hit Trainee on DOWN and UP, and advanced to the briefing with difficulty=0
- The fixed tap log simultaneously showed the renderer globals describing the background menu, directly exercising the repaired mismatch
- Visually verified the feedback ring at (1700,500) and its subsequent removal
- Final APK packaging and mixed Kotlin/C code quality passed; D1 was compiled but its menu was not exercised on-device
