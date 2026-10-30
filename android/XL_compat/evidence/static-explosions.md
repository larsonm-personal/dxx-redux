# Static explosion objects

Type-19 stationary explosions now use XL-owned animation, lighting and permanent
spatial sound, with two narrow D2 powerup dispatch hooks. Redux retains its
existing animation and rendering paths outside XL. This is component support,
not original-mission qualification.

## Behavior and boundaries

Admission checks powerup control (13), no movement, vclip rendering (5), the
single-player shape, clip/frame bounds and positive clip timing. Negative initial
frame timers are valid source state: they advance at the next update and must
not be rejected as malformed. No object record is silently repaired.

The actual pinned initializer looks up `explode2`, inversely translates the
physical sample, and creates permanent class-1 loops at full gain with a
256-unit range. The loaded renderer/clip becomes RT_POWERUP/object ID only when
that name exists. Missing-name and unmapped-name behavior are separate controls.
Object lighting uses the ID independently of the rendered clip. Collision skips
the stationary effect, with an ordinary robot at the same position as a positive
control. Deletion and signature reuse invalidate its sound owner.

Animation always moves forward, without Redux's object-slot fudge or alternating
direction. It retains the source float conversion of elapsed simulation time,
including rounding at large steps. Source-valid negative initial timers are
included in the comparison. Immortal objects remain alive; the source expiry
path has a separate control.

Rendering uses the selected vclip frame, bitmap aspect ratio and full opacity.
The Alpha Effects setting selects normal alpha blending or ONE/ONE additive
blending. This maps the existing frontend setting onto the source's sprite
effects profile; it does not reproduce every XL graphics-settings combination.
High-resolution replacement animation, global addons and particle/lightning
systems remain separate work.

## Independent source comparisons

The reference is `arbruijn/d2x-xl` at
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. Hash-checked adapters execute the actual
SetSoundSources, inverse/forward sound translation, object-list insertion,
UpdatePowerupClip, DoPowerupFrame, DrawPowerup, and sprite opacity/blend policy.
GL blend arguments are captured from the unchanged source overload. These are
source-function comparisons, not a run of the full XL renderer.

- 17 initializer/name cases include missing names, first inverse matches,
  low-memory tables, D1/D2 tables and mixed ambient/static ordering
- 3,205 animation/bitmap-selection comparisons plus expiration include negative
  timers, odd/even slots, frame wrap and source float rounding
- Twelve render-setting profiles compare full opacity and actual blend factors
- Each loaded fixture produces 48 animation steps and two sound-owner receipts
  for independent comparison, including authored versus installed clip selection

The normal fixtures use 23/40, 27/40 and 17/36, plus a signed secret ordinal.
Native integration also compares actual mixed PCM, collision, indexed/RLE
explosion light output, saved phases through a stock level, deletion and reuse.
Five malformed/unsupported object shapes and two invalid clip timings reject
without changing live object or audio state; the negative-timer fixture is a
positive admission control despite its historical `stbadtm` filename.

## Android and regression checkpoint

The reusable owner is `android/tests/test_xl_audio_scopes.ps1`; its default run
includes scope, classic-resource and static-explosion scripts. `-StaticOnly`
selects the static script for focused iteration, retaining publication and full
state backup/restoration. CI membership and the Android master timeout are
unchanged. The native audio integration's overall bound increases from 110 to
150 seconds for its four additional static profiles; every child-process bound
and all existing assertions remain unchanged.

The first completed static run passes 75 steps, 24 offscreen pixel cases and
seven actual audio output checks (62,464 samples at 48 kHz), including rewind,
normal/legacy/secret transitions and disk restoration through Counterstrike.
All 234 protected files restore with matching hashes. This run precedes the
subsequent negative-timer admission correction; final validation is pending.

The pixel test calls the real object renderer and checks every RGB pixel of a
32x32 framebuffer for two frames with different aspect ratios, half/full texture
alpha, normal blending and additive blending. It restores renderer and game
state afterward. It does not substitute a screenshot or a mock draw call.

Android manual secret saving currently remains restricted. The test verifies
that rejection without changing earned state, then saves a normal level before
the stock transition. XL's optional mission/settings-dependent secret-save
override is not qualified here. Native low-level serialization separately
covers the signed secret ordinal; that is not evidence for Android manual-save
UI support.

The first full Linux run passes 105/106 required tests. The existing audio-scope
subprocess exceeds its 45-second bound while Android tests/builds run alongside
it. The exact subprocess then passes alone in 37.57 seconds. The next full run
passes that phase and two static profiles, then reaches the old 110-second total
limit before the final two profiles. The revised total budget accounts for the
added coverage, without increasing child limits or removing checks. The final
Linux acceptance passes all 106 required tests in 389.33 seconds, with 16 format
witnesses and three modern source levels. Final Windows audio integration also
passes all four static profiles. The corrected Android build still awaits its
focused rerun at this publication checkpoint.

## Original mission evidence

Boiling Point HOG SHA-256:
`5cde12e3188fca12b655de3be424871f61bd464e9318b0da6bcdbc65b79551f3`.
The refreshed inspector decodes four of seven levels. The pinned reader matches
267 object identities/ends and 92 effect payloads in those four levels.

| Level        | Format | Static explosions | Remaining inventory findings |
| ------------ | ------ | ----------------: | ---------------------------: |
| boilpnt.rl2  | 23/40  |                 0 |                           72 |
| canyon.rl2   | 27/40  |                 1 |                           68 |
| canyon2.rl2  | 27/40  |                 1 |                           68 |
| watrwrks.rl2 | 27/40  |                 0 |                           34 |

The original static objects are canyon object 49 and canyon2 object 37, both
ID 0 / control 13 / movement 0 / render 5. Their independent particle, lightning,
segment, object and trigger requirements remain. `control.rl2` fails a packed
light-index bound; `subzero.rl2` has an unsupported movement-255 record;
`control2.rl2` fails the reactor-trigger count. Those levels were explicitly
excluded from reader comparison, not counted as supported. Pumo has no type-19
objects, so this change makes no Pumo playability claim.

Artifacts use `temp/xl-ambient-audio-20261030` and
`temp/xl-audio-scopes-android` in the disclosed checkout
`C:/local/dxx-redux/temp/xl-batch2-20261027`. Draft PR #223 contains this work.
