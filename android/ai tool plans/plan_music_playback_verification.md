# Plan: Music Playback Verification + Bug Fixes

## Goal
Add music playback verification to the existing GOG installer test, then fix bugs causing:
1. "removed stale audio sources" incorrectly removing GOG audio source
2. MIDI tab showing "no midi tracks found" after GOG import
3. CD audio tracks visible but not playing

## Phase 1: Add setup commands for music preview [done]
Add SETUP_COMMAND handlers in SetupActivity.kt:
- `music_midi_play` -- enumerate MIDI tracks, play track N from source M
  - params: --ei source 0 --ei track 2 (0-based)
  - calls MidiEnumerationBridge.enumerateTracks, then MidiPreviewBridge.start
- `music_midi_stop` -- stop MIDI preview
- `music_cd_play` -- play CD audio track N from source M
  - params: --ei source 0 --ei track 2 (0-based audio track index)
  - calls CdPreviewBridge.start with resolved paths
- `music_cd_stop` -- stop CD audio preview

## Phase 2: Add music preview state to setup introspection [done]
In writeIntrospectJson(), add a "music_preview" object:
```json
"music_preview": {
  "midi": {"state": "playing|paused|stopped", "position_ms": 1234, "duration_ms": 5678},
  "cd": {"state": "playing|paused|stopped", "position_ms": 1234, "duration_ms": 5678},
  "midi_sources": [{"id": "d2-builtin", "label": "Descent 2 (built-in)", "track_count": 32}]
}
```

## Phase 3: Add music test steps to test_gog_installer_redbook.ps1 [done]
After the GOG import + setup introspection verification (step 5), add:
1. Send music_midi_play command (source 0, track 2 = track index 3)
2. Wait 3 seconds
3. Request setup introspection
4. Assert midi.state == playing, midi.position_ms > 1000
5. Send music_midi_stop
6. Send music_cd_play command (source 0, track 2 = audio track 3)
7. Wait 3 seconds
8. Request setup introspection
9. Assert cd.state == playing, cd.position_ms > 1000
10. Send music_cd_stop

## Phase 4: Build and run test [done]
Expect failures due to bugs. Document the failures.

## Phase 5: Fix stale audio source removal bug [done]
Root cause: `findGogPair()` returns lowercase "descent_ii" but actual files from
GOG InnoSetup extraction are uppercase "DESCENT_II.GOG". On Android's case-sensitive
filesystem, `File(filesDir, "default/descent_ii.gog").exists()` returns false.

Fix: Change `findGogPair()` to return the ACTUAL filename from dir listing,
preserving original case. Also make `pruneMissingSources()` do case-insensitive
file existence checks.

## Phase 6: Fix MIDI enumeration no tracks bug [done]
Root cause: `midi_enumerate_tracks()` looks for HOG files directly in `filesDir`
(app root), but after GOG import they're in `filesDir/default/` (the active set dir).
Also uses hardcoded lowercase "descent2.hog" but actual file is "DESCENT2.HOG".

Fix: Pass the active set directory path to `midi_enumerate_tracks()`. Also use
case-insensitive file matching (scan directory listing) for HOG file detection.

## Phase 7: Rebuild, re-run test, lint [done]
Final test run: all checks passed (MIDI playing at 5717ms, CD audio at 4053ms, in-game automation PASS)
