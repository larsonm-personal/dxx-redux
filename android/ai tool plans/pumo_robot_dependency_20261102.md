# Pumo robot dependency: corrected investigation

The earlier missing-Vertigo diagnosis was premature. Original Pumo supplies
mods/pmines/pmines.ham and pmines.vham. The manual and published requirements
list D2 v1.2 and D2X-XL, not Vertigo. VHAM contains 12 appended robot definitions.
Original LoadModData loads mission HAM, HXM and VHAM after optionally trying
Vertigo. Robot68 alone cannot identify the required source.

Next: implement and validate owned mission HAM/VHAM load order and isolation,
using Pumo original assets and reference engine behavior. Do not substitute
original Vertigo data for custom Pumo definitions or bypass validation. Preserve
progress-bar work and user data. Removed uncommitted misleading Vertigo guidance
and its test/UI note. Generic robot/model ID/count diagnostics remain. Earlier
Vertigo tests only validate Vertigo, not Pumo custom gameplay fidelity. Phone
logs confirm only66 loaded robots and mounted XL resources; release package
com.dxxrevival.app denies private read access via run-as.

## Implementation in progress

Native XL hook now resolves owned mods/<mission>/<mission>.ham/.hxm/.vham and
loads in source order before capturing mission baseline. Main HAM uses existing
bounded validation and native readers, with mission precedence over global metadata
patches. HXM decoding is staged before publication. XL VHAM structural validation
is separate from unchanged stock loaded-table validation; final reachable closure
remains in the XL level gate. Failed layers reset retail definitions and retain a
specific rejection. This first implementation is not complete or release qualified.

Evidence: original Pumo main HAM passes stock structural validation; VHAM does not,
because inactive entries reference unavailable models/joints. Native source
LoadModData and InitAIObject show static behavior135 installs CT_NONE/MT_NONE.
Pumo robot68 uses model17 and has unused animation offsets1175+, while loaded joints
are1145. Added source-scoped static dependency profile for level21+; normal/dynamic
uses of the same ID still validate animation independently. First integration run
loaded the correct78 robots and failed that overly broad joint check. Updated run
pending. Do not treat the first failure as a reason to import stock Vertigo.

Outstanding: verify exact custom robot/model/weapon values and non-XL round trips,
malformed/foreign owner recovery, actual Pumo level startup and save/load; audit
normal entry points, per-level HAM precedence, save fingerprints and graphics reset.
Add reproducible test registration, run relevant stock/D1/Android builds, preserve
uncommitted progress UI work, and deliver only after exact candidate validation.

## Full-level check follow-up

The native build completed and the full original pmlev1 preflight now reaches
camera weapon validation after loading all78 Pumo robots. Fixed a test diagnostic
lifetime bug: evaluate xl_level_supported before retrieving its rejection string.
The first newly exposed rejection was fusion14 with radius0, children-1, flash25.
Reference CollideWeaponAndRobot only invokes radial explosion/flash processing
when damage radius is nonzero; zero-radius flash is unused. Scoped the camera
validator accordingly, retaining child and radial profile checks and adding values
to its rejection diagnostic. No stock loading path changed by this adjustment.

Latest build: temp/xl-robot-import-20261102/mod-native-build7.log succeeded.
Latest test: temp/xl-robot-import-20261102/mod-controls-full.log FAILS at smart17,
radius1638400 (25 units), children19, flash5, strength2621440 (40 units).
The current radial camera profile hardcodes nonzero flash to FLASH_ID even though
its implementation already consumes definition.flash generically. Next: qualify
custom smart flash and children together against source and physical collision
controls before widening that guard. Do not erase flash or substitute stock data.
Earlier two stock round trips passed before adding full-level preflight; latest
full-level failure happens before round-trip assertions and does not re-prove them.
No new APK or phone mutation. Work remains uncommitted and goal incomplete.

## Custom flash qualification and expanded isolation

Reference effects/fireball.cpp drives robot stun and player blindness from the
weapon flash field, independent of weapon ID. Camera radial validation now accepts
nonnegative custom flash values while bounding strength*flash to signed32 range.
It still enforces child mappings and high-force mine-chain limits. Updated the
negative test to reject negative flash rather than an arbitrary non-flash ID.
Parameterized camera_flash_native_controls and exercised Pumo smart17 values
(flash5/radius25/strength40) through collide_two_objects in both orders and with
self/other signatures. Asserted six children alongside stun, blindness, pause,
composition and snapshot recovery. Entire projectile native suite passed:
temp/xl-robot-import-20261102/camera-custom-flash.log, build8.

Expanded Pumo->stock comparison to model headers/payloads, dying/dead mappings,
texture definitions, object bitmap mappings, sound mappings and player ship,
in addition to robots/weapons/powerups/joints. Two round trips pass in build10;
full-level check now runs after those assertions and still FAILS at gauss32:
radius11, children41, flash2, strength54. Evidence: mod-controls-full.log.

Next: source CreateSmartChildren uses definition.children generically. Existing
camera child creation already does so, but the camera splash profile hardcodes
stock mappings and contact_projectile omits custom child IDs. Qualify the actual
Pumo child41 construction/contact (including its definition, speed semantics,
asset dependencies and recursive children) rather than simply deleting guards.
Then full Pumo startup/save/restore, malformed/foreign-owner and non-XL controls,
Android/D1 builds, formatting/registration, committed exact APK and handoff remain.
No phone changes, no APK produced, no commits. No running build/test process left.

## Data-driven camera children and mine chains

Build13 completed. Full projectile native suite PASSES in camera-custom-chains.log.

- Camera contact qualification follows children from supported player weapon roots
  (bounded visited table handles cycles), so custom41/44 descendants are included.
- Radial validation uses definition.children range instead of stock ID mappings;
  malformed child index still rejected, and flash/force numeric limits remain.
- Added gauss32->41 controls with Pumo speed750/strength1/bounce1 child values,
  checking constructor RNG order, source ID-specific speed scaling, ownership,
  targeting, robot damage and camera impact. Custom flash+children also passes.
