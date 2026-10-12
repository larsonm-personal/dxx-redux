# Public robot loot and runtime save continuation

Continue the compact public corpus after the door/damage segment, using pinned
shareware content and ordinary level-start controls. Keep every replay/oracle
pair below 100000 bytes and leave all retail recordings and goldens unchanged.

1. Inspect authored pickup-carrying robots and demonstrate a reachable route,
   kill, spawn and collection without changing RNG seeds or injecting state
2. Reuse existing causal loot observations; assert the killed robot's drop,
   inventory/resource change and applicable autoselection, with no-fire and
   no-collect negative controls
3. Publish source before capture, independently capture twice, compare strict
   state/diagnostic/terminal/SIM oracles in true-headless and headed playback,
   and inspect Xvfb screenshots before admitting compact payloads
4. Add a separate runtime save/load continuation integration using existing
   save/replay infrastructure and temporary native saves. Compare post-restore
   gameplay and SIM RNG to uninterrupted execution without normalization
5. Preserve source and asset provenance, run required format/build/catalog
   gates, publish focused stacked draft PRs without merges, and watch final CI

Initial candidate investigation identified a D1 level 1
laser-upgrade carrier (object 39, signature 40, segment 231), and D2 level 2
shield/quad carriers (objects 32/33, signatures 33/34, segments 195/196). The
current D1 combat target legitimately fails its random drop; the D2 target has
no authored pickup. A shorter probabilistic route must use the existing seed,
not seed shopping. Existing wall-blast persistence tests exercise real saves
but do not establish full-game-frame continuation; inspect actual runtime
save compatibility and report any unsupported state rather than hiding it.

## Route experiments and scope

- Every attempt used the pre-existing initial SIM seed 1234, the same normal
  difficulty-two spawn and authored carrier 39/20/40. No world edits or RNG
  compensation were used
- Full center-guided 30 FPS route: real kill, laser drop and collection, 628
  frames and 106895 combined bytes. Rejected for exceeding the byte cap
- Same full route at 25 FPS: player died before reaching the carrier. Rejected
- Portal-only 30 FPS route: player drifted into segment 95 at the segment-100
  junction and died. Rejected
- Portal guidance through the broad early rooms, retaining centers from hub 100:
  real kill/drop/collection, 598 frames and 101609 bytes. Still rejected as oversized
- Same route with ordinary faster pickup pursuit and no idle tail: 582 frames,
  98839 combined bytes in development. Laser gain is frame 580; the consumed
  physical object retires at 581. Clean published-source captures remain required
- The route also collects an original energy pickup and has an uncredited robot
  death before the selected carrier. The semantic gate isolates the causal
  player-owned laser kill and exact new drop, while every raw event remains in
  the immutable oracle. It does not claim exactly one world robot death
- The upgrade advances the existing selected laser from level 1 to level 2.
  Selection stays on primary 0; this is not a new-primary autoselection claim

## Save-continuation boundary

The implementation is a separate following segment. Ordinary save readers
intentionally rebase the game clock and reset the absolute SIM draw count.
Full trajectories are not equivalent: even the short flight checkpoint shifts
ship/robot motion, while a longer active-door continuation also changes health,
SIM state and door evolution. Do not change save formats or clock semantics in
this task. Gate exact immediate promised persistence and narrowly qualified
flight inventory/health/RNG continuation, retain raw differences and report
full trajectory equivalence as unqualified. Temporary native saves stay local.

## Review and admission preparation

Independent source review strengthened physical collection evidence to require
positive pickup lifetime, the native consumed flag, exact next-frame retirement
and exclusion of competing laser pickups. The no-collect control also requires
the living player and exact unconsumed terminal drop, rather than treating any
semantic error as success. Its development replay completes all 582 frames,
keeps the original strict hashes through the post-drop boundary, leaves score
500 and laser level 0, and retains powerup 79 / signature 301. Source-only
publication and fresh repeated capture follow the final published door corpus.
