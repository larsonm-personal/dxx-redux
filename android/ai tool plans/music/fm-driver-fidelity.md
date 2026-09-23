# DOS FM fidelity and broader bank support

Scope: complete music-plan items 1 and 2: improve the original HMI driver's
voice allocation, retrigger, volume and stereo behavior; support D2 banks and
hardware-rhythm patches. Preserve user-selected SoundFont playback and its
measured event conversion. No GPL implementation code or new instrument assets.

## Work and acceptance

1. Inventory original banks and existing captures. Extend reproducible unattended
   capture to OPL, retaining original executables, configurations and hashes
2. Derive volume, panning, paired-voice and retrigger behavior from DOS register
   evidence, including controlled probes when soundtrack data is ambiguous
3. Implement the measured behavior in the shared renderer and compare ordered
   key-on/off, patch, pitch, level and routing state against independent captures
4. Decode D2's actual bank variants and implement hardware-rhythm behavior from
   documented formats and original-driver observations, with bounded validation
5. Exercise full songs from each bank family, repeats, control changes, seek and
   renderer switching. Verify D1 and D2 native playback and preview on Android,
   all shipped ABIs, relevant host/JVM tests and a signed installable debug APK

Completion requires evidence for both driver fidelity and expanded support.
Fallback remains a safety measure, not evidence that an unsupported case works.
Record remaining mismatches explicitly and keep the full scope active until
these requirements have been verified. Original bank assets stay local.

## Initial state

The shared renderer currently converts ADLIB banks into WOPL for BSD ymfmidi,
uses 18 independent OPL3 voices with coarse panning, and rejects rhythm patches
and D2 bank signatures. Existing DOS captures include Level 7 and Level 8;
neutral pitch and arrangement corrections are already integrated. MIDI settings,
reset defaults and custom SF2 import are complete and must retain their behavior.

## Evidence and experimental implementation (2026-09-22)

- Extended the private DOSBox runner to OPL recording and direct window messages
  using SDL's windib backend. This avoids dependence on foreground keyboard
  activation. Both D1 and D2 unattended OPL captures now work. D2 also needs its
  original GOG CD image mounted, matching sound IRQ, Redbook disabled and intro
  movies dismissed. Failed captures retain a DOSBox diagnostic screenshot
- Fresh D1 Level 7 capture matches all 687 notes from the opening. Original Level
  8 matches 666 notes. D2 Level 1 matches all 891 captured notes when AMLIB/ANLIB
  banks are decoded with the same record layout and the HMQ arrangement is used
- Generated 463 controlled velocity/volume/pan notes. The measured rule is two
  sequential 127/128 gain stages on channel volume, then velocity/128, then a
  capped linear pan factor/64, then a 64-entry attenuation lookup. Only indices
  0..62 are reachable with these gain stages; index 63 remains unmeasured
- Carrier TL is attenuation + (63 - attenuation) * bank TL / 63 with integer
  truncation. Modulator TL stays unchanged even in additive mode. This matches
  every probe and all three independent song captures: 5,414 scaled levels,
  including 926 probe sides; 8,976 song operator-level observations overall
- Original routing is bank 0 to 0x20 and bank 1 to 0x10, including with
  StereoReverse=0. Preserve observed routing rather than assuming bank numbers
  are left/right output channels
- Tested all five rhythm-flagged D1 bank entries (four hamdrum toms and one
  rickdrum hi-hat). DOS uses ordinary paired melodic voices with no BD writes.
  Mode=1 is not a reason to reject these entries or enable hardware rhythm.
  Their unusual field values must be shifted as whole bytes and then truncated,
  rather than individually masked. This changes the hamdrum carrier mode to E0
- Added the isolated `fm_probe_hmi_driver` build and reusable register verifier.
  All four controlled cases pass: 926 volume, 72 allocation/retrigger, 64 ham
  rhythm-entry and 64 rick rhythm-entry key-ons, zero register-state mismatches.
  Production was unchanged at that initial experimental gate

### Resolved acceptance work

The initial first-free/slot-zero allocator had 26
voice assignment mismatches in Level 7. Controlled probes now resolve them:
choose the first free voice, otherwise the first voice on the lowest-numbered
eligible MIDI channel. A melodic channel becomes ineligible after any pitch
message, including center, and stays ineligible after later notes. Channel 9
ignores pitch messages. When no eligible voice remains, use incoming channel
modulo nine. All 16 incoming channels were tested for this final fallback.

The apparent spacing effect was a confound: the earlier slow probe also sent
pitch-wheel messages. An independent spacing sweep (1..36 ticks) rules out
spacing as the cause. Modulation, pitch value and instrument changes do not
explain the allocation differences. Header-priority speculation is superseded.

