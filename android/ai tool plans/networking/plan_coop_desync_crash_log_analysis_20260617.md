# Coop desync and mine exit crash log analysis plan

## Scope

Study only. Analyze `game_data/game_logs_net_desync_2/` for the June 16, 2026 coop desync and crash after blowing the mine, without source edits.

## Goals

- Identify the crash signature from the tombstone and nearby debug log lines.
- Build a rough session timeline from launch through desync symptoms, mine exit, and crash.
- Compare the timeline against recent desync-related code areas without changing code.
- Produce focused hypotheses and a next-step instrumentation or fix plan.

## Work Plan

- [x] Read repo instructions and confirm study-only scope.
- [x] Inventory supplied log files and existing worktree state.
- [x] Parse the tombstone and classify whether it has enough native crash detail.
- [x] Summarize debug log categories, timestamps, and high-volume message patterns.
- [x] Extract lines around network sync, level transition, mine exit, object/lifetime errors, asserts, fatal logs, and Java/native crash markers.
- [x] Review recent modified files and likely related networking/desync paths for context only.
- [x] Write conclusions, risks, and proposed next investigation steps.

## Notes

- Do not edit d1, d2, android runtime code, or tests during this pass.
- Preserve unrelated working tree changes.
- Prefer derived summaries over copying large log excerpts into the plan.

## Findings

- The tombstone does not contain a native backtrace. It only reports an xcrash dumper failure: `child terminated normally with non-zero exit status(102)`.
- The debug log is 660,307 lines from `2026-06-16T20:25:20.783` to `2026-06-16T21:58:30.886`. Texture logging dominates the file: 646,107 `TEXTURE` lines vs. 10,869 `GAME LOGS`, 3,264 `LAUNCHER`, and 54 `NETWORK`.
- The relevant coop run starts at `2026-06-16T21:50:34` with Player32 hosting, Player68 joining, and D2 level 2 starting at `2026-06-16T21:51:00`.
- Host auto-restore triggers at `2026-06-16T21:51:09` from `Players/save_sets/coop/d2/coopsave.mg7`, slot 7, id `1129271120`.
- The restore maps player slot 0 from saved callsign `coopsave` over current callsign `Player32`. The log then says restore completed with callsign `coopsave`.
- Later gameplay messages attribute local-player damage and warp targets to `coopsave`, including `Player68 warped to coopsave` and damage from `coopsave` weapons. This is strong evidence that the autosave sentinel callsign leaked into live player state after restore.
- Reactor destruction is logged at `2026-06-16T21:55:04.456`. The next strong lifecycle break is `2026-06-16T21:56:18.880`, where the app resumes and LAN discovery is refreshed. At `2026-06-16T21:56:43`, a new native startup begins and auto-join to `127.0.0.1:42430` times out after 29 requests.
- No `request_resync` diagnostics appear in this client A log. The recent `coop desync fix` still matters because it added peer-side autosave restore resync logic, but the strongest evidence in this log is the host restore identity leak.

## Hypotheses

1. Primary: coop autosave restore copies saved `player` structs over live `Players[i]` without preserving the current callsign/client identity. For autosaves, slot 0 was saved under `COOP_AUTOSAVE_CALLSIGN` (`coopsave`), so Player32 becomes `coopsave` after restore. This can desync identity-sensitive packets, friendly damage attribution, autosave filenames, absent-player metadata, and rejoin/endlevel behavior.
2. Secondary: the peer-side resync path added in commit `ebda5272` can force a client into `NETSTAT_WAITING` and repeatedly call `net_udp_send_request()` from inside an existing game. It should be audited so it cannot run during `Control_center_destroyed`, `Endlevel_sequence`, or normal next-level sync.
3. Crash classification is still incomplete because the exported tombstone lacks a backtrace. The visible post-crash behavior looks like the launcher/native game restarted after the mine exit transition, then attempted an auto-join that timed out.

## Proposed Next Work

- Fix or instrument `d2/main/state.c` and the D1 twin so Android coop restore preserves live session identity fields after copying saved player state. At minimum preserve `Players[i].callsign` and likely align `Netgame.players[i].callsign` and `client_id` with the active session rather than the save sentinel.
- Add before/after restore logs for `Players[i].callsign`, `Netgame.players[i].callsign`, `client_id`, `objnum`, `connected`, and `Player_num`.
- Guard `net_udp_request_resync_from_host()` against endlevel/reactor/level-sync states, and log when the guard skips.
- Add endlevel/level-sync diagnostics around `net_udp_endlevel()`, `net_udp_level_sync()`, `net_udp_wait_for_requests()`, and `net_udp_wait_for_sync()` so the next device run shows whether next-level transition fails before or after sync.
- Reduce or gate `TEXTURE` diagnostics before the next network run so the useful network/game logs are not buried.

## Follow-up Static Analysis: Unavoidable Level 2 Restore

### User Symptom

- Players A and B appear to have unavoidable save games.
- Any attempted level load or host choice shunts the session back to level 2's save.
- The forced restore corrupts one or both players and prevents joining.

### Static Analysis Plan

- [x] Trace how `coop_restore_slot.txt` is written, read, and cleared.
- [x] Check whether restore intent is scoped by game, mission, level, host, or session.
- [x] Check whether auto-restore can arm for a host who did not explicitly choose restore.
- [x] Check whether launcher/auto-net start arguments can accidentally imply restore.
- [x] Identify static root causes and propose the smallest fix set.

### Static Findings

- `coop_restore_slot.txt` contains only a slot number. It has no mission, level, timestamp, host/client identity, lobby id, or one-shot launch token.
- `CreateGameDialog` defaults `selectedSave` to the newest full coop save whenever any full save exists. That makes restore opt-out rather than opt-in.
- `CreateGameDialog` also rewrites the level field to the selected save's level. This can silently replace the user's requested level with the latest save's level.
- The online recent-save shortcut writes `coop_restore_slot.txt` immediately when the card is clicked, before the user confirms the dialog. Dismissing the dialog can leave a stale restore slot on disk.
- `LobbyScreen` and `LanDiscoveryTab` both auto-enable save restore offers when a matching save exists, write `coop_restore_slot.txt` as a side effect, and can update the advertised lobby level to the save level.
- Native `coop_arm_auto_restore()` only checks whether the selected slot has a game id. It does not validate the save metadata mission/level against the requested launch or the current lobby/player identity before arming.
- `state_restore_all_sub()` reads `current_level` from the save and calls `StartNewLevelSub(current_level, ...)`. Therefore a stale level 2 restore slot can override any launcher-requested level and force the game back to level 2.
- This combines with the earlier identity leak: the forced restore then copies saved `coopsave` player data into a live slot and can corrupt active player identity.

### Fix Shape

- Make coop full-save restore opt-in everywhere. Default to fresh start unless the user explicitly selects a save or presses an explicit resume action.
- Never write `coop_restore_slot.txt` before the final host/create confirmation, except for a deliberate one-click resume action.
- Delete `coop_restore_slot.txt` whenever fresh start is selected, when a create/host dialog is dismissed after a prefilled resume flow, and before normal non-resume host launches.
- Replace the restore intent file with structured data or an adjacent sidecar including at least game, mission, level, slot, timestamp, and selected restore type.
- In native `coop_arm_auto_restore()`, read the save metadata trailer before arming and reject mismatched mission, level, stale timestamp/token, or missing current-host/client identity.
- Preserve live player identity during restore before re-testing multiplayer.
