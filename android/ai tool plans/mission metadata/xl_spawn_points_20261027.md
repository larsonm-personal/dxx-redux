# Authored XL spawn points

Continue the authorized eight-hour run on xl_more_18_spawns, stacked on PR #161
(239cf7a87). PR #160 is green and unmerged; monitor the latest PR #161 head while
working. Do not merge PRs

## Required behavior

Implement type-28 wall and admitted nonplayer object actions in a separate XL
runtime, leaving Redux's Player_init table unchanged. The surveyed originals
contain 20 wall actions across six selected levels. This is priority evidence,
not qualification of those missions

1. Follow pinned XL DoSetSpawnPoints: cycle authored links over the 16 XL spawn
   slots, use segment centers and source-matched orientation, preserve raw action
   fields, and show the action-specific message. Reject empty lists and invalid
   segment/side links before loading; the source's zero-link modulo is undefined
2. Apply the retained destination during real player initialization after death.
   Keep activation distinct from immediate teleportation. Preserve earned spawn
   locations through death, disk and rewind; clear them on a new source/level
3. Respect the existing named SPAWN marker override in XL singleplayer and remove
   the first such marker when an authored spawn action replaces it. Keep ordinary
   marker slots stable instead of reproducing the source's unsafe table compaction.
   Do not record an internal trigger effect as a new direct player-input command
4. Store checked XL spawn state in the existing atomic feature trailer, replacing
   its development schema directly. Bind destinations/transforms to retained
   source geometry. Delayed object-master configuration survives removal of its
   initiating actor; it must not teleport a replacement object
5. Compare actual extracted DoSetSpawnPoints and TriggerSetOrient functions.
   Exercise real wall crossings, direct object autoplay/removal, nested/delayed
   changes, repeated activation, marker precedence/cleanup, lethal death/respawn,
   source transitions, malformed restores and stock isolation. Use controlled
   poses and combat, never autonomous navigation
6. Add a registered Android runner with owned JSON and master timeout. Verify
   death/respawn and checkpoint/disk state through stock. Run focused regressions,
   scoped quality, both catalogs and private-policy checks; update maintained
   evidence and open a separate PR. Require latest-head green CI

## Source audit

Reference revision: arbruijn/d2x-xl
2576a98b5de310e3ac7c9777df2677d1627b8bae, pinned CRLF tree under
C:/local/dxx-redux/temp/xl-boss-transport-20261027/reference-bootstrap-crlf/reference-source

- main/trigger.cpp: DoSetSpawnPoints, TriggerSetOrient, actor restrictions,
  message convention and stateful action dispatch
- objects/player.cpp: MovePlayerToSpawnPos and exclusion of the local player
  from spawn occupancy checks. Multiplayer displacement is outside this scope
- gameio/loadgame.cpp: InitPlayerPosition and ordinary death/new-ship lifecycle
- gameio/savegame.cpp: source persistence of all 16 initial transforms
- objects/marker.cpp: exact SPAWN label, first-match lookup, movement and forced
   deletion; include/marker.h defines nine singleplayer marker slots

Manual marker teleport shortcuts and multiplayer spawn selection remain outside
this singleplayer authored-spawn qualification. Existing marker placement is
sufficient to exercise named-marker respawn precedence without adding UI

## Completed acceptance

- Separate XL table, runtime dispatch and respawn hook; Redux table untouched
- Schema 8 atomic destination/geometry validation and lifecycle clearing
- 1,536 extracted-source transforms and 16 player/object HUD cue comparisons
  pass under MSVC and Linux GCC -Wall -Werror. Object events retain the source
  message; TF_NO_MESSAGE suppresses it independently of bits 32/64
- Native real crossing, lethal death/respawn, named-marker disk precedence,
  removed-owner nested deadlines, boost/event/spawn saves through stock,
  13 malformed trailers and fresh-level isolation pass
- 24 focused regressions pass in 240.74 seconds; final formatted four-test
  verification passes in 31.82 seconds; final cue/spawn integration in 6.58 seconds
- Final Android run ffb2553ca8044d6b8e4be5da34d252cb passes 56/56 steps in
  195,937 ms, including two actual deaths and disk/memory/stock transitions.
  Protected emulator hashes are restored exactly. See the evidence for the
  corrected narrow-room fixture and absent stock-only-field assertion
- Scoped mixed quality passes; catalogs report 399 top-level entries and
  454 support scripts; all 12 private-asset policy checks pass
- Six original spawn-point candidates are freshly inspected, five decode and
  one still rejects an invalid trigger target. None receives runtime qualification

Publish this as one commit over xl_more_17_movement and require latest-head green
CI. Continue with modern water/robot/generator interaction qualification; keep
legacy fluid profiles, custom gameplay overrides and multiplayer separately gated
