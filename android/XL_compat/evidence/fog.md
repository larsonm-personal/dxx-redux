# Authored volume fog

The XL path implements detected water, lava, light and dense fog using owned
source colors/distances and actual scene depth. Version-27 volumes select their
four authored settings; light/dense fog in older admitted modern layouts uses
source defaults. Existing pre-27 water/lava profiles keep their qualified
non-volumetric behavior. Redux submits no fog work and retains its projection.

This is a rendering/profile increment, not original campaign qualification.
Pumo's eleven fog-bearing levels still have independent effect, audio, event,
wall and resource requirements. Its remaining unsupported segment-property
requirements occur in three levels; removing fog bits does not admit outdoors.

## Source and behavior

Reference: arbruijn/d2x-xl commit 2576a98b5de310e3ac7c9777df2677d1627b8bae.
The live path is RenderFogFaces/RenderFogSegments and effects/postprocessing.cpp
with FOG_SHADER_TYPE=1, not the obsolete ogl/fog.cpp sample. The comparison hashes
and extracts actual source functions and compiles the production shader math:

| Source                     | SHA-256                                                          |
| -------------------------- | ---------------------------------------------------------------- |
| render/fastrender.cpp      | 51f893295a5f8e254315037e413fa6299debb9b88a2f8224d22d23ac75dd3d4d |
| effects/postprocessing.cpp | c2291476f102dcbed9038aaf42ce4c16e12af243c4fa58dc73fa679174e956b2 |
| include/segment.h          | 4f43198ba394971128c81373940b5a58283cbbbc2851ca6a6e4187e78382a667 |
| gameio/loadobjects.cpp     | 01fe3cbc1cf781d1180921c9e235593f1b79d68924c0d49a42502ef8c3ffcd3c |

Property priority is water, lava, light fog, dense fog. RGB bytes divide by 255;
distance bytes multiply by 20, with zero disabling opacity. Boundary ownership,
opposite-face selection and reversed exterior culling follow the extracted source.
Near/far channels accumulate MIN/MAX in two RGBA32F buffers. Caps behind opaque
geometry are discarded; a fresh depth snapshot after objects bounds composition.
Paired RGB values multiply and opacities add/clamp, followed by two alpha blends.

The active profile uses the source near/far planes 1/5000 consistently for scene
projection and depth decoding; stock keeps 0.1/5000. Nonzero viewport origins and
multisample resolves retain identical source/destination bounds. Batching by type,
cap and culling reduces submission to at most sixteen draws per view without
changing the order-independent MIN/MAX result. Caller GL and readback state is
restored, and GPU names are rebuilt after context retirement.

Two reference quirks are intentional: disconnected volumes form a global min/max
envelope, and equal-type internal boundaries are skipped even when a closed door
occupies the portal. If that door hides all far caps, fog can disappear in that
view. The Android controls include this negative case alongside a fogged exterior
face. These tests do not claim physically accurate volumetric integration.

## Capability admission

Before a fog level is admitted, real float MIN/MAX blending and depth16/depth24
transfer/sampling must work. The actual surface format, full-size allocations
and scene-depth transfer are also checked before committing the level.
Missing capabilities produce a useful load error;
no mission-name or GPU-vendor exception is used. Headless and GLES1 renderers
cannot establish this visibility profile. Runtime failures are reported rather
than silently switching to a different effect.

The local emulator's legacy Google SwiftShader backend failed scene-depth copy:
a rasterized plane and direct depth-texture clear/sampling worked, while depth
transfer did not. All 17 rejection/subsequent Counterstrike controls passed on
that backend (final run f3d77fd39e484cdcb090bf4ec0cf127f, 23233 ms). Host NVIDIA ES3.1 and desktop
Mesa/llvmpipe pass the depth-transfer controls. This is measured driver behavior,
not a general claim about every SwiftShader version or physical Android GPU.

## Verification scope

- Extracted-source comparison: 3072 color records, 262144 boundary configurations
  and 15568 shader cases. MSVC and strict Linux/UBSan controls pass
- Public offscreen desktop rendering: 180 authored and 180 zero-distance cases,
  including depth16/depth24, 0x/2x/4x MSAA, inside/outside, object/geometry
  occlusion, disconnected envelopes, both pairs, empty and zero-thickness volumes.
  Each suite compares 540 pixels, including pixels outside the scene viewport;
  maximum observed error is one RGB byte. Presentation alpha is unchanged
- Another 32 public cases call the production volume submission path using
  known projected geometry, both culling directions, all four types and
  inside/outside views. Analytical world intervals and normal-frame readback
  agree. The complete offscreen test takes about 1.5 seconds locally
- The normal-frame diagnostic compares 25 actual before/after scene pixels to
  CPU composition of the actual volume/depth inputs. Coverage and changed-pixel
  assertions are separate, so an empty or invisible pass cannot qualify fog
- The normal Windows engine and Android app/instrumentation build. The three
  focused MSVC fog/layout/inventory controls and canonical runtime-layout
  integration pass. Four focused strict Linux/UBSan checks pass, and a fresh
  public CI configuration passes all fourteen contracts in 11.18 seconds
- The affected Android canonical-layout regression passes all 87 steps,
  including the updated unsupported-property negative control
- The normal Android app passes all 208 steps, including disk/rewind, 2x/4x MSAA,
  a real lost context while MSAA is selected, all seven derived levels, stock
  transitions and restoring fog from stock. Run f50a99d5dcb6410ea1811e0af4ae841d
  completes in 227915 ms and restores all 28 protected files byte-for-byte

The Android fixture derives seven labeled 27/40 levels from unchanged Neptune
geometry (source HOG SHA-256
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303).
Four single-type levels, mixed types, zero-distance settings and a dry level
exercise the normal loader. These are derived controls, not original missions.

- Derived HOG: 56b224bdec60f8b89a55e51d5b9d9ab548c00cd27a8e75493923253b66c00c1e
- Descriptor: e4d88a385ce95b952a5bcadb0c470e4500c7cb0fe67872a8e31c81f2ec7ba41b

The registered runner preserves/restores existing player, save, configuration,
graphics safety records, preferences and mission bytes. It initializes a known
graphics baseline and uses the persistent graphics queue; a temporary native
option alone is correctly replaced by launcher settings on resume. GPU rebuild
checks follow a verified new frame, rather than assuming activity resume has
already allocated lazy renderer resources. Its script requires disk/checkpoint restoration,
MSAA changes, actual context recreation, dry-level and Counterstrike transitions,
and restoration through stock into the fog mission. No autonomous navigation is
used. Physical Android devices, alternative presentation profiles and full
original campaign playthroughs remain separate qualification work.

Android receipt SHA-256:
790f30972292ac1dd91483dd584fc6084aa335914bebf71e5dbc994b33275b95.
The tested APK is
83f1da6b6f66cb6ea138e7b2d986d6c1a70cdbc6a7147bd3a481402e1c00893b.
The final rejection receipt SHA-256 is
578c21ad03c3327ab5c8b9776a9cb076859beb3787188cd1fcd3fbf2d804fbaf.
Both final runs restore all 28 protected files; the emulator is returned to its
original graphics backend afterward.
The original requirement audit removes only implemented fog/property blockers;
all fourteen Pumo entries remain runtime_qualified=false. Regeneration formats
the result before publication and preserves unchanged bytes and modification time.
