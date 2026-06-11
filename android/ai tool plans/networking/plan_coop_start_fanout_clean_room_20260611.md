# Coop start fanout clean-room plan

## Goal

Allow cooperative games with more players than author-defined coop starts to begin reliably by synthesizing extra, non-overlapping player start positions near the existing start point or points.

This must be implemented as Redux-native work. Rebirth was used only as behavioral research. Do not copy or transliterate Rebirth code or structure.

## Current Redux behavior

- `d1/main/gameseq.c` and `d2/main/gameseq.c` both scan level objects in `gameseq_init_network_players()`.
- In coop, the first normal player start plus all `OBJ_COOP` objects become `Player_init[]` entries.
- After scanning, the current Redux code clones start objects until `MAX_PLAYERS`, using `Objects[k % NumNetPlayerPositions]`.
- Those cloned starts inherit the exact same position and segment as the source object, so extra players can spawn overlapped.
- `InitPlayerPosition()` uses `NewPlayer = Player_num` for coop, so the `Player_init[]` entry for each player is the actual spawn source.
- UDP sync checks `NumNetPlayerPositions < Netgame.max_numplayers`; the clone-up-to-`MAX_PLAYERS` path usually prevents the sync abort, but does not prevent overlap.
- Android launcher hosting can request 2-8 players, but the classic in-game host menu still caps coop at 4.

## License boundary

- Treat the Rebirth implementation as a feature description only.
- Do not reuse upstream Rebirth function names, comments, placement order, capacity-bit model, logging text, or formulas.
- Build the algorithm from local Redux primitives already used in this repo:
  - `obj_create`
  - `Player_init[]`
  - `Objects[]`
  - `obj_relink`
  - `find_point_seg`
  - `get_seg_masks` if available in both games with compatible signatures
  - `vm_vec_*` helpers
  - existing coop warp random-near-target validation as a local precedent, but avoid random placement for level starts.

## Implementation strategy

### 1. Keep the hook point

Use `gameseq_init_network_players()` as the only gameplay hook.

Reason:
- It already owns `Player_init[]`, player object assignment, and synthetic player object creation.
- It runs before `multi_level_sync()` and before `InitPlayerPosition()`.
- Both host and clients load the same level and should derive identical synthetic start positions without extra network protocol.

### 2. Record real start count before synthesis

In both `d1/main/gameseq.c` and `d2/main/gameseq.c`:

- Keep the existing scan loop that fills entries `0..k-1`.
- Store `int real_start_count = k`.
- If `Game_mode & GM_MULTI` and `real_start_count > 0`, synthesize entries until `MAX_PLAYERS`.
- If `real_start_count == 0`, keep current failure behavior. A level with no player starts is malformed and should not invent a segment.

### 3. Replace exact-position clone with deterministic fanout

For each synthetic player index `k`, choose a source start:

- `source = k % real_start_count`
- `source_pos = Player_init[source].pos`
- `source_orient = Player_init[source].orient`
- `source_seg = Player_init[source].segnum`

Generate candidate offsets around `source_pos` using a small fixed table, not RNG. Recommended table:

- Center is reserved for the source player and should only be used if no clear alternative exists.
- Try offsets along local orientation vectors first:
  - right
  - left
  - up
  - down
  - right + up
  - left + up
  - right + down
  - left + down
  - forward
  - backward
- Scale offsets from the player ship radius:
  - start with `radius * 3`
  - if no candidate works, try `radius * 2`
  - if still no candidate works, use exact clone as a last fallback and log it

Use the source start's orientation if possible:

- right axis: `source_orient.rvec`
- up axis: `source_orient.uvec`
- forward axis: `source_orient.fvec`

This makes fanout stable relative to how the level author oriented the start, and avoids relying on segment vertex ordering.

### 4. Validate candidates

A candidate is acceptable only if:

- `find_point_seg(&candidate, source_seg)` returns a valid segment.
- The resulting segment is the source segment or a directly valid containing segment found by `find_point_seg`.
- The candidate is not too close to any already assigned `Player_init[0..k-1]` position.
- The candidate is not too close to any already created player object with `OBJ_PLAYER`.

Suggested distance check:

- Compute `min_dist = player_radius * 2`.
- Use squared distance when possible to avoid extra fixed-point square roots.
- Require `vm_vec_dist_quick(&candidate, &Player_init[i].pos) >= min_dist`.

Optional stronger validation:

- If `get_seg_masks()` is compatible in both games, reject positions whose sphere would be too close to segment walls.
- Keep this optional for the first implementation if signature drift or compile risk is high. `find_point_seg` plus spacing is already an improvement over exact overlap.

### 5. Create synthetic player object at chosen candidate

For each generated start:

- Fill `Player_init[k]` with chosen `pos`, source `orient`, and chosen segment.
- Call `obj_create(OBJ_PLAYER, k, Player_init[k].segnum, &Player_init[k].pos, &Player_init[k].orient, Polygon_models[Player_ship->model_num].rad, CT_NONE, MT_PHYSICS, RT_POLYOBJ)`.
- Set `Players[k].objnum = i`.
- Set or verify `Objects[i].id = k`.

