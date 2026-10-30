# External linked sound ownership

External WAV sounds now bind to the prepared mission audio bank and use the
shared XL mixer. Object ownership retains the resource, native object signature,
spatial controls and playback phase. Ordinary physical samples keep their
separate identity. This foundation does not yet enable sound-effect objects.

## Source behavior and corrections

The reference is arbruijn/d2x-xl
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. The existing hash-checked extraction
executes the actual `CreateObjectSound`, `FindObjectSound`, owner startup and
frame consumers. Fifty-six fresh external creation controls verify filename
handoff, class/decay, ambient/custom flags, loop markers, deferred startup and
unavailable channels. Native receipts add 672 external cases to the previous
768 ordinary object/segment cases. Both level formats compare all 1,440 against
the actual source, including every class, zero/negative/full gain, range,
one-shot/persistent, deferred and unavailable-output combinations.

Two source lookup defects are reproduced explicitly. External creation searches
for physical sample zero but stores sample -1. Repeating an external request
therefore allocates another owner; a live sample-zero owner can instead satisfy
the external lookup at a coincident fixture position. The fixture supplies only
coincident distance, not a replacement implementation of source vector math.
The already-documented object/position union defect remains separate.

The port matches external resource identity and object lifetime. Repeated
requests retain the same owner, including stopped/deferred owners; distinct WAVs
remain distinct. Ordinary-sample removal cannot select these owners. These are
explicit defect corrections, not claims of matching the erroneous lookup.

Persistent owners retain source class-as-decay; one-shots use the supplied decay.
The SDL_mixer S16 output profile retains source full-sample looping behavior and
the existing main/ambient gain policy. Arbitrary alternate output profiles are
not qualified by this increment.

## Resources and persistence

Preparation registers authored named sounds and render-13 sound resources from
the selected mission. Resource lookup still requires the owned HOG or owned
normal/secret-level and mission sound folders. It preserves bounded source and
mixer storage and validates the bank before installing it. Playback requests
cannot open an arbitrary filename or load an unprepared resource.

Saved linked owners distinguish external resources from ordinary samples.
External one-shot voices have a distinct saved type, preserving the existing
ordinary-voice rejection checks. Bank identity, resource index, owner type,
object lifetime and channel serial must agree before any state is applied.

The shared native/Android fixture checks eight actual PCM outputs covering
one-shot playback, simultaneous ordinary and two external owners, earned phase
restore, movement, range stop/restart, ordinary-only removal, reused object
lifetimes and deferred activation. Eight malformed external bindings must fail
atomically. Existing linked persistence now includes active and deferred
external owners in trailer, disk-through-stock and output-rate restoration;
Android additionally exercises its existing save/rewind flow.

## Verification

Windows passes both 23/40 and 27/40 audio fixtures, all 1,440 source comparisons
per format, scope/rejection controls and exact Redux combat/afterburner/surface
PCM checks. Linux passes `xl_audio_voice_contract`, `xl_named_audio_source`,
`xl_named_audio_runtime` and `xl_effect_inspection_gate` (65.50 seconds total).
The effect gate still rejects sound objects before replacing live level state.
Windows, Linux and Android builds pass. The normal Linux metadata build and
`xl_stock_loader_probe` also pass (2.39 seconds).

Android passes 101/101 steps in 444.955 seconds, checking 1,577,472 samples across
119 output cases at 48 kHz, including all eight external cases. Saved active
and deferred external owners survive disk and rewind restoration. All 26
protected files are restored with matching hashes. Run ID:
`337e690996d4493e82941fd8becaf6f0`. Both automation catalogs and scoped formatting
pass. The existing 480-second Android timeout is unchanged.

Original Pumo preparation passes at 44.1 and 48 kHz: fourteen levels, sixty named
actions using 22 WAVs, and all 644 ambient references bound to their 41 owned
WAVs in the actual prepared bank. This is resource preparation, not original
ambient playback or campaign acceptance. Pumo remains outside private CI.

Local artifacts are under `temp/xl-ambient-audio-20261030` in
`C:/local/dxx-redux/temp/xl-batch2-20261027`: `source-external.log`,
`source-receipts.log`, `windows-runtime-detail.log`, `linux-audio.log`,
`pumo-preparation.log`, platform build logs and automation catalog logs.
Android receipts are `android-result.json`, `android-state.json`,
`android-before.sha256` and `android-after.sha256`.

Qualified binary SHA-256 values:

- Windows: `28ebff9c71e7418f56d15544ddfc3db9b0ceb5e1e7334e8fd4e256ca5cc29edb`
- Linux: `a7e0f047c8b684e6aE0efcb6d5d8b863e6c4eae72ff3a41c0ec1a18cf1efe979`
- Android APK: `38e0482380f22a22e12fcbf29de444585e1beebeae17a6743e6dedac0e735511`

## Remaining support

The subsequent [ambient-object tranche](ambient-objects.md) addresses the sound
object work listed below. The remaining audio requirements are tracked there;
this section records the external-owner tranche's original boundary.

Sound-effect objects still need native loading, an XL-owned enabled state tied
to object lifetimes, real activation/deactivation consumers and saved object
state. Static explosion ambience, classic replacement installation and global
addon provisioning remain separate. Admission is unchanged. No original
mission completion follows from these controlled audio tests.

This extends existing source and native/Android test owners. It adds no CI job,
top-level test, Android automation step or timeout relaxation.
