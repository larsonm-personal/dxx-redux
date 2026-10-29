# Owned XL named audio

Continue the authorized run through 2026-10-29 04:54:16 UTC on
xl_more_45_named_audio, based on the validated message increment in PR #190.
Keep the root checkout and original/private assets unchanged. Do not merge.

## Evidence and order

All 25 embedded Pumo WAVs are 44.1 kHz, stereo, signed 16-bit PCM. The existing
replacement-sound decoder intentionally averages stereo to mono for classic
Redux samples. Named XL sounds need channel-preserving data before playback is
enabled. Pumo has 60 sound actions, including a looping alarm, repeated ambience
and dialogue, counted from all fourteen decoded original levels. The original
SND tables already match the pinned text reader.

1. Share the existing bounded RIFF validation with a channel-preserving PCM
   decoder. Preserve classic mono conversion and its original golden samples.
   Compare original stereo samples independently; do not equate mono decoding
   with faithful named-sound support.
2. Audit pinned LoadAddonSound, DoPlaySound, frame maintenance and channel
   duration/loop semantics. Named WAV playback in the reference requires its
   SDL mixer profile. Define owned resource resolution and explicit unsupported
   profiles, avoiding global sample-slot replacement.
3. Implement owned sample preparation, playback channels and completion,
   repetition, cleanup and persistence. Keep XL controls separate from stock
   sounds and preserve stereo. Check real mixed output and rejected resources,
   not just successful API calls.
4. Enable action 30 only after ordinary wall/object activation, source semantics,
   actual mixer output, disk/rewind and Redux recovery pass. Other effect objects
   and independent original-mission requirements remain gated.
5. Extend meaningful public/private tests and registered Android ownership
   checks. Publish completed, reviewable increments and inspect CI. If a
   prerequisite remains unfinished, retain the action gate and document exactly
   which foundation is implemented.

Use controlled events and poses; no autonomous campaign traversal. Keep source
defect corrections, resource decoding, audible playback and campaign acceptance
as separate evidence claims.

## Completed decoding foundation

The shared bounded RIFF reader now has separate mono and channel-preserving
consumers. Existing mono golden samples still match. An independent Python WAV
reader verifies 28 generated cases and every PCM sample from all 25 original
Pumo WAVs on Windows and Linux. Both reject malformed inputs through the same
validation path. Normal Android builds successfully.

This increment leaves named playback and action 30 gated. SDL mixer voice
ownership, looping, real output, cleanup and persistence need their complete
runtime increment before admission. The reference's Duration() adds half a
second before returning fixed-point duration, and time zero produces infinite
looping through its SDL loop count; these are obligations to test explicitly.

## Completed source contracts

PR #192 completes the source audit in step 2. The 126 required controls execute
the pinned sound action, resource selection, frame maintenance and restoration
helpers. All 60 Pumo actions resolve through twelve owned SND tables to 22 of
the 25 embedded WAVs on both hosts. Reuse
[named-audio-source.md](../../XL_compat/evidence/named-audio-source.md) and its
executable oracle; playback, live voices and their persistence remain unfinished.

The reference can report a successful request without starting a channel, and
its restore cooldown can lose a finite sound. Preserve the intended playback
and saved phase explicitly rather than using either outcome as acceptance.

## Next runtime increment

Build an XL voice owner independent of the Redux sample bank. Prepare bounded,
owned PCM and stable resource identities before activating a new level or save.
The first integration must produce real stereo output and retain an explicit
playback cursor; merely assigning a WAV to a spare classic sound ID is not
sufficient for simultaneous voices, looping or restoration.

The local SDL integration has concrete boundaries to handle:

- Redux owns a 64-entry channel array. Its global completion callback indexes
  that array without a bounds check and is installed again on each stock start.
  Additional XL channels need explicit dispatch and bounds protection without
  enlarging Redux's allocator or allowing either owner to release the other.
- digi_mixer_is_channel_playing currently returns zero unconditionally. Use
  verified voice state and actual mixer completion for XL. Do not infer either
  playback or completion from that stub.
- Movie playback already installs Mix_SetPostMix. An XL mixer integration must
  coexist with movies, music, the Android output capture and stock effects.
  Do not silently replace a global callback owned by another subsystem.
- Audio callbacks and game saves run on different threads. Define a coherent
  snapshot and atomic restore, preserving resource identity, source position,
  repetition count, pause and resampling phase. Keep allocation and filesystem
  access out of the audio callback. Cover intentional muting separately from a
  failed playback start or unavailable backend.

Prove stereo separation, finite/infinite repetitions, completion, simultaneous
voices, replacement and stop behavior in actual captured mixer output. Then
exercise wall/object activation, partial-playback disk/rewind, device lifecycle
and stock recovery on the normal Android binary. Keep action 30 gated until
that complete path passes. Ambient sound objects can then use the same owned
voice lifecycle with their separate placement, attenuation and activation rules.
