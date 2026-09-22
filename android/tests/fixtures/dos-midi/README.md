# DOS MIDI capture and playback comparison

Run commands from the repository root. Captures and game assets stay local;
the checked-in JSON manifest records their identity and comparison window.

## Capture another song

Use an installed/extracted GOG DOS runtime containing the game executable,
original HMI drivers, HOG/PIG, and `DOSBOX/DOSBox.exe`. For this investigation
the runtime is `temp/dos-midi-capture-20260921/gog`. An offline GOG installer can
be extracted with innoextract 1.9 (`innoextract --extract --output-dir <dir>
<installer.exe>`); the installer is not the source-directory argument below.

```powershell
.\android\helpers\capture_dos_midi.ps1 `
  -SourceDirectory 'temp/dos-midi-capture-20260921/gog' `
  -OutputDirectory 'temp/dos-midi-game03' `
  -Song game03.hmp -Launch
```

The helper refuses to overwrite a session. It copies the runtime, sets the
private game's music to General MIDI port 330 at volume 8, creates a DOSBox
config and private capture directory, and hashes the runtime/config/pilot.
`-Song` extracts the requested HMP for comparison; it does not replace music or
select a level. Omit `-Launch` to prepare without opening the interactive game.
For another executable/config use `-GameExecutable`, `-GameConfig`, and `-Hog`.
Only the D1 GOG workflow has been captured and validated so far. To capture its
default AdLib/FM audio, add `-MusicDevice adlib -MuteEffects` and use **Ctrl+F6**
for WAV capture. This selects the observed GOG D1 device 0xa009 at port 388.
The MIDI shortcut does not capture FM synthesis.

1. At the DOS pause prompt, press **Ctrl+Alt+F8** to arm raw MIDI capture
2. Press Space to launch; keep startup/title/briefing in the same recording
3. Select the desired level using the copied pilot's available levels
4. Skip briefing and open automap to keep the ship safe while music continues
5. Record through a full loop and at least 30 seconds of the repeat
6. Press **Ctrl+Alt+F8** again to finalize the `.mid`, then exit DOSBox
7. Retain the MIDI, extracted HMP, `capture.conf`, `provenance.json`, and the
   actual route through menus/levels together under `game_data/music/dos-references/`

The original test pilot V has levels 1-3 available. The retained Level 7 capture
pilot has level 7 available; its provenance records the disposable pilot edit.
To gather later levels, use a disposable copy of a pilot with those levels unlocked. The helper does
not modify pilot progress or original installed files. Song names/level mapping
come from the game's `descent.sng`, so do not assume every level has a unique song.

DOSBox 0.74 starts timestamps at the first MIDI message and records integer
milliseconds. It expands running status and omits realtime bytes >= 0xF8.
A file whose track length is still zero is unfinished; the diff rejects it.
No audio/soundfont is needed to capture the MIDI messages.

## Repeat the existing regression

The retained local fixture is
`game_data/music/dos-references/descent14-game02/{game02.hmp,dos.mid}`.
It survives scratch-directory retention. Its hashes are in
`descent14-game02.json` beside this README. Level 7 has the corresponding
`descent14-game07` directory/manifest.

```powershell
.\android\tests\test_dos_midi_parity.ps1
```

This builds the host exporter from the production `hmp_android_shared.c` and
the pinned TinyMidiLoader, runs converter/timeline/synth-state tests, then checks
both DOS captures. Use `-Level 2` or `-Level 7` to select one reference and
`-SyntheticOnly` without proprietary fixtures.
Use `-SkipBuild` when the host binaries are current.

The reference test requires:

- All 29341 channel messages in the selected 279.6-second window match in
  per-channel order and value, including the first global loop
- Each corresponding event is within 10 ms (measured maximum: 6 ms)
- Program, bank, pan, pitch, sustain, volume and expression agree at every note
- The old converter fails the same comparison as a negative control

Level 7 additionally requires 13336 matching messages over 250 seconds,
including an EOF restart (measured maximum timestamp difference: 5 ms). Its
second negative control clears the device table in a disposable HMP copy;
playing all arrangements together must fail even with corrected timing/resets.

There is no automatic alignment or tempo stretching. Channel separation permits
different ordering between channels at the same timestamp; order within a
channel is preserved. The window ends between events to avoid an edge moving
across the cut due to millisecond quantization.

The candidate is a standalone song. The reference contains preceding songs.
For this transition test, its initial program/bank/pan are explicitly primed
from the reference prefix, matching the state retained by `hmp_tsf_state.h`.
Volume and pitch are **not** copied: the converter's initial CC7=0 and captured
HMI pitch reset (data bytes 64,64, value 8256) must establish them.
Separate native tests exercise TSF reset/restore and subsequent pan updates.
Without this shared context, the strict comparison correctly reports the
briefing's inherited pan positions as different from a cold start.

Outputs are under `temp/midi-parity/reference/descent14-gameNN`: raw converted MIDI,
`*.mid.tml.mid` containing the actual parsed event stream, per-channel JSON
diffs, and `summary.json`. The TML export quantizes timestamps exactly as the
Android MIDI loader does. Loop scheduler tests separately check repeated cursor
wrapping, retained controller state, and silence through the explicit end time.

## Compare a new recording

Build once using the regression runner, then:

```powershell
& android/build/host-extract-tests/Release/hmp_midi_export.exe `
  temp/dos-midi-game03/game03.hmp temp/game03.mid repeat

python android/tests/midi_diff.py temp/dos-midi-game03/captures/descentr_000.mid `
  --dump-events temp/game03-capture-events.json

