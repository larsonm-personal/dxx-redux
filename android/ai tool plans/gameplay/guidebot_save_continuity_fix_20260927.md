# Guidebot save continuity fixes

- Persist ordinary escort commands, secret goals, timers and thief clocks using explicit portable fields
- Persist Enhanced navigation state and published/in-progress live route state without pointers or ABI padding
- Restore new save payloads without legacy normalization; retain fallback for older saves and reset only on a mode change
- Extend fixed-step continuity coverage beyond Scram to active guidance, verify both modes and repeats
- Build Windows/Android, run continuity and routing lifecycle coverage, and document limits

## Implementation

Completed: D2 saves use version 41 and D1-in-D2 saves use version 42. Version 40
imported saves remain readable. Older saves use the existing reconstruction path;
new saves restore the recorded runtime without clearing commands or retiming AI

The shared field schema uses portable scalar encodings and relative clocks,
including Enhanced route recovery, publication and pending certifier work.
The Enhanced block is framed so metadata-only tools can consume live-game saves.
Validation reads without applying state and rejects truncated payloads. A routing
mode change still resets navigation; replay and co-op mode policy remain authoritative

## Validation

- Windows D2 build and Android debug builds for configured ABIs passed
- Native save continuity passed twice on D2 levels 1 and 11, for Scram, Next,
  Exit and Hostages in both modes and Unexplored in Enhanced
- Each scenario checks unchanged state across save and load, then 120 resumed
  frames against uninterrupted execution, including movement, goals, timers,
  HUD message bytes, path allocation and RNG state
- Payload validation accepts complete data, rejects a one-byte truncation, and
  leaves the observed live state unchanged in either validation pass
- Native Original reference-routing comparisons, save mode conversions and replay
  startup checks passed twice on levels 1 and 11
- Android `test_guidebot_routing_modes.jsonc` passed 58/58 steps, including retained
  commands on same-mode restores and switching modes on load
- CTest passed upstream compatibility, guidebot goal messages, RNG seed resume,
  Android save metadata and co-op save format (5/5)
- Scoped formatting/lint and `git diff --check` passed; final Windows build was current

Logs: `temp/guidebot-persistence-build.log`,
`android/temp/guidebot-persistence-build.log`,
`temp/guidebot-persistence-continuity.log`,
`temp/guidebot-persistence-navigation-final.log`,
`temp/guidebot-persistence-android-test.log`

## Scope

These checks exercise real saves and live guidebot navigation/physics, with a
stationary player. They do not certify the entire engine frame loop, co-op
continuity or all-world demo parity. Android planner work retains its existing
wall-clock budget, which can independently affect scheduling. Legacy saves lack
the new fields and cannot offer the same preservation guarantees
