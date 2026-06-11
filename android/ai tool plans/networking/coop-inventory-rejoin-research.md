# Coop inventory rejoin research

## Goal
- Find existing coop inventory restore or cache behavior.
- Identify why it may only work for save restores or same-level reconnects.
- Propose a low-risk implementation path for coop QoL inventory caching and same-level spew cleanup.

## Plan
- [done] Trace current coop QoL, reconnect, player leave, and coop save restore paths.
- [done] Identify reusable inventory serialization/copy helpers.
- [done] Map same-level versus next-level reconnect behavior.
- [done] Produce implementation recommendations and test points.

## Findings
- Existing inventory cache lives in `android/app/src/main/cpp/shared/coop/coop_save.c` and is wired from both `d1/main/multi.c` and `d2/main/multi.c`.
- Disconnects call `coop_track_absent_player()`, rejoin sync calls `coop_send_restore_inventory()`, and clients apply `MULTI_COOP_RESTORE_INV`.
- Save restore metadata and next-level progress sidecar also store `coop_player_record`.
- Same-level spew is not deleted before restore. Drop powerups are mapped to the creating player through `object_owner`, so a cleanup can use existing `multi_send_remobj()`.

## Follow-up: failed level-2 join
- [done] Trace level mismatch dump/kill path and fresh launcher rejoin state.
- [done] Check whether a failed join overwrites or loses the host's absent-player cache.
- [done] Record likely fix points.

## Follow-up findings
- The UDP host rejects a join when the joining client's advertised level does not match `Current_level_num`, before player identity matching or cached inventory restore.
- LAN in-game join uses the advertised current level from the LAN announcement.
- Online late join `GAME_STARTING` only carries original lobby `game_info`, whose `level_num` can remain level 1 even after host reaches level 2. The server tracks `runtime_level`, but does not include it in `GAME_STARTING`.
- This can launch the client at level 1, produce `DUMP_LEVEL`, and prevent `coop_send_restore_inventory()` from ever running.

## Revised simpler approach
- Restore inventory by identity, not by level. If a joining coop player's client id or callsign matches a cached record, send the cached weapons, ammo, energy, shields, score, durable flags, and stats after successful object sync.
- Keep the UDP level match requirement for admission. The client still needs to launch the same level as the host before object sync can work. Do not accept a level-1 client into a level-2 mine.
- Treat level as a limited detail:
  - Same level: remove player-owned dropped powerups before applying the cached inventory, so reconnecting does not duplicate the same inventory as both spew and restored items.
  - Different level: restore inventory, but do not restore per-level keys. Existing `coop_apply_record_to_player(..., same_level)` already has this key split, but the packet currently passes the wrong source level.
- Prefer client id when present, then callsign. Callsign-only matching is acceptable as the requested fallback, but client id avoids collisions when two devices use the same pilot name.

## Revised bug-fix plan
- [done] Fix online late join launch level. Include the server runtime `current_level` in `GAME_STARTING`, or patch `game_info.level_num` from `runtime_level` for in-progress late joins. This is still needed because UDP object sync requires matching levels.
- [done] Fix progress inventory one-shot behavior. Replace `coop_progress_restore_attempted` with a per-level attempt marker or reset it on level start, so level 2 can load the level 1 progress sidecar.
- [done] Fix restore packet source-level handling. Store or transmit the cached record's source level instead of always writing `Current_level_num`, so same-level keys and spew cleanup are decided correctly.
- [done] Add spew cleanup before same-level restore. On host, before sending inventory to a rejoining player whose cached record came from the current level, remove `OBJ_POWERUP` objects owned by that player using existing object ownership mapping and `multi_send_remobj()`.
- [done] Send `MULTI_COOP_RESTORE_INV` directly to the joining player after object sync instead of broadcasting it.
- [done] Gate live rejoin restore and spew cleanup on `NETGAME_FLAG_COOP_QOL`, while leaving explicit coop save/autosave restore behavior independent unless product behavior says otherwise.
- [partial] Add diagnostics and tests. Cache lookup/source level/current level/spew cleanup logging was added; focused multiplayer integration tests were not added in this tranche.

## Implementation verification
- `git diff --check` passed.
- `cargo check -q` passed in `server/`.
- Linting/formatting passes were intentionally skipped because other tasks were running.
