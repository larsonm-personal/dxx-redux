# Modern XL no-damage and water properties

Objective: support modern split no-damage and water properties through detected
semantics in the shared binary, with honest feature admission and stock isolation.
Teleports are a later tranche. Do not navigate levels automatically.

1. Survey original local missions for 23/40 property usage and remaining blockers;
   pin candidate identities and verify reference property consumers.
2. Centralize old/new property interpretation without conflating modern function
   IDs with legacy combined types. Preserve independent modern functions.
3. Qualify no-damage through existing behavior cases, source restoration and
   negative controls, then enable the supported property in admission/browser.
4. Qualify water separately, including water/no-damage combinations, boundary
   physics, sound/rendering behavior, save/restore and stock transitions. Do not
   silently retain an experimental-only gate as completed normal support.
5. Run affected native and Android checks, reconcile original-mission coverage
   and update support evidence. Unsupported bits, damage values and other
   requirements remain rejected; a property count is not mission qualification.

Initial state: branch xl_more_4 contains completed Neptune 23/40 support. All
modern nonzero properties reject. Legacy no-damage uses function 13; water uses
function 7 and an experimental opt-in profile with additional restrictions.
The unrelated untracked populate_xl_assets.md plan is preserved.

Survey: 311 unique DHF HOGs in the existing extracted corpus contain 19 inspected
23/40 levels. Modern no-damage appears in mb-arena (10 segments) and Glass House
(48 segments); both retain other blockers. No inspected 23/40 level uses water.
No original mission can be promoted solely by this tranche's property support.
Use original records plus explicitly derived fixtures, preserving all other gates.
Full per-level counts, hashes and blockers: `temp/xl-modern-properties-20261025/survey.json`.

Reference `CSegment::Upgrade` and `ReadExtras` confirm the v20/v21 boundary:
legacy functions become independent function/property pairs; modern functions
must be retained when decoding property bits. Header SHA256:
4f43198ba394971128c81373940b5a58283cbbbc2851ca6a6e4187e78382a667.
`main/segment.cpp` SHA256:
eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d.
Collision, firing and physics use HasNoDamageProp/HasWaterProp after conversion.

First implementation checkpoint centralizes property interpretation for browser
counts and runtime no-damage/water consumers. Translation only clears legacy
combined function IDs; modern functions remain independent. Exhaustive byte-bit
controls cover modern/legacy interpretation. Admission remains conservative until
native behavioral and normal-APK qualification pass; this checkpoint is not
completed modern property support.

Initial validation: standalone inventory and native runtime builds pass. Four
affected tests pass in 25.88 seconds: inventory contracts (including all 256
property bytes across 17 function values), Neptune native features, Observatory
descriptor/no-damage behavior and the existing water arena. Scoped formatting
and diff checks pass. Next, extend no-damage fixtures to modern encoding and
source-based restoration, then qualify normal modern admission and combined water.

Modern no-damage checkpoint: bit 8 is admitted independently of modern function.
The native test runs the existing damage, firing, guided, fusion, splash, contact
and boundary cases against a derived modern property map. Paired ordinary controls
now clear the modern bit rather than only clearing the legacy function. An initial
fusion accounting failure came from testing inside a fuel station, which correctly
replenished energy; the isolated fusion test uses an ordinary protected function,
and fuel/no-damage coexistence is asserted separately.

The same test writes two no-damage bits into a private Neptune-derived HOG,
loads it through ordinary admission, saves, switches to stock and restores by
mission identity. Both property bits and the fuel function survive; restored
damage suppression and firing controls pass without a private binding. The four
affected tests pass in 18.65 seconds (modern properties, inventory, Neptune and
Observatory). This is derived-fixture evidence, not newly qualified original
mb-arena or Glass House gameplay. Android and water work remain outstanding.

Neptune's reusable negative property fixture now uses still-unsupported bit 4
(blocked) instead of newly supported bit 8. Its previous 65-step receipts describe
the pre-change bit-8 rejection; a refreshed APK run remains required.

Modern water physics checkpoint: the explicitly requested experimental profile
now accepts only water/no-damage combinations (bits 0, 1, 8 and 9); exhaustive
tests reject every other byte combination. Ordinary admission still accepts
only 0/8. The same property decision is shared by preview and translation.
The derived water fixture checks 75% native displacement without rescaling stored
velocity, idle/thrust turbulence RNG counts, damage in water-only regions,
combined protection and preservation of both bits through stock-to-save restore.
A separate process without the profile flag verifies the default rejection.
Four tests pass in 19.09 seconds: modern water, modern no-damage, inventory
contracts and the existing water arena. This is not completed water support.