- Camera radial explosions now recursively detonate mines at source dist*force
  > 8000, marking dead before recursion. Mine profiles are preflighted. Replaced
  > blanket high-force rejection with signed fixed-point product bound.
- Added threshold tests1900 vs2500 strength/radius16 with mine at8 units and second
  mine at blast center: second is reached through first mine only. Both cases pass.
  Reference effects/fireball.cpp208+ and weapons/fireweapon.cpp778+.

Full original pmlev1 preflight now reaches 'XL skybox requires an active OpenGL
renderer' in mod-controls-full.log, after all camera weapon checks. This is the
headless runner boundary, NOT a passed level load. Two expanded Pumo->stock table
round trips still pass. Next step must use actual rendered desktop/owned Android
runtime; do not bypass skybox/fog requirements or claim player traversal.

Current modified tests: camera_children_test.h, camera_flash_test.h,
camera_missile_test.h, camera_weapon_test.h, mod_data_test.h. Runtime additions:
camera_collision.h/.cpp and camera_splash.cpp. Last native build13 log clean.
No active owned processes, no phone changes, no new APK or commits. Remaining
loader audits (per-level HAM precedence, Android ownership/save fingerprints,
failed layer recovery), Android/D1 validation, formatting/test registration and
exact candidate/handoff delivery remain. Goal active.

## Rendered startup and missing scoped model

Built desktop dxx-redux-d2 successfully (mod-render-build.log and build2.log).
Added developer-only -xl-preview-level with bounds checking for ordinary mission
levels; starts through StartNewGame, not progression proof. Developer preview now
writes fatal errors to stderr instead of modal dialogs. Desktop uses
-route-confirm-user-dir (NOT headless -userdir); owned fixture root mod-controls.
First wrong-userdir attempts exited1; correct-userdir test process32816 displayed
an error dialog. Computer Use skill read, but both node and cua initialization
failed 'failed to write kernel assets: The system cannot find the path specified'.
Stopped only owned32816 after verifying executable path. Rerun30640 exited1
normally; no processes remain. Logs in mod-controls/gamelog.txt.

Actual rendered pmlev1 startup passes renderer admission and rejects missing
model. Added model ID/count diagnostics and headless primary/secondary dependency
checks. Build14 succeeds; mod-controls-full.log now identifies PRIMARY WEAPON11
references MODEL171 while native model_count166. Thus earlier headless skybox
failure masked later data checks; do not claim all level dependencies passed.

Original Pumo contains mods/pmines/models/model171.oof (2094bytes), model177.oof,
and model170/172/173/174.ase with .bin caches. Reference hiresmodels.cpp
LoadOOFModel reads model<number>.oof and named aliases from mission scope.
Current compatibility.cpp explicitly says OOF unsupported and scoped ASE aliases
unqualified. This is the next actual loader gap, not evidence for separate Vertigo.
Implement owned mission model lookup/OOF loading with correct model slots, skins,
bounds and lifecycle isolation (audit existing ASE infrastructure), then retry
rendered startup/save-load and Android. Never substitute stock model171/Vertigo
or erase the weapon reference. Pending recovery/non-XL/Android/D1/formats/CI and
exact candidate delivery remain. No commit/APK/phone mutation this turn.

## OOF decoder implemented, runtime integration still pending

Added android/XL_compat/oof.h bounded version2300 reader and tests/oof_test.cpp;
registered xl_oof_contract in standalone XL CMake. Preserves TXTR names, OHDR
bounds/detail levels, SOBJ hierarchy/offsets/normals/vertex alpha/UVs/colors,
movement/properties and additional chunks. Unknown chunks remain explicit data,
not silently treated as supported runtime behavior. No renderer admission yet.
Rejects truncated chunks, excessive counts, nonfinite values, duplicate records,
missing vertex/material references and cyclic/missing parents.

Standalone MSVC /W4 /WX build passed; synthetic truncation/reference controls and
all15 original Pumo OOF files parsed successfully (oof-controls.log). CMake target
and CTest xl_oof_contract PASS (oof-cmake.log, oof-build). Original OOF files all
version2300. model171 has5 submodels/3 textures, names emberbody, emberishcorona,
puffemberish, empty submodel properties and all root parents. No original assets
were added to Git or altered. Runtime still rejects missing native model171.

Existing d2/xmodel/xmodel.cpp only reads ASE and its renderer flattens submodel
offsets, groups texture batches, and lacks OOF vertex-alpha representation.
xcfile.h Open ignores its directory argument, so reusing it without explicit
owner resolution would borrow global skins. Implement scoped model/skin resolution
and rendering with alpha/normals and source bounds; audit source LoadOOFModel /
hiresmodel mapping and native collision/sphere publication. Don't claim the parser
alone loads missions. Preserve non-XL xmodel behavior and cleanup across missions.
No active processes, no APK, no commits. Goal remains active.

## OOF mesh and owned resource staging

Added oof_mesh.h to compile ordered faces into triangles while preserving per-face
texture grouping/order, normals, vertex alpha, color, UV direction and cumulative
parent offsets. Calculates geometry bounds, enforces native fixed-point coordinate
range and triangle budget, rejects unimplemented property/facing-list/animation
chunks (PINF metadata is harmless). All15 original Pumo OOFs compile; synthetic
mesh alpha/UV/offset/bounds test and CTest pass (oof-mesh-controls.log,
oof-mesh-cmake.log).

resource_validation.cpp now transactionally stages named modelNNN.oof files and
same-owner models/<skin>.tga via existing mission ownership checks. Stores CPU
OofResource (model, mesh, decoded skins) and clears map on mission selection and
load rejection. Exposes xl_owned_oof_model(id) through runtime/oof_resource.h.
No renderer/native model publication yet, so admission correctly still rejects
weapon11 model171. Mod test verifies model171 five submodels/three128px skins and
checks getter absent after stock selection; both round trips PASS in build16.
Build15 failed Windows min/max macros; fixed parenthesized calls, build16 clean.

