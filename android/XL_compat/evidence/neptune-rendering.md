# Neptune bounded rendering comparison

This is a controlled native visual comparison, not a playthrough or universal
rendering qualification. Original-mission Android texture installation and
lifecycle evidence are recorded separately in the Neptune plan.

## Capture provenance

The original Neptune HOG has SHA256
`4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303`.
A private visual fixture changes only the 36-byte player orientation at HOG
offset 40824 to exact axis vectors. Its SHA256 is
`5bb1ea52867d11b63b458843936a1be52a12a1d7f541fd9acbe10092b375fc22`.
No geometry, lighting, texture or other object record changes.

The reference uses the previously labeled still/backbuffer/visibility diagnostic
executable, SHA256
`b0aa42704392c51438892a9b0050fa7afd7a5a2a0c2799ec4fd6caaffd9fb090`.
It disables wiggle and permits reliable framebuffer capture; it is not the
unchanged reference release. Redux uses its explicit still capture, normal
compiled face splits, and binary SHA256
`bf79364805df60ec09a2000b34e7a450663605fa5ae64fd235901489cabe43a5`.

Both cameras have position `[-2734005, -524288, -3767285]`, right vector
`[-65536, 0, 0]`, up `[0, 65536, 0]`, forward `[0, 0, -65536]`, and a
1024 by 1024 viewport. Effective projection differs by 0.000008472 relative.
Reference camera inspection is not synchronized to the screenshot frame; the
still fixture fixes the player pose. Both use nearest texture filtering.

Artifacts and scripts are under `temp/xl-neptune-20261025`. Initial captures
with different filtering and cockpit viewports are retained but not used as
matched comparisons. `nearest-comparison.json` and `effects-comparison.json`
retain exact rectangles, frame hashes, mean RGB, p95 and maximum errors.

## Direct pixel results

Mean absolute RGB channel error on the 0-255 scale, without image fitting:

| Region                           | Reference effects disabled | Reference effects enabled |
| -------------------------------- | -------------------------: | ------------------------: |
| Near ceiling                     |                      0.053 |                     0.053 |
| Sloping ceiling                  |                      0.072 |                     0.713 |
| Back wall                        |                      0.182 |                    10.689 |
| Right wall                       |                      0.255 |                     0.255 |
| Floor                            |                      0.128 |                     0.128 |
| Light panel and surrounding glow |                     42.674 |                    48.138 |

Redux effects remain enabled in both comparisons. The first reference profile
had its effects master switch disabled, despite coronas individually enabled.
The second capture changes that master switch only. The comparison retains the
light-panel region rather than hiding the largest discrepancy. Static regions
exclude the HUD and rotating pickup, whose timing and presentation differ.

The effects-enabled reference produces a broader, brighter glow. Redux's
`runtime/corona_renderer.c` deliberately draws a procedural soft halo without
external artwork; the reference uses corona/glare bitmap sprites. This bounded
comparison does not establish equal halo shape or brightness. Do not tune a
mission-specific multiplier to fit this screenshot or call these images equal.
The existing pilot acceptance excludes exact images, but this visual difference
must remain visible in subsequent support claims.

The five static regions with reference effects disabled support correct baked
surface lighting and texture application for this view. They do not establish
all views, dynamic-light parity, Android framebuffer parity or gameplay completion.
The independent original-reference check compares all 287 baked ambient colors;
Android verifies all seven installed POG payloads and their GPU residency.
