# Ambient surface audio

XL levels now initialize texture-effect sounds through an owned surface
consumer. Redux retains its original initializer. This does not expand mission
admission or qualify ambient sound objects and wall-change behavior.

## Source contract

The oracle extracts `SideIsSoundSource` and `SetSoundSources` from
arbruijn/d2x-xl commit `2576a98b5de310e3ac7c9777df2677d1627b8bae`.
Consumed source files are hash pinned. The source initializer executes against
storage adapters, with audio creation recorded separately from production.
Effect-object and static-explosion iteration is empty in this surface fixture;
neither consumer is claimed by these results.

- Visible faces select their overlay effect first, falling back to the base
  only when the overlay has no effect. An existing silent overlay effect stays
  silent.
- Each segment counts qualifying emitting sides before sample translation.
  Every source receives volume `F1_0 / (2 * count)`, including when another
  qualifying side's logical sample is unavailable.
- A lower-numbered connected segment suppresses the opposite source when sound
  can cross the face and the overlay identities match. Texture rotation is
  separate from identity in XL.
- Creation uses the actual side center, generic class, distance 256, infinite
  playback and deferred permanent ownership. Forcefield hum retains custom
  volume scaling and the source minimum.

The standalone source suite checks 775 selection/setup cases. Native receipts
export actual loaded segment connectivity, doorway flags, texture/effect tables,
logical sound mappings, side centers and resulting owners. The independent
source initializer consumes those inputs and compares ordered sound, position,
gain, range, class, loop and lifetime fields. Each format requires all 16 cases
and compares 35 owners; missing cases cannot silently pass.

## Production and stock isolation

The shared native/Android fixture exercises the real level initializer with
one through six emitters, overlay fallback and silence, unavailable samples,
forcefield scaling, reciprocal portal orientation, different overlays and
disconnected faces. It captures 13 mixer outputs, verifies deferred creation,
earns nonzero loop playheads, restores exact runtime ownership/phase and checks
that repeated initialization does not accumulate owners. PCM comparisons use
the independently qualified mixer oracle; source semantics are checked by the
separate receipts above.

A native stock transition selects two real boundary faces and compares the
actual Redux initializer with two direct half-volume Redux sounds, sample for
sample. Both channels remain in the Redux registry with no XL voices or owners.
This catches accidental application of XL's per-segment gain distribution to
ordinary missions. Existing stock combat and afterburner controls also pass.

Windows builds and both 23/40 and 27/40 audio fixtures pass, including source
receipt comparisons, audio scopes and 2,232 loaded-geometry comparisons.
Scoped formatting and both automation catalogs pass. Linux and Android builds
pass. Five Linux checks pass in 87.87 seconds: source/runtime audio, texture
actions, doorways and water actors. Android passes all 101 automation steps in
381.678 seconds, checking 1,400,832 samples across 101 output cases at 48 kHz,
including all 13 surface cases. All 26 protected files are restored with
matching hashes. Run ID: `14cf38c6ab59470b8e3ad87a4fb48f1a`.

Artifacts are under `temp/xl-gameplay-audio-20261029` in the active worktree,
including `windows-surface-runtime.log`, `surface-publish-source-test.log`,
`linux-surface-runtime.log`, `linux-surface-test.log`, `android-surface-test.log`
and the preserved `android-surface-result.json`, `android-surface-state.json`,
`android-surface-before.sha256` and `android-surface-after.sha256` receipts.

Qualified binary SHA-256 values:

- Windows: `18a94bf8ac3d5e23813d40ec48798fdb251d4d1bf343102679054b264880bdcd`
- Linux: `29ee57ff3faf2a312e1e7a633d690a1092d72d627d9e59fbe252c29acba7258e`
- Android APK: `3d1eec4001db9e5fa21441a59ecd6ee74901167cd87ff5a7bbf240124fded188`

## Remaining requirements

Ambient external sound objects, static explosion ambience, activation/deletion,
wall and texture changes, classic replacement installation, alternate output
profiles and original campaign qualification remain separate requirements.
Generic linked-owner persistence is already tested through disk saves, but this
surface-specific phase check restores the runtime snapshot. It is not an
original-mission playthrough or a claim that every ambient consumer is finished.
The checks join existing source/runtime test owners, without new CI jobs,
additional Android automation steps or relaxed timeouts.

The next known lifecycle gap is route invalidation: source `CAudio::Update`
clears router endpoints after texture changes and wall lifecycle events. The
current owned spatial cache refreshes after listener-segment changes, so a
stationary listener can retain a stale route. That behavior requires a separate
fix with actual wall/texture controls; these initializer checks do not cover it.
