# Touch menu and exit movie input research - 2026-05-25

## Goals
- Investigate why the player select menu does not reliably register taps at the expected locations
- Investigate why held touch controls immediately skip the D2 mine exit movie
- Prefer narrow Android-specific fixes or targeted debug logging that can confirm the next theory on device

## Plan
- [x] Trace Android touch event flow into SDL/game menu input
- [x] Trace menu hit testing and coordinate conversion for player selection/listbox/newmenu paths
- [x] Trace movie skip input handling and identify where held touch state reaches the movie loop
- [x] Implement narrow fixes where the root cause is clear, otherwise add Android debug logging around uncertain branches
- [x] Run focused validation or document why validation needs on-device reproduction

## Findings
- Touch mixer joystick buttons start at 100, but the existing cutscene suppressor only tracked buttons 0-15, so most held touch-control buttons could still send a movie-skipping `EVENT_JOYSTICK_BUTTON_DOWN`
- The pilot select listbox used tight source-space row bounds after touch coordinate remapping. Android now uses a row-based hit test across the visible listbox frame with a small vertical tolerance, and logs the interpreted row plus menu-scale state

## Validation
- `./gradlew.bat :app:externalNativeBuildDebug` passed on Windows with JDK 21
- On-device feel still needs confirmation because the pilot menu symptom depends on real touch coordinates and finger-up routing
