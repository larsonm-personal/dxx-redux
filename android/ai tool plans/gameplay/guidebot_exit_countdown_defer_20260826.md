# Guide-Bot Exit Countdown Deferral

## Goal

Make the explicit Exit command follow the established end-of-level route while
deferring reactor and boss objectives that start the escape timer.

## Constraints

- Keep the normal Next objective sequence unchanged.
- Preserve every non-countdown prerequisite in its existing order.
- Preserve the Counterstrike level 21 switch-before-exit correction.
- Use current physical passability for Guide-Bot movement.
- Keep classic behavior as the fallback when usable route metadata is absent.

## Plan

- [x] Add a focused policy for projecting the live route past reactor and boss
  objectives for an explicit Exit request.
- [x] Retain an Exit-specific semantic goal and select a useful physical
  frontier when countdown-controlled geometry is still closed.
- [x] Add host regressions for reactor, boss, retained switch prerequisites,
  and physical frontier behavior.
- [x] Run scoped code quality, focused and full D2 host tests, the Windows D2
  build, and Android assembly.

## Results

- Explicit Exit now projects past only reactor and boss countdown starters.
- Other required steps remain active, including Counterstrike level 21 switch
  trigger 16.
- Closed countdown-controlled links produce the nearest useful physical
  frontier without making those links currently passable.
- Exit target mode persists through save metadata and multiplayer owner state.
- Scoped code quality, Windows D2 build, 43 D2 host tests, Android debug APK
  assembly, and the 27-step Counterstrike level 21 emulator test passed.