Reference replacementModels fifth field bFlipV is0 for model171 and original
Pumo OOF slots; mesh currently uses false. Source skin files checked: emberbody,
emberishcorona,puffemberish are128x128 uncompressed24bit TGA. Existing CTGA-compatible
particle_image decoder handles them without substituting assets. Resource staging
currently scans numeric OOF slots; named aliases/ASE precedence still need audit
before final qualification. Native renderer, model geometry publication/collision,
save-fingerprint integration, failure cases and Android validation remain.
No active processes, no APK, no commits. Goal active.

## First rendered level startup with OOF integration

Added oof_geometry.h: exact authored triangle/vertex payload for native missing
model slots (existing native models kept). Runtime publishes missing numeric OOF
slots transactionally after all CPU resources stage. Gaps remain empty/rejected,
not substituted. OOF renderer uses owned materials and view transform, vertex
alpha/fade/cloak, caller blend policy, and classic-depth enablement; GL resources
clear on mission selection. Hook is DXX_XL_RUNTIME-only before optional stock
xmodel replacement. New oof_renderer.h/.cpp registered in runtime/configure.cmake.

CMake/CPU OOF tests including native geometry bounds PASS for all15 originals
(oof-native-controls.log, oof-native-cmake.log). Build17 reconfigured/rebuilt483
objects and both native/desktop targets PASS; build18 adds canvas blending/fade
state handling and passes. Pumo stock round trips remain passing; weapon
primary/secondary preflight now passes and headless check again reaches expected
skybox renderer boundary. Native absent model171 is now populated from its OOF.

Rendered StartNewGame(11) succeeds with build18, no fog/skybox guard bypass.
Owned process17520 reached developer frame30 capture. Viewed screenshot:
mod-controls/screenshots/scrn0000.png and xl-preview-camera.json. This is a normal
level-loader startup with automatic briefing skip, not earned hub progression,
combat, verified model171 appearance or physical-phone evidence. Spawn effect
covers much of view; no visual fidelity claim. Stopped only owned17520 after
verifying path; shell session68269 finished. No live resources remain.
Desktop executable SHA256 D4E176BE8C72D557380B7A40244D25DAA35853D61392A2E40A293E61F1CD8ED9.
Screenshot SHA256 D4F728D891B8BCC798D896547259945824CC4D3B695769E551507157AED97656.

Next must audit/test renderer state/context reset, normals/lighting and source
OOF collision radius/hitboxes, model-slot/ASE precedence and aliases; current
static geometry adapter is not an animated OOF implementation. Native existing
model geometry is preserved while visual OOF override applies; compare source
collision choices before claiming fidelity. Verify actual weapon11 shot and save
through stock in rendered/owned Android tests. Then Android/D1 builds, malformed
layer/owner recovery, metadata/save fingerprints, per-level HAM precedence,
formatting/catalog/CI and exact APK/handoff remain. No commit or APK yet.

## Malformed-layer recovery and ARM64 compilation

Android ARM64 D1 and D2 native builds completed successfully (855-step build,
android-arm64-loading.log; original session6909 exited0). This is compilation,
not an APK installation or device runtime result.

Corrected mod_data_test to assert the engine's actual contract: descriptor
selection can succeed while resource admission is rejected. load_mission_ham
returns the extra-robot flag, not a success flag; changing that interpretation
would break ordinary stock loading. The test now checks nonempty resource
rejection and xl_level_supported(pmlev1.rl2)==false for each malformed layer.
Native headless rebuild passed; mod-recovery-controls.log exits0. All five
HAM/HXM/VHAM/OOF/TGA corruptions reject before level admission, restore byte-equal
stock gameplay and render definitions, clear OOF ownership, and permit stock
then valid Pumo reload. Original fixture bytes were restored in every case.
No production admission guard was relaxed. Tests also pass valid Pumo weapon
resource closure; rendered gameplay/save/load still requires separate evidence.

Save identity audit found an outstanding gap: Android mod_save_capture_definitions
resets extra_ham when the mission full HAM loads; bm_read_extra_robots captures
VHAM, but mission HAM/HXM and OOF resources/skins are not all captured in the
save identity. Add mission-owned source identities after successful staging,
including absence where needed so adding/removing a layer changes identity.
Keep non-XL save behavior unchanged. Test changed assets reject before replacing
saved world, unchanged assets restore, and stock round trips remain valid.

No processes started by this tranche remain live. No APK, commit or phone writes.
Goal remains active; renderer fidelity/ownership, source precedence, Android
runtime, save/load and final delivery checks remain as listed above.

## XL mission source identity and save admission

Implemented mission_mod_sources as a transactionally published map of SHA256
identities for the loaded full HAM, mission HXM, VHAM, numeric OOF models and
owned TGA skins. Clears on mission selection and rejected staging. Missing
optional layers are represented by absence from the complete compared map;
adding/removing a layer therefore changes identity. Hashes describe loaded bytes,
not files re-read at save time. No assets are bundled or rewritten.

mod_save_compat writes xl_mod_sources only when the map is nonempty and compares
it during nonlegacy save preflight before world replacement. D1 compilation and
non-XL serialization retain their existing paths. Old XL saves lacking this
identity cannot silently match newly loaded mission resources. Existing extra
HAM, level HXM and base source checks remain. This covers the newly added mod
loader resources; source precedence/other renderer resources still need audit.

Native source identity controls passed: required layers present, unchanged
reload identity stable, malformed loads clear identity, stock clears identity,
a valid one-byte skin change changes identity, original bytes restore identity.
Native actual save preflight also PASS: synthetic empty-world save accepts
unchanged assets, rejects changed skin specifically for XL source mismatch,
then accepts the SAME save after original resources reload. Evidence:
temp/xl-robot-import-20261102/mod-save-admission-controls.log (exit0).
This is preflight/metadata coverage, not rendered gameplay save/load proof.

A separate mod_save_identity_test.cpp uses the same DXX_REWIND_FILE_WRAPPER
compile setting as mod_save_compat/checkpoint_export. Initial attempt in the
lifecycle translation unit failed because desktop uses raw PHYSFS there; moved
the helper rather than changing engine-wide ABI. Final native build passed.
Android ARM64 D1+D2 rebuild passed: android-arm64-source-identity.log (exit0).
No active processes, APK, commit, or phone changes. Full goal remains active.

## Ownership regression and model precedence audit

