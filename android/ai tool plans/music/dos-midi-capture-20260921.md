# DOS D1 MIDI capture investigation

Research only: capture the original GOG DOS executable's MIDI output and compare
Level 2 startup, controller state, and loop behavior with the HMP data. Do not
change game playback code or existing game installations.

1. Extract the complete GOG DOS runtime into `temp/dos-midi-capture-20260921`
2. Configure General MIDI and a private capture directory
3. Record startup and Level 2, including a loop; record a second transition if feasible
4. Decode captured MIDI and compare channel 5 notes, CC7/CC11, effects, and loop timing
5. Record provenance, results, limitations, and reproduction steps here

## Confirmed result: the early Level 2 bass is muted by DOS

The original executable emits the disputed notes. They are silent because the
driver sends channel volume zero before starting the song, and the HMP does not
raise that channel's volume until later. This is a concrete sequencing/state
difference, independent of the soundfont.

Capture: `temp/dos-midi-capture-20260921/captures/descentr_001.mid`

Channel numbers below are human-readable (channel 5 is MIDI index 4). Capture
times are milliseconds from the first recorded MIDI message, including title
and briefing music.

| Capture time | Channel 5 event | Meaning |
| --- | --- | --- |
| 59916 | CC123=0, CC121=0, bend bytes 64/64, CC7=0 | Reset at the briefing-to-Level-2 transition |
| 69065 | CC91=35, CC93=77 | Reverb/chorus sends from HMP tick 1096 |
| 74024 | Program 39 | Synth Bass 2, zero-based program numbering |
| 74474 | Note 41, velocity 127 | First disputed bass note, HMP tick 1745 |
| 81698 | CC7=125 | First nonzero song volume, HMP tick 2612 contains CC7=127 |
| 81699 | Program 39 | Repeated program selection at the same HMP tick |

All 20 positive-velocity bass notes before tick 2612 appear in the capture while
CC7 remains zero. No CC11 expression messages were observed on this channel.
The correct first-pass compatibility behavior is to preserve these notes and
reproduce the starting controller state, not remove the notes from the asset.

## Loop and conversion results

The first loop begins at capture time 283786 ms, about 223.845 seconds after
the first Level 2 event. The HMP global loop-end marker is tick 26863; its loop
start marker is tick 12. The file continues to tick 27067. DOS jumps at the
marker, before EOF, and emits a batch of program/controller settings consistent
with restoring saved loop state. HMI CC108-111 markers never reach MIDI output.

Channel 5 gets program 0 at the loop boundary, then the song selects program 39
again at 297769 ms. Crucially, DOS does not send CC7=0 or CC121 at this boundary:
its prior CC7=125 remains in effect. The same 20 intro notes recur before the
repeated CC7=125 at 305444 ms, this time at nonzero volume. Thus the original
first pass and subsequent passes intentionally or accidentally differ. A blanket
volume-zero initialization on every loop would not reproduce this capture.

All **16976 note-on/note-off messages** before the first loop boundary match the
source HMP exactly in per-channel order, pitch, and velocity. This includes
zero-velocity note-ons. Four source note-offs on channels 2 and 8 at tick 26863
are beyond that comparison: the DOS loop jumps before those events are emitted.
This accounts for their absence without assuming missing tracks or bad decoding.

Linear fits of corresponding first-pass notes give approximately
**8.33318 ms per HMP tick** with sub-millisecond residuals. Some tracks have a
one-tick relative offset, which still needs driver-level investigation. The
current converter uses 1605632 microseconds/quarter with division 192 for this
song, giving **8.3626667 ms/tick**, about 0.35% slower. The capture supports
investigating the legacy tempo formula rather than treating it as exact.

At music volume 8, every observed first-pass CC7 mapping fits
`output = max(source - 2, 0)`; in particular 127 becomes 125. This is an empirical
mapping for this setting, not a reverse-engineered general volume formula.
No SysEx was recorded. Effects CC91/93 are emitted intact.

At this transition DOS resets channels used by the preceding song; the capture
does not establish an unconditional reset of every possible channel. The bend
reset bytes are literally 64/64 (value 8256), not the usual center 0/64 (8192).
Later HMP events send the normal center. Preserve this as an observation rather
than silently normalizing the capture.

## Runtime and capture procedure

- Installer: local `game_data/gog installers/setup_descent_1.4a_(16596).exe`
- Extracted into `temp/dos-midi-capture-20260921/gog`, without installing over an existing game
- Original `DESCENTR.EXE`, HOG, PIG, and HMI drivers; bundled DOSBox 0.74
- Original GOG music config was Sound Blaster FM (`MidiDeviceID=0xa009`, port 388)
- Used original SETUP.EXE to select General MIDI (`MidiDeviceID=0xa001`, port 330)
- Music volume 8, DOSBox cycles 50000, windowed surface output
- New disposable pilot V; highest accessible level changed from 1 to 3 in its player file
- The pilot change is a fixture convenience, not a game executable or music edit
- Started a fresh DOSBox with `capture.conf`, which pauses before `descentr.exe`
- Armed Ctrl+Alt+F8 before releasing the pause, then selected New Game, Level 2, Rookie
- Skipped briefing, entered Level 2, then opened automap to keep the ship safe
- Original DOS music continued in automap; MIDI timestamps and notes kept advancing
- Stop capture explicitly with Ctrl+Alt+F8 to flush the buffer and finalize the SMF

