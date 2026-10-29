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
