# Classic sample and spatial audio foundation

This prepares ordinary XL playback; it does not enable it. Ordinary/ambient
dispatch, shared ordinary/named output, moving owners, wall changes and saved
state still require integration before admission changes. Android has not been
run for this increment.

## Source and scope

The source oracle extracts consumers from arbruijn/d2x-xl commit
`2576a98b5de310e3ac7c9777df2677d1627b8bae`, with file hashes checked by CMake.
It executes stock sample resampling, the Dial heap and router, CAudio::Distance,
GetVolPan, FixSinCos, FixACos and the vector consumers. Storage, geometry inputs
and router setup use adapters. Geometry construction has its separate existing
qualification; a captured native graph is an input here, not proof of the graph
builder itself.

The qualified audio conversion profile is the source high-resolution SDL
44.1 kHz signed-16 stereo output, with stock 11.025/22.05 kHz input. Device-rate
conversion checks duration and channel preservation; they do not qualify a
different source-native profile. Ear-angle comparisons use default math format 2. The allocated all-destination router is covered; its fallback is not.

## Verification

| Check                | Evidence                                                                                                                      |
| -------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| Stock conversion     | 690 source/device controls; 2,664,720 exact PCM values; seven malformed-input rejections                                      |
| Complete native bank | All 183 samples retained; four scoped replacements preserve unrelated PCM; malformed samples leave live named state unchanged |
| Routing              | 252 traversals and 6,372 paths, including ties, bucket wrap, blocked edges and visibility-shortened interiors                 |
| Distance             | 6,615 source comparisons for range boundaries, same-segment returns, visibility correction and endpoint adjustments           |
| Decay and pan        | 394,376 source comparisons, including every signed 16-bit ear angle and reverse/high-resolution settings                      |
| Listener geometry    | 181,200 source comparisons of normalization, dot/cross, inverse-cosine interpolation and final pan                            |
| Loaded native mine   | 2,232 outputs from the 93-segment fixture, 1,858 audible; all match source distance/ear-angle/volume/pan on Windows and Linux |

The existing native audio runner still passes both level layouts, scoped assets,
actual named PCM, pause, output reopen and disk-through-stock checks. Those
existing lifecycle checks do not qualify ordinary playback. No new CTest
processes are added.

Production ear-angle math passes ASan/UBSan for all 181,200 source cases.
The reference translation unit alone disables shift sanitization because the
original FixQuadAdjust shifts negative signed values; production uses defined
arithmetic and retains shift checks. Other linked existing geometry helpers were
not rebuilt with sanitizers in that focused run. Stock conversion separately
passed its earlier ASan/UBSan check.

Artifacts: `temp/xl-shared-audio-20261029/`, including `ear-asan.log`,
`windows-runtime/runtime.log` and `linux-runtime/runtime.log`.

## Preserved source details

- Stock bytes are scaled by 128 without a signed offset, doubled with midpoint
  interpolation, faded at endpoints and duplicated to stereo.
- The inverse-cosine unit boundary returns zero even for the negative unit
  input. The returning Cross overload rounds negative results down.
- Reverse-channel configuration is compared with the integer high-resolution
  profile value, not a coerced boolean.
- Linux requires the source's global exp/log overloads to preserve attenuation
  rounding. A real mismatch was fixed instead of accepting a tolerance.
- Routes are cached by listener segment. Out-of-range and same-segment sounds
  do not populate that cache. Dynamic wall mutation behavior remains to be
  exercised before activating the consumer.

## Shared playback and persistence

The runtime bank installs all 183 stock samples alongside named WAVs. Explicit
physical-sample entry points use the same 128-channel allocator and SDL callback
as named actions. Per-channel gain and pan are saved with class, persistence,
playhead and allocation state in schema 20. Platform headroom remains separate
from effects gain; changing the effects setting does not apply a second global
gain to existing unlinked voices. Linked-owner persistence is still rejected.

Both Windows and Linux level-format integrations pass mixed output, 64 ordinary plus 64
named voices, class/volume replacement, live controls, 11 atomic malformed-state
rejections, disk restore through stock and a 44.1-to-48 kHz device reopen. The
shared PCM checker evaluates each saved channel in slot order against the known
fixture waveform and source-qualified stock conversion, including per-channel
integer gain, pan, saturation and any captured preexisting music. It does not use
the production voice mixer to manufacture expected output. Stock conversion and
device resampling retain their separate source/SDL qualification.

Android's existing owned audio test now has 87 steps, preserving the earlier
named-only checks and adding mixed playback, live gain/pan, full-pool replacement,
pause, rewind and disk restore. The final 48 kHz run
`0616ffa275c64e0eb099cddeabab903b` passes 39 output checks, including five shared
checks, against 818,688 captured samples. All 26 protected state-file hashes
match after restoration. The earlier uncached run also passed; neither run
claims ordinary gameplay call-site or moving-owner qualification.

Repeated preflight and restore reuse one bounded canonical stock bank after
checking all native sample bytes, names, rates and formats. Existing output PCM
can be shared when content and device rate match; authored actions and resource
validation remain fresh. Native controls invalidate the cache for changed bytes,
changes within the same allocation, and another valid bank rate. Malformed data
still rejects before reuse. This addresses redundant conversion work without
increasing any test timeouts.

The captured-mine oracle mode now runs just its 2,232 source comparisons; it no
longer repeats the full primitive suite that the required source-contract test
already runs. Both modes pass separately on Windows and Linux. No test cases or
platform lanes were removed.

Current artifacts are under `temp/xl-shared-audio-20261029/`: native
`windows-runtime/runtime.log`, `cache-android-test.log`, `dispatch-*-source.log`,
`dispatch-*-runtime.log`, and the fast Linux suite's `cache-fast-tests.log` and
`shared-fast-tests.xml`. All 106 required Linux fast checks pass in 477.01 seconds
on this Windows-mounted WSL checkout, including the named runtime in 39.75
seconds. The suite uses its normal two-process limit and original timeouts;
campaign navigation is excluded by the existing authoritative selection. These
host timings are not a measurement of native-filesystem CI performance.
