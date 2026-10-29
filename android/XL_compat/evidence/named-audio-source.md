# Named-audio source contracts and original resource closure

Action 30 remained gated in this source-only increment. These tests establish
the behavior and resource inputs; they do not play sound, restore a live voice,
or qualify an original mission. The subsequent
[owned playback implementation](named-audio-runtime.md) has separate output and
persistence evidence, with its remaining support boundaries recorded there.

## Executed source

The new required `xl_named_audio_source` contract compiles unchanged functions
from XL revision 2576a98b5de310e3ac7c9777df2677d1627b8bae. CMake pins every
consumed source/header by SHA-256 and extracts the trigger constants, fixed-point
macros, addon table, sound action, duration calculation, resource selection,
frame maintenance and start/stop restoration helpers. The SDL repetition
expression comes from the actual `Mix_PlayChannel` call site.

Storage, channel allocation and mixer entry points are narrow recording
adapters. The frame adapter records ordinary-operation arguments and scheduler
callbacks; it does not stand in for the whole trigger dispatcher. These limits
are explicit so a source-contract pass cannot be mistaken for runtime parity.

| Controls                    | Count | Observed behavior                                                                                                                                          |
| --------------------------- | ----: | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Sound requests and duration |    47 | Fractional fixed-point IDs, strict cooldown boundary, negative/zero/positive time, failed starts, disabled/missing IDs and both mixer sample formats       |
| Resource selection          |    38 | All HOG/level/mission/addon availability combinations, read-only source behavior, malformed selected input, numeric/prefix aliases and builtin cache reuse |
| Frame and restore lifecycle |    41 | Wall completion flags, object asymmetry, autoplay/permanent dispatch, stop bookkeeping and restore cooldown                                                |

The source has several details the port must handle deliberately:

- Negative time requests infinite playback. Zero time also reaches an infinite
  SDL loop count through the nonnegative branch. Positive time requests that
  many complete repetitions.
- `Duration()` uses converted mixer bytes and adds half a second before fixed
  conversion. Positive repetition cooldown multiplies that result by the count.
  The comparison is strictly greater, including on restore.
- A failed channel start can return success from the sound action without
  setting its playing flag. The runtime must establish real channel/output
  evidence rather than treating that return value as audible success.
- Wall maintenance clears completed sound state and conditionally disables the
  action. The object-trigger loop does not perform equivalent sound expiry.
- Stop bookkeeping clears channel IDs without clearing playing flags. Restore
  can fail to restart a finite voice because the normal cooldown still applies.
  A quality port needs an explicit restoration policy and persisted playback
  position; these observations are not instructions to reproduce lost audio.
- Source resource selection tries a HOG, level sound directory, mission sound
  directory, then global addon directory. Read-only source mode skips HOG
  extraction. Builtin numeric IDs and name prefixes resolve before file lookup.
  A selected malformed WAV does not fall through to a different resource.

Use owned in-memory resources in Redux rather than inheriting source temporary
file extraction or cross-mission cache leakage. If a profile is unsupported,
reject it explicitly. The original Pumo references below need no external
addon files or alias behavior.

## Original Pumo audit

The unchanged original HOG has SHA-256
6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043.
The native audit decodes all fourteen levels, twelve SND tables with 35 rows,
and all 25 WAVs. Every one of the 60 authored wall/object sound actions resolves
through its SND row, the actual source sound action and the actual source
resource-selection function to a WAV inside that same HOG. Those actions use
22 distinct WAVs. None needs a directory fallback or builtin alias.

The audit examines every authored action, including disabled ones; it does not
claim those actions are reachable during gameplay. The source storage/mixer
adapters receive decoded WAV sizes. Independent checks of every actual channel
sample are recorded separately in [stereo-pcm.md](stereo-pcm.md).

The optional original command requires exact expected counts:

```text
xl_named_audio_source <unchanged-hog> 14 60 25
```

Check the original HOG hash before and after this command. A count mismatch,
missing text row, missing WAV, foreign fallback or unexpected conversion format
fails the audit. The asset-free CTest needs no original bytes or credentials.

## Required CI and remaining implementation

Public selection now requires 40 Windows / 41 Linux contracts. The fast private
owner requires 96, retaining its native save/restore, Redux and original-format
checks. The new source contract is required even though sound playback remains
gated; it will be the reference for future implementation comparisons.

Named playback still needs owned prepared samples and resource identity,
channels/voices, stereo mixing and conversion, repetition, completion, pause,
replacement, cleanup, disk/rewind, actual Android output and stock recovery.
Ambient sound objects and particle/lightning objects are separate consumers.
No original Pumo level is qualified by this increment.

## Validation

Windows passes all 40 required public cases in 9.33 seconds of CTest time
(16.531 seconds for the incremental owner). Linux passes the complete 96-case
private selection in 267.35 seconds, plus sixteen original-format outcomes and
three modern source comparisons; the owner records 270.05 seconds for that
test/inspection phase. No experimental failure is excused. These incremental
measurements are separate from the earlier clean build timings.

All 126 named-audio source controls and the complete original Pumo resource
audit pass on both hosts. Supplying a wrong expected original level count
rejects. The original HOG hash matches before and after. All nineteen owner
unit tests, both automation catalogs and scoped formatting pass.

The existing Android classic-sound lifecycle test also passes all 42 steps on
the normal stereo-foundation APK, SHA-256
092bc7d605c660c752dfa06a45e74ba2f8d4755c706d71e4c63779227c054b52.
Run 13db3dbe730d40f2aca9da5c6a146104 completes in 38367 ms. Sample 82 records two
starts, one natural completion, one requested stop during restore, no failure,
and 524964 converted bytes. The captured mixer output contains 481792 stereo
16-bit frames at 48 kHz with no overflow. All 41 outer protected file hashes
match after restoration; the test's own protected state also matches, and the
app is stopped.

This Android control uses a derived import package containing four unchanged
original Dragon Pit members: its HOG, descriptor and two boss WAVs. It checks
the existing mono/scoped playback path after sharing the RIFF validator. It
does not test action 30, named stereo playback, a complete original package,
rendered-reference parity or mission completion. Local evidence is retained in
`temp/xl-audio-contracts-20261028` and
`android/temp/xl-classic-sound-lifecycle`.
