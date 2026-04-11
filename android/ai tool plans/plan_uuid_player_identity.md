# Plan: UUID-based player identity for coop saves

## Problem statement

Coop autosaves swap the host callsign to `COOP_AUTOSAVE_CALLSIGN` ("coopsave")
and the game_id to `COOP_AUTOSAVE_GAME_ID` before writing. This lets the save
file survive cross-session callsign changes, but it breaks player mapping on
restore: the save file's player array has callsign="coopsave" for the host, which
never matches any real player.

The current fix is a slot-position fallback (`i == j` when saved callsign is
"coopsave"). This is fragile -- it only works if players rejoin in the same order.

Meanwhile, the infrastructure for proper UUID-based matching **already exists**
but is disconnected from the save/restore path:

- `ClientIdentity.kt` generates a per-install UUID, cached in SharedPreferences
- JNI bridge (`nativeSetClientId`) copies it into `auto_net_client_id[37]`
- Network layer copies it into `Netgame.players[i].client_id[37]`
- Metadata trailer stores `coop_player_record.client_id[37]` per player
- `coop_find_player_in_metadata()` matches by client_id first, callsign second

The gap: `state_restore_all_sub()` maps players purely by callsign comparison on
the base save data, before the metadata trailer is ever consulted.

## Existing infrastructure inventory

| Component | File | Status |
|---|---|---|
| UUID generation | `ClientIdentity.kt` | Working |
| JNI bridge | `jni_main.c:nativeSetClientId` | Working |
| C global | `auto_net_client_id[37]` in auto_net.h/c | Working |
| Network propagation | `net_udp.c` -> `Netgame.players[i].client_id` | Working |
| Metadata storage | `coop_player_record.client_id[37]` | Working |
| Metadata matching | `coop_find_player_in_metadata()` | Working, unused in restore |
| Save header callsign | `state.c:913-918` (write), `1313-1323` (read) | Uses "coopsave" |
| Player mapping | `state.c:1730-1742` | Callsign-only + slot fallback hack |

## Design

### Core change: use metadata trailer for player mapping

Instead of mapping players by callsign in the base save data, read the metadata
trailer first and use `coop_find_player_in_metadata()` to map current players to
saved players by client_id.

### Phase 1: Wire client_id matching into state_restore_all_sub

**Files**: d2/main/state.c, d1/main/state.c

In the player mapping loop (d2 ~line 1730), replace the
callsign/COOP_AUTOSAVE_CALLSIGN/slot-position hack with client_id matching:

