# FM library feasibility experiments

## Scope

Begin with offline Level 7 experiments, then integrate selectable sound profiles
in the increments below. Prefer BSD/MIT dependencies; LGPL is allowed only when
a suitable permissive substitute is unavailable. Do not introduce GPL library
code. Keep experimental FM driver changes separate from production until verified.

## Plan

1. Pin and inspect ymfmidi/ymfm and emu8950 source licenses and build requirements
2. Read original Descent instrument banks and select the FM HMP arrangement
3. Build reusable host renderers and produce Level 7 WAV candidates
4. Measure output, rendering cost and differences against the existing DOSBox
   capture, keeping driver fidelity separate from chip-emulation feasibility
5. Verify Android cross-compilation where practical and document reproducible
   commands, findings, limitations and the recommended next experiment

Use original game assets from the local installation, not bundled third-party
instrument banks. Keep downloaded dependencies and generated audio under temp/.

## Results

Completed the offline feasibility pass with reusable tools in
`android/tests/fm_feasibility/`. That pass left production playback unchanged.

- Pinned BSD ymfmidi/ymfm and MIT emu8950; no GPL library added
- Rendered Level 7 with original game banks and FM arrangement, plus short
  openings covering the other three D1 bank families
- Built and ran on Windows x64 and Android x86_64; ARM64 cross-build passed
- Found and reproduced a broken external-MIDI note-off/retrigger path in the
  pinned ymfmidi; an isolated BSD patch makes Level 7 external/file playback
  byte-identical, with the unmodified library retained as a negative control
- Both permissive OPL2 cores render the same register stimulus successfully
- Produced raw and approximately aligned/gain-matched DOS/candidate listening
  clips, timings, hashes and regression checks; no claim of exact DOS sound
- Identified unused rhythm-flag bank entries and D2 signature variants as explicit
  boundaries of this prototype

See `android/tests/fm_feasibility/README.md` for reproduction, measurements,
limitations and source provenance. Local listening page:
`temp/fm-feasibility/game07-hmp/listen.html`.

Next: original DOS OPL register capture/replay to distinguish HMI driver behavior
from chip-emulation differences. BSD ymfm is the preferred core so far; LGPL
fallbacks have not been necessary.

## OPL trace follow-up (2026-09-22)

1. Capture Level 7 DOS OPL register writes and WAV in an isolated GOG runtime
2. Decode the DRO capture without importing GPL implementation code
3. Replay the original writes through native BSD ymfm and MIT emu8950
4. Compare instrument registers, pitch, volume and note timing with our prototype
5. Apply only evidence-backed experimental corrections and extend reproducible
   checks/listening artifacts; retain the original capture and baseline

### Follow-up results

- Preserved a 126.467-second original Level 7 DRO plus same-session WAV and hashes
- Identified actual OPL3 paired stereo voices despite the dual-OPL2 DRO header
- Matched 1,971 consecutive HMP notes to DOS patch/pitch signatures without gaps
- Replayed original writes through native BSD YMF262 and both diagnostic OPL2
  paths; all three WAVs match byte-for-byte on Windows and Android x86_64
- Added an isolated measured neutral-pitch variant; all 34 observed pitch/block
  combinations pass, with the baseline retained as a failing negative control
- Added strict DRO decoding, source/register diff, fixture regression and a new
  listening page at `temp/fm-feasibility/opl-game07/listen.html`
- Found remaining differences: stereo/volume policy, fewer prototype key-on
  edges over 120 seconds, and drift between DOS and nominal source timing
- The trace starts at source time 5.583 seconds and WAV later still; it cannot
  settle the first-note drum balance. Controlled opening capture remains next

The follow-up supports native BSD ymfm OPL3. Production integration and accurate
HMI driver behavior remain future work; no LGPL/GPL library was added.

## Sound profiles and user tuning

User requirement: account for users choosing the sound they remember from their
old computer, including imported soundfonts and other tuning. DOS fidelity is
one selectable profile, not the only desired sound. The user prefers ymfm in the
current comparison and hears no meaningful difference between its dual OPL2 and
OPL3 clips. Keep chip details in advanced settings rather than making them the
primary choice presented to users.

### Profile model

A saved sound profile combines these independent choices:

| Choice | Responsibility |
| --- | --- |
| Target music device/arrangement | Select compatible HMP tracks and driver semantics |
| Renderer | Generate audio with a sample synth or FM synth |
| Instrument source | Bundled/imported SF2, or original/override FM banks |
| Tuning | Output gain, percussion balance, stereo width, tuning and supported effects |

Initial profile families should be "DOS FM (original game instruments)",
"SoundFont (bundled)" and "SoundFont (custom)". Support saving named variations
and restoring each profile's defaults. Use familiar device-inspired preset
names only when their instrument assets and behavior have been identified and
tested. A soundfont resembling a particular card is not a claim of full hardware
emulation; some remembered sounds require a different synthesis backend.

Start with the existing bundled SF2 and a user-imported `.sf2` path. Keep the
renderer interface extensible to other instrument formats or synthesis systems,
without promising those backends or accepting untested files in the first UI.
The original-game FM profile resolves the melodic/percussion banks specified for
each song; it must not hardcode Level 7's banks as a global instrument set.
An advanced FM bank override may replace either bank independently, with explicit
handling of incompatible or missing patches.

