# Coop Lobby Save Auto-Offer -- Plan

Implements the three deferred items from Phase 4 of coop-qol-features.md:
1. ~~MULTI_COOP_SAVE_INFO packet~~ -- not needed; host matches locally from coop_autosave_history.json
2. Host-side logic: scan saves, match player sets, pick newest
3. On accept: host triggers MULTI_RESTORE_GAME with callsign remapping (already works via existing coop_restore_slot.txt -> coop_arm_auto_restore -> multi_send_restore_game -> state_restore_all_sub)

## Design

When the host is in the LobbyScreen in coop mode:
- Read coop_autosave_history.json filtered by mission + host's client_id
- Match each save's callsigns against the current lobby player callsigns
- Auto-select the best match (most callsign overlap, then newest)
- Show a UI section: "Save found: L5 - 2p - Alice, Bob - 3m ago" with [Restore] / [Start fresh]
- On selection change, write/delete coop_restore_slot.txt
- Re-evaluate when the player list changes (players join/leave)

The MULTI_COOP_SAVE_INFO packet (peer -> host save metadata) is skipped because:
- Only the host's saves matter for restoration
- The host has coop_autosave_history.json locally with enough metadata
- Client_id filtering already ensures the host sees only their own saves
- Callsign matching against lobby players gives good enough accuracy

Client_id-based remapping in state_restore_all_sub (instead of callsign-only) would be
a nice enhancement but is out of scope -- the metadata trailer is at EOF, after all
save data, so it would require a two-pass read. Callsign remapping already works.

## Changes

### MultiplayerScreen.kt
- [x] Add `"game"` to gameInfo JsonObject in CreateLobbyDialog
- [x] Change CoopSaveEntry from `private` to `internal`
- [x] Change formatTimeAgo from `private` to `internal`
- [x] Change readCoopAutosaveHistory from `private` to `internal`
- [x] Change writeCoopRestoreSlot from `private` to `internal`

### LobbyScreen.kt
- [x] Add CoopSaveOffer composable
- [x] Integrate into LobbyScreen above Start Game button (host + coop only)
- [x] Auto-select best match by callsign overlap + newest timestamp
- [x] Write/delete coop_restore_slot.txt on selection change
- [x] Re-evaluate when lobby.players changes

### No C changes needed
- state_restore_all_sub already does callsign-based remapping
- multi_send_restore_game / multi_restore_game already exist
- coop_restore_slot.txt mechanism already works

## Build + lint
- [x] assembleDebug passes
- [x] run-code-quality.ps1 --fix passes
