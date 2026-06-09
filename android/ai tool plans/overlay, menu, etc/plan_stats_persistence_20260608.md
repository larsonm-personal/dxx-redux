# Stats persistence investigation

Goal: determine why robot kill counts appear to drop after save/load, and check whether hostage and secret counts have related persistence or level-transition issues.

- [x] Trace robot, hostage, and secret count fields in D1 and D2 player/state structures
- [x] Compare save and restore coverage for those fields in normal saves and D2 secret-level transfers
- [x] Patch missing persistence or transition handling with minimal base-game changes
- [x] Add or update focused tests where practical
- [x] Run scoped formatting and relevant build/test verification

Findings:
- Normal D1/D2 save files already serialize `num_kills_level`, `num_kills_total`, robot totals, and hostage totals through `player_rw`.
- D2 live return from a secret level restores the base-level player record but did not restore `num_kills_level`, leaving the base-level robot numerator reset after the transition.
- D2 secret levels do not run normal level stat initialization, so the optional HUD could show main-level hostage counts against secret-level objects. The HUD now suppresses robot/hostage lines on D2 secret levels while still showing secret-area counts.

Verification:
- Scoped code quality passed for the touched files.
- `.\run-windows-build.ps1 -Target d2` completed successfully.
- No new automated gameplay test was added because this fix targets D2's built-in secret-level transition save/restore path and would need a maintained scripted playthrough that enters and exits a secret level.