Added an actual foreign-origin mount to mod_data_test. The disposable shadow
contains mods/pmines/pmines.ham while the original mission HOG remains in its
owner. Stock selection with that mount present retains byte-equal gameplay and
render definitions and no rejection. Pumo selection rejects specifically for
outside-owner shadowing, blocks level admission, publishes no save identity,
and retains stock definitions. Unmounting recovers the original mission identity.
Native rebuild and full controls PASS (mod-owner-controls.log, exit0), including
all previous malformed-layer and save admission tests. Removed the shadow file
and unmounted the fixture; empty owned fixture directories may remain.

OOF context audit: ParticleTexture::State registers every skin in its linked
registry; xl_particle_images_discard_context reaches discard_context for all
registered textures, releases or abandons handles, and handle() lazily uploads
again. GpuModel owns OofResource before skins (reverse destruction destroys skins
first), so retained pixel pointers outlive textures. No extra OOF reset callback
is needed from this inspection; rendered context recreation still needs testing.

Reference hiresmodels.cpp confirms ASE precedes OOF; within each format named
aliases precede numeric filenames. Null-name rows share the previous model.
LoadModData stage1 loads level-specific model folder before mission model folder.
Current numeric OOF-only implementation does not implement that complete order.
Original Pumo has many named ASE replacements and numeric ASE slots14,17,20,31,
40,47,54,89,170,172,173,174. Thus do not claim full authored model fidelity from
the prior level11 startup. Existing d2/xmodel is not a safe drop-in: CFile::Open
ignores directory argument, texture lookup can escape mission ownership, parser
has globals/cache behavior, and renderer excludes submodels using stock filters.
Next model work must preserve ownership and original geometry/materials; avoid
silently borrowing global skins or treating stock substitutes as equivalent.

Clarified earlier per-level HAM concern against actual source ReadHamFile:
its type1 is the default HAM name with a leading archive-search marker, not
<level>.ham; type2 is mods/<mission>/<mission>.ham. Existing custom base-HAM guard
still rejects mission archive descent2.ham. Supporting that broader override is
separate from Pumo's supplied mission HAM and must preserve trusted stock reset.
Pumo mod root has no level-specific directories. Reference stage1 does load
per-level HXM before camera/model resources; our existing HXM path remains.
No phone actions, APK, commit or active owned processes. Goal remains active.

## ASE structural reader foundation

Added android/XL_compat/ase.h, an independent bounded record reader. Preserves
all authored records/values/block nesting (including unknown properties and
inline face material tokens), literal Windows path backslashes and quoted text.
Rejects malformed headers, unbalanced/truncated blocks, invalid quoted values,
embedded NUL, excess depth/tokens/records and oversized documents. It performs no
file lookup, binary cache access, global-state mutation or asset installation.

Registered xl_ase_contract in the existing XL CMake tests. Native target build
and CTest PASS. Parsed all54 original Pumo ASE files /137 geometry objects:
temp/xl-robot-import-20261102/ase-structure-controls.log. This is structural
coverage only; no claim of mesh/material or rendered ASE qualification yet.
No live loader behavior changed by this foundation.

Next implement bounded semantic conversion from these records into owned mesh
resources and materials, preserving source precedence. Reference models/aseread:
ASE_ROTATE_MODEL=1 maps (x,y,z) to (x,z,-y); ASE_FLIP_TEXCOORD=1 negates V.
ReadNode intentionally zeroes TM_POS (vertices are authored world coordinates).
ReadMeshNormals fills face normals and last supplied vertex normal by index.
Source ignores per-face MESH_MTLID in favor of submodel MATERIAL_REF; preserve
inline material tokens in the document until the semantic policy is qualified.
Treat billboard/glow/thruster/conditional ship submodels explicitly, not as an
excuse to discard geometry. Owned texture resolution and save identity must
include ASE and its actual skins; don't reuse xmodel's global fallback/cache.

No active processes, APK, commits, or phone writes. Full loading goal remains
active; prior model/source/save and delivery limitations remain outstanding.

## ASE geometry/material decoder

Added ase_model.h with bounded indexed decoding for materials, submodels,
vertices, normals, triangle indices and UV indices. Retains authored names,
parent links, diffuse/opacity texture names, material color/transparency, and
submodel material reference. Coordinates map (x,y,z) to (x,z,-y); UV V is negated
per pinned XL source. Referenced vertex normals must exist; duplicate/missing
indexed records, invalid indices/nonfinite values/counts and parent cycles reject.
Repeated per-corner vertex normals follow source's last-value-per-vertex behavior.
Submaterial nesting currently rejects explicitly. No asset resolution or live
renderer hook yet, so this has not changed non-XL/XL runtime behavior.

All54 original Pumo ASE documents /137 geometry objects decode successfully.
Added golden triangle/material controls plus mutations for out-of-range vertices,
UV/material indices, duplicate vertex, NaN and parent cycle. Native build and
xl_ase_contract CTest PASS; original-file run PASS in ase-model-controls.log.
These establish decoded data, not rendered appearance or model activation.

Next: build mesh/material resources and connect source-qualified selection
(ASE before OOF, named before numeric, level-specific before mission-wide).
Audit conditional submodel names and animation records before any runtime
admission. Preserve per-material source semantics instead of treating ASE opacity
maps as arbitrary diffuse replacements. Integrate owned skins and save hashes,
then test actual model17/170/171 rendering and stock reset on an owned device.
No active processes, APK, commits or phone changes. Goal remains active.

## Numeric ASE runtime integration and rendered startup

Added ase_mesh.h converting decoded static ASE parts to the existing owned
triangle/material mesh. Keeps authored coordinate/UV/normal/face data and
material boundaries. Strips exporter directory components from skin names per
XL, then existing owned_mod_file resolves skins strictly within mission models.
Conditional ($/GLOW/FLARE) parts, opacity maps and animated transforms reject
explicitly until implemented. Numeric mission ASE now precedes numeric OOF.
All files and skins participate in the transactional save identity map.
Untextured materials use authored diffuse color; transparency is retained.
Source material details still require rendered comparison before full fidelity.

