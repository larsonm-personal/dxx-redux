# Offline FM feasibility experiments

This standalone test project does not change gameplay, preview, Gradle, or the
shipping dependency graph. It builds pinned BSD-3-Clause ymfmidi/ymfm and MIT
emu8950. No GPL library is used. Original instrument data comes from the supplied
game HOG; bundled third-party patch collections are not loaded or linked.

## Reproduce on Windows

Run from the repository root with Visual Studio 2022 C++ tools, Python 3 and Git:

```powershell
.\android\tests\fm_feasibility\run.ps1 `
  -Hog 'temp/dos-midi-capture-20260921/gog/DESCENT.HOG' `
  -AdlibWav 'game_data/music/dos-references/descent14-game07/adlib/dos.wav'
```

The local paths above are the original GOG installation/capture used in this
session. Substitute another original D1 HOG when needed. A DOS WAV is optional.
For both Android cross-builds and an optional run on an already running device:

```powershell
.\android\tests\fm_feasibility\run.ps1 `
  -Hog 'temp/dos-midi-capture-20260921/gog/DESCENT.HOG' `
  -AdlibWav 'game_data/music/dos-references/descent14-game07/adlib/dos.wav' `
  -AndroidNdk 'C:/local/android-ndk-r30' -AdbSerial emulator-5554
