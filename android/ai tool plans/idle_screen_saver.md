# Idle screen saver

Implement an Android inactivity screen saver, default five minutes with Off/1/5/10/15 minute choices

- Track deliberate touch/key/controller activity using elapsed real time, ignoring gyro and stick drift
- Only count single-player inactivity while simulation is paused; save to the existing automatic recovery slot and require success before allowing OS screen-off
- Multiplayer keeps simulation, network, and gameplay audio running behind a dim black overlay; music and expensive scene rendering suspend
- Consume the dismissal gesture, preserve pause/manual music state, and compose correctly with actual Android background/resume
- Expose state through introspection and add integration coverage for both games, save failure, input reset/wake, and multiplayer continuity
- Run scoped formatting, Android CMake/Gradle build, catalog validation, and device integration tests

Implementation complete

- Shared native policy and Android overlay/settings serve both games; hidden multiplayer uses sleeping frame pacing capped at 60 Hz
- Scoped formatting/lint and git diff whitespace checks passed
- Android debug builds passed for x86_64 and arm64-v8a, including both native engines
- Single-player integration passed all 69 steps in each game, including failed saves, actual screen lock/resume, held inputs, and touch/button/stick dismissal
- Two-emulator LAN integration passed for each game with the saver active on host and client: game time advanced, both players stayed connected, display swaps stopped, frame pacing stayed bounded, and dismissal restored music
- Audio lifecycle tests passed (9 tests); automation and master-suite catalog validation passed
- Physical Samsung and Retroid screen brightness/power behavior still needs hardware verification