Presentation finding: pinned renderlib.cpp IsColoredSegFace/ColoredSegmentColor
uses water boundaries and missionConfig.m_bColoredSegments (default enabled).
The method-0 colored-face path differs from optional volumetric fog; function
differences and adjoining water volumes affect boundary eligibility. Our existing
experimental arena suppresses emitters/ambient rebuilding and has no corresponding
colored-volume implementation. Complete and verify that presentation path before
removing the gate; do not certify physics alone as modern water support.

Water presentation checkpoint: paired pinned diagnostic-reference captures now
hold geometry, resources, exact camera orientation and viewport constant, changing
only segment zero's modern water bit. Source/fixture hashes and the precise byte
offset are in temp/xl-modern-properties-20261025/water-view-manifest.json; captures
and camera receipts are under watdry/ and watwet/. The reference profile uses
method 0, quality 0 and fog disabled. The paired image shows surface tint rather
than a uniform screen overlay. Segment zero retains function 5; differing-function
portal suppression must therefore be tested separately from a same-function portal.
These are explicitly derived diagnostic views, not an original mission acceptance.

The pure lighting layer now implements directional water-face eligibility and the
ComputeStaticFaceLight material path: blend material into ambient before lighting,
then multiply final RGB by material RGB. Focused checks cover dry/internal faces,
function mismatches, team exceptions with/without walls, fluid textures, disabled
colored segments, volumetric exclusion and both textured/portal color arithmetic.
The native lighting calculation test passes. Runtime draw integration, independent
reference-function comparison, actual Redux rendered comparison and Android remain
outstanding; ordinary water admission remains blocked.

Textured-face runtime checkpoint: modern water now selects the material through
begin-face/vertex/end-face hooks, with reset at end-face and mission clear. Native
controls observe changed water vertex colors and exact restoration after end-face.
The normal renderer builds. Paired Redux dry/wet captures use the same derived
fixtures and camera as the reference. In ceiling region [320,30,660,170], mean
absolute RGB differences (0..255) are dry [0.13639,0.02941,0.00473] and water
[0,0.00332,0]. Receipts: water-surface-comparison.json and port-watdry/port-watwet/
under the current artifact directory. This small surface region excludes portal
faces, objects, HUD, and coronas; it cannot establish whole-scene fidelity.

Modern water no longer inherits legacy arena suppression of ambient rebuilding or
texture emitters. The above images precede this emitter-restoration change and
must be refreshed before any broader rendering claim. Native lighting, modern
no-damage, modern water (including new face-lifetime controls), and legacy water
arena tests pass 4/4 in 18.16 seconds. The admission gate remains unchanged.

Further reference audit: SetupFace sets bTextured=true for no-wall faces even
when nBaseTex is negative, so the static portal calculation must follow that
flag rather than infer the blend from absence of a texture. The pure helper's
untextured arithmetic case is not yet a verified portal oracle. Capture a
same-function wet/dry portal separately before integrating its draw path.

The paired same-function portal reference capture is now available as
reference-portal-pair.png, with wportdry/wportwet camera receipts and
portal-view-manifest.json recording the additional function-5-to-0 edit. Unlike
the earlier differing-function pair, this view visibly attenuates the scene
through the portal (including white lamps). It confirms that textured surface
tint alone is insufficient. Dynamic object animation differs between captures;
exclude that object from pixel comparisons. Next: integrate portal drawing in
the transparent geometry pass and verify this controlled boundary view.

Portal-rendering checkpoint: water boundary preparation now supplies original
vertices, compiled face diagonals, current vertex lighting and reference alpha
0.333 to an XL-only GL backend during the transparent geometry pass. Dry sides,
internal water boundaries and wall-backed faces are excluded. GL depth writes
are disabled during the draw and restored afterward; normal alpha blending is
used. Textured-wall handling remains separate.

The same-function wet/dry Redux capture now includes the boundary. Static wall
region [520,430,650,490] has mean absolute wet RGB error against XL below 0.2
on each 0..255 channel. This is colored attenuation, not a black overlay:
both renderers retain a small blue material contribution. Exact receipts are in
portal-surface-comparison.json; executable SHA256
78aafab3d4313571c068942e588e0693dd2d67b6b4a748f10cf46c2f647b22f5.
Animated objects, HUD and the known differing corona artwork are excluded.

Native portal checks cover independent functions, original vertices/splits/alpha,
dry/internal/wall exclusions and absence of portal state after stock switching.
Four affected tests passed in 16.51 seconds before the following depth-profile
fix: modern water must use the scene-depth rendering profile even when the user's
stock preference is classic depth. The experimental admission gate remains in
place. Added depth-policy checks cover both preferences, stock recovery and save
restoration; these need their refreshed run. Android qualification remains next.