Runtime validates/stages model17 with three owned skins and installs original
ASE geometry for previously empty model170/172/173/174 slots. Existing POF slots
retain native collision geometry. Added malformed ASE rollback to existing
HAM/HXM/VHAM/OOF/TGA controls. Native headless and desktop builds PASS; all loader,
stock isolation, ownership, malformed-layer, save identity/admission controls
PASS in mod-ase-controls.log. Original numeric ASE mesh controls and CTest PASS
(ase-mesh-controls.log). ARM64 D1/D2 build PASS (android-arm64-ase.log).

Rendered desktop StartNewGame(11) using owned disposable mod-controls root
reached frame30 with ASE staging enabled. Viewed screenshots/scrn0001.png:
cockpit/room/object visible, no admission error. This is startup proof only,
not earned hub transition, player combat, named/level-specific model fidelity,
full gameplay save/load or phone validation. Owned process33428 stopped after
path verification. Prior scrn0000.png retained; previous camera JSON copied to
oof-only-preview-camera.json. Current camera JSON is new ASE run evidence.
Desktop SHA256 979F4CEB080E69B9DEB79BC1D4058A6CAB70FD1B67A32774CC4B958B1913C698.
Screenshot SHA256 8D3F5387DDC0845C03793F912FE71309AFE0A000ED0D2DD89D15B0E7701DC7F0.

CORRECTION to prior folder audit: Pumo DOES have level-specific model overrides
under models/level02 through level14. Earlier inspection looked only at the mod
root. Must implement level-folder precedence before claiming full authored
models. Named aliases, conditional parts (e.g. level09 gunpoints), runtime model
reload/save lifecycle, source collision choices, exact APK and device checks
remain. Shared mesh resource/renderer still has OOF naming; rename when stable
so ASE/OOF ownership is clear. No active processes, APK, commit or phone writes.

## Recursive ASE coverage and conditional-part source audit

Extended xl_ase_test with explicit --inventory mode. It recursively checks all
original ASE files, reports each unqualified mesh, and exits2 if any remain;
it does not turn partial support into a passing qualification. Normal CTest and
root-only fixture behavior remain unchanged. Native build PASS. Inventory:
77 files /334 geometry objects decoded,21 meshes unqualified solely by current
conditional-part guard (ase-complete-inventory.log, exit2). These are missile
thrusters, player ship gunpoint/thruster parts and level09/model49 gunpoints.
The remaining56 mesh documents compile. Named overrides are not yet enabled.

Source audit moved beyond old d2/xmodel adaptation to pinned reference 3d:
fastmodels.cpp G3FilterSubModel excludes gunpoint marker polygons; thruster
rendering depends on directional flags. G3AnimateSubModel rotates rear/frontal
thrusters about their center with source32-frame animation; they must not be
flattened into static geometry or discarded indiscriminately. buildmodel.cpp
SetGunPoints stores marker submodel centers by gun index; Size excludes thruster
and gunpoint markers from collision dimensions. ase2model.cpp generates thruster
face normals from triangle vertices and propagates semantic flags. Thus merely
relaxing compile_ase_mesh's '$' guard would change gameplay/appearance.

Next implement preserved marker/conditional metadata with source-qualified
geometry filtering, gunpoint consumption and thruster rendering, then enable
named mappings/level precedence. Source replacementModels maps aliases before
numeric filenames, ASE before OOF, and null-name rows share previous slot.
Level resource staging must be transactional in XlPreparedResources so browser
preflight does not mutate installed live models or source-save identity.
No owned processes, APK, commits or phone changes. Goal remains active.

## ASE gunpoint metadata

compile_ase_mesh now recognizes $GUNPNT-0..7, preserves centers keyed by gun
index, and excludes marker triangles from rendered faces and collision bounds.
Centers use referenced face vertices, fixed-point bounds then integer midpoint,
matching reference CSubModel::Size and CFixVector::Avg. Duplicate markers reject.
OofResource now retains this map for subsequent consumption; runtime loading
still explicitly rejects nonempty maps until the firing hooks are connected.
No normal progression is admitted using silently ignored gunpoints.

Golden controls check a marker far outside the visible triangle: render count
and collision bounds remain unchanged, decoded gun2 center is(104,106,95), and
adding a duplicate marker rejects. CTest and native headless rebuild PASS.
Recursive inventory now compiles level09/model49's marker-bearing static mesh;
remaining conditional files require thruster handling. Inventory log:
ase-gunpoint-inventory.log (partial qualification, nonzero expected).

Source firing audit: weapons/fireweapon.cpp GetGunPoints selects model-specific
points when gunInfo.nGuns>0, otherwise player/robot default arrays. Crucially,
objects/robot.cpp CalcGunPoint STILL applies Robot_info gunSubModels animation
and offsets after selecting the custom point. Do not force submodel0 when
adding our robot hook. Player firing applies ordinary object orientation and
position after selecting local point. Handle absent marker indices consistently
with source initialization; validate before changing native firing behavior.

Next connect preserved marker data to XL-only local gunpoint selection and test
actual transformed firing coordinates, then thruster semantics/named aliases
and transactional level overrides. No APK, commits or phone changes; goal active.

## XL firing consumes owned gunpoint markers

Added xl_model_gun_point to select loaded mission model marker positions for
valid gun indices. Models without markers and non-XL missions return the caller's
fallback unchanged. With a marker array, unpopulated indices are zero-initialized
like the source model gun array. Core hooks are DXX_XL_RUNTIME guarded:
robot.c selects the replacement local point before its EXISTING gunSubModels
animation/offset chain; laser.c selects player local point before existing
orientation/position transforms. Removed the temporary marker-consumption guard.
Other unsupported conditional parts remain guarded.

Integration fixture appends a valid marker to disposable original model17,
loads through the actual mission loader, restores original file bytes, and
checks selected point(2,3,-3). Actual calc_gun_point with a controlled robot
submodel offset(4,5,6) and world position(10,20,30) produces(16,28,33).
Original Robot_info/model metadata restored before assertions. Stock selection
makes query return fallback unchanged; original Pumo reload restores source
identity. Full malformed/ownership/save/stock controls PASS:
mod-gunpoint-controls.log exit0. Native headless rebuild PASS; Android ARM64
D1/D2 build PASS (android-arm64-gunpoints.log). This is transform/selection
coverage, not actual rendered player/robot projectile or device validation.

