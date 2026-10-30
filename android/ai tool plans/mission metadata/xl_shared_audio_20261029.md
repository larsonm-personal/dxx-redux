# Shared XL ordinary audio runtime

Worktree: `C:/local/dxx-redux/temp/xl-batch2-20261027`.
Branch: `xl_more_60_shared_audio`, above PR #207
(`6b5fba73ec86300f1390456a8a49fae1df869864`). Keep the published stack intact.
The active eleven-hour block ends October 30 at 04:57:30 UTC. No autonomous
navigation or merges; use actual consumers and controlled native/Android events.

The prior ownership library is verified but not an ordinary playback consumer.
Continue the full integration described in `xl_audio_ownership_20261029.md`.

## Additional source findings

- Stock 8-bit samples do not use the WAV conversion path. Actual XL
  CAudioChannel::Resample converts bytes to uint16 values by multiplying by 128,
  performs repeated midpoint doubling and applies a 500-sample endpoint fade
  while duplicating to stereo. Replacing it with ordinary signed PCM decoding
  would change both values and envelope.
- The high-resolution SDL_mixer source profile opens 44.1 kHz signed 16-bit
  stereo. Stock banks are 11.025 or 22.05 kHz. Qualify that source profile before
  adapting its PCM to the actual output rate (48 kHz on the emulator).
- Source low-resolution U8 mono and OpenAL are distinct profiles; do not claim
  parity merely because the S16 profile passes. Unknown native stock rates
  should fail before activation instead of guessing.
- Channel Start calls SetPlaying(false) before installing a replacement.
  Source StartSound first destroys disappeared object owners. Reconcile native
  lifetimes before resolving a dense owner index, or resolve by monotonic serial
  after any cleanup that can move owners.

## Work order

1. Extract and execute the actual stock converter and compare boundary lengths,
   sample patterns and both native bank rates. Keep sample preparation under
   XL_compat, including a documented canonical 44.1 kHz source profile.
2. Prepare complete replaced/unreplaced sample banks with stable identities and
   bounded memory. Install atomically with named resources; do not create a
   separate pool or silently retain Redux stock voices.
3. Route ordinary entry points and real caller classes, then linked spatial
   lifecycle, through the existing mixer and owned registry. Audit caller class
   semantics rather than infer them only from sample IDs.
4. Extend canonical saves, output reopen/pause and native/Android integration.
   Verify full-pool mixed output, object deletion/reuse, disk/rewind, resource
   scopes and return to stock. Admit replacements only when their consumers pass.

No original inventory refresh is needed until admission changes. Keep the
primitive and runtime evidence separate. Continue looking for proven duplicated
CI work while preserving game-specific and headed/headless coverage.

## Prepared-bank checkpoint

The canonical S16 converter passes 690 source/device controls on Windows and
Linux: 2,664,720 exact PCM values plus seven malformed-input rejections. The
source comparisons execute ResampleFormat, both ResampleRate overloads,
ResampleChannels and CAudioChannel::Resample, with independent buffer storage.
Device-rate cases check duration and stereo duplication; they do not claim a
separate source-native 48 kHz profile. The pinned high-resolution SDL setup uses
44.1 kHz.

The native scoped-audio integration passes in 28.40 seconds with all 183 stock
samples prepared. Four real scoped WAV replacements replace sample zero without
changing other samples. Missing data, wrong bits, invalid rates and zero lengths
are rejected without changing the live named snapshot. Restoring the baseline
produces the same complete-bank identity. Complete-bank preparation is callable
but not installed into production gameplay yet; current admission remains gated.

Before runtime activation, resolve gain ownership: source per-channel gain
already incorporates effects volume. Leaving the present named-only global
slider multiplication in place would attenuate ordinary voices twice. Source
SetFxVolume invokes SyncSounds; inspect its linked versus unlinked update rules.
Keep Android application headroom separate from source channel gain. Save all
nondefault gain/pan/class fields and verify stock transitions.

The converter/source comparison also passes ASan/UBSan. The Windows native
integration now passes both level layouts and all scope/save/stock checks,
including the same 183-sample complete bank and four replacements. Artifacts
are under `temp/xl-shared-audio-20261029/windows-runtime/`. No native process is
left running. The only latest production edit after the Linux runtime run adds
sample index/name context to converter errors; the subsequent Windows native
build and integration include it.

## Spatial and gain rules to implement next

The source audit confirms further differences that ordinary routing must not
inherit from Redux:

- GetVolPan has linear, exponential and cubic decay paths; it computes panning
  with DeltaAngleNorm/FixSinCos and compares reverse-channel configuration with
  UseHiresSound's value. Do not simplify that expression to a guessed boolean.
- Distance handles same-segment sound directly, otherwise uses XL routing and
  visibility with endpoint corrections. It expands range by 1.25 for linear
  decay or 2 for nonzero decay. Reusing Redux's connected-distance helper is not
  yet justified by evidence. Port and compare the XL geometry consumers.
- SyncSounds deletes linked owners whose native object is absent, OBJ_NONE or a
  different signature. This differs from DestroyObjectSound(-1), which merely
  detaches forever owners. Disabled sound/lightning effects stop but retain
  their owner. Custom voices skip automatic gain/pan updates.
- Source SetFxVolume stores the scaled effects volume and calls SyncSounds;
  it does not apply a second mixer-wide SFX gain to every unlinked voice.
  Preserve source channel gain and keep platform headroom separate.

## Published checkpoint scope

Publish this foundation as a draft above PR #207 so CI covers the work while
ordinary playback integration continues. It prepares a complete classic bank
and adds spatial primitives; it does not install ordinary playback or change
mission admission. All changes remain under `android/XL_compat/` and this plan.

The route comparison executes the pinned Dial heap and router consumers:
252 traversals and 6,372 paths cover costs, ties, bucket wrap, blocked edges and
visibility-shortened interiors. Another 6,615 comparisons execute CAudio::Distance
for same-segment boundaries, visibility corrections and endpoint adjustments.
Storage, input geometry and PathLength setup use independent adapters. The
allocated-router profile is covered; the fallback router is not qualified.

The volume/pan comparison executes GetVolPan and default-format FixSinCos for
394,376 inputs, including every signed 16-bit ear angle, reverse settings and
high-resolution profile values. Distance and ear angle are supplied inputs:
this does not verify orientation-to-ear-angle geometry or live engine routing.
Linux exposed a global-versus-std math overload rounding difference; production
now uses the source's global exp/log overloads rather than tolerating mismatches.

Complete runtime dispatch, ear-angle geometry, gain ownership, persistence and
native/Android lifecycle coverage before claiming ordinary playback admission.
No new CI processes are added; the existing named-audio source and native runtime
tests own these checks. Android has not been run for this checkpoint.
