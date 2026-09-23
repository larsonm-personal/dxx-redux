# Game08 SF2 percussion balance

Current follow-up: finish the DOS GM capture/comparison and restore unattended
capture as explicitly requested after the desktop plugin stopped connecting.
Use a repository-owned Windows runner targeting only a newly launched private
DOSBox process. Require ownership and foreground checks before keyboard input,
release modifiers on failure, close only that process, and reject missing or
unfinalized MIDI. This is test tooling using the existing GOG executable; no
new synthesis dependency is added to Android. Validate two unattended runs and
retain a hash-verified game08 GM fixture with an explicit comparison window.

Identify the user's loud bell-like opening sound in the bundled-SF2 rendering.
Check note events, selected drum kit, soundfont zones and synth behavior before
changing production gain or mapping. Export isolated percussion and controlled
soundfont variants against the supplied SC-55 recording for listening review.

Initial evidence: no note 56 (cowbell) in the opening 20 seconds; note 67 uses
Agogo Bell in bank 128/program 24 (Electronic). The preset includes two identical
agogo regions with no region modulators. Check audible contribution and avoid
assuming all percussion or every soundfont needs the same correction.

The user confirmed two hits within roughly the first second, matching note 67
at 0.642 and 1.033 seconds. A separate melodic Agogo program enters at 8.392
seconds and should not be reduced along with the opening percussion.

The initial SF2-edit experiment exposed another compatibility issue: adding
60 to generator 48 produced only -0.600 dB, not -6 dB. The pinned TinySoundFont
uses 0.01 and a 14.4 dB ceiling. The written SoundFont specification describes
centibels, while established EMU/FluidSynth compatibility uses 0.04 dB per unit.
Do not distribute SF2 edits calibrated around TinySoundFont's 0.01 behavior or
change the global attenuation model as an unverified fix for this one sound.

Sources for follow-up:
- https://github.com/schellingb/TinySoundFont/blob/853a0a171759f1ddba0de1442133a75912bbeffa/tsf.h
- https://www.synthfont.com/sfspec24.pdf
- https://github.com/FluidSynth/fluidsynth/discussions/1708

Added an offline note filter and diagnostic per-note gain to midi_tsf_render,
plus `android/tests/game08_agogo_experiment.py`. The latter checks the exact
agogo regions, renders the same MIDI/font at unchanged mix gain, measures actual
-6/-12 dB reductions and checks the later melodic agogo remains identical.
It writes an isolated opening agogo, the later melodic part, SC-55 reference,
and full-mix trials to `temp/game08-percussion/listen.html` with a JSON report.
The host target builds without warnings and the experiment assertions pass.
Production playback and the bundled SF2 are unchanged. Listening feedback and
an independent synth comparison are needed before selecting a default balance
or changing the general SF2 attenuation implementation.

The user cannot choose a gain correction without a trusted HMP reference.
Capture game08 from DOS in General MIDI mode using the original HMP as the title
song, then compare ordered events, timing and note state with production output.
The existing DOS FM capture uses HMQ. The supplied SC-55 recording's arrangement
and playback provenance have not been established. A matching GM MIDI capture
would validate conversion, but would not establish an ideal soundfont or timbre.
Do not select a production attenuation from the subjective -6/-12 dB trials.

Completed the original DOS GM capture in `temp/game08-gm-reference`, with
effects muted and game08 substituted for the title. The desktop plugin still
reports missing native pipe even after restarting VS Code. The user explicitly
requested an unattended alternative. Added `run_dos_midi_capture.py`, with
`capture_dos_midi.ps1 -Unattended -Seconds 60` as the preparation/run entry point.
The initial posted-message attempt did not reach SDL; the working runner uses
foreground-checked input and excludes DOSBox's separate status console.

The retained reference is `game_data/music/dos-references/descent14-game08/dos.mid`
with fixture `android/tests/fixtures/dos-midi/descent14-game08.json`. Its first
event is the HMP's pitch reset at 0 ms and its first notes occur at 125 ms.
All 3,714 channel messages match production in a 50.010-second window, including
all note states, within 1 ms. No alignment, tempo stretch or preceding-song
state is needed. The cutoff includes a note-off at DOS 49,999 / TML 50,000 ms,
avoiding an artificial boundary mismatch. The legacy converter fails as the
negative control. This validates the opening, not the full 173.9-second loop.

A second independent 40-second unattended run in `temp/game08-gm-unattended-check`
matches the retained DOS reference for the first 30.010 seconds. Both recordings
were finalized and their DOSBox processes exited without user input. Level 2,
7 and 8 MIDI parity checks and the three native MIDI tests pass together.

`render_game08_gm_reference.py` reproduces the parity check and renders both
streams through the same SF2/TinySoundFont at fixed gain. The listening page is
`temp/game08-gm-reference/comparison/listen.html`. Opening agogo note states all
match, including the first hits at 642/1033 ms with velocities 115/99, kit 24,
CC7=125 and expression=127. Production minus DOS agogo RMS is -0.0008 dB; full
mix RMS differs by 0.0012 dB. Both full mixes contain two clipped samples at the
existing renderer gain. This rules out a MIDI conversion cause in this window,
not a synth/soundfont imbalance. No production gain or instrument mapping changed.

Reproduce (numpy 2.2.6 and soundfile 0.13.1 required):

```powershell
python android/tests/game08_agogo_experiment.py --hog PATH/TO/DESCENT.HOG --reference "game_data/music/D1 MIDI mp3 sc55/game08.ogg" --output temp/game08-percussion
```
