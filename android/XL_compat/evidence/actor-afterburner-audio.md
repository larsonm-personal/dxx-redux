# Actor and afterburner audio

This increment routes ordinary robot death, owned boss teleport/death, and the
player afterburner through their XL audio policies. Redux remains a separate
runtime alternate. It does not expand mission admission.

## Source and behavior

The oracle uses arbruijn/d2x-xl commit
`2576a98b5de310e3ac7c9777df2677d1627b8bae`, with consumed-file hashes checked by
CMake. It executes the actual `DoAfterburnerStuff` controller and object-sound
lookup/creation/destruction in 576 cases. Sixty extracted actor call expressions
check sound, class, gain, distance and loop selection. These are source controls;
production behavior is checked separately below.

- Boss teleport retains the morph cue and creates a persistent robot-class see
  sound at volume 1 and distance 512. Boss groan uses robot class, volume 4 and
  distance 1024. Completion uses explosion-class sound 58, volume 4, distance 512.
- Ordinary robot groan retains its authored death-roll scale and uses robot
  class. Completion uses explosion-class sound 11, volume 2, distance 512.
- XL afterburner startup follows owned-loop presence, independent of Redux's
  previous-input transition. Held input retains the owner, missing ownership is
  recovered, and ordinary release removes only the ignite loop and emits the
  player-class shutdown cue. Death/endlevel removes player-linked sounds.

The qualified conversion profile is XL's high-resolution SDL signed-16 stereo
profile. `CAudio::Setup` sets its source rate to 44100; the afterburner macros
therefore select loop markers 16013 and 24226. Redux's 11.025/22.05 kHz bank
selection and later output-device resampling cannot change those markers.
The source backend assignment is checked alongside the controller extraction.
This does not qualify the alternate U8 or OpenAL profiles.

There is an explicit source correction: its channel-based lookup misses stopped
owners. The 576-case source run reproduces 48 cases where an inaudible/stopped
owner accumulates another owner or survives release. XL retains that owner while
held and removes it on release even without a live channel. Zero effects volume
alone can retain a silent channel, as in the source; the regression also tests
losing that channel while muted. Unmuting cannot resurrect a released owner.

## Production checks

The same fixture runs on the native engine and Android, with actual mixer output
compared against the independently qualified PCM oracle:

- Eight afterburner output cases cover startup under three Redux bank-rate
  settings, earned trailer restoration, missing-owner recovery, stopped-channel
  recovery, normal release with an unrelated player loop, and depletion.
  Additional state checks cover repeated held input, stopped/muted restoration
  and release, equipment loss, missing sound, death and endlevel.
- Five actor output cases invoke the actual ordinary robot death consumer and
  owned boss teleport, groan and completion functions. Boss completion reaches
  real owner retirement and reactor countdown. Geometry is fixed; the fixture
  sets zero frame time to exclude secondary random fireballs and suppresses
  reactor wall links. This is event/audio verification, not campaign traversal.
- Recording an actual afterburner start/hold/release produces exactly 43 bytes:
  create, kill and shutdown events, with logical IDs, signature, gain and loop
  markers checked. The new controller explicitly records the release event.
  Complete legacy XL demo playback remains unqualified.
- Native stock-transition checks compare exact Redux ignite/shutdown PCM and
  preserve its transition-based missing-loop behavior. The wider audio runner
  retains its mixed-owner disk-through-stock, rewind, pause and device-reopen
  checks. The afterburner-specific earned-state check uses the runtime trailer;
  it is not an afterburner campaign save/load playthrough.

The actor fixture restores temporary objects/definitions, player position,
simulation time/RNG and reactor state. No autonomous level navigation is used.
Both new suites run inside the existing audio integration process, and the
source cases join the existing source executable. No CI job or timeout is added.

## Validation record

Windows source and both 23/40 and 27/40 native runs pass, including scope and
source-creation comparisons. Linux's five selected checks pass in 85.45 seconds:
source audio, full audio runtime, boss spawning, boss transport and boss groups.
After the final profile separation, the affected Linux source and audio runtime
checks pass again in 50.44 seconds; unchanged boss checks are not repeated.
Final Android verification passes all 101 steps in 298.450 seconds, checking
1,230,848 samples across 88 output cases at 48 kHz, including eight afterburner
and five actor cases. All 26 protected files match before/after. The Android
build, both automation catalogs, scoped formatting and diff checks pass.

Final tested binary SHA-256 values:

| Artifact              | SHA-256                                                            |
| --------------------- | ------------------------------------------------------------------ |
| Windows native engine | `d981b1d70364ba32f6ddafdefbefc6cae02c53a429dfe541d0f67c246a25f180` |
| Linux native engine   | `91245d022a6cf1d1c33b9d2bec050337060e2cce7df1e60f5486ad21a5a6e347` |
| Android debug APK     | `5f014da257073826087b5b740850106b1a8abe72346282fd3d2412e7f97c0467` |

The first runtime attempt exposed a test assumption about muted channels; the
next actor attempt used an invalid out-of-mine predicate object, corrected to a
real segment center. Android compilation exposed the raw PhysicsFS versus
rewind-file interface difference, corrected with the shared wrapper. Logs are
retained under `temp/xl-gameplay-audio-20261029` in
`C:/local/dxx-redux/temp/xl-batch2-20261027`.

## Remaining work

Ambient surface distribution, external sound objects, wall-change lifecycle,
optional addons and installation of classic replacements remain separate work.
This does not establish full gameplay sound fidelity or qualify alternate audio
profiles, multiplayer sound messages, or complete demo playback.
