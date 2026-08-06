# Coop duplicate powerup energy investigation

- [x] Trace duplicate primary and quad pickup behavior in D1 and D2
- [x] Compare the implementation with single-player behavior and relevant history
- [x] Fix the co-op all-players-owned case if the implementation is incorrect
- [x] Evaluate regression coverage
  - The decision is embedded in the engine pickup path and depends on synchronized
    peer state, so validation uses both engine builds rather than a synthetic unit
    seam
- [x] Run scoped formatting, build, and relevant tests

## Result

- Duplicate regular laser and quad pickups now convert to energy in D1 and D2
  when every connected cooperative player owns the corresponding upgrade
- Duplicate D2 super laser pickups use the same rule at maximum super laser level
- Single-player behavior is unchanged, and competitive multiplayer continues to
  leave duplicate pickups available
- D1 and D2 Windows builds passed
- Android debug native builds passed for all configured ABIs, and debug unit
  tests passed
