# MIDI preview fast approximate seek

## Goal

Make launcher MIDI/HMP preview seeking fast and responsive without pre-rendering or retaining whole-track PCM.

## Design

- Queue seek requests to the existing MIDI render thread instead of reconstructing on the Compose/UI thread.
- Reconstruct channel programs, controllers, pitch bend, sustain, and currently sounding notes by scanning MIDI events without synthesizing intervening PCM.
- Accept that notes sounding across the seek point restart their envelopes at that point.
- Publish playback state through nonblocking atomic snapshots so UI polling cannot wait behind synthesis.
- Make pause/resume use atomic control flags so startup ring-buffer rendering cannot block the UI.
- Keep track bytes and parsed MIDI only for the preview dialog lifetime, as today.

## Work plan

- [complete] Add render-thread approximate seek and nonblocking state publication.
- [complete] Update synchronization and behavioral regression coverage.
- [complete] Run scoped quality and native/unit validation.
- [complete] Build the debug APK for arm64-v8a, armeabi-v7a, and x86_64.
- [blocked] Run focused `briefing.hmp` timing on the emulator. An unrelated full-suite process owns the shared device, so installing or driving this APK would invalidate that run.