```

Android execution uses `/data/local/tmp/dxx-fm-feasibility` and does not install an
APK or modify game settings. The native executable uses static libc++ to avoid
depending on the application's shared libraries. Physical ARM64 devices can use
the same command with their serial; the results below are from an x86_64 emulator.

`-Song game02.hmp -Seconds 5` selects another song and its bank names through
`descent.sng`. Work products default to `temp/fm-feasibility/`:

- `deps/`: exact upstream checkouts, license texts, and no local source edits
- `game07-hmp/listen.html`: human listening comparison and expandable raw clips
- `game07-hmp/report.json`: hashes, arrangement, patches, timing, and measurements
- `game07-hmp-android/`: equivalent Android outputs
- `host-vs/`, `android-arm64-v8a/`, `android-x86_64/`: native build products

The listening copies approximately align DOS audio using RMS-envelope correlation
and adjust gain toward -20 dBFS RMS, capped below clipping. Raw files retain their
original levels. Correlation is a rhythm/alignment diagnostic, not a timbre score.
No subjective listening judgment is inferred from these measurements.

## Portable pieces

The Python preparation/analysis and CMake project also work independently of the
Windows wrapper. Linux/macOS builds have not been run in this experiment:

```text
python android/tests/fm_feasibility/experiment.py --work temp/fm-feasibility --fetch-only
cmake -S android/tests/fm_feasibility -B temp/fm-feasibility/host -DCMAKE_BUILD_TYPE=Release -DFM_DEPS=<absolute-path-to-temp/fm-feasibility/deps>
cmake --build temp/fm-feasibility/host
python android/tests/fm_feasibility/experiment.py --hog <DESCENT.HOG> --probe temp/fm-feasibility/host/fm_probe --probe-fixed temp/fm-feasibility/host/fm_probe_live_fix
python android/tests/fm_feasibility/verify.py temp/fm-feasibility/game07-hmp/report.json
```

Dependency revisions are checked against `dependencies.json`. Preparation rejects
modified tracked files; CMake also checks revisions. Only the required source
files are compiled, excluding upstream applications, SDL integration and banks.
The separate `fm_probe_live_fix` target generates a modified BSD `player.cpp` in
the build directory with `patch_live.py`, leaving the baseline checkout intact.
Both compiled variants also correct an upstream debug-only `size_t` printf format
for Android LP64; this does not change audio behavior.

## Experiments and findings

Validated on 2026-09-21:

| Experiment | Result |
| --- | --- |
| D1 Level 7, original melodic/drum banks, FM tracks 0,1,2,3,4,5,8 | 20-second render succeeds |
| ymfmidi nine-voice and eighteen-voice variants | Both render; no PCM rail samples in Level 7 |
| Direct MIDI API on unmodified ymfmidi | Fails note-off/retrigger behavior |
| Direct MIDI API with isolated bookkeeping fix | Byte-identical to file playback for Level 7 |
| Native ymfm YM3812 and MIT emu8950 YM3812 | Same nine-voice register stimulus renders on both |
| Windows x64 and Android x86_64 | Level 7 and both core-tone WAVs byte-identical across platforms |
| Android ARM64 | Cross-build/link passes; no physical-phone benchmark yet |
| D1 Levels 2, 3, 8 | Five-second renders cover the other three bank families; fixed live/file PCM matches |

In a representative optimized run, 20 seconds of Level 7 took approximately
0.20 seconds on the Windows host and 0.14 seconds in the x86_64 Android emulator.
Both chip-only tone tests were over 100x real time in that run. These are single
offline render timings, excluding WAV writes, not battery, callback-latency or
physical-phone performance measurements. See generated reports for each run.

The baseline live API bug is concrete: its `justChanged` flags and voice ages only
advance inside the file sequencer. With external events, both note-off encodings
are ignored in the probe, and a repeated note is suppressed. A clip with note-offs
is byte-identical to one with no note-offs. The isolated change clears those flags
and ages voices after a nonempty externally scheduled render batch. Regression
checks retain the broken baseline as a negative control. Aging by event/render
batch follows the existing driver's approach; it is not a reproduction of HMI
voice stealing and needs review before production use.

The reference comparison still does not establish DOS fidelity. The candidate's
20-second raw RMS is about -15.9 dBFS; the DOS capture's first 22 seconds, including
briefing lead-in, is about -19.3 dBFS. Those different windows/gains cannot identify
a drum-volume correction. An envelope search places the song start near 960 ms
in the reference (correlation about 0.62). Inverting the BNK connection bit reduces
that correlation to about 0.47; this supports, but does not prove, the initial
bank interpretation. Both candidates remain available for listening.

## Deliberate limits and next work

- This is a short opening-pass experiment, not a replacement HMP converter.
  The Python parser is independent of production and rejects clips crossing an
  HMI branch. It preserves raw CC7 and default MIDI pitch rather than applying
  corrections measured against the separate DOS General MIDI driver.
- ymfmidi's built-in HMP loader ignores device track selection. The experiment
  explicitly selects the FM arrangement before passing MIDI to the library.
- Its OPL2 mode actually uses nine voices of an OPL3 core. The separate native
  YM3812 test proves that true OPL2 cores are available, but those tone tests do
  not yet include the complete HMI instrument/voice driver.
- HMI's percussion pitch is taken from the name-record flag byte. The follow-up
  DOS register comparison confirms it for the notes in the tested Level 7 span;
  other banks and legacy rhythm-mode entries still need verification.
- Five D1 bank entries carry the legacy rhythm-mode flag (Ham drums 41-44 and
  Rick drum 93). They are outside the tested openings. The exporter marks them
  unavailable and rejects a clip that actually uses one; it does not silently
  approximate them. Whether HMI honors that flag still needs investigation.
- D2's `d2melod.bnk` and `d2drums.bnk` have `AMLIB-`/`ANLIB-` signatures. The
  strict reader rejects them pending verification of their semantics; D2
  feasibility beyond compiling the synths has not been established here.

## Original DOS register follow-up (2026-09-22)

Captured GOG D1 Level 7 using its HMI `0xa009` music device, SB16 emulation,
50,000 cycles, music volume 8 and muted effects. Preserved the DRO, WAV, config,
hashes and capture notes locally under
`game_data/music/dos-references/descent14-game07/opl/` (ignored game data).

The trace is 126.467 seconds and contains 53,475 register writes. It starts
partway through the song: its first matched note corresponds to HMP time
5,583.333 ms. The WAV begins approximately 35.34 seconds into that trace. These
artifacts are useful driver/chip references, **not a capture of the opening drums**.
The separate startup/menu trace remains in `temp/dos-opl-registers-game07/captures/`.

The game writes `0x105 = 1`, enabling OPL3, then pairs nine logical voices across
both register banks. All 2,202 pairs have the same pitch and instrument signature;
bank 0 routes right (`0x20`) and bank 1 left (`0x10`). 1,602 pairs have different
carrier levels, providing panning. The DRO header says dual OPL2; that label is
insufficient to determine the active hardware mode. The paired writes occur
within one millisecond of each other.

`compare_opl.py` matches 1,971 consecutive source notes, source indices 69-2039,
against the first 1,971 bank-0 notes. No gaps occur in that span. It compares the
ordered pitch/operator/connection signatures, excluding level registers, then
reports levels separately. KSL bits also agree throughout. This supports the
FM arrangement and BNK operator/percussion mapping, but **does not establish
volume, note-off or voice-allocation parity**.

Observed neutral-pitch FNUMs by chromatic pitch class are:

```text
C    C#   D    D#   E    F    F#   G    G#   A    A#   B
343  363  385  408  432  458  485  514  544  577  611  647
```

The observed block is `MIDI note / 12 - 1` (integer division). These frequencies
are slightly lower than the prototype's A440 table. The separate
`fm_probe_hmi_pitch` target adds the measured neutral-pitch behavior to the
live-bookkeeping variant. Isolated-note tests match all 34 observed pitch/block
combinations exactly; the original table matches zero. Pitch bend, out-of-range
notes and detuning retain the generic behavior and are **not HMI-verified**.

The longer MIDI test also found a remaining driver discrepancy: 2,040 source
note-ons produce only 1,953 rising key-on edges in both prototype variants.
The original DOS trace has an uninterrupted match across the available span.
Do not treat the earlier file/live equality result as proof of HMI retrigger or
voice-stealing behavior. This needs event/voice lifetime comparison before a
production renderer is selected.

Timing is another independent discrepancy. The offset between DOS and nominal
120 Hz source timing increases by about 649 ms over the 114.4-second matched
span, with variation rather than a single fixed offset. The comparison does
not stretch time or hide that drift. Whether it is HMI scheduling, emulated
driver overhead or another cause requires a controlled capture.

`registers-opl3` replays the original writes on native BSD ymfm YMF262 at 49,716
Hz. Its 20-second aligned RMS-envelope correlation with the same-session DOS
WAV is 0.962573. Listening copies need only about 0.07 dB difference in gain.
These are encouraging diagnostics, not a waveform/timbre equivalence score;
DRO timestamps are quantized to milliseconds and the two renderers use
different sample rates. No subjective listening judgment was automated.

Diagnostic `registers-ymfm` and `registers-emu8950` modes replay each bank on a
separate native OPL2 core, with bank 0 right and bank 1 left. This fixed routing
fits this capture; they are **not general OPL3 or DRO players**. Their raw output
levels differ substantially, so listening copies are gain-matched.

All three 127-second replay WAVs are byte-identical between Windows x64 and the
Android x86_64 emulator. Updated ARM64 builds pass without new warnings, but
were not executed on a physical phone. Native OPL3 replay took about 1.49 seconds
on this Windows run (85x real time), excluding file writes.

### Reproduce the follow-up

After building the standalone targets with `run.ps1` above:

```powershell
python android/tests/fm_feasibility/replay_capture.py `
  --capture game_data/music/dos-references/descent14-game07/opl/dos.dro `
  --wav game_data/music/dos-references/descent14-game07/opl/dos.wav `
  --probe temp/fm-feasibility/host-vs/Release/fm_probe.exe `
  --output temp/fm-feasibility/opl-game07

