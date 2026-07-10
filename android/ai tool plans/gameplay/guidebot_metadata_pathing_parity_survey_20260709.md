# Guidebot metadata pathing parity survey

## Goal
Survey metadata route waypoint kinds and activation kinds, then fix any guidebot handling gaps so guidebot can target the same route progress points the metadata analyzer emits.

## Plan
- [x] Read project instructions and current guidebot route handling
- [x] Enumerate metadata route step and activation kinds
- [x] Compare metadata route dependency/path rules against guidebot reachability, satisfaction, and goal selection
- [x] Patch concrete parity gaps with scoped source changes
- [x] Add or update focused regression coverage
- [x] Run scoped quality, build, and the most relevant automation or host tests

## Notes
- Start from the existing Android-only guidebot route logic in `d2/main/escort.c`.
- Keep D2 source edits small and Android-scoped where possible.
- Metadata emits route steps for start, key, trigger, hidden door, boss, reactor, exit, and hostage; guidebot now keeps boss/reactor/exit as route goals instead of falling back to default object search.
- Boss/reactor reachability now matches analyzer behavior more closely by allowing a reachable segment with line of sight to the target object or control-center segment.
- KCXF2 and Obsidian route-next scripts now assert route goal identity rather than fixed absolute route indices after the analyzer rebuilds the remaining route from the current position.
