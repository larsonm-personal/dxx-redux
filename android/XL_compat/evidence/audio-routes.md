# Audio route invalidation

An XL sound could remain silent after a wall opened, or remain audible after it
closed, while the listener stayed in the same segment. The owned route cache
previously refreshed only when the listener changed segments. XL now invalidates
that cache at the source's explicit wall/texture lifecycle points, preserving
sound owners, channels, playback phase, deferred creation and static visibility.

## Reference and regression

The pinned source is arbruijn/d2x-xl
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. The oracle extracts the actual inline
`CAudio::Update` from hash-checked `include/audio.h`, alongside the existing
actual distance, router and heap consumers. It proves that an unchanged listener
retains stale closed/open paths until an update clears both router endpoints.
The listener identity itself remains unchanged.

The corresponding source events are texture actions, active-door allocation and
retirement, opening/reopening a door, cloak allocation and retirement, and
exploding-wall allocation and retirement. The port adds these hooks to owned
wall code, with two guarded calls in the existing D2 exploding-wall consumer.
The inactive Redux path performs no route mutation. Cache invalidation does not
recreate the sound bank or recompute static visibility.

The new native regression was first run against the unfixed engine. It failed
with `Opened route retained a stale inaudible owner`; the failure is preserved
in `routes-before-fix.log`. With the fix, the same fixture passes.

## Controlled runtime coverage

The shared native/Android fixture restricts loaded geometry to three real
connected segments and keeps a detached listener stationary. Actual opaque and
transparent textures, texture actions, cloak/decloak completion, door
opening/closing and exploding-wall retirement change the route. It queries
between start and completion so an allocation update cannot hide a missing
retirement update. The same sound-owner identity survives every transition.

Ten audible outputs are compared with the independent PCM oracle, interleaved
with blocked-route assertions. Empty texture actions preserve exact earned
playback and deferred state. Restoring an earned runtime snapshot while the
route is blocked causes the next sync to stop playback; reopening resumes the
owner. Existing disk/rewind and Redux-transition controls run afterward.

The explosion case invokes the real allocation and retirement consumers but
sets elapsed time at the endpoint to avoid unrelated damaging fireballs. It
qualifies route invalidation, not explosion physics. The route-specific restore
check uses the runtime snapshot; broader linked-owner persistence is covered by
the existing disk/rewind controls.

Windows passes both 23/40 and 27/40 audio fixtures, source comparisons, scopes,
and exact Redux surface/combat/afterburner isolation. Windows, Linux and Android
builds pass. Both Android catalogs and scoped formatting pass. Android passes
101/101 steps in 392.735 seconds, checking 1,467,392 samples across 111 output
cases at 48 kHz, including all ten route cases. All 26 protected files are
restored with matching hashes. Run ID: `70c6848dd42545dbb069a8ef1495734e`.
The complete private owner passes all 106 required tests with no skips or
failures, plus 16 explicit original-format outcomes and three original modern
source-reader comparisons. CTest takes 417.15 seconds; the complete test/original
verification phase takes 421.187 seconds. Registration and required source
arguments are checked by the owner. The normal D2 metadata executable is rebuilt
and its stock/recovery controls pass alongside the developer executable.

Artifacts are in `temp/xl-gameplay-audio-20261029` in the active worktree:
`routes-before-fix.log`, `routes-after-fix.log`, `routes-source-test.log`,
`windows-routes-runtime.log`, `linux-routes-build.log`,
`linux-routes-full-private.log`, `android-routes-build.log` and
`android-routes-test.log`.
The complete private receipts are `routes-private-result.json` and
`routes-private-ctest.xml`; the full audio output is `linux-routes-runtime.log`.

The Android result, state and protected-file receipts are preserved as
`android-routes-result.json`, `android-routes-state.json`,
`android-routes-before.sha256` and `android-routes-after.sha256`.
Qualified binary SHA-256 values:

- Windows: `1a997fc0fd3cfb4d4ed85ad167c6f81f3256f6db6552383aa5bd9148b2db06cf`
- Linux: `eaa1a69f4281bf2899d39bc4307eb4d5472ae02d15eecb49c8dfc050f5776bac`
- Android APK: `9a318ad1e4581d8ce4e8c8815534d22bf9e506fdda406c5c63722b173a148e38`

## Remaining support

This fixes the route-cache lifecycle, not all ambient audio consumers. Ambient
external sound objects still need resource-qualified linked creation, enabled
state bound to object lifetimes, and saved external-owner validation. The current
restore decoder deliberately rejects external linked owners. Static explosion
ambience and classic replacement installation remain separate. Admission is
unchanged, and no original campaign completion is claimed.

The checks extend existing source and native/Android owners. They add no CI job,
extra Android automation step or timeout relaxation.
