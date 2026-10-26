# Modern water and no-damage properties

The 23/40 profile decodes water (bit 1) and no-damage (bit 8) independently of
segment function. Bits 0/1/8/9 use the shared OpenGL runtime. Production admission
uses detected requirements and resource validation, never fixture names or hashes.
Other property bits, nonzero signed damage, water with robots/contained robots or
robot generators, and multiplayer remain rejected. Legacy water is still opt-in.

The supported rendering profile uses colored segment boundaries, default fluid
physics and method-0 lighting. Optional volumetric fog is not implemented. This
record concerns feature support and controlled behavior, not route completion,
all XL settings, or universal image parity.

## Fixtures and provenance

The local survey inspected 311 distinct DHF HOGs and found 19 modern levels.
Modern no-damage occurs in mb-arena and Glass House, both of which have additional
blockers. No surveyed modern level contains water. This tranche does not qualify
another original mission solely by adding these properties.

Tests therefore use explicitly derived Neptune geometry. The original HOG remains
unchanged: SHA256 `4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303`.
The native test verifies the source hash before and after generating private HOGs.

| Fixture            | Changes                                                          | SHA256                                                             |
| ------------------ | ---------------------------------------------------------------- | ------------------------------------------------------------------ |
| No-damage          | Property 8 in segments 1 and 92                                  | `f6e4fefa751bb6811f48783c462605a08eb834bdcefc24c679f4d5f2c9b5f261` |
| Combined water     | Property 9 in segments 1 and 92, property 1 in segment 91        | `f880f0e636786f939a294c163db2d96d1dc2303b47ff34f98d0457a021dd037c` |
| Dry visual control | Exact player orientation; segment 0 function changed from 5 to 0 | `1f204c5b8334692770f9daadac708e65e3e4aa629049347da0f4acf39fa28dfc` |
| Wet visual control | Dry visual control plus water bit in segment 0                   | `0666081786f887eea4c07f51d094e3937c43e7b88528f9916357e64d8ef7071e` |

The runtime-test descriptor hash is
`10b208867a6704bf6f2e0593650f508db4b5afd59463cc071b4861e91c0ceb63`.
Fixture 92 retains its independent fuel function. Visual controls are separate
from the behavioral fixtures and are not original mission acceptance evidence.

## Behavioral evidence

- `xl_inventory_contract`: all 256 modern property bytes, legacy/modern function
  independence, helpful unsupported-bit and water-actor rejections
- `xl_modern_properties`: existing protection cases for firing, guided missiles,
  fusion, splash, contacts and boundaries; independent fuel operation; normal
  source admission, save/restore and stock switching
- `xl_modern_water`: 75% displacement without changing stored velocity, turbulence
  RNG behavior, water-only damage versus combined protection, original vertex and
  face-split preservation, material lifetime, boundary exclusions and persistence
- Android runner `test_xl_modern_properties_runtime.ps1`: pinned inputs, controlled
  placement, native damage, saves, stock recovery and exact app-state restoration
- Android water script: actual linked/mixer water-loop playback during thrust into
  an ordinary wall and removal after leaving contact, plus rendering diagnostics

No automated 3D route navigation is used. The external-exit boundary control
temporarily changes one private runtime child from solid (-1) to external (-2),
checks material preparation and restores topology before gameplay resumes.

The initial normal APK passed 45/45 modern no-damage checks. The experimental
water checkpoint passed 52/52 including contact audio. These receipts precede
normal-water admission and are superseded for that requirement by the following
normal-APK results. No experimental profile-enable step is present in either
final property script.

| Final Android run                       | Steps | Run ID                             |
| --------------------------------------- | ----- | ---------------------------------- |
| Modern no-damage                        | 45/45 | `ee3546ef661a4d839cc27c90607aa6c6` |
| Modern combined water                   | 51/51 | `ba83bde889144557a26648bae8fb6cda` |
| Unchanged Neptune and negative controls | 65/65 | `c313e04994a14673baba00625e3eb32d` |

APK SHA256: `926d4af944009f4c739a657f4e34025ff68d0a6e6be18ba6b4ddd5c4e2087717`.
Water boundary draws occur after restoration with zero depth failures and reset
to zero in stock. Audio checks reach actual linked/mixer playback, then stop on
leaving contact. Each runner verifies exact backed-up app-state restoration.
Neptune now rejects blocked property bit 4, not the newly supported no-damage bit.

Nine affected native tests pass: inventory, lighting, both modern-property tests,
Observatory, legacy water arena and three Neptune tests. A final direct modern
boundary control crosses wet/protected to dry and back with native physics,
checking arrival damage and firing gates without private property bindings.
Windows renderer/headless builds, normal Android build, and both automation
catalog validators pass. The runner has a 360-second master timeout; its JSON
scripts are owned support scripts. Local original fixtures are required.

## Rendering evidence

Reference source is arbruijn `2576a98b5de310e3ac7c9777df2677d1627b8bae`.
`CSegment::Upgrade`/`ReadExtras` establish the legacy-to-modern boundary.
`InitColoredFace`, `ColoredSegmentColor`, `SetupFace` and
`ComputeStaticFaceLight` establish directional eligibility, function/texture
exclusions, ambient blending and material multiplication. No-wall colored faces
retain SetupFace's textured blend flag despite having no base bitmap.

The running reference uses the explicitly labeled diagnostic still executable
with SHA256 `b0aa42704392c51438892a9b0050fa7afd7a5a2a0c2799ec4fd6caaffd9fb090`.
Captures use method 0, quality 0, fog disabled and nearest textures. Redux keeps
compiled splits; there is no reference-fan override.

Matched Android camera: position `[-2734005,-524288,-3767285]`, right
`[-65536,0,0]`, up `[0,65536,0]`, forward `[0,0,-65536]`, viewport
`[0,0,1024,1024]`, zoom 37126. The frame export validates camera generation,
RGBA payload length and pixel SHA256. Emulator display and app state are restored.

| Static region                      | Rectangle           | Mean absolute RGB error (0..255) |
| ---------------------------------- | ------------------- | -------------------------------- |
| Tinted ceiling                     | `[320,30,660,170]`  | `[0,0.003319,0]`                 |
| Wall viewed through water boundary | `[520,430,650,490]` | `[0.046026,0.052308,0.051026]`   |

Final ordinary-admission frame SHA256:
`56aa41fa321d19b106e2599401730dc0b4b497d133a852893b91b8c001ed8316`.
Its 21-step capture runs without enabling the experimental profile and verifies
actual boundary draws with no depth failures on the final APK above.
An earlier capture included startup effects and MSAA and differed more. It is
retained rather than overwritten. The settled capture waits for startup effects
and matches MSAA settings. Animated objects, HUD and the already documented
procedural-corona artwork difference are excluded from these regions.

Artifacts are under `temp/xl-modern-properties-20261025`, including `survey.json`,
fixture manifests, paired reference/native captures, Android behavior receipts,
and `android-view-normal/comparison.json`. The earlier settled opt-in frame is
retained separately with identical static-region errors. These bounded comparisons do not prove
every camera, transparency ordering case, optional rendering profile or device.