The first file, `descentr_000.mid`, is a separate startup/menu calibration capture.
The second file is the research capture. `pilot-original.plr` preserves the
new pilot before unlocking level selection, and `capture-run-start.cfg` records
the configuration at the start of the research run.

## What the capture measures

DOSBox assembles MIDI messages at its emulated MIDI output and writes an SMF
format 0 file, division 500, with millisecond delta times under the default
500000 microseconds/quarter tempo. It records events, not rendered audio.
Timing starts with the first message, so the initial pre-message silence is lost.
Running status is expanded. This version forwards realtime bytes >= 0xF8 without
recording them; this is not a complete byte-level UART trace. SysEx is captured,
subject to DOSBox's fixed buffer, but none was observed in this run.

The corresponding bundled source was extracted to
`temp/dos-midi-capture-20260921/dosbox-source/dosbox-0.74`:

- `src/gui/midi.cpp`, `MIDI_RawOutByte`: message assembly and capture point
- `src/hardware/hardware.cpp`, `CAPTURE_AddMidi`: PIC_Ticks timestamping
- `src/hardware/hardware.cpp`, `CAPTURE_MidiEvent`: header and finalization

This is a reference for this retail DOS build's General MIDI path. It does not
establish an AdLib/OPL reference sound, or a universal result for every HMI driver
version and hardware synthesizer.

## Android path relevant to the difference

Gameplay uses `songs.c` -> shared `digi_tsf_music.c` -> engine `hmp2mid()` ->
TinyMidiLoader -> TinySoundFont -> stereo PCM -> SDL_mixer. Launcher preview uses
the separate shared memory converter in `hmp_android_shared.c` and OpenSL ES.

TinySoundFont is pinned to commit `853a0a171759f1ddba0de1442133a75912bbeffa`.
The bundled bank is TimGM6mb. `digi_tsf_music.c` resets TSF for a new song and
again when restarting the MIDI list at EOF. TSF initializes channel volume and
expression to 16383 (full 14-bit scale). It does not receive the original HMI
driver's song-transition volume-zero messages from a plain HMP conversion.

The game dispatcher forwards notes, program changes, controls, and pitch bend,
but not pressure. TSF does not implement CC91/93 effect sends or CC1 modulation.
The captured channel 5 effect sends are therefore another real fidelity gap,
separate from the now-confirmed silent-intro issue.

## Analysis artifacts

Scratch Python scripts use only the standard library:

- `temp/dos-midi-capture-20260921/analyze.py`: decode HOG/HMP and DOSBox SMF
- `temp/dos-midi-capture-20260921/compare.py`: compare per-channel note sequences and timing
- `temp/dos-midi-capture-20260921/comparison.json`: derived evidence
- Matching `.json` files beside the MIDI and source HMP event dumps
- `temp/dos-midi-capture-20260921/manifest.json`: sizes and SHA-256 of the runtime, config, and captures

Re-run the two scripts after capture finalization. File buffers can leave an
unfinished capture with a zero track length and only a partial event stream;
the scratch reader supports that for live inspection. Final verification must
check the closed file's declared length and end-of-track event.

Final capture verified: 129444 bytes, 32425 parsed events including EOT,
339.660 seconds from the first message. SHA-256:
`c1b39708d3d52815391fbf89ab3730814dc5080038805378d2e2a330252789fe`.

## Next research steps

1. Model HMI transition initialization separately from loop-state restoration
2. Compare the captured stream and converted HMP through the same synth/bank
3. Implement a scratch HMI marker interpreter and compare multiple songs,
   especially nested/local loops, before changing the production converter
4. Check other music-volume settings to recover HMI's scaling rule
5. Repeat after a different preceding level to test additional state carryover
6. Then assess a synth supporting modulation, pressure, reverb, and chorus;
   switching soundfonts alone cannot correct the sequencing differences

No alternate-preceding-level run or rendered-audio comparison was performed in
this session. The captured startup/title/briefing/Level-2 sequence is the tested
path. A second full song loop was not recorded; the capture includes the first
loop and roughly 56 seconds of its repeat.

## Status

- Completed extraction, GM setup, and startup-to-Level-2 recording
- Confirmed initial volume-zero behavior and identical early bass note data
- Completed loop capture and validated SMF length, EOT, and all first-pass note streams
- No playback code changed