# Replace START and DURATION with the measured song section, in milliseconds
python android/tests/midi_diff.py temp/dos-midi-game03/captures/descentr_000.mid `
  temp/game03.mid.tml.mid --reference-start-ms START --duration-ms DURATION `
  --candidate-initial-state --shared-initial-state --report temp/game03-diff.json
```

Use `once` instead of `repeat` to export only the first pass. The `repeat` export
contains a first pass plus one reusable repeat. Gameplay wraps that repeat
without resetting TSF. At EOF, channel resets and a one-tick restart delay are
part of the reusable sequence. `--candidate-initial-state` separates generated
startup CC7=0 and pitch-reset messages from the event-list comparison; their
effect on each note is still checked. `--shared-initial-state` is appropriate for a transition from a
known preceding song; omit it when comparing cold-start playback.

## Fidelity boundaries

Track device records are 20 bytes each, beginning at 0x90 for track 1 (the
timing track has no record). The first word is reserved, followed by four
device designators. Empty lists are universal; 0xa000 selects General MIDI.
Level 7 selects musical tracks 6,9,10,11,12,13 and excludes seven FM/digital
alternatives. A file without any selected GM note-on events retains the old
approximate all-track fallback and reports `no_gm_arrangement`; this keeps
FM-only HMQ files playable without claiming faithful GM instruments.

The implemented branch mode uses a single global infinite HMI loop, validated
against D1 Level 2. It reads branch-table offsets/program/controller records,
skips ended tracks, and consumes CC108-111 internally. Unsupported finite/local
or multiple loop ends are reported and use linear/EOF fallback. D1/D2 stock
corpus conversion is a parser smoke check, not proof of DOS parity for every song.

The full-volume CC7 transfer is the measured `max(value - 2, 0)` at DOS volume 8.
Android's volume slider remains a PCM gain; lower DOS volume settings have not
been characterized. The driver clock runs slightly differently from the ideal
120 Hz used by the export, accounting for a few milliseconds over minutes.

MIDI equality is not audio equality. TinySoundFont/TimGM6mb still determines
timbre and does not implement the captured reverb/chorus sends or modulation.
Pressure events are retained in the exported MIDI but not synthesized by the
current dispatcher. DOS AdLib/OPL playback requires a separate synthesis path.

## Level 7 listening clips

After running the regression:

```powershell
python android/tests/render_dos_midi_comparison.py `
  --output game_data/music/dos-references/descent14-game07/listening `
  --adlib-wav game_data/music/dos-references/descent14-game07/adlib/dos.wav
```

Open `listen.html` in that directory. The host `midi_tsf_render` tool uses the
same pinned synth, soundfont, 48-voice limit, -10 dB gain and sample scheduler
as Android, and explicitly reconstructs preceding-song instrument/bank/pan.
The comparison includes full mixes, isolated percussion, and captured DOS GM
rendered through that same synth. Before/after here isolates the device-table
change; both use the corrected timing and reset semantics.

Over the first 20 seconds, the percussion RMS changes by -6.015 dB when the
overlapping arrangements are removed. The after and DOS-GM full mixes have
RMS levels -29.062 and -29.067 dBFS, respectively. These measurements support
the layering diagnosis; they do not establish waveform identity or subjective
equivalence to FM synthesis.

The optional AdLib WAV is captured from the original GOG DOS game, with effects
muted. It includes a short briefing lead-in and keeps DOSBox's gain; it is not
time-aligned or loudness-normalized against the GM clips. The raw recording and
its provenance are retained locally under `descent14-game07/adlib`.
