# Music info stale selected track diagnosis

## Goal

Determine and fix why the in-game music info menu reports the previously playing mission ZIP MIDI after choosing a new track in the track selector.

## Plan

- [x] Trace the track selector, playback transition, and music info data sources in D1 and D2
- [x] Identify the repeatable stale-state sequence and confirm the responsible state update or ordering
- [x] Keep pending state asserted until the game thread publishes the applied command snapshot
- [x] Refresh the selector panel and permanent label after queued commands complete
- [x] Add focused regressions and run scoped quality, relevant tests, and paired build verification

## Scope

Keep changes in the shared Android music bridge and Kotlin overlay code so D1 and D2 receive the same behavior.

## Result

- Track selection and next/previous JNI calls enqueue commands; playback and snapshot publication happen later on the D1/D2 game thread
- Both the music panel and permanent touch-overlay track label reread the native snapshot immediately after enqueueing, so they read the prior track
- The panel has no delayed refresh for ordinary track commands, although source changes already wait for the pending queue to drain
- The permanent label's periodic change detector reads the Redbook-only current-track snapshot, so mission/built-in MIDI changes do not trigger a corrective reread
- The playback path itself updates `Song_playing` and publishes the newly selected mission track correctly after the queued command is consumed

## Implementation

- Keep the command pending signal active from dequeue through snapshot publication
- Publish the unified current track instead of the Redbook-only track in the polling snapshot
- Route successful panel, previous-track, and next-track commands through one Activity-owned delayed refresh
- Refresh both the open panel and permanent track label after the command becomes fully applied

## Validation

- `python android/tests/test_android_audio_lifecycle.py`: 8 tests passed
- Scoped code quality passed for all five changed implementation, test, and plan files
- `:app:compileDebugKotlin :app:externalNativeBuildDebug`: passed for arm64-v8a, armeabi-v7a, and x86_64
- `:app:testDebugUnitTest`: passed
- `:app:assembleDebug`: passed
- `test_music_track_controls_unified.json5`: passed on the emulator for D2 and D1
