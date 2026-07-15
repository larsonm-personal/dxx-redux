# Guide-Bot Phase 7 Multiplayer Lifecycle

## Goal

Complete owner-authoritative Guide-Bot route-intent coverage across observer hosts, voluntary abdication, cooperative save/restore, slot remapping, disconnect adoption, and host migration without changing classic movement, path timing, flares, or simulation RNG.

## Plan

- [x] Audit existing ownership packets, save-state identity remapping, host-migration adoption, and two-emulator fixtures.
- [x] Keep the assigned co-op Guide-Bot control slot out of generic idle timeout and robot-slot eviction.
- [x] Rebuild the new owner's semantic waypoint immediately on handoff without requesting or changing a classic physical path.
- [x] Build the semantic `Unexplored` waypoint when the command is selected rather than waiting for unrelated classic AI cadence.
- [x] Run the existing observer-host scenario and prove the observer never owns or plans Guide-Bot guidance.
- [x] Add and run an explicit voluntary-abdication scenario that preserves route intent and transfers planning to an eligible peer.
- [x] Run cooperative save/restore with reversed player slots and prove ownership follows player identity rather than the saved slot number.
- [x] Run two successive host migrations and prove one synchronized owner remains after each disconnect and rejoin.
- [x] Add owner-local planner-count and automap assertions so nonowners cannot silently execute semantic route scans.
- [x] Fix demonstrated lifecycle defects in companion control persistence and owner-local high-level route activation only.
- [x] Run scoped quality, native owner/host-migration policy tests, Windows D2 and Android builds, and focused two-emulator scenarios.
- [x] Update the master Phase 7 plan and keep Phase 8 deletion gated on a green multiplayer regression cycle.

## Boundary

Ownership and lifecycle hooks may synchronize owner identity and endpoint mode, clear transient semantic intent, and schedule an owner-local replan. They must not construct `Point_segs`, alter classic Guide-Bot movement state or cadence, aim, fire, consume simulation RNG, or synchronize a peer's private automap-derived route result.

## Result

- Observer hosts remain nonowners and perform zero semantic route scans.
- Voluntary abdication transfers ownership and `Unexplored` intent; only the new owner scans its private automap.
- Disconnect adoption immediately rebuilds the new owner's semantic waypoint without requesting a physical path.
- Cooperative restore remaps ownership by callsign identity when player slots reverse.
- Two successive host migrations retain one synchronized Guide-Bot owner and valid robot control after each disconnect and rejoin.
- The owned companion's multiplayer control slot no longer expires or gets displaced as an ordinary idle robot.
- Scoped quality, Android all-ABI assembly, Windows D2, all 22 D2 native tests, and the focused ownership and observer-host emulator scenarios pass. The reversed-slot restore and two-host-migration scenarios also pass on this build line.
