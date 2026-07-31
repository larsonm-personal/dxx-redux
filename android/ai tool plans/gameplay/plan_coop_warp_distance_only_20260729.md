# Coop Warp Distance-Only Availability

## Goal

Allow coop warp based only on whether a live teammate is far enough away, while preserving warp placement safety and normal coop feature gating. Evaluate a short-lived locked-room escape mechanism separately.

## Plan

- [x] Audit the shared warp policy, UI status mapping, packet validation, and D1/D2 engagement hooks
- [x] Reduce warp availability to the requested distance rule and update user-facing status behavior
- [x] Add or extend focused regression coverage for distance-only availability
- [x] Run scoped formatting, tests, Windows host builds, and the Android debug build
- [x] Document locked-room escape options and recommend a network-safe design

## Locked-Room Escape Recommendation

The distance-only warp is the first rescue path because it now crosses locked and
trigger-closed doors without mutating level state. For cases where teammates are
too close for warp, add a manual `Escape` action that enables local-player-only
ghost physics for 10 seconds:

- Reuse the existing D1/D2 FVI ghost-physics path, which passes internal segment
  boundaries but does not pass through the exterior of the mine
- Restrict activation to live coop players with Coop QoL enabled
- Queue activation to the game thread and use game simulation time for expiry
- Do not modify wall or trigger state, and do not make weapons or robots pass
  through walls
- Show a visible countdown and allow early cancellation
- At expiry, wait until the ship is fully inside a valid segment; if it is not,
  extend only long enough to reach clear geometry
- Let normal multiplayer position packets carry the player's resulting position

This is safer than temporarily opening a door because opening it changes shared
mission state, can fire or bypass scripted behavior, and requires synchronized
restoration across every peer.