python android/tests/fm_feasibility/verify_opl.py `
  --hog temp/dos-midi-capture-20260921/gog/DESCENT.HOG `
  --capture game_data/music/dos-references/descent14-game07/opl/dos.dro `
  --probe-fixed temp/fm-feasibility/host-vs/Release/fm_probe_live_fix.exe `
  --probe-pitch temp/fm-feasibility/host-vs/Release/fm_probe_hmi_pitch.exe `
  --output temp/fm-feasibility/opl-game07
```

Open `temp/fm-feasibility/opl-game07/listen.html` for the DOS/native OPL3/dual
OPL2 listening comparison. `verify_opl.py` is specific to this hashed Level 7
fixture and checks malformed-capture rejection too. For another song/capture,
use `compare_opl.py --hog ... --song ... --capture ... --output ...`; it reports
matches without asserting this fixture's counts. Matching repeated motifs can
be ambiguous; inspect the timing bins and matched context.

For Android replay, substitute `--probe temp/fm-feasibility/android-x86_64/fm_probe`,
add `--adb C:/local/android-sdk/platform-tools/adb.exe --serial emulator-5554`, and
use a separate output such as `temp/fm-feasibility/opl-game07-android`.
Use the ARM64 executable with a physical ARM64 device. Only the replay path is
device-enabled in this follow-up; the Python regression driver runs host probes.

To gather more original register captures:

```powershell
.\android\helpers\capture_dos_midi.ps1 `
  -SourceDirectory temp/dos-opl-game07/game `
  -OutputDirectory temp/dos-opl-next `
  -Song game07.hmp -MusicDevice adlib -MuteEffects -OplCapture -Launch
```

The example source contains the local unlocked pilot; substitute your extracted
GOG runtime as needed. Arm **Ctrl+Alt+F7** at the initial pause, release keys, then
launch and select the level normally. Capture startup/menus too and separate
the song afterward; restarting capture at a briefing risks advancing it and
missing the opening. **Ctrl+F6** toggles WAV capture independently. Stop both
captures before exiting so headers are finalized. `-Song` extracts the HMP for
analysis; it does not select the level. Call the retention helper before using
a new scratch output, as the PowerShell capture/experiment wrappers do.

Next work: a controlled first-note capture, paired stereo/volume behavior, and
retrigger/voice lifetime comparison. Prefer BSD ymfm YMF262 for the core; keep
the MIDI/event interface but treat ymfmidi's driver policy as provisional.
There is still no demonstrated need for an LGPL dependency. No GPL library
has been introduced into the experiment or application; existing GOG DOSBox is
used externally to capture reference behavior.

## Sources

- [ymfmidi, BSD-3-Clause](https://github.com/devinacker/ymfmidi/tree/94b0ab10f901c2208ffe8c28f7373e56a782dde7)
- [ymfm, BSD-3-Clause](https://github.com/aaronsgiles/ymfm/tree/d641a80631fe1fa5a3689fa5b6def07685772424)
- [emu8950, MIT](https://github.com/digital-sound-antiques/emu8950/tree/c27078c654de8f9cf37e9f02f040cc251aba33d9)
- [Tero Totto's programming guide, BNK layout and OPL registers](https://franticware.github.io/miscellaneous/adlib-programming-guide.html)

The reader and WOPL writer were implemented for this experiment from format
documentation, local game data and the pinned BSD reader, without importing a
GPL converter or synth. Game data is kept local and is not checked into the repo.