Depth-policy refresh: modern no-damage and water tests pass 2/2 in 6.84 seconds,
including both stored classic-depth preferences before and after stock/save
transitions. The normal x86_64 Android APK builds successfully and is installed
only on emulator-5554. The new pinned derived-fixture runner is registered in the
automation catalog with its two owned JSON scripts and a 360-second timeout.
Both catalog validators pass. The initial scoped formatter found mixed PowerShell
line endings; normalization and a scoped rerun pass.

Android modern no-damage passes 45/45 checks in the normal APK: ordinary admission,
controlled protected/unprotected damage, rendering activity, XL menu indication,
save/restore, stock isolation and restoration back into the derived mission.
Artifacts android-no-damage-result.json, android-no-damage-steps.jsonl and the log
are retained in the current artifact directory. No original mission is promoted.
The experimental combined-water Android run is in progress; admission remains
gated pending completion of water-specific visual/audio/device qualification.

The combined-water Android run finished successfully: 46/46 steps, including
water-only damage, combined protection, source save restoration and stock
isolation. Runner state restoration passed. Receipts are android-water-result.json
and android-water-steps.jsonl. APK SHA256:
9cc9cccbc0240c0020531e6bc3cdabc3d4ad939b01d63bbbf9e0a73ded2e077c.
This run explicitly enables the experimental profile. It establishes the tested
behavior/lifecycle cases, not Android water-face pixels or contact-loop audio.
Those checks, normal-admission promotion, negative fixture refresh and final
support/evidence reconciliation remain outstanding.

Android audio/visual qualification: the experimental combined fixture passes
52/52 steps including actual mixer-linked water-loop playback during thrust into
an ordinary wall and loop removal after leaving contact. The first still capture
showed startup effects and MSAA, producing larger static-region differences; it
is retained as android-view/. The settled matched-settings capture is under
android-view-settled/: exact camera, 1024-square viewport, nearest textures, MSAA
off, normal compiled splits. Mean absolute RGB differences are ceiling
[0,0.003319,0] and through-portal [0.046026,0.052308,0.051026] on the 0..255 scale.
Frame SHA256 36b78b61371a19d1b10cea322ff32db6b0816d59036b7d59858453a1e9fdb1af.
The camera receipt and pixel hash are verified; display/app state restores.
This is bounded static water-material evidence, not universal visual parity.

Normal modern admission is now enabled for bits 0/1/8/9 in 23/40. Water with
robots, contained robots or robot generators has an explicit shared eligibility
blocker; all other property bits, signed damage and multiplayer remain rejected.
The browser distinguishes this profile from legacy experimental water. Modern
water uses ordinary scene-depth lighting, without legacy arena suppression.
Native property-byte/lighting contracts pass 2/2; modern no-damage/water,
Observatory and legacy water arena pass 4/4 (27.93 seconds). The refreshed Android
build and tests must verify no profile-enable step is needed. Added actual water
boundary draw/depth diagnostics are still awaiting that APK run.

Completion audit: all five plan items are implemented and verified. Final normal
APK SHA256 926d4af944009f4c739a657f4e34025ff68d0a6e6be18ba6b4ddd5c4e2087717
passes modern no-damage 45/45, modern combined water 51/51 with no experimental
enable step, and unchanged Neptune plus updated bit-4 negatives 65/65. Actual
water boundary draws occur with zero depth failures and clear in stock. Runners
restore backed-up app state. Both automation catalog validators pass.

The final normal-admission matched-camera capture passes 21/21, with verified
camera/frame generation and pixel hash. Static-region errors match the earlier
settled capture. Frame SHA256:
56aa41fa321d19b106e2599401730dc0b4b497d133a852893b91b8c001ed8316.
A final native source-only boundary test crosses the combined wet/protected and
dry cells in both directions, checking firing and damage immediately on arrival
without private bindings. It passes in the refreshed xl_modern_water test.
Windows renderer/headless and normal Android builds pass; affected native checks
include inventory, lighting, both modern properties, Observatory, legacy water
arena and all three Neptune tests.

The durable requirement/fixture/results record is
android/XL_compat/evidence/modern-properties.md. EXTENSIONS and Neptune's historical
rejection note are reconciled. No additional original mission is promoted by this
tranche. Optional volumetric fog, robots/generators in water, other modern bits,
signed damage, multiplayer and legacy-water promotion remain explicitly outside
this implemented profile. Teleports are still a separate future tranche.