The renderer now matches all 7,604 ordered key-on states across thirteen
controlled cases and the three song excerpts, including all Level 7 notes.
All 6,232 controlled key-on/off transitions and 5,247 settled active-state
snapshots match. Maximum relative transition timing error is 5 ms (most cases
2 ms). These are actual renderer register traces, not an allocation simulation.
Both the isolated player and shipping converter/timeline/renderer pass these
checks. Evidence: `temp/fm-driver-production-probes/controlled-register-report.json`
and `temp/fm-driver-allocation/controlled-register-report.json`.

Stronger pitch probes show a key-on is first nominal, then the persisted wheel
value is applied immediately afterward. Matching only key-on state misses that
second write. A full 12-semitone/128-wheel-position sweep supplies a 3 KB pitch
lookup, preserving DOS rounding and octave-boundary behavior. A linear interval
approximation had 76 errors; the measured lookup passes. Independent captures
cover other octaves, note-on after bend, and all LSBs at six wheel positions.
The LSB changes have no effect. `measure_hmi_pitch.py` regenerates the lookup
from the hash-checked probe; no original instrument asset or driver code is used.

Held-note probes also establish sustain behavior, ignored CC1/CC11, and the
one-sided pan update: CC10 below 64 changes bank 1, otherwise bank 0. The other
side retains its current level until a volume update or new note. Default pan
is balanced full gain on both sides until the first CC10.

Production bank decoding now accepts ADLIB/AMLIB/ANLIB and the observed rhythm
entries with full-byte packing. The new driver is enabled in shared production
code. Removed synthetic startup pitch/volume messages from FM conversion;
GM conversion retains its measured initialization. Native production traces
match all 4,488 key-ons in the three song captures. The four relevant native
suites pass. All 27 D1 songs pass source-prefix comparison, full repeat,
unclipped rendering and deterministic reset tests.

EOF differs from initial play: DOS centers the wheel and subsequent allocation
changes accordingly. A declared-channel override probe proves that EOF resets
the track's declared channel, not every event channel. FM conversion now follows
that rule and accounts for the initial countdown decrement when computing EOF.
Three production probes match two complete captured passes: 200 paired key-ons,
all corresponding transitions and settled states. See `temp/fm-driver-production`.
D2 briefing has GM-only music tracks and intentionally retains SF2 fallback.
All seven available D2 songs pass their applicable native checks; six use FM.

Production validation now skips TinyMidi metadata before reading channel/key
fields. End-of-track metadata leaves those fields uninitialized and caused an
intermittent, incorrect SF2 fallback in the voice-stress probe. The probe now
passes through the shipping path, including same-tick retriggers.

All-ABI debug assembly and 1,066 JVM tests pass (one skipped). The final APK
installs successfully and passes Android FM profiles, persistence, both reset
defaults, preview seek/pause/resume and renderer switching. D1 gameplay passes
41/41 steps and D2 48/48, with native logs confirming ymfm and the expected banks
and HMQ arrangements. Evidence: `temp/fm-driver-android/device/report.json`.

Custom SF2 import/rejection, persistence, preview switching and gameplay pass
in both engines (D1 44/44, D2 51/51). The test now reapplies its profile after
the shared gameplay fixture clears game preferences; previously that reset
caused the test to exercise bundled defaults instead of its custom asset.
Evidence: `temp/fm-driver-android/soundfont-validated/report.json`.

Items 1 and 2 are complete for the original game assets and measured driver
behavior. Generic BNK hardware-rhythm mode is not claimed: the original game's
rhythm-marked entries use ordinary paired voices and are verified accordingly.
Sample-identical timing, physical-phone performance, custom FM bank import,
user percussion/effect controls and curated nostalgic presets remain outside
this completed increment. The preexisting bundled SF2 license remains a separate
distribution decision; no GPL implementation or new instrument asset was added.

Installable, signature-verified ARM64/ARMv7/x86_64 debug APK:
`temp/fm-driver-android/dxx-fm-fidelity.apk`.
SHA-256: `a8307371483f65d1b1db233725bc6c35571f558f4af277e72d839a835536b21d`.
Library and existing soundfont license notices are included in the APK.

Evidence directories: `temp/fm-driver-game07`, `temp/fm-driver-d2-game01`,
`temp/fm-driver-probe`, `temp/fm-driver-voices`, `temp/fm-driver-rhythm-ham`,
`temp/fm-driver-rhythm-rick`, `temp/fm-driver-volume`, `temp/fm-driver-candidate`.
See `android/tests/fm_feasibility/README.md` for reproduction.
