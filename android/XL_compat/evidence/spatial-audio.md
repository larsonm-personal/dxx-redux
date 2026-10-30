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
