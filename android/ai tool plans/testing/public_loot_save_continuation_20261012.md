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

## Published-source capture

Published source-only commit `b70c4bb4c2aa40001ccc2157501d336ab5a8b17d`
parents the final door corpus `4d747f99cde4351c4daac7233858a5f7ff654822`.
Both native fixtures, console mains and headed mains were rebuilt after CMake
reconfiguration. Two independent fresh-process captures produced byte-identical
replay/oracle/manifest files: 582 frames, 53097 replay bytes and 45742 oracle
bytes, totaling 98839. The original seed remains 1234. A third Xvfb-rendered
capture matches every strict gameplay, terminal, SIM and causal observation
hash; inspected screenshots show the real carrier and the level-2 laser HUD.

The selected carrier takes eight player-owned laser hits, dies at frame 542,
creates powerup 79 / ID 3 / signature 301 at 550, and retires at 566. Laser
inventory advances at 580; its consumed powerup retires at 581. All 1360 SIM
events and 10 damage/loot observations are immutable capture-derived evidence.
The twelve-case corpus leaves every older replay/oracle and action entry intact.

Scoped mixed-language formatting, 66 public semantic unit tests, 18 runner
unit tests, 14 compact-format tests and automation catalog validation passed.
The inherited subprocess catalog wrapper remains blocked in this container by
`/proc/self/task/<tid>/children` returning ENOENT; direct normal and extended
catalog discovery each returns 342 unique, positively timed entries. CI runs
the unchanged complete catalog wrapper on its normal Linux host.

A focused native/imported D1 diagnostic run additionally proves complete native
repeatability across frame, object, world, terminal and SIM observations.
Imported loot compatibility remains unqualified: an incidental earlier robot
death awards 200 extra points at frame 385 (terminal 700 versus native 500),
and SIM actor-signature metadata differs although draw values match. Original
strict failures and independent gameplay differences are retained without new
normalization or exclusions. The focused loot-only probe does not satisfy the
existing scoped wrapper's required D1 flight inventory; complete CI retains that
unchanged flight-only imported qualification policy and all diagnostic scenes.

Final native qualification passes in both true-headless and headed playback:
two exact positive runs and two runs of each wrong-oracle, no-fire and no-collect
control per backend. Each positive compares all 582 frames, 1360 SIM events and
10 causal observations. Each changed-input replay completes every frame and
fails the real terminal comparison plus the intended causal outcome. No-fire
leaves the original carrier alive and laser level 0. No-collect preserves the
strict prefix through the post-drop boundary, keeps score 500 and laser level 0,
and leaves the exact unconsumed powerup beside a living player. This is 16 native
runtime runs; negative outcomes are expected failures, not skipped tests.

## Imported-scope follow-up

Initial exact-head CI for loot and save passed the full headless positive and
negative matrices, plus every headed native case. The aggregate headed gate
correctly rejected the inherited level-three door's nondefault D2-only
`wall.controlling_trigger=5`. Carry the reviewed per-scene imported-scope fix
from door commit `46890f1ea6d1ea053b0598a107e3188f3bbb6916`: preserve required
flight, retain all original imported diagnostics, explicitly exclude only that
unsupported door import with its evidence, and declare loot diagnostic-only.
No field is normalized away. All replay/oracle bytes, native cases and the
published source-only capture commit remain unchanged. A focused inventory test
requires the loot diagnostic to remain staged alongside the four original cases.

After integration, all 21 runner, 66 public semantic and 14 compact-format tests
pass, as does scoped mixed-language formatting and catalog validation. The
container-specific catalog child-supervisor limitation is unchanged. All twelve
replay/oracle pairs and their original action entries are byte/value preserved.

The final loot tree is linearly restacked onto the corrected door parent rather
than duplicating its repair commit, which caused conflicts in the stacked PR.
The exact original capture source is preserved on the public branch
`replay-source-loot-b70c4bb4`; replay provenance continues to name the original
published `b70c4bb4c2aa40001ccc2157501d336ab5a8b17d`, not the restacked source.
This ancestry-only repair changes no runtime engine, capture controller,
replay/oracle or original action bytes and needs no replacement capture.