Keep `NumNetPlayerPositions = MAX_PLAYERS` after synthesis so existing sync and join code continues to work.

### 6. Preserve deathmatch behavior

This feature is only intended to solve coop start shortages, but the existing code currently clones starts for all multiplayer modes.

Conservative first implementation:

- For coop, synthesize non-overlapping starts.
- For non-coop multiplayer, preserve the existing exact clone behavior unless testing shows deathmatch also benefits.

Reason:
- Deathmatch spawn selection has separate random/secluded logic.
- Changing deathmatch spawn geometry could alter competitive behavior and tests unrelated to the request.

If we want one helper for both modes later, add it as a separate tranche after coop testing.

### 7. Multiplayer max-player caps

The Android launcher path already allows 2-8 players, and `net_udp_auto_host()` stores the passed `max_players` into `Netgame.max_numplayers`.

The classic in-game network setup still caps coop at 4:

- `d1/main/net_udp.c`
- `d2/main/net_udp.c`

Plan:

- Do not change this in the same first tranche unless the user wants classic host menu support too.
- Add a follow-up task to lift coop classic menu cap to 8 after the spawn fanout is proven.

Reason:
- Android is the immediate target.
- Start fanout and menu policy are separate risks.

### 8. Diagnostics

Add concise `con_printf(CON_VERBOSE, ...)` or Android coop log lines for:

- number of real coop starts found
- number of synthetic starts generated
- fallback to exact overlap if no candidate fits

Avoid chatty per-candidate logs unless debugging a failing level. Android users can inspect debug log export, but routine logs should stay compact.

### 9. Introspection support for tests

Extend Android introspection only if current multiplayer state is insufficient.

Useful fields:

- `multiplayer.players[]` with player number, connected state, object type, segment, x/y/z
- `multiplayer.num_net_player_positions`
- `multiplayer.max_players`

If similar fields already exist, reuse them. The test should assert positions, not screenshots.

### 10. Test plan

Unit-style helper test, if a shared helper is introduced:

- Input: one start, ship radius, valid mock segment or simple adapter.
- Expected: generated positions are distinct and deterministic.
- Input: tiny/no-space conditions.
- Expected: fallback behavior is explicit.

Headless or automation test:

- Create or reuse a tiny test mission with one coop start.
- Host Android coop with 4 players if two-emulator automation is not ready for 8.
- Verify host and client enter gameplay.
- Use introspection to verify active player objects do not share identical positions.

Manual two-emulator verification:

- Host D2 coop from Android launcher with `max_players=5` or higher on a mission/level known to have fewer coop starts.
- Join with a second emulator.
- Start game.
- Confirm no "Not enough start positions" abort.
- Export debug logs and confirm generated-start line.

Regression checks:

- Build D1 and D2 Windows host with `run-windows-build.ps1`.
- Run scoped code quality on touched C files and any test/introspection files.
- If Android native files change, run relevant Gradle/unit tests and at least one launcher/game automation script.

## Edge cases

- One start in a cramped segment: generate as many non-overlapping positions as fit, then exact-clone fallback for remaining players. This preserves playability over aborting.
- Multiple starts with one cramped and one roomy segment: round-robin source selection may waste roomy capacity. If this appears in testing, revise candidate source selection to prefer starts with successful prior generated candidates.
- Remote clients must derive the same starts as host. Avoid randomness and avoid using object allocation order outside the existing deterministic `obj_create` loop.
- Save/restore should not need new metadata because the feature only affects fresh level starts. Coop restore remaps actual objects from save state separately.
- Observer mode currently benefits from `MAX_PLAYERS` starts existing. Preserve that invariant.

## Proposed phases

1. [x] Add a plan and confirm scope.
2. [x] Implement deterministic coop fanout in `d1/main/gameseq.c` and `d2/main/gameseq.c`.
3. [x] Add compact diagnostics.
4. [x] Add a focused automation or headless test for underspecified coop spawning.
5. [x] Run Windows host build for D1 and D2.
6. Optional follow-up: raise classic in-game coop max-player UI cap from 4 to 8.

## Implementation notes

- Implemented in both `d1/main/gameseq.c` and `d2/main/gameseq.c`.
- Coop synthetic starts now try deterministic offsets from the source start's local right, up, and forward vectors.
- Candidates are accepted only when `find_point_seg()` finds a containing segment and the candidate is not too close to already assigned starts.
- If no candidate fits, the code falls back to the source start position and counts that fallback in a verbose log line.
- Non-coop multiplayer keeps the previous exact clone behavior.
- Verified with `.\run-windows-build.ps1 -Target both`.
- Added `-coop-starts-json-out` to the host headless metadata dump tool so mapsets can be loaded headlessly and checked after `gameseq_init_network_players()` runs in coop mode.
- Added `android/tests/test_coop_start_fanout_mapset.ps1`, defaulting to Plutonian Shores.
- Verified Plutonian Shores with `.\android\tests\test_coop_start_fanout_mapset.ps1`: all 32 levels reported 8 net player positions with no duplicate positions, and no start pairs closer than two ship radii, among the first 8 starts.
- Rebuilt `dxx-redux-d2-headless-metadata` and `dxx-redux-d1-headless-metadata`.
