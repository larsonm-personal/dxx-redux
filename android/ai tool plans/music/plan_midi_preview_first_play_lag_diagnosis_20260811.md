# MIDI preview first-play lag diagnosis

## Goal

Determine why starting an individual MIDI/HMP track from the metadata browser blocks pause and seek input for roughly five seconds on a flagship Android phone.

## Scope

- Trace the Compose/JNI/native call path used by mission metadata MIDI previews.
- Identify synchronous work performed on first playback and distinguish UI-thread work from audio-thread work.
- Use existing tests, local fixtures, and focused instrumentation or profiling where practical.
- Diagnose and report; do not implement a behavior change unless separately requested.

## Work plan

- [completed] Map the preview UI, JNI bridge, and native synthesizer startup path.
- [completed] Inspect first-play-only initialization, parsing, conversion, allocation, and rendering costs.
- [completed] Reproduce or measure the suspected blocking work with the available fixture/test paths.
- [completed] Summarize the root cause, confidence, and likely fix directions.

## Findings

- The slider release calls `MidiPreviewBridge.seek` synchronously from the Compose main thread.
- Native seek resets TinySoundFont and reconstructs its state by synthesizing every audio frame from time zero to the selected target. Work therefore grows linearly with the seek position.
- On `emulator-5554`, seeking the real built-in `briefing.hmp` to roughly three-quarters of its 94.6-second duration delayed a main-thread setup-introspection broadcast by 2.716 seconds. This directly reproduces the inability to pause or otherwise interact while seek reconstruction runs; a roughly five-second delay on another debug/device build is plausible.
- Playback startup creates secondary jank. The render thread aggressively fills a 262,144-sample stereo ring, about 2.73 seconds at 48 kHz, while holding `s_playback_mutex` across each 2,048-frame TinySoundFont render. Compose synchronously calls `getState` from the main thread, so its 100 ms poll and button recomposition contend with the producer. Emulator frame statistics during the first six seconds reported 18 of 39 frames janky, a 34 ms median, a 48 ms 90th percentile, and all 39 frames flagged for high input latency.
- Metadata-track byte extraction, HMP conversion, MIDI parsing, duration calculation, OpenSL creation, and first SoundFont load occur before playback becomes controllable. Most start work is dispatched to `Dispatchers.IO`; the metadata preview still calls SoundFont initialization synchronously from `DisposableEffect`, but the emulator logged SoundFont load in about 50 ms and MIDI parse/start in about 10 ms. These are not the measured multi-second freeze.
- `compute_midi_duration_ms` parses the converted MIDI a second time. This is redundant startup work but not the dominant observed cost.
- Obsidian's `briefing.hmp` song-list entry is a base-data reference and is not contained in `Obsidian.zip`; the current scoped Obsidian list intentionally leaves it non-previewable. The same built-in `briefing.hmp` is available in the general MIDI track viewer and exercises the identical preview implementation.

## Likely fix direction

- Move seek reconstruction completely off the main thread and make it cancellable or generation-based so a later seek, stop, close, or pause can supersede it.
- Prefer render-thread ownership of the TinySoundFont state with queued seek/control commands and a synchronized published state snapshot. This avoids both UI-thread native waits and concurrent synth ownership.
- Bound producer lock hold time and avoid synchronous JNI state reads during Compose rendering. Publish atomic/snapshot position and state instead.
- Optionally reduce initial prefill and cache duration during the first parse. Those are smaller improvements and do not replace asynchronous seek.
