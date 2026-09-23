# Game08 opening investigation

Compare the user's SC-55 recording under `game_data/music/D1 MIDI mp3 sc55/`
with the current production FM and SF2 playback, including the first lead entry.

1. Audit HMP device tracks, program changes, bank selection and original BNK
   records for the opening. Check converted events against independent parsing
2. Export production audio and a local reference comparison, retaining source
   provenance and instrument/event evidence
3. Where practical, capture original DOS FM output to distinguish original
   song-specific patch choices from errors in the new driver
4. Fix only demonstrated implementation errors; otherwise document the precise
   profile difference and provide a useful listening comparison

## Confirmed cause

Both devices select the same tracks *inside game08.hmp*, but DOS selects a
different file for FM: **game08.hmq**. The original SNG selects rickmelo/rickdrum,
whose programs belong to that HMQ arrangement. Combining HMP with those banks
made its opening bass parts play WUMP.ins and DSnare.i, and several other parts
play nosound. HMQ instead starts with program 76 Bass.ins and 81 Slapbass.

A fresh GOG DOSBox 0.74 capture of original DESCENTR.EXE confirms all 666
captured bank-0 note-ons, consecutively from the first note, against HMQ pitch
and operator signatures. HMP only matches 24 scattered notes. The capture uses
an isolated runtime and a loose DESCENT.SNG replacing only the title row with
game08's original row, so the song starts without navigating to Level 8.
The original HOG, sequence bytes, banks and DOS driver remain unchanged.

This is stronger evidence than comparing MIDI against a parser making the same
HMP-only assumption. FM pitch, patch selection and note order are verified here;
this does not establish exact volume, stereo, envelopes or voice allocation.

## Change

- The shared resolver loads an optional matching HMQ when FM is selected
- Preview's HOG reader and gameplay's mounted reader both supply that asset,
  bounded by the existing 16 MiB MIDI limit; SNG/BNK retain their 64 KiB limit
- Conversion uses HMQ before FM track filtering. Malformed/unsupported HMQ
  falls back to the original GM HMP, not HMQ played with an SF2
- Explicit HMQ previews resolve banks through the corresponding HMP SNG row
- Native logs include the actual sequence filename
- Title/Levels 1/2 now use their HMQs and FM, correcting the earlier apparent
  hardware-rhythm limitation caused by feeding HMP programs into HMQ banks

## Reproduction and evidence

Local evidence: `temp/game08-investigation/`. The original source is
`temp/dos-midi-capture-20260921/gog/`. Capture another song with:

```powershell
./android/helpers/capture_dos_midi.ps1 -SourceDirectory PATH/TO/GOG -OutputDirectory temp/new-capture -Song game08.hmp -SongAsTitle -MusicDevice adlib -MuteEffects -OplCapture -Launch
```

At the pause arm Ctrl+Alt+F7 (OPL) and Ctrl+F6 (WAV), press Space, let the title
play, then stop both captures with their shortcuts before exiting DOSBox.
The helper records the title substitution and hashes the staged SNG.

```powershell
python android/tests/fm_feasibility/compare_opl.py --hog PATH/TO/DESCENT.HOG --song game08.hmq --capture PATH/TO/CAPTURE.dro --output temp/opl-report --seconds 40
python android/tests/test_fm_playback.py --renderer android/build/host-extract-tests/Release/test_music_synth.exe --hog PATH/TO/DESCENT.HOG --output temp/host-report
```

`render_game08_comparison.py` accepts `--renderer`, `--hog`, `--reference`
(the user's OGG), `--dos-wav`, optional `--before-fm`, and `--output`. It requires
numpy 2.2.6 and soundfile 0.13.1 for offline analysis, with no new app dependencies.
The local listening page is `temp/game08-investigation/listening/listen.html`.
It includes SC-55, DOS FM, pre-fix FM, corrected FM and SF2. Copies are level
matched; DOS startup silence is trimmed, not precisely time-aligned.

Host validation passes: eight songs match the independently parsed selected
HMP/HMQ events, deterministic FM reset, Level 7 full repeat, explicit HMQ versus
automatic selection, and malformed-HMQ fallback with sample-identical GM PCM.
The four native suites and 15 preview synchronization checks pass. Normal APK
build succeeds for ARM64/ARMv7/x86_64. Device validation is recorded separately
in the local report.

Installed APK validation passes on emulator-5554: HMQ previews for game08/title/
Level 1; renderer persistence/switching; Level 7 seek/pause/resume; D1 gameplay
41/41 steps including game08 with ymfm; D2 soundfont fallback 48/48 steps.
D1 game08 reports nonzero PCM and zero clipped samples. Physical-phone testing
and exact DOS mix/voice behavior remain outside these checks.

The signed normal debug APK includes ARM64, ARMv7 and x86_64 and installs without
the test-only override: `temp/game08-investigation/dxx-game08-fm-fix.apk`.
SHA256: `321b15da9fbb7cd5a217b615f0ee493a39ee3646e56a73aea7a3f846518b5c6c`.
Capture preparation was rerun and reproduced the captured loose SNG exactly.
The durable OPL/WAV reference is under `game_data/music/dos-references/descent14-game08/opl/`,
identified by `android/tests/fixtures/dos-midi/descent14-game08-fm.json`.