### Playback boundary

Retain shared event scheduling and an interchangeable renderer boundary:

```text
HMP + song metadata
  -> target-device track selection and HMI playback semantics
  -> timed events and controller state
  -> selected renderer + instrument source
  -> optional profile processing and music gain
  -> existing audio output
```

Do not feed FM solely from an already General-MIDI-adjusted export. The measured
GM CC7/startup behavior, FM track selection, percussion mapping and FM controller
handling belong to the relevant device path. Preserve enough source information
to choose either path without destructive conversion. Ordinary MIDI files use
their existing tracks; a renderer switch must not apply HMP-only rules to them.

Existing playback and preview separately load APK `gm.sf2` in
`android/app/src/main/cpp/shared/digi_tsf_music.c` and `midi_preview.c`.
Replace those fixed asset decisions with one shared profile/asset resolution
policy when integrating. Preview, D1 gameplay and D2 gameplay must resolve the
same profile, instrument data and settings. Keep game-format and synthesis policy
in shared native code; Kotlin handles selection, import and persistence.

### Controls and persistence

- Show a profile selector and short preview first, with import/save/reset actions
- Offer music gain and percussion balance as initial tuning controls; keep them
  separate from authored MIDI CC7/CC11 and make changes reversible
- Reserve stereo width and tuning controls for advanced settings. Expose reverb,
  chorus or other effects only when the selected backend or shared processor
  implements them; a settings field alone does not establish support
- Preserve an untouched reference preset beside user variations. Label a tuned
  variation as custom so reference comparisons remain reproducible
- Store imported assets through the launcher's durable asset mechanism, with
  display name, content hash and stable identity rather than a temporary picker
  path. Bank licensing/provenance is separate from renderer licensing
- Use a global default with an optional per-game profile selection; retain each
  profile's controls when switching to another renderer
- Validate a new profile before activation. Load banks off the audio callback,
  stop old voices and restart/reseek through the shared timeline without mixing
  stale synth state. A failed import leaves the working selection active and
  explains the problem. Bound asset memory use on Android
- Include renderer version, device semantics, bank hashes and settings in any
  future rendered-audio/seek-state cache keys and in diagnostic render reports

### Integration order and acceptance

1. Establish profile/asset resolution around the current SF2 backend, including
   importing a custom SF2 and hearing the same choice in preview and gameplay
2. Add ymfm as the FM backend after resolving the outstanding HMI behavior,
   using the same profile selection and renderer boundary
3. Add independent percussion gain and named user variations, then advanced
   controls as supported and validated
4. Grow nostalgic presets from identified instruments and reference recordings

Validate profile persistence, import failures, switching/seek/loop state, and
preview/gameplay consistency in both games. Changing only an SF2 or user gain
must preserve the MIDI event schedule. Switching device families may deliberately
select a different HMP arrangement and must be tested against that target's
reference. Keep event/register fidelity tests separate from listening preference.
Compare preset previews at matched listening levels without silently changing
the saved playback gains.

Library constraints remain BSD/MIT preferred, LGPL only if no suitable permissive
substitute exists, and no GPL library integration. This section records the
design requirement; custom soundfont import and production profiles are not yet
implemented by the offline experiments.

## Soundfont integration work

Implement the first profile family around the current SF2 renderer:

1. Add a bounded shared native soundfont loader and validation used by both
   gameplay and preview; retain the bundled font as the default
2. Store imported SF2 files by content hash in private app storage, persist the
   global choice atomically, and preserve the selection after a failed import
3. Add bundled/imported soundfont selection and import to the MIDI music page
4. Resolve the same selected asset in preview and each game process at startup;
   safely replace the preview synth when selecting a different font
5. Test import/persistence/failure paths and native validation, build both game
   libraries, and run an emulator import/select/preview/gameplay smoke test

This increment does not yet expose FM as a production renderer, per-game
overrides, effects or user gain presets. It establishes shared asset selection
so those follow-up controls have one source of truth.

### Soundfont integration results (2026-09-22)

- Added bundled/imported SF2 selection to the MIDI music page, backed by a global
  private asset catalog and content hashes; imports are limited to 64 MiB
- Preview and both game engines share the same validated native loader and
  persisted selection. Preview switching safely stops the previous playback;
  games pick up changes on their next launch
- Kept file publication compatible with Android API 24 and retained the working
  selection on invalid import or failed activation
- Native SF2 loading/rendering and malformed-input tests pass, as do existing
  MIDI seek tests and all 15 preview synchronization checks
- Three JVM integration tests pass for import/selection persistence,
  deduplication, invalid activation and interrupted copy cleanup
- Debug APK builds; D1/D2 native libraries also cross-build for ARM64
- Emulator import/reject/restart/switch/preview checks pass. Existing D1 music
  control automation passes 38/38 steps (including Level 7 unclipped playback),
  and D2 passes 48/48. Native logs confirm both engines loaded the custom asset
- Scoped code quality checks pass; test soundfont selection/assets were restored

Reproduction: `android/tests/soundfont-profiles.md`. Local integration evidence:
`temp/soundfont-profiles/report.json`. Phone execution and broader third-party
soundfont compatibility are not established by the bundled-font-derived fixture.
Next integration work is the ymfm/FM profile and its outstanding HMI driver
behavior; percussion balance and other user tuning remain planned controls.