Remaining: thruster animation/semantics, named mappings, transactional
level-specific overrides, renderer state/fidelity/collision audit, actual
combat/save/load, formatting/catalog/CI, committed exact APK and final handoff.
No active processes, APK, commits or phone changes. Goal remains active.

## ASE thruster geometry semantics

compile_ase_mesh now preserves thruster submodel/direction/center metadata.
Implements pinned direction flags (rear1/front2/left4/right8/top16/bottom32,
frontal64/lateral128). Plain $thruster and nonrear/frontal polygons are excluded
from visible mesh per G3FilterSubModel. Rear/frontal parts retain triangles and
center-driven spinning metadata. All thrusters are excluded from native
collision payload and bounds, including visible parts. Rejects bodyless meshes.
Gunpoint metadata remains separate from both rendered and collision geometry.

Added pure model_spin.h matching source32-frame, five clockwise Z revolutions.
OOF/ASE renderer applies spin to position/normal before object/view transforms,
advances once per eligible draw after >1ms using SDL ticks, and resets with the
GPU model owner. Current admitted runtime has no slow-motion support, so this
uses source normal-speed threshold. Thruster metadata is retained for exhaust
placement; custom exhaust effect emission is not implemented/qualified here.
Named model activation and rendered source comparison remain outstanding.

Golden tests: plain/side thruster hidden, rear/frontal visible, exact native
collision bytes and bounds unchanged in all cases, frame8 rotation maps the
expected vertex/normal about authored center. ASE+OOF CTests PASS. Recursive
original Pumo inventory now ALL77 documents /334 geometry objects compile
(ase-thruster-inventory.log exit0). Native headless and ARM64 D1/D2 builds PASS
(android-arm64-thrusters.log). No runtime/human fidelity claim from CPU tests.

Next named mapping audit: source replacementModels has literal native slots
0..177 plus virtual powerup/hostage slots based on sourceMAX_POLYGON_MODELS=500;
our native capacity is200. Do not map virtual slots using native200 (would
collide with real robot/model IDs). Named ordinary polygon slots can use source
literal mapping; sprite-to-hires powerup conversion is distinct and unqualified.
Null-name rows share prior loaded model, while failed prior rows allow numeric
fallback. Then transactional level override integration remains.
No active processes, APK, commits or phone changes. Goal remains active.

## Named ordinary polygon model activation

Added model_names.h with135 literal polygon mappings from pinned
models/hiresmodels.cpp (source SHA256
D3154FA522E7E90BC6ADC61B08DA014FBA987E6D7872EEDE259E87A43C9C72EB).
Virtual source500-based powerup/hostage/bullet IDs remain excluded rather than
being incorrectly rebased onto native200. Candidate order is named ASE,
numeric ASE, named OOF, numeric OOF; OOF uses each mapping's source flipV.
Null-name slots share their predecessor when loaded, otherwise retain numeric
fallback. File-level CPU sharing avoids parsing pyrogl three times. GPU cache
now keys the owned resource, so aliases share textures and spin phase too.

Live mission loading now activates named player/missile polygons alongside
numeric ASE/OOF. Explicit controls assert pyrogl108 has8 gunpoints and is shared
with109/110, and concussion127 is shared with137. Full loader/ownership/recovery/
stock/save/gunpoint controls PASS (mod-named-controls.log exit0); alias candidate
order/unit controls and OOF CTest PASS. Native headless+desktop and ARM64 D1/D2
builds PASS (android-arm64-named-models.log). No rendered named-model or actual
projectile verification yet; do not infer it from loading/CPU assertions.

Next: level-specific staging within XlPreparedResources, not mutation during
browser preflight; named file precedence within each folder, per-level skins,
installed-resource/save identity lifecycle. Then rendered named-model/fire/
context-loss checks and exact APK delivery. Existing source collision/fidelity
and custom thruster exhaust-placement limitations remain. No live processes,
APK, commits or phone changes. Goal remains active.

## Pure level-specific model preparation

Refactored mission model decoding into OwnedModelSet preparation. New
xl_prepare_owned_models(ordinal) validates mission bounds, resolves levelNN or
slevelNN folders before mission-wide models, retains named/ASE/OOF preference
within each folder, and resolves skins within the selected folder then the same
mission's model root. Returns owned CPU resources and hashes without publishing
native/GPU models or save identity. Mission initialization now uses this helper
with ordinal0. Level install has NOT been hooked yet.

Integration controls prepare every ordinary Pumo level1..Last_level, verify
level02 player override, level09 robot gunpoints and level11 reactor override,
and assert installed ship pointer and complete live save identity unchanged.
Malformed level11 model99 rejects with installed resources unchanged; original
bytes restored. Native rebuild and full loading/save/ownership/gunpoint/stock
controls PASS (mod-level-model-staging.log exit0). This proves staging only,
not installed level-specific rendering. No APK/device test for this refactor.

Next integrate OwnedModelSet into XlPreparedResources::State and publish during
install_hxm even when no HXM exists. Preserve mission baseline separately from
installed level resources. Regenerate synthetic native geometry for overridden
ASE/OOF-only slots (e.g.172) while preserving source-backed native POF policy.
Rebind ObjectDependencies pointers after resizing staged model tables. Browser
preflight must remain pure. Save preflight runs BEFORE level installation, so
compare saved level-model identity against staged target ordinal rather than
current mission-wide registry; update synthetic save tests to install the
appropriate model set before writing their fixture. Do not merely compare
against global mission_mod_sources after adding level hashes.
No live owned processes, APK, commits or phone changes. Goal remains active.

## Level model publication and save-target identity

Integrated OwnedModelSet into XlPreparedResources: stage the target ordinal,
regenerate missing-native model geometry, and publish model/source maps only at
install_hxm, including levels without an HXM. Keep mission definition hashes
separate from installed level model hashes. Save preflight prepares the target
level identity before installation instead of comparing the currently installed
level's model set.

