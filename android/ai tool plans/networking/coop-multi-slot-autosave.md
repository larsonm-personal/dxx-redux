# Multi-Slot Coop Autosave + Lobby Save Picker

## Goal
Replace the single-slot coop autosave (slot 9) with 5 rotating slots (5-9).
Present matching saves in the lobby dialog so the host can pick which to restore.

## Design

### C-side (coop_save.c / coop_save.h)
- Rotating autosave: slots 5-9, counter increments each save
- `coop_autosave_history.json`: JSON array of up to 5 entries with slot, mission,
  level, timestamp, callsigns[], client_ids[]
- `coop_restore_slot.txt`: written by Kotlin lobby with a slot number (e.g. "7").
  Read and deleted by `coop_arm_auto_restore()` at game start.
  If absent, scan slots 5-9 for any viable save (backward compat).
- `coop_try_auto_restore()` uses dynamic slot instead of hardcoded COOP_AUTOSAVE_SLOT

### Kotlin-side (MultiplayerScreen.kt)
- `readCoopAutosaveHistory()`: reads history JSON, returns list of save entries
- Lobby dialog: filters saves by mission + current player's client_id
- Shows selectable list of matching saves
- On create, writes selected slot to `coop_restore_slot.txt` (or deletes if none)

### Shared constants
- COOP_AUTOSAVE_SLOT_FIRST = 5 (C only, Kotlin reads slot from JSON)
- COOP_AUTOSAVE_SLOT_COUNT = 5 (C only)

## Phases

### Phase 1: C-side autosave rotation [DONE]
- [x] coop_save.h: COOP_AUTOSAVE_SLOT_FIRST, COOP_AUTOSAVE_SLOT_COUNT
- [x] coop_autosave(): rotating slot, history JSON write
- [x] coop_write_autosave_history(): JSON with player callsigns + client_ids
- [x] coop_append_other_slots(): preserves entries for other slots

### Phase 2: C-side restore update [DONE]
- [x] static coop_auto_restore_slot variable
- [x] coop_arm_auto_restore(): read coop_restore_slot.txt, delete after read
- [x] coop_arm_auto_restore(): fallback scan of slots 5-9
- [x] coop_try_auto_restore(): use coop_auto_restore_slot

### Phase 3: D1 duplication [DONE]
- [x] d1/main/coop_save.h: add slot constants
- [x] d1/main/coop_save.c: mirror all d2 changes

### Phase 4: Kotlin lobby [DONE]
- [x] readCoopAutosaveHistory(): parse history JSON, filter by mission + client_id
- [x] CreateLobbyDialog: show save picker when saves match
- [x] Write coop_restore_slot.txt on create (or delete if no save selected)
- [x] Clear file on dismiss too

### Phase 5: Build + Lint [DONE]
- [x] gradlew assembleDebug
- [x] run-code-quality.ps1 --fix
- [x] Rebuild after format fixes
