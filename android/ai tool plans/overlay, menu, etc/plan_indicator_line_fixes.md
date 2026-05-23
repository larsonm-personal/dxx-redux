# Plan: Indicator line fixes -- sphere clipping, guidebot gate, exit endpoint

## Problem 1: Broken sphere clipping on helper lines

The current `draw_path_lines()` has accumulated complexity:
- `past_keepout` flag that disables clipping once the path exits the sphere
- Only handles "crossing OUT" case, not "crossing INTO"
- Re-anchors segs[0] to player position causing dist=0 for first point
- Visibility skip (`target_is_on_screen`) can hide lines unexpectedly

The desired behavior is simple: for each line segment, clip it against the
player's keepout sphere (3x ship radius). Each segment is handled independently:
- Both endpoints outside sphere: draw full segment
- One endpoint inside, one outside: clip to sphere boundary, draw the outside part
- Both endpoints inside: skip entirely

No `past_keepout` tracking, no path-level state. Just per-segment sphere clipping.

### Implementation

Replace `draw_path_lines()` with a clean rewrite:

```
for each consecutive pair of points (A, B):
  if neither segment is visible: skip
  dA = distance from player to A
  dB = distance from player to B
  a_in = (dA < keepout_r)
  b_in = (dB < keepout_r)

  if a_in && b_in: skip (both inside sphere)
  if !a_in && !b_in: draw full segment A->B
  if a_in && !b_in: clip A to sphere boundary, draw clipped->B
  if !a_in && b_in: clip B to sphere boundary, draw A->clipped
```

The clipping interpolation:
- `t = (keepout_r - dA) / (dB - dA)` for entering case
- Clipped point = A + t * (B - A)

This is clean, per-segment, no accumulated state.

Also remove the `target_is_on_screen` skip -- the lines should always draw when
the feature is enabled (the user wants to see them even when the target is
visible).

## Problem 2: Guidebot line shows before guidebot is released

The guidebot is behind a destructible wall at the start of each level. Until
the player breaks the wall, `Buddy_allowed_to_talk` is 0. The path to the
unreleased guidebot shows a line toward the exit or its cell.

Fix: In `update_paths()`, gate guidebot path computation on
`Buddy_allowed_to_talk`. If not yet released, set count=0.

## Problem 3: Player line should point to exit when all players exited

When all other players have exited the mine (reactor countdown), there's nobody
left to draw a line to. The line should point to the mine exit instead.

Fix: In `find_nearest_player()` (or the caller), when no connected players are
found AND `Control_center_destroyed` is set, find the exit trigger's wall segment
and use its center as the target. Return a sentinel or set the path target to the
exit segment.

Implementation: Add a helper `find_exit_segment()` that iterates triggers/walls
to find TT_EXIT (d2) or TRIGGER_EXIT (d1). When no players found and reactor is
blown, compute path to exit segment instead.

## Cleanup to remove

- `past_keepout` variable and all its logic
- `target_is_on_screen()` function and calls
- First-waypoint re-anchoring (segs[0] = player pos) -- this creates a 0-distance
  first point that's always "inside" the sphere. Instead, just let the path start
  from wherever create_path_points puts it. The sphere clipping handles the rest.
  Actually, we DO want segs[0] = player pos so the line starts from the ship.
  That's fine -- it'll be inside the sphere and get clipped properly.

## File changes

Only `coop_indicator_lines.c` needs changes. No d1/ or d2/ edits.

## Status
- [x] Rewrite draw_path_lines with clean sphere clipping
- [x] Gate guidebot line on Buddy_allowed_to_talk
- [x] Add exit segment fallback for player line
- [x] Remove dead code (past_keepout, target_is_on_screen)
- [x] Build and verify