The first integration run exposed two test/loader issues. The synthetic save
fixture passed no Inventory, correctly tripping the existing CLR guard; it now
reads and inspects the original pmlev1.rl2. This exposed camera append validation
revalidating earlier generated model replacements as animated robots. Camera
append now validates its own changes, then merges the earlier publication set;
normal level dependency checks retain their authored static/animated semantics.
The changed-skin test keeps changed bytes present during target-level preflight
and restores them on both success and exception.

Native headless build and full CPU mod-data controls PASS (exit0,
mod-level-install-controls.log), including original level11 preparation/install,
unchanged save acceptance, changed skin rejection, recovery, malformed layers,
owner isolation and stock round trips. ARM64 D1/D2 build PASS after camera fix
(android-arm64-level-models.log). These are CPU checks, not rendered traversal or
actual gameplay save/load. All test handles are terminal; no phone changes,
commits or new APK.

Next: explicit successive installed level override assertions (including
level02 ship and level11/13 generated model172), rejected preparation preserving
live registry, then current rendered build and actual save/load. Existing
collision/lighting/exhaust fidelity audits remain required; do not infer full
Pumo gameplay qualification from these controls.

## Consecutive installation control exposes level13 HXM dependency gap

Added a real Inventory-backed resource preparation helper and a consecutive
2 -> 13 -> 11 -> 2 install control. It checks pure preparation, published source
identity, player model folder selection, model172 native geometry replacement
and restoration, followed by byte-exact stock definition restoration.
Native test build PASS. Execution is currently FAIL at ordinal13 (pmlev3.rl2):
"HXM definitions are malformed or require unsupported normalization: pmlev3.HXM".
Do not describe this expanded suite as passing; previous passing coverage did
not install level13. Level2 preparation/installation preceded the failure.

Read-only inspection of the original HOG (including its negative-size extended
filename records) shows pmlev3.hxm has four robot replacements and zero joint
replacements. Robot64 references joints1255..1264, robot72/model172 references
1225..1234, robot31/model49 references1265..1284; robot65 has empty ranges.
This matches the earlier missing-joint issue in mission VHAM, but level-HXM
structural validation still requires every robot animation range to fit loaded
tables before authored reachability/static behavior can be considered.

Next determine actual usage/behavior of these robots and source XL HXM loading
semantics. If deferred XL structural validation is warranted, give it a separate
entry point retaining stock HXM semantics, replacement-index bounds and native
capacity checks; validate reachable final dependency closure after all level
layers. Never simply accept invalid active animation or substitute Vertigo.
Keep model/bitmap validation and camera append isolation intact. Expanded native
suite must pass before claiming consecutive installation qualified.
No live test processes, phone actions, commits or APK. Goal remains active.

## XL level-HXM closure and large generated model geometry

Source LoadRobotReplacements in gameio/loadmodeldata.cpp bounds replacement
indices to the loaded registries but reads robot definitions before their use.
Original pmlev3 inventory has robot31 x10, robot64 x1 and robot72 x31, all behavior
135 with zero velocity; robot65 x12 uses behavior129 and empty animation ranges.
Added separate d2_ham_validate_xl_hxm / bm_validate_xl_level_hxm entry points.
Stock HXM callers retain exact previous validation. XL robot references may be
staged within native capacity; replacement indices, record structure, model and
bitmap checks stay bounded. Final level admission walks reachable robot profiles
after level HXM/models. HXM validation defers only robot closure in that path;
mission staging and camera append retain their separate existing policies.

Original level13 preparation then exposed model172's >1000 unique vertices.
Native DEFPOINTS replaces the scratch vertex bank (d2/3d/interp.c). The generated
collision payload now emits bounded batches for large meshes, retaining every
triangle, vertex and normal in order rather than simplifying geometry. Payload
size remains bounded to16MiB. Small models keep their previous bytes. New test
reconstructs a 1200-vertex mesh from successive native banks and compares each
triangle corner to its source; validates bank limits, termination and radius.

Validation: native headless build PASS; full original Pumo CPU suite PASS
(mod-level-install-controls.log exit0), now including consecutive installs
2 -> 13 -> 11 -> 2, ship override/restoration, generated172 geometry change and
restoration, source identity and byte-exact stock definitions. Actual level13
static robot profiles pass; attempts to use each as animated reject. Standalone
D2 HAM/HXM controls PASS including unchanged stock rejection, truncated XL HXM,
out-of-range replacement index, invalid/over-capacity animation ranges. Direct
MSVC invocation has the existing fopen C4996 warning (normal build suppression
was not supplied); no new compiler warnings in CMake builds. ASE+OOF CTest2/2
PASS. ARM64 D1/D2 build PASS (android-arm64-level-models.log exit0).

These are CPU/resource tests, not rendered level13 traversal or earned progress.
Next add failed full preparation preservation against an installed override, run
current rendered build with actual firing/save/load and context recreation, and
finish remaining model collision/lighting/exhaust fidelity audits. No active
processes, phone writes, APK, commits or publication. Goal remains active.

## Rendered startup, Windows mesh ABI and shared skins

Built current native rendered target and exercised original level11 using the
owned mod-controls desktop profile. Initial startup failed after XL admission.
Windows inferno reopened stdout/stderr to CON, hiding redirected diagnostics;
the preview flag now preserves redirection like the existing route runner.
Developer preview also reports uncaught C++ exceptions. LLDB from local NDK
captured the actual renderer error: model20 bad allocation. Temporary abort
stack instrumentation was removed after it did not help.

The root cause was Windows header packing: legacy pstypes.h leaves pack1 active
in resource_validation.cpp, while oof_renderer.cpp includes model definitions
before it. OofRenderFace's new bool made the vector offset differ across these
translation units. Added scoped MSVC pack8 guards to OOF/ASE record, mesh and
resource headers, restoring caller packing. The OOF contract now includes the
headers under pack1 and checks both triangle-vector offset24 and caller packing
restoration. This fixed rendered startup. Separate immutable skin sharing now
avoids repeated pixel decoding/storage per resolved owned skin path and shares
GPU texture objects via weak cache entries; ownership remains with model sets,
and publication clears the GPU cache. Sharing alone did not fix the ABI crash.

