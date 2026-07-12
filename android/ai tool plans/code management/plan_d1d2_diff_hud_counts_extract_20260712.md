# HUD count overlay extraction plan

## Goal

Move the duplicated robot, hostage, secret, and coop count renderer out of the
upstream-original D1 and D2 gauge files into one unconditional shared source.

## Boundary

- Keep private score animation state and corner-inset policy in each gauge file.
- Pass only the selected player, score/timer row occupancy, D2 secret-level
  policy, and the existing right-inset callback to the shared renderer.
- Preserve D2 negative-level behavior, which draws only the secret count.
- Compile the same source directly into each game's prefixed main target.

## Validation

- Compare the duplicated renderer bodies and the two intentional D1/D2 policy
  differences before movement.
- Build D1 and D2 on Windows and all configured Android ABIs.
- Run both-game HUD/automap gameplay coverage with robot, hostage, secret, and
  coop count assertions where available.
- Record inherited-file metrics and residual hunk sizes.

## Result

- The common renderer now lives in `hud_counts_shared.c`; each game retains a
  24-line policy wrapper for private score state, timer occupancy, and D2's
  negative-level rule.
- D1 `gauges.c` changed from 226 to 126 upstream additions and D2 from 282 to
  177.  Including one CMake source line per game, 203 inherited additions were
  removed.
- `git diff --check`, both Windows builds, and all arm64-v8a, armeabi-v7a, and
  x86_64 Android native links pass.