1. After reading all `restore_players[]` from the save file, read the metadata
   trailer (it's at the end of the file -- the file is already open)
2. For each current player `i`, call `coop_find_player_in_metadata()` with
   `Netgame.players[i].client_id` and `Players[i].callsign`
3. The returned index maps into `meta.active_players[]`, which corresponds to the
   save-file player order
4. Use that index as `j` in the restore loop (instead of the inner `j` loop)
5. If `coop_find_player_in_metadata()` returns -1, the player is new -- skip
   them (they spawn fresh)

This approach:
- Prefers client_id match (survives callsign changes across sessions)
- Falls back to callsign match (works for non-Android or pre-UUID saves)
- Handles player count mismatches (fewer or more players than saved)
- Removes the need for the `i == j` slot-position hack entirely

**Key detail**: The metadata trailer's `active_players[]` array is written in
player-slot order (0..N_players, filtered to CONNECT_PLAYING). The index returned
by `coop_find_player_in_metadata()` (0..7) is the active_players index, NOT the
original player slot index. We need to either:
- (a) Store the original slot index in `coop_player_record`, or
- (b) Ensure the metadata active_players order matches restore_players order

Looking at `coop_write_save_metadata()`:
```c
for (i = 0; i < MAX_PLAYERS; i++) {
    if (Players[i].connected == CONNECT_PLAYING) {
        coop_snapshot_player(i, &meta.active_players[meta.num_active_players]);
        meta.num_active_players++;
    }
}
```

Active players are written in slot order, but gaps are collapsed: if slots 0,1,3
are playing, active_players[0..2] maps to slots 0,1,3. So active_players index 2
corresponds to restore_players slot 3, not slot 2.

**Fix**: Add an `original_slot` field to `coop_player_record` so we can map back.
This bumps `COOP_SAVE_META_VER` to 4.

### Phase 2: Stop swapping host callsign in autosaves

**Files**: d2/main/coop_save.c, d1/main/coop_save.c

Currently `coop_autosave()` swaps caller callsign to "coopsave" before calling
`state_save_all_sub()`. This serves two purposes:
1. **Stable filename**: `coopsave.mg5` instead of `randomcallsign.mg5`
2. **Stable save header**: The callsign in the header gates file opening

Once client_id matching is in place, we can stop swapping the callsign in the
player data. However, the filename and header callsign still need to be stable
so any host can open/find the file.

**Approach**: Keep the filename as `coopsave.mgN` (it's a shared save, not
player-specific). Keep `COOP_AUTOSAVE_CALLSIGN` in the file header only -- don't
swap the actual `Players[Player_num].callsign`. This means:

```c
/* In coop_autosave(): */
/* Save with real callsign in player data, but sentinel in header */
uint saved_game_id = state_game_id;
state_game_id = COOP_AUTOSAVE_GAME_ID;
/* DON'T swap callsign anymore -- let real callsign flow into player data */

/* But we need the header callsign to be stable... */
```

The header callsign is written in `state_save_all_sub()` at the very top:
```c
PHYSFS_write(fp, &Players[Player_num].callsign, sizeof(char)*CALLSIGN_LEN+1, 1);
```

We have two options:
- (a) **Add a parameter** to `state_save_all_sub` for header callsign override
- (b) **Swap only during header write**, not in the Players array

Option (b) is simpler: swap callsign, write header, swap back, then continue
with the rest of the save (which writes the real callsign in the player_rw
structs). But `state_save_all_sub` writes the header and players in one flow.

Option (a) is cleaner. Add `const char *header_callsign_override` parameter. If
non-NULL, use it for the header callsign instead of `Players[Player_num].callsign`.
This avoids any need to temporarily mutate player data.

**Selected: Option (a)** -- add override parameter. This is a small signature
change, callers other than `coop_autosave` pass NULL.

### Phase 3: Clean up save header validation

**Files**: d2/main/state.c, d1/main/state.c

Currently the callsign check in `state_restore_all_sub` and `state_get_game_id`
accepts either the real callsign or `COOP_AUTOSAVE_CALLSIGN`. This stays -- it's
still needed because the header callsign for autosaves remains "coopsave" (the
file is shared, not owned by any single player).

No changes needed here. The header callsign serves as an ownership/access-control
check. For autosaves, "coopsave" is a valid shared-ownership sentinel.

### Phase 4: Unmatched players get HUD message

**Files**: d2/main/state.c, d1/main/state.c

After the mapping loop, for any current player `i` where `coop_player_got[i] == 0`
and the player is connected, print a HUD message:

```c
if (!coop_player_got[i] && (Players[i].connected == CONNECT_PLAYING ||
    Players[i].connected == CONNECT_WAITING))
    HUD_init_message(HM_MULTI, "Player '%s' not in save -- spawning fresh",
                     Players[i].callsign);
```

Also log via COOPLOG for diagnostics.

### Phase 5: Remove slot-position fallback

**Files**: d2/main/state.c, d1/main/state.c

Delete the `i == j` fallback code and the `callsign_match` variable. The entire
inner `j` loop is replaced by the metadata lookup from Phase 1.

## Implementation order

### Step 1: Add `original_slot` to coop_player_record (d2+d1)
- Add `uint8_t original_slot` to `coop_player_record`
- Bump `COOP_SAVE_META_VER` to 4
- Set it in `coop_snapshot_player()` (pass pnum, store it)
- Wait, `coop_snapshot_player()` already takes `pnum` -- just store:
  `rec->original_slot = (uint8_t)pnum;`
- Files: d2/main/coop_save.h, d2/main/coop_save.c, d1/main/coop_save.h,
  d1/main/coop_save.c

### Step 2: Add header_callsign_override to state_save_all_sub (d2+d1)
- Change signature: `int state_save_all_sub(const char *filename, const char *desc)`
  -> add `const char *header_callsign_override`
- In the coop header-write section, use override if non-NULL
- All existing callers pass NULL
- `coop_autosave()` passes `COOP_AUTOSAVE_CALLSIGN`
- Remove the callsign swap in `coop_autosave()` (keep game_id swap)
- Files: d2/main/state.c, d2/main/state.h, d2/main/coop_save.c,
  d1/main/state.c, d1/main/state.h, d1/main/coop_save.c

### Step 3: Replace callsign mapping with metadata/client_id matching (d2+d1)
- In `state_restore_all_sub()`, after reading all restore_players, seek to end of
  known data and call `coop_read_save_metadata()` to get the trailer
- For each current player i, call `coop_find_player_in_metadata()` with
  `Netgame.players[i].client_id` and `Players[i].callsign`
- Map using `meta.active_players[found_idx].original_slot` as the `j` index into
  `restore_players[]`
- If no metadata found (old save), fall back to existing callsign loop
- Delete the `i == j` slot-position hack
- Add HUD message for unmatched players
- Files: d2/main/state.c, d1/main/state.c

### Step 4: Build and test
- Build both d1 and d2
- Run existing coop autosave regression tests
- Verify: host loads correctly with real callsign in player data
- Verify: peer loads correctly matched by client_id
- Verify: new player not in save spawns fresh with HUD message
- Verify: old saves without metadata still work (callsign fallback)

### Step 5: Code quality
- Run `android\run-code-quality.ps1 --fix`
- Check for warnings in new code
- Verify d1/d2 diffs are minimal and match existing style

## Simplification (pre-release)

No version bumping or backwards compatibility with old saves needed. No fallback
for pre-metadata save files. All current saves have metadata trailers
(`coop_write_save_metadata` is called for ALL saves, not just autosaves).

DO retain: fallback for unmatched players (not found in metadata -> spawn fresh
with HUD message). This handles the case where host matches but a peer doesn't.

## Simplified implementation

### Step 1: Add original_slot to coop_player_record (d2+d1)
- Add `uint8_t original_slot` to the struct
- Set `rec->original_slot = (uint8_t)pnum` in `coop_snapshot_player()`
- Files: d2/main/coop_save.h, d2/main/coop_save.c, d1/main/coop_save.h,
  d1/main/coop_save.c

### Step 2: Replace mapping loop in state_restore_all_sub (d2+d1)
- After reading restore_players, seek to end of file to read metadata trailer
- Seek back to continue normal sequential reads
- For each current player, call `coop_find_player_in_metadata()` (client_id first,
  callsign second)
- Map using `active_players[idx].original_slot` as the j index into restore_players
- Unmatched players: log + HUD message, spawn fresh
- Remove the i==j slot-position hack and the double for loop
- Keep non-Android path unchanged (#else)

### Step 3: Build and test

### What stays unchanged
- Callsign swap in coop_autosave() (still swaps to COOP_AUTOSAVE_CALLSIGN)
- state_save_all_sub signature
- Save header sentinel (COOP_AUTOSAVE_CALLSIGN / COOP_AUTOSAVE_GAME_ID)
- Filenames (coopsave.mgN)
- Non-Android code paths

## Status
- [ ] Step 1: original_slot in coop_player_record
- [ ] Step 2: mapping loop replacement
- [ ] Step 3: build + verify