Current desktop exe SHA256:
75CB02FD562F2C3DE8F530D4A4AB9C201D4BAAC82337EF8B061FC03848055270
Original level11 rendered frame30 screenshot scrn0002.png inspected:
270ED0AA9079505D75AA9A7F330F2313807BFB0E3D5CE72B0B28746FB0C24C27
Original level13 rendered frame30 screenshot scrn0003.png inspected:
4A0960179A271C0E14F5CA54B798B02CB3A817CA5248F100DA47EE9798DF3AAF
Screenshots remain under mod-controls/screenshots. Per-level camera JSON copied
to level11-models-camera.json and level13-models-camera.json in scratch root.
Logs: rendered-packed-models.stdout/stderr.log and rendered-level13.stdout/stderr.log.
Both show source level admission, capture and zero unresolved emitters at that
view. These were -xl-preview-start -xl-water-physics -mission pmines with explicit
level11/13, no still fixture; briefs skipped. This is rendered startup evidence,
NOT earned progression, combat qualification or human/phone traversal. Observed
private memory about486MB level11 and594MB level13 on this32-bit desktop build;
not a measured phone budget or before/after memory comparison.

Native headless+desktop builds PASS. OOF/ASE CTest2/2 PASS after packing regression.
Full CPU original mission suite PASS after shared skins and ABI fix:
mod-shared-skins-controls.log exit0. ARM64 D1/D2 build PASS:
android-arm64-shared-skins.log exit0. Owned rendered PIDs32092 and21364 stopped
only after exact executable path checks; debugger and build/test sessions all
terminal. Earlier failed preview children exited themselves. No user devices
modified, no commits or APK. Previous screenshot evidence retained.

Next actual gameplay save/load and rendered firing/model gunpoints, full failed
preparation preserving an installed override, GL context recreation, and remaining
collision/lighting/exhaust fidelity checks. Preserve exact ownership/asset hashes
and distinguish direct level startup from the reported hub-warp workflow. Goal
remains active.

## Real loaded-world save/restore probes

Added explicit developer -xl-preview-save-roundtrip probe after StartNewGame.
Requires -route-confirm-user-dir so the fixed save filename is confined to an
explicit disposable profile. Uses existing native state_save_all_sub and
state_restore_all_sub, changes player energy between calls, and verifies mission,
level, energy/flags, position/segment, robot count and model source identity.
The ordinary rendered frame30 capture then checks continuation after restore.
This is controlled native save/load, not human menu interaction or earned travel.

Original level11 PASS with53 robot objects; original level13 PASS with210.
Both post-restore screenshots viewed. Logs rendered-save11.stdout/stderr.log and
rendered-save13.stdout/stderr.log; private saves and camera evidence preserved.
Executable SHA256 2C9992BD90149C9CF2DE81C1AC2B212EA0466EB38589197AC32C75550D028AAB
Level11 screenshot scrn0004.png SHA256
A8314424707F92C5F7DFA2A8C409BFA980FBFD45E6167546524E838459E92DB2
Level11 save level11-model-roundtrip.sav SHA256
3084D6FC40D12D72006183CCFED68ED087ACBE9358F48AFDD6B7CB53C6973C63
Level13 screenshot scrn0005.png SHA256
21A48107BC7FAAAA924F1C8384714C30AA602B42626FD2D6E8E6E4F1E3928730
Level13 save level13-model-roundtrip.sav SHA256
53CAF9094F3E128C76C27E80B76C02ACFC25728A89811761351926AF967A450A
Screenshot paths remain mod-controls/screenshots; preserved saves and camera
JSON reside in scratch root. Owned processes316 and17656 stopped after exact
executable path verification; launch handles terminal. ARM64 D1/D2 build PASS
(android-arm64-save-probe.log). No phone changes or APK.

Extended the consecutive CPU installation check with malformed full level11
preparation while the level2 override remains installed. Restores fixture bytes
and compares native definition bytes, source hashes and live ship pointer.
Its fresh run is recorded below when terminal.

Expanded CPU suite PASS, exit0 (mod-rejected-install-controls.log): rejected full
preparation preserves installed native definitions, model pointer and source
identity, then stock restoration and remaining source/save controls pass.
No active owned processes. Next rendered firing/context recreation and actual
hub-transition regression on owned staging, then scoped formatting, candidate
build/identification and exact-APK validation. Source collision/lighting/exhaust
limits still require resolution or precise supported-boundary documentation.
Goal remains active; no commit or new APK this batch.

## Candidate preparation and refreshed stack

Refreshed origin refs. PR330 remains open/unmerged at b6ed776473a6649134a44bc70baadad8c3dd84cc,
parent PR328 remains open/unmerged at020c322894e56a507d8f4c89d678854e489dd522.
GitHub CLI reports all checks for both heads pass or skipped. Created child
branch xl_pumo_owned_models from PR330. No merges or upstream writes.
Scoped mixed-language formatting PASS after sandbox denied installed formatter
execution; authorized elevated retry completed. Focused ResourceModStore JVM
suite and instrumentation Kotlin compile PASS. Automation catalog PASS; master
catalog initially hit4GiB disk reserve, then PASS after reclaiming space.
Compressed only private original test fixtures with NTFS LZX: HOG SHA unchanged,
all2663 mod file hashes unchanged, about1GiB recovered. No source/bundle deletion.
Moved five owned failed-preview crash dumps from repository root into scratch.

Restarted owned DxxPhoneFog20261102 from repository-local avds on emulator-5556,
PID28660 (start21:55:19 November2). Fresh AVD identity verified; no other devices
connected. Saved13 protected pilot/save/settings hashes and a tar backup as
pre-loading-profile.json/.tar. Emulator remains running for candidate validation.

The old POG runner mandated Vertigo even for original Pumo. It now permits full
PumoMods without Vertigo; historical explicit Vertigo fixtures remain accepted.
When omitted, only the already backed-up mission d2x.ham path is temporarily
removed and restored by existing finally cleanup. This enables a correct
hub-activation regression using Pumo's own definitions. Exact APK validation and
hub activation are still pending; this checkpoint is not a delivered candidate.
