# SC-55 fidelity investigation

Research scope: inspect the user's game01 recording and the current SF2 path;
identify a realistic next comparison within the MIT/BSD preferred, LGPL allowed,
GPL prohibited constraint. No production renderer changes in this investigation.

1. Locate the recording, inspect its timing and compare against a current host
   rendering with nitro-shoe v1.34, without claiming subjective listening
2. Check renderer limitations separately from soundfont sample accuracy
3. Evaluate LGPL libEmuSC and FluidSynth as possible experiments, retaining the
   existing HMP conversion and scheduling where appropriate

The supplied old path now resolves under
`game_data/music/D1 MIDI mp3 sc55/game01.ogg`. Direct audio listening is unavailable
in this session; waveform statistics and listening artifacts are not a listening
verdict. Recording hardware, module revision and settings are not authenticated
by its directory name.

## Findings

- Reference: stereo Vorbis, 44,100 Hz, 200.2605 seconds, SHA-256
  `6727d6c707bdeea8630788a02d44d15fae16aff9a6c593a216708f5357731a88`
- Rebuilt `test_music_synth` and rendered 60 seconds of the local D1 game01 HMP
  through the production synth with nitro-shoe v1.34. The old diagnostic binary
  initially predated the empty-loop loader fix; rebuilding resolved that rejection
- The first 60 seconds have side/mid energy ratios of -5.70 dB (reference) and
  -8.90 dB (current), a measurable stereo difference. This cannot identify its
  cause: effects, source samples, pan laws and recording processing all contribute
- RMS-envelope correlation peaks at 0.677 with the reference delayed 60 ms relative
  to the render. This does not establish MIDI-arrangement identity or timbral accuracy
- The first GM HMP pass lasts 199.4833 seconds. Converted events contain volume,
  pan, modulation-off, portamento-off and sustain-off controls, no CC91/93 and no
  SysEx. Therefore effects defaults need testing; this song is not simply losing
  explicit reverb/chorus commands
- Instrument slots: Synth Bass 1, Sweep Pad, Square Wave, Synth Strings 2,
  Melodic Tom, Synth Drum, Saw Wave and TR-808 kit. The prior coverage audit found
  no missing notes or preset substitutions in this song
- Pinned TinySoundFont explicitly omits SF2 modulators and chorus/reverb sends
  (`tsf.h` lines 17-20, 570-571, 848-857), and uses linear sample interpolation
  (line 1294). `music_synth.cpp` renders its SF2 output without an effects stage
- Nitro-shoe describes its bank as an approximation, mostly using Microsoft GS
  Wavetable samples. Correct instrument numbers cannot make those samples and
  envelopes identical to the hardware

Baseline listening page: `temp/sc55-fidelity/listen.html`. Copies retain timing,
use the first 60 seconds, and are approximately RMS-matched with a peak cap.
No EQ/effects were added. `analysis.json` contains hashes, measurements, MIDI
controls and instrument mappings. No subjective preference was inferred.

## Proposed experiments

1. Render the identical MIDI and bank with FluidSynth, first effects off and then
   effects on, holding timing, bank selection and output level constant. Its LGPL
   license fits the accepted constraint. This separates our renderer's omissions
   from soundfont limitations; it will not reproduce the SC-55's precise DSP
2. Prototype libEmuSC alone (LGPL-2.1-or-later), excluding its GPL desktop frontend.
   Its C++17/CMake library accepts MIDI/SysEx and emits stereo float frames, so it
   can sit behind `music_synth` while retaining HMP conversion and scheduling.
   User-supplied original control/PCM ROMs are required; the emulator's code license
   does not supply permission to bundle those. No ROMs were fetched in this work
3. Verify library/dependency licenses at a pinned commit, host-render game01/07/08,
   then test Android ARM CPU cost, underruns, pause/seek/reset/loop behavior. The
   upstream README still acknowledges audible shortcomings, so an integration
   commitment should follow comparison, not precede it
4. Use instrument-isolated renders to investigate bass, lead, pad and drums if a
   mismatch remains. Establish reference arrangement, module revision and effects
   settings where possible before tuning. Avoid track-specific compensating EQ

For this exact recorded performance, the existing external OGG playback path is
the direct reproduction option. Live synthesis should remain available for other
songs and missions.

## Sources checked 2026-09-23

- [Nitro-shoe bank description](https://github.com/nitro-shoe/sc-55-soundfont)
- [FluidSynth license](https://github.com/FluidSynth/fluidsynth/blob/master/LICENSE)
  and [synth settings](https://www.fluidsynth.org/api/settings_synth.html)
- [EmuSC project and license split](https://github.com/skjelten/emusc)
- [libEmuSC dependencies and accuracy caveat](https://github.com/skjelten/emusc/blob/master/libemusc/README.md)
- [libEmuSC API](https://github.com/skjelten/emusc/blob/master/libemusc/src/synth.h)
- [Nuked-SC55 current raw license](https://raw.githubusercontent.com/nukeykt/Nuked-SC55/master/LICENSE)
  is GPLv2; excluded under the user's instruction. Cached GitHub HTML still showed
  the older noncommercial MAME terms, so use the current raw file for this finding
