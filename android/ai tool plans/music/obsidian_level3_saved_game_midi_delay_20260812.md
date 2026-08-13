# Obsidian level 3 saved-game MIDI delay investigation

## Scope

Diagnose the reported 2-3 second delay before MIDI playback begins after loading an Obsidian level 3 save. Do not change runtime code during this investigation.

## Plan

- [x] Extract relevant timestamped audio, MIDI, mission, level, and save-load events from the supplied debug log
- [x] Correlate the observed delay with the corresponding music and save-load code paths
- [x] Report the likely cause, supporting evidence, and the smallest useful next diagnostic or fix

## Findings

- The log confirms Obsidian level 3 is `argntitr.rl2` and repeated startup-save restores complete in about 0.56 seconds
- Each level load spends about 68 ms in the measured music setup phase, so the reported delay occurs after successful playback setup
- Obsidian level 3 resolves to `game03.hmp`
- Direct inspection of `game03.hmp` found its earliest audible Note On at tick 360; its timing value is 120 ticks per second, producing exactly 3.000 seconds of authored leading silence
- No runtime code change is warranted for the save-load path. If the silence is unwanted, the narrow fix is to trim the leading silence from this mission track rather than alter global MIDI timing
