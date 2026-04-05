# Coop Autosave Multi-Slot + Lobby Save Picker

## Goal
Keep the last 5 coop autosaves (rotating slots 5-9) and present matching saves
in the CreateLobbyDialog based on player client_id matching.

## Design

### C-side (coop_save.c / coop_save.h)
- Rotate autosave slot 5..9 via a persistent counter in `coop_autosave_history.json`
- Each autosave entry stores: slot, mission, level, timestamp, num_players,
  and arrays of callsigns + client_ids
- `coop_autosave_history.json` replaces `coop_autosave_info.json` (keep reading
  old format for one migration cycle)
- `coop_arm_auto_restore()` reads `coop_restore_slot.txt` (written by Kotlin)
  to know which slot to restore from. Falls back to newest if file missing

### Kotlin-side (MultiplayerScreen.kt)
- `readCoopAutosaveHistory()` reads the history JSON
- Filter entries by mission + player client_id match
- Display matching saves as selectable list in CreateLobbyDialog
- On selection, write slot number to `coop_restore_slot.txt`
- When no selection, auto-restore is not armed

## Phases
- [ ] Phase 1: C-side multi-slot autosave + history JSON
- [ ] Phase 2: C-side restore-slot file reading
- [ ] Phase 3: Kotlin lobby save picker UI
- [ ] Phase 4: Duplicate d1 C changes
- [ ] Phase 5: Build, lint, test
