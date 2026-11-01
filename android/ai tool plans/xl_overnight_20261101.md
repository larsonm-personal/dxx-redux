# D2X-XL overnight development

## Objective and cutoff

Continue original Pumo compatibility from the final October 31 handoff until
November 1, 2026 at 10:00 America/Los_Angeles (PST, 18:00 UTC).
Keep authored effects and original mission/app data intact. No autonomous mine
navigation or subagents. Publish no commits or remote changes without explicit
authorization in this session; the handoff's suggested prompt is a template.

## Verified baseline

- Clean cmake checkout fast-forwarded to d66ae0802 after fetching origin
- PR 276 merged as 45faba16c; all seven final-head workflows passed
- Local development branch: xl_overnight_122_bubble_runtime
- Read committed bubbles.md and pumo-interlude.md evidence
- Original emulator-5554 is running; preserve it and use an owned test instance
- Scratch evidence: temp/xl-overnight-20261101 (retention checked)

## Implementation sequence

1. Integrate original bubble image ownership and emitter binding with full
   orientation and compiled water predicates; extend native lifecycle controls
2. Carry queued bubble frames through transparency ordering and consume wiggle
   FX randomness at source-compatible buffer flush
3. Wire the qualified multi-image shader with original textures, blend/depth
   behavior, GL state restoration and resource/context lifetime
4. Extend native and protected Android controls for pixels, water retirement,
   activation, checkpoint restore and context recreation; retain unsupported
   profile rejection until every required consumer is qualified
5. Run scoped formatting, relevant source/GPU/native builds and tests, Android
   build, automation catalogs and serial protected device validation
6. At cutoff stop only owned work, restore protected app data, record actual
   results and remaining blockers in a concise handoff

## Boundaries

Original pmlev3 object 316 has independent camera/lightning blockers. Isolated
bubble validation does not qualify its level or full campaign playability.
Reuse unchanged completed validation and track process handles to termination.

## Resource ownership checkpoint

First implementation adds bubble.tga preparation, four-by-four atlas validation,
prepared-level resource ownership and a bubble profile in the existing native
resource owner. Admission and runtime emitter/rendering remain unchanged.

- Native MSVC build passed without reported warnings
- Existing effect inspection owner passed, including all five image profiles,
  save/mission lifecycle controls and loaded-world source receipt comparisons
- Loaded-world receipt: four cases, 507 segments, 9633 geometry queries
- Scoped mixed-language quality passed with elevated access to clang-format;
  the two initial sandbox launches failed before formatting
- Android build first failed because the sandbox could not access the Gradle
  distribution/cache; elevated retry passed in 2m57s, handle 9386 terminal
- Evidence-only formatter passed, handle 30566 terminal
- Logs and reusable native wrappers: temp/xl-overnight-20261101/122-*

Next implementation must carry a deferred bubble draw through ParticleSceneItem
and the transparency queue. Current quads are built during emitter traversal;
bubble Setup must consume FX RNG only for submitted source buffers, preserving
singleton suppression and mixed smoke/bubble compatibility. The shader requires
smoke on samplers zero/one and bubble on two; depth occupies a fourth texture
unit. Preserve state for all touched texture units and actual context recreation.

At the resource checkpoint Android printed BUILD SUCCESSFUL and exited zero.
Current logs are bounded, and neither emulator has been touched (only
5554 exists). No commits or remote mutations made. The goal remains active until
18:00 UTC; resource foundation is the first small part, not completion.

Useful implementation findings: particle_objects.h variant currently contains
FireEmitter/SmokeEmitter only. particle_scene_items eagerly creates quads before
sorting. transparency_scene.cpp drain_buffered invokes Setup-equivalent work in
its submit callback; this is the appropriate boundary for deferred bubble quad
construction. Preserve world-X wiggle before camera transformation, queued frame
before animation, and live atlas frame after animation. Source Render calls Add
before advancing animation; source Flush suppresses buffers with fewer than two
entries before Setup. Admission currently calls prepare_authored_particle, whose
shape check excludes bubbles. Native binding tests deliberately use subtype 2 as
their malformed case and will need a genuinely unsupported subtype when binding
is extended. Keep admission explicitly closed until renderer/device qualification.

## Emitter work in progress

Full-orientation BubbleEmitter now joins the production variant, using the
compiled water world and existing activation/restore lifetime. Definition
preparation accepts bubble shapes, while admission.cpp explicitly rejects bubbles
pending renderer AND audio qualification. Visible bubbles now retain a deferred
reference until source-buffer submission, never falling through the fire path.

Extended the existing native owner with original object-316 payload on isolated
Neptune geometry: full basis, cadence/jitter, wet/dry retirement, pause, activation
restore, fresh clouds and signature recycling. Old malformed subtype-2 control is
now genuinely unsupported subtype 4. Scoped quality, native build and native test
passed, logs 122-emitter-native-runtime/controls.log (46712 terminal).

Additional source requirement found in objsmoke.cpp DoStaticParticles: bubble
creation links airbubbles.wav, generic sound class, half volume, 256-unit range,
loop bounds -1/-1. Existing named_audio.cpp can prepare external resources and
linked_audio owns spatial loops. Audit release KillObjectSmoke carefully: DBG
destroys effect audio, but the non-DBG body only stops particle life/unbinds its
system. Do not invent stop behavior. Audio must be qualified before admission.

## Deferred queue and multi-image renderer checkpoint

The production transparency buffer now resolves bubble quads at flush, with
queued-frame world-X wiggle followed by camera transform and live atlas UVs.
Mixed smoke/bubble submissions carry image selectors and are not split by image.
Native queue controls passed actual production drain_buffered: ordered mixed
batch, object boundary, two FX draws and singleton suppression, exact transformed
wiggle and live atlas coordinates. Full native owner passed (85151 terminal),
including the new bubble lifecycle test and existing source-world receipts.

The runtime backend now implements the portable multi-image shader, four texture
units (smoke/smoke/bubble/depth), 5/3/5 depths, ONE/SRC_ALPHA blend, dedicated
program/VAO/buffer lifetime and context disposal. ParticleSoftRestore now preserves
four texture units. ParticleTexture::draw routes image-selected batches to this
backend; public draw_multi_particle_quads permits the protected GPU owner to use
retained original resources without opening admission. Bubble scenes request the
smoke dependency, and smoke-only buffers in these scenes also use the multi shader.
restore_particle_objects clears queued references before replacing emitter slots.

- 122-queue-quality and 122-multi-quality passed
- 122-multi-native-build passed, no warnings
- 122-multi-android passed BUILD SUCCESSFUL in 1m22s; 36976 terminal
- Android structured-binding capture warning in particle_objects.cpp predates
  this work (present in 122-resource-android-retry.log); no new warning identified
- Final native rerun passed: 122-multi-native-runtime/controls.log; 70329 terminal
  includes bubble queue/lifecycle and four source-world cases (507/9633)
- Evidence-only formatter 44670 is running; follow it before editing bubbles.md
- New header particle_multi_renderer.h is untracked and must be retained

Owned emulator started via Start-Process Hidden, PID 42276, AVD DxxLightning112,
ANDROID_AVD_HOME=temp/xl-evening-20261031/line-avds, port5556, host GPU, no-window,
no snapshot load/save. Verified boot_completed=1 and both 5554/5556 present.
No installation or device test yet. Original5554 remains untouched. Stop only
owned5556 at cutoff. Logs: temp/xl-overnight-20261101/emulator*.log and emulator.pid.

Next: extend protected test_xl_pog_runtime owner with BubbleGpu/BubbleImage mode,
original-asset hashes/staging and test_xl_bubble_gpu.jsonc. Use new probe include in
pog_probe.cpp after particle_gpu_probe.h, with retained smoke/bubble resources and
pixel/depth/GL-state checks of draw_multi_particle_quads before/after actual EGL
context recreation. Readback and ParticleProbeTarget already provide isolated FBO
and 16x16 color attachment; add a matching depth attachment for soft-depth cases.
Register support script ownership and run both catalogs. Shader equations passed
desktop previously; the new runtime backend still needs actual Android pixel
qualification. Then live bubble scene/lifecycle, sound consumer and admission.

## Protected GPU and source audio checkpoint (06:31 UTC)

All previously listed formatting/build/test handles are terminal. The protected
BubbleGpu owner passed 28/28 steps on owned emulator5556 (retry handle8238 exited
0). Receipt temp/xl-bubble-gpu-android/result.json, run
7bf3da23173e44d2aad5c021bfd225d3: two checks, 774 samples each, 646 visible,
two mixed batches, one actual EGL recreation. Protected before/after hashes
match. First install failed VERSION_DOWNGRADE before testing; the retry uses
install -r -d on the owned debug app, preserving data. Catalogs and Android
build passed. This is backend qualification, not a live bubble-level witness.

Pinned bubble audio source qualification now passes MSVC and GCC, including
2592 creation-parameter comparisons, bound reuse and release disable behavior.
Logs: 122-audio-source-{msvc,linux}-{build,test}.log. Handles71295/73538 exited0.
Scoped source quality passed. Extracted actual release KillObjectSmoke replaces
the old scaffold, and hash-checked audio/piggy headers and loadsounds table supply
class0, addonindex11 and filename11:airbubbles.wav. Release disable does not kill
the sound (debug differs). Original airbubbles.wav extracted from the verified
1.18.77 archive without execution into original-audio/sounds/d2x-xl, hash
539b434ca912a17442017e4f68aad21953c285ba2d816bb5bb7c2d6678f0e5cc.

Next implement named_audio preparation for bubble inventory, creation-time
generic linked owner, and particle_audio_matches in atomic restore validation.
Use existing named_audio_runtime owner with original wave, actual spatial PCM,
disable/re-enable, range, pause, signature deletion and restored phase controls.
Existing bubble_objects_native_case currently has no bubble audio bank; extend
its preparation through the existing owner when linking audio, without bypassing
the real audio consumer or weakening the guard. Then qualify a derived live
scene and ordinary checkpoint on the owned emulator. Admission stays closed.
No source publication authorized; no commit/push performed. Continue until
18:00 UTC (10am PST). Original emulator5554 remains untouched.

## Runtime bubble audio checkpoint

Implemented named_audio bubble bank preparation, creation-time generic linked
owner and particle_audio_matches in atomic trigger restore. New
runtime/bubble_audio_test.h extends named_audio_test production controls. Generated
audio fixtures now include a labelled synthetic airbubbles.wav; optional
BUBBLE_WAVE in the runner hash-checks and overrides it with the original asset.
The runner requires the bubble witness. Portable tests need no private addon.

Native runtime and fixture builds passed, as did scoped quality. Android session
81391 exited0, BUILD SUCCESSFUL3m26s in122-audio-android.log. Test-only diagnostic
and correction followed, so no further Android build is needed for those native
test header changes. An initial audio assertion failed because first normal sync
initialized owner.audio_volume from-1 to32768; all playback fields matched.
Retained logs122-audio-runtime[-diagnostic].log and respective output dirs prove
this. Corrected by priming the normal slider cache before the disable comparison,
without weakening its full-snapshot equality check. Removed temporary JSON dump.

Current native build122-audio-sync-build.log passed (86925 terminal0). Full audio
owner41797 is finishing, output122-audio-runtime-qualified.log and
named-audio-qualified/runtime.log. Both23/40 and27/40 bubble controls passed with
original wave, including actual PCM, activation, release disable, range, pause,
restored phase and signature lifetime. Last output reached final27/40 seismic
test. Follow41797 to terminal before claiming the complete audio owner.
Native effect inspection98147 exited0,122-audio-effects-runtime/controls.log,
including bubble lifecycle, queues, original admission and water-world controls.
No formatter currently live until the evidence-format call following this edit.

Next live scene/checkpoint: existing test_xl_smoke_authored.jsonc and
tests/skybox_fixture.cpp show derived-hub layout and protected existing owner.
Use a distinctly labelled bubble fixture with original pmlev3 object316 payload,
owned original atlases/WAV and water properties; never alter the original HOG.
Ordinary checkpoint needs real derived authored metadata, not merely injecting an
emitter into original hub. Admission is still explicitly closed, pending these
remaining qualification gates. No device test since protected BubbleGpu success.

## Derived live bubble qualification (06:58 UTC, work in progress)

Full audio owner41797 and evidence formatter12803 finished successfully.
Added tests/bubble_fixture.cpp and CMake targetxl_bubble_fixture. It copies the
actual31-byte payload from originalpmlev3 object316 into hub object1, keeps its
position/orientation, changes only segment59's water bit, and writes distinctly
labelled pbubble/pbubdry missions. All unrelated source bytes are checked equal.
Fixture layout27/40. The original HOG remains unchanged. Outputbubble-fixture.
Wet HOG b738f6ad5de4a601efb4d9102a6a5268acebbb43a8be1d2a37a0ed22050697bf;
wet descriptor a4c724eaf38695192ee19b6dd2a0d2ec07903a4d11355e826ed5946509c9e9a2.
Dry HOG0817d2396f68bad8b1851ba18bfc7e873428e0736b807fd185ae4767c8d41cc0;
dry descriptor7bf4d7ba5f4b734ba9c17756b886218c6905236319a68bcbec0f24fb36519a93.

Local admission now provisionally accepts ONLY the exact original point-emitter
payload in27/40, with initial enabled0or1. Other bubble profiles remain rejected.
This is for full integration qualification; do not claim completed support until
the live device checks pass. Generator admission controls pass both wet/dry,
disabled initial state and eleven nearby unsupported payload variations.

New runtime/bubble_lifecycle_probe.h uses bubscene_ operations. It starts either
fixture, checks actual owned resources/linked sound, live BubbleEmitter, activation
and draining, saves/loads ordinary quick checkpoints and compares audio phase plus
authored activation, checks stock isolation. pog_probe exposes the existing live
particle scene counters for these fixtures. New scripts test_xl_bubble_authored
and test_xl_bubble_dry are support scripts of existing test_xl_pog_runtime.
Runner BubbleFixture/BubbleDry options validate hashes and stage original wave
and atlases through protected app-data backup/restore. Wet script90steps includes
new scene readbacks after checkpoints/context; dry scriptrequires observed birth
before disable and retirement. Dry has not run.

Quality passed122-live-quality,122-live-script-quality,122-dry-script-quality.
Both catalogs passed427top-level501support. Nativebuild10356, fixture53282,
MSVC contract122-live-contract, nativeeffect81676 all terminal0.
Android56154 terminal0 BUILD SUCCESSFUL1m55s. No new native warnings.
All these handles and all formatters are terminal.

FIRST WET DEVICE RUN FAILED, preserve evidence. Wrapper122-live-device.ps1,
log122-live-device.log, handle15174 terminal1. Outputtemp/xl-pbubble-android,
run0d26560703064696adf9e91c8e48122f,14/90,scene completion timed out at15seconds.
state.json shows72live bubbles,1system, but scene batches/quads/samples all0,
no scene error. Thus emitters/audio bound, but no qualifying visible batch.
Protected before/after hashes identical. Native state: camera segment59,
position(-196.8923,-0.3725,0.8031), forward(+1,0,0), drifting up1.36.
The view copied from the large smoke plume may miss the small bubble plume.
Do not assume a camera issue: instrument production queue/scene probe to separate
no submitted batches from entirely clipped batches; inspect actual object/particle
positions before changing view. SceneReadback particle_probe_before currently
returns before incrementing counters if all quads are offscreen, so zero counters
are ambiguous. Existing source queue/native tests and isolated GPU pass do not
resolve this. particle_scene_probe.h and transparency_scene.cpp are the relevant
paths; drain_buffered wraps real ParticleTexture::draw with before/after probes.

Dry wrapper122-dry-device.ps1 prepared, uses same APK, but do not run concurrently
with another device test. Owned5556 remains running;5554 untouched. Next retry
should use fresh wrapper log and allow protected owner retention to keep failure.
No commits/pushes. Deadline remains18:00UTC.

## Live scene diagnosis and checkpoint comparison (07:14 UTC)

The first scene failure was the inherited camera view. Engine xl_inspect output
122-bubble-inspection.json shows object1 atx=-13689968/65536=-208.8923; camera
atx=-196.8923 looking+x put the emitter12units behind it. Segment59 extends from
-208.8923 to-188.8923, so looking toward side2 from distance8 stays inside and
faces the emitter. Changed only the scripted view (side0to2), not runtime visuals.
Added requested-probe submitted/clipped counters in transparency_scene.h,
particle_scene_probe.h and pog_probe state, plus one-shot scene geometry logging.
Native and Android diagnostic builds passed; Android11246 exited0 in1m46s.

Retry4683 failed later at30/90 on exact audio checkpoint equality. Live scene
PASSED:1submitted,0clipped,5quads,8266changed pixels. Activation/drain/re-enable
also passed. Run2c3f0278a43e41a2a4371972f8651896; archived all result/state/logcat/
steps/hash files under122-checkpoint-failure (stable runner output is reused,
so preserve receipts manually before retries). First failure's full state remains
in122-live-device.log, even though stable output was overwritten.

Added bounded per-row restore diagnostics. Initial diagnostic build failed on
legacy max macro; fixed with(std::max), quality passed, retry21966 exited0,
Android1m22s. Diagnostic device16716 terminal1, same checkpoint assertion;
archive122-checkpoint-diagnostic-failure. Exact evidence: all11voices identical,
all43linked owners retained, bubble owner identical. Only3other cached spatial
values changed: segment61 pan31to30, segment62 volume4201to4202, ambient object2
volume30044to30048. Test face_view moves the listener while paused, leaving caches
from its previous pose. No audio phase reset occurred. Added xl_audio_sync_objects
in the save probe before AudioSnapshotPause/snapshot to canonicalize the final
controlled pose. Full equality assertion is retained; no production audio change.

Pose formatter12118 terminal0. Android45902 currently running, output
122-checkpoint-pose-android.log; an unrelated Get-Content used android-relative
path and failed before Gradle, but Gradle launched normally. Follow45902 before
installing. Then rerun122-live-device.ps1 with new122-checkpoint-pose-device.log.
Preserve new failure artifacts if any; full90-step qualification still pending.
Dry wrapper122-dry-device.ps1 and its observed-birth script are ready, not run yet.
All earlier builds/formatters/device handles are terminal. No device test live.

## Bubble runtime qualified (07:24 UTC)

Pose build45902 failed because linked_audio.h was included inside the anonymous
probe namespace, shadowing global vms_vector. Moved that include to pog_probe.cpp
global includes. Scoped quality38528 passed; Android retry1518 exited0,
BUILD SUCCESSFUL1m18s in122-checkpoint-pose-android-retry.log.

Wet device59002 passed90/90 in123099ms, runfab51744d5b5460988d06c33abd72d78.
Wrapper122-live-device.ps1, log122-checkpoint-pose-device.log. Complete receipts
archived under122-wet-qualified. Four live scene readbacks pass, including after
ordinary checkpoint, EGL recreation and stock-to-XL restore. Final scene:
1submitted,0clipped,3quads,9349changed pixels; EGLrecreatecount1. Full audio
voices/linked owners equality passed after normal sync at the paused save pose.
Protected before/after hashes identical. No production renderer/audio behavior
was changed during these camera/checkpoint-control corrections.

Dry device62053 passed11/11 in34177ms, rund91edbb33022430b85ef41f6e554c9e2.
Log122-dry-device.log, archived122-dry-qualified. It observed live particles,
disabled emission, then reached zero particles/systems after527ms; its linked
sound remained owned. Protected before/after hashes identical. Both devices use
actual original atlases/WAV and labelled derived27/40 hub geometry. Original
pmlev3 not loaded or qualified. All build/test/formatter/device handles terminal.
Emulator5556 still running and app stopped/restored;5554 untouched.

Updated bubbles.md with completed narrowly scoped qualification and honest failed
receipts. No need to rerun unchanged renderer/source/native controls without a
new reason. Rebuilt xl_inspect against current admission; original Pumo inspection
122-original-current-inspection.json is being generated to identify the next
independent original-level blockers. Inspect that result before choosing work.
Keep original payload admission narrow, no commits/pushes, continue until18:00UTC.

## Next bounded camera foundation

Current original inspection confirms pmlev1 has camera102 plus lightning105;
pmlev2 has cameras206/207/208 and no other blocker families; pmlev3 has cameras
295-299 plus lightning320-322. Bubble316 no longer appears as a blocker.
Object type16 is OBJ_CAMBOT, distinct from transient OBJ_CAMERA6. Source loads
cambot.hxm and normalizes it to CT_CAMERA18 with the addon model. Do not substitute
an invisible object or a stock robot. pmlev1 and pmlev2 cameras have AIB_STILL128;
pmlev3 includes behavior129 and135 as well. Camera triggers establish monitor
bindings at level initialization, not merely an action on ordinary trigger fire.

First camera implementation chunk is pure scheduling, with admission unchanged:
camera_schedule.h mirrors source Ready/Reset and stable oldest-waiter selection.
tests/reference_cameras.cmake extracts actual hash-pinned Ready, Reset and manager
Render bodies; reference_cameras.cpp compares all readiness state and complete
manager frame sequences through the existing source-effect owner. Invisible
cameras still age, death flags gate eligibility, timeout is latched, ties favor
the earlier camera, shadow-map type3 is skipped and only one camera updates per
frame. Source manager restores viewer/cockpit; rendering itself is stubbed in
this scheduling oracle and is not qualified by it.

Next gates: original monitor topology and ownership, source UV alignment,
owned cambot.hxm/model rendering and collision/lifetime, actual view-to-texture
rendering with GL/world-state restoration, native/protected device lifecycle and
ordinary checkpoints. Camera admission stays closed until actual consumers pass.

## Camera scheduler and alignment checkpoint

At about07:38UTC, camera admission still closed. Original addon extracted without
execution from verified1.18.77 installer: original-camera/data/d2x-xl.hog SHA
df69c62aad949f2d92bc1bbea4b597623cd29d0a0f594cdcc3d66568d6ce482a.
New xl_inspect --archive-only uses existing HOG parser; cambot.hxm at1021870,
3994bytes, SHAbdf3055e5a994b842fcbae5780cd5be77f1706cd5f0946caff15cce1cd3e1a28.
Both archive-only and normal14-level inspection repeated successfully with
unchanged report modification times. No model decode/render qualification yet.

Added camera_alignment.h for release float monitor UV path used by renderlib.cpp.
Existing reference_cameras source owner extracts actual Align along with scheduler.
98304 exact UV comparisons pass on GCC/MSVC; includes quads/both triangle diagonals,
rotation/mirror, buffer and resolution dimensions, epsilon, cache and invalidation.
Legacy fixed UVL path not qualified. Source unused quad entries4/5 not compared.
First GCC adapter aggregate assignment failed; explicit field initialization fixed.
Failed log123-camera-alignment-linux-build.log retained. Final Linux retry and MSVC
build/test terminal0; scopedquality69312and73879terminal0. No live childprocesses.
Docs evidence/cameras.md updated; final docformatter58773 passed, all handles terminal.

Next: camera topology/monitor ownership and original model native resource consumer.
Source SetupMonitorFace sets visible before first valid image, selects base vs overlay
from nOvlTex/bmBot, and honors bDoCameras. Source LoadRobotReplacements bAddBots appends
robots/joints/models but retains serialized bitmap/pointer indices; source InitCamBots
normalizes robot/model/control fields. Existing resource_validation.cpp owns per-level
HXM tables and NativeModel copies, but only replacement indices so far. Extend actual
native readers/owners instead of a parallel ad hoc model parser. All original mission
bytes unchanged. Continue until18:00UTC, no commits/pushes, no otheremu changes.

## Camera topology and original model geometry checkpoint

07:45UTC: previous turn made concrete validated progress; goal remains active.
Added camera_bindings.h, reference_camera_bindings.cpp and source extraction from
actual managerCreate/BuildObjTriggerRef/CmpObjTriggers plus existing pinnedquicksort.
258 comparisons pass GCC/MSVC (including99/100capacity), five invalid/overflow
controls rejected. Original14-levelPumo manager bindings matched both toolchains:
pmlev1 one monitor, pmlev2 six monitors (twoeach206/207/208), pmlev3 five monitors.
Actualsource separateentryperface; lasttargetwins objectmap and duplicateface map.
Objectsort has unstableequal-keyswaps, helper preserves. Wallselfmonitor mirror.
Teleporter creation disabled/unqualified. Capacity>100failsclosed. No admission.
Helpernotruntimeconnected; noGPU/orientation/allocationqualificationfromadapter.

Logs123-camera-bindings-{msvc,linux}-_, finalLinuxcapacitylogs use
123-camera-bindings-boundary-linux-_. Original receipts
123-camera-bindings-original-{msvc,linux}.log; MSVCoriginalrun predates extra
capacitycases but sourcebindingcode unchanged, finalMSVCsourceowner passed258.
All handles terminal0:59892,76895,2618,60994. Qualitypassed.

Extended existingreference_hxm.cpp optionalmemberargument (defaultalarlof.hxm)
to use actualaddonHOG cambot.hxm with existinggeometryoracle. GCC/MSVC both pass
originalmodel58 computedcustomradius116018, bothfacingbranches. Existinggeometry
CTestalso passed. Logs123-camera-model-original-{msvc,linux}.log and
123-camera-model-reference-{msvc,linux}-*. Formatter27507terminal0.
This does NOT prove nativeinstallation/collisionradius/renderedappearance.

Next priority is actualnativecameraresource ownership/decoding/install lifecycle.
resource_validation.cpp already owns MissionDefinitions/HxmChanges/NativeModel,
validate_hxm_dependencies uses native robot_info_read_n/polymodel_read readers.
bm_validate_level_hxm validates replacement indices only; sourceaddon appends
robot/joint/model indices but retains bitmap/pointer indices. OriginalcambotHXM
has1robot,serializedrobot38/model58. Do not infer serializedindicesareliveindices.
ObjectDependencies currently checks global N_robot_types/N_polygon_models;
staged appendeddefinitions will need explicitcountviews withoutmutatingglobalstate.
Source InitCamBots normalizes fields afteraddonload. Source AdjustSize defaults
ModelRadius(0); computedcustomradius is slot1, so inspect CPolyModel::Read/SetRad
before deciding whichradiusnativecamerauses. Preserve actualoriginalmodel, no
hidden/stockrobotsubstitute. Remaining monitor viewsetup/render-to-texture,
collision/control/ownership/save/context/device gates stay closed.

Final docs formatter12684 passed, all handles terminal. Further source audit:
CPolyModel::Read(0,1) writes serializedradius toslot1, LoadRobotReplacements then
overwrites slot1 withcomputedSize; Rad(0) fallsbacktoslot1 whenbase0. BUT source
InitCamBots calls AdjustSize BEFORE assigning new camera model to each object.
Originalpmlev2 render_data[0] is0,size399147. CObject::ModelId uses currentpoly
model fornonplayers. Audit fullinitialization ordering (and compileactual
InitCamBots controls) before choosingobjectcollision size; do not assume116018.

## Tranche124 native camera resource checkpoint

At08:02UTC, native resource chunk passed; camera admission remains closed.
resource_validation.cpp extends existing native HXM decoder with camera append
mode (one robot, zero joints, one static model, no simpler/dying/dead variants).
ObjectDependencies now carries explicit staged robot/joint/model counts.
State constructor rebinds pointers and counts; prepare_camera_model stages into
a temporary State, then transfers owned definitions/models only after success.
Constructor automatically prepares when Inventory includes type16; no blocker
removal. Public prepare_camera_model/camera_robot_id/camera_model_id support
native controls and later object binding. install_hxm publishes appended counts.
Constructor accepts either baseline counts or the current owned installed tables
so read-only next-level preparation can precede normal native asset reset.

CameraAddonFile uses existing PhysFS HOG archiver via temporary private mount
**xl_camera_resources**.hog at **xl_camera_resources**. Loosecambot.hxm wins,
then data/d2x-xl.hog or d2x-xl.hog. Exact selectedmission archive/directory or
startupinstallation ownership required; unownedhigherpriorityshadow rejected.
Native readers reused, no parallel formatparser. Mount/filecleanup covered.
Normalizedrobot sourcefields preserve originaln_guns2 (firstattempt wrongly
requiredzero). Addednative camera_resource_test.h under existinghxm_test owner.
hxm.cmake optionalCAMERA_HOG verifiesoriginalhash and stagesprivatecopy.

NativefullHXM PASS handle45260terminal0: originalcameraappend/staging, fivebad
inputs, unownedshadow, threeprepare/reset/installcycles, repeatedinstall, stock
isolation. PriororiginalHXMcombat/projectile/checkpointcontrols alsoPASS.
Logs124-camera-native.log, 124-camera-native/controls.log/reference.log.
Nativebuild124-camera-resource-receipt-native-build.log no newwarnings.
OriginaladdonSHA df69c62aad949f2d92bc1bbea4b597623cd29d0a0f594cdcc3d66568d6ce482a
andApocalypticHOG f0611f1428549373526dfca8ca5a1e1883ace910f4209a938698086d189919db
verifiedunchanged. Wrapper124-camera-native.ps1 containsallrepropaths.

Preserved failures124-camera-native-profile-failure*, receipt-failure*,
diagnostic-failure*. Diagnostic identified stockreceiptbyte15638: appended
Dying_modelnums slot changesunused0 to-1; active stockmodel/bytes unaffected.
hxm_model_receipt now optionalmodel_limit (defaultoldMAX unchanged); camera
controls comparestockactiveprefix andexactappendedreceipt independently.
No productionworkaround forreceiptfailure. InitialMSVCbuild62041terminal0;
firstnativefailure nohandleterminal1;43839and95778terminal1; allarchived.
Formatters85569,68488,80481,79302allterminal0.

Broadernativeeffectowner124-camera-effects.ps1 PASS3019terminal0; logs
124-camera-effects-runtime.log andcontrols.log include bubbleownership,
deferredqueue andwatergeometry plus priorruntimeeffectregressions.
FinalAndroid124-camera-resource-final-android.log BUILD SUCCESSFUL1m25s;
handle82535 still needs final terminal poll at this writing. Priorbuilds22358
and72086terminal0; baselinewarnings only. No devicecameraqualification yet.
Originalbubbledevicechecks from122 retained; owned5556 stillrunning,5554untouched.

Next priority: source InitCamBots objectinitialization ordering/size oracle and
actual native camera objects; then view-to-texture/monitor integration with
world/GL-state restoration, modelrendering/collision, checkpoint andprotected
device qualification. Source AdjustSize occurs before replacing model index;
do not shortcut it to computed camera radius. Complete originalpmlev2 support
faithfully before admission; originalpmlev3 rotation/lightning remainindependent.
No commits/pushes. Continue until18:00UTC. Finaldocsformatter33118 passed;
Android82535terminal0. All process handles terminal at this checkpoint.

## Tranche125 camera object initialization checkpoint

Source camera_object_state.h shared initializer preserves old-model size before
assigning appendedcamera model. Actual InitCamBots/ModelRadius/inlineAdjustSize
extracted in reference_cameras.cmake with additional robot.cpp/object.cpp/aistruct
hashes. reference_camera_objects.cpp compares324states onGCC/MSVC: oldmodels,
base/customfallback, zero/nonpoly, life/control/movement/AImode. Active source
iterator is OBJ_ITERATOR_TYPE=1 with OBJ_LIST_TYPE=1, linkedstatics includecameras;
the unused fallback CStaticObjectIterator::Match omitscameras, do not use it.

XlPreparedResources::initialize_camera_objects prevalidates alloldmodelindices,
usesnativePolygon_models oldradius, updatesonlysourcechangedfields. It preserves
type16, authoredAIbehavior, pose/signature. Newexternxl_initialize_camera_objects
calledfromnormal load_level_robots_file afterpreparedHXM install androbotverify.
Only3guardedlines addedtod2/main/gameseq.c. No MAX_OBJECT_TYPES or CTdispatch
change yet. Camera admission stillclosed; no nativecamera simulation claimed.

NativeexistingHXM owner125-camera-object-hook-native PASS1210terminal0. Includes
actualnormalresource-load hook, twoobjectinvalidoldmodel transaction, priorradius
distinctfromcamera116018, threecycles andall124resource/stock/HXMregressions.
Log125-camera-object-hook-native/controls.log; wrapper .ps1 atscratchroot.
Nativebuild125-camera-object-hook-retry-native-build.log terminal0, no newwarnings.
SourceMSVC58797andGCC89080terminal0, tests125-camera-object-msvc-test.log and
125-camera-object-linux-final-test.log. FinalAndroid15096terminal0,
125-camera-object-hook-android.log BUILD SUCCESSFUL2m11s, baselinewarnings only.
Formatters20591/26887/71753passed. No livehandles atthischeckpoint.

Failures: initialsourceextract helper couldnotfind inline closingdelimiter;
125-camera-object-linux-build.log retained; fixed withhash-pinned regex extraction.
Nativehooktest hadmissingbrace; failed125-camera-object-hook-native-build.log
retained, fixedbeforefinaltest. No productionworkaround. Initialdirectnative
initialization also passed in125-camera-object-native/controls.log.

Next collision/control audit: nativeMAX_OBJECT_TYPES16 excludesCAMBOT16,
OBJ_CAMERA6isdistinct. SourceCollideInit enablescamera withplayer/weapon/fireball/
robot; source dispatch handlesweapon->CollideWeaponAndRobot, camera/player->
CollideRobotAndPlayer, fireballnoop; robot/camera dispatchappearsdefault andneeds
audit. IsStatic() tests AIbehavior AIB_STATIC135, NOT movementtype orstaticlist.
Originalpmlev2 behavior128 thus sourceplayercontact goesrobotbranch. BumpTwoObjects
earlybranch withnonphysicsnonstatic camera reflects theotherphysicsactor via
velocity*(-2*mass), thenApplyForce; doesnotmove camera. Sourceweaponcollision
hasoptionalROBOTINFO(pRobot), allowingCAMBOT whennull. Audit this before reusing
Reduxrobotcollisionpaths (theyassume OBJ_ROBOT andhaveothergameplay effects).
Need actual type/control/render/collision/checkpoint/device qualification before
admission. No commits/pushes; continueuntil18:00UTC. Finaldocsformatter83247
passed; all handles terminal. Scoped git diff --check also passed.

### 126 - Camera controller and collision semantics qualification (in progress)

Collision audit confirms camera weapons cannot blindly reuse native robot hits:
source pointer ROBOTINFO returns null for cameras (weapon effect selection), while
integer ROBOTINFO(id) in ApplyDamageToRobot sees the normalized strength -1 and
Indestructible returns true. Thus damage exits without changing camera shields.
Player bump reflects the moving actor with -2*mass; native generic stationary
bump uses -mass. Static means behavior135, not movement MT_NONE. Dispatch and
native contacts remain unimplemented and admission closed.

Added portable heading controller to existing camera_object_state.h and actual
hash-pinned CCamera::Rotate extraction to existing source owner. It preserves
source t0 (never advanced), integer speed quotient, authored behavior129 gate,
angle reflection and brakes clock. Matrix adapters observe original-orientation
multiply and repair call ordering; they do not qualify actual matrix arithmetic.

126 controller source comparisons PASS on GCC and MSVC: 71,580 states/frames,
34,587 rotations. Actual source Rotate body; matrix adapters qualify only call
ordering and heading, not arithmetic. Reference logs archived as
126-camera-controller-{msvc,linux}-reference.log. MSVC handle40103 terminal0;
GCC41994 terminal1 from shell CRLF redirection after successful build. Corrected
wrapper to LF and ran existing built test:126-camera-controller-linux-retry-test.log
PASS terminal0. No failed comparisons. Scoped formatter26981 terminal0.
Native126-camera-controller-native-build.log terminal0. Android38139 pending.

Further collision audit: source Indestructible uses ROBOTINFO(integer id), sees
normalized camera strength -1. DoAIRobotHit returns for control != CT_AI before
RNG. Source ApplyForce release uses float inverse mass, not native fixed division;
also gates automap/spectator and player speed boost. Gauss camera hit still changes
SKIP_AI_COUNT/rotThrust/PF_USES_THRUST with three source random draws. Preserve
these when connecting actual collision, do not silently substitute robot path.

126 final checkpoint: Android handle38139 terminal0, BUILD SUCCESSFUL in 1m48s
(126-camera-controller-android.log), no warnings reported. Documentation formatter
40297 terminal0; scoped git diff --check passed. All processes terminal. Current
clock08:25UTC; goal remains active until18:00UTC. No original camera admission,
commits, pushes or changes to original mission/addon assets. Next: implement and
qualify actual camera collision/controller dispatch, then monitor view rendering
and native/device checkpoint lifecycle. The portable heading controller still
needs actual native orientation multiplication/repair integration.

### 127 - Native camera/player contact (in progress)

Added a native collision-dispatch hook for the original nonstatic camera/player
pair. It retains impact flash, awareness and sound, indestructible camera health,
and original release fixed/float reflection math. Static behavior135, multiplayer
and modified robot traits remain unqualified and rejected explicitly. No FVI
eligibility or original camera admission has changed; weapon contact remains.
Existing HXM owner now exercises actual ordered dispatch with original installed
camera resources. Existing source owner compiles actual release ApplyForce and
float vector scaling to compare rounding and automap/spectator/speed-boost gates.

127 validation: source MSVC47438 and GCC62974 terminal0. Archived reference logs
127-camera-contact-{msvc,linux}-reference.log:1008 ApplyForce states,75 non-exact
reflection cases. Native39597 failed missing object_is_observer declaration;
added multi.h and formatter16652 passed. Retry native build terminal0 in
127-camera-contact-retry-native-build.log, no new warnings (initial full rebuild
showed existing POrderList/SOrderList warnings). Native owner42261 terminal0,
127-camera-contact-native/controls.log:8 contact cases per each of3 cycles,
original resource/stock/HXM combat/projectile/disk coverage passed. Camera sound
call exists but headless contact test does not independently measure audio.
Android63017 remains live. Added mandatory contact witness to existing HXM owner.

127 final checkpoint: Android63017 terminal0, BUILD SUCCESSFUL in1m55s;
127-camera-contact-android.log reports no warnings. Formatter29900 terminal0.
Final HXM owner with mandatory player-contact witness passed terminal0 in
127-camera-contact-final-native/controls.log. All processes terminal. Camera
admission remains closed, MAX_OBJECT_TYPES still16 and no FVI camera eligibility.
Native player-contact hook is qualified through direct dispatcher only; no live
camera level, controller frame, monitor rendering or camera checkpoint claim.
Next implement original weapon contacts with source effect selection, persistent
history, Gauss state/RNG, indestructible damage, reflection and kill ordering.
Then connect full object/controller/FVI handling and monitor views. No commits.

### 128 - Camera projectile contacts and missile dependency (in progress)

Added direct native primary-projectile camera dispatch preserving null robot
pointer effect selection, indestructibility, harmless reflection gate, persistent
history before parent-signature check, final weapon strength/removal, and Gauss
signed draws. Existing projectile owner gains original-addon camera controls in
an actual admitted Neptune level; synthetic camera only, no original camera
admission. Actual pinned Gauss branch and SRandShort are compiled in source owner.

Missile contact remains explicitly unfinished: original CreateWeaponEffects calls
CreateExplBlast (default effects/shockwaves enabled) and requests missile lightning
for mega/shaker variants. CreateShockwave returns early for weapon type, but blast
still needs its actual render consumer. Splash also needs original contact-point
and target behavior. Runtime rejects these unfinished paths instead of dropping
them or routing through ordinary robot impact effects. Reprogrammed/cheat robot
shooters also remain unqualified. None of this opens camera level admission.

128 critical finding: new native projectile owner FAILS at actual Gauss id32,
damage_radius655360 (10 units). Do not remove Gauss, zero its radius, or bypass
splash to pass. Initial failed folder/log archived as128-camera-weapon-native-profile-failure.
Targeted Error now prints weapon/radius; diagnostic native build terminal0,
diagnostic rerun terminal1 in128-camera-weapon-native/controls.log. Full actual
splash consumer is next requirement, not just missiles. Existing hook is WIP and
admission closed. Tests use contact at camera center currently; source adjustment
would divide by zero there. Change test to real surface contact when implementing
splash and audit degenerate branch explicitly.

GCC9838 and MSVC27307 terminal0:270 source Gauss cases with actual branch and
SRandShort. Native27349 terminal0, no new warnings. Android28446 terminal0,
BUILD SUCCESSFUL2m7s in128-camera-weapon-android.log. It may have compiled before
the diagnostic-only Error wording edit; final build needed after splash changes.
Formatter5215 and13356 terminal0. Archived source receipts128-camera-weapon-
{msvc,linux}-reference.log. Source blast default effects/shockwaves enabled;
weapon CreateShockwave returns early, CreateExplBlast stillrequired. Gauss does
not call missile blast but DOES call splash. Source radial distance/normalization
precise; native generic path uses quick variants and fixed ApplyForce, so do not
claim source splash parity by merely forwarding to native explode_badass_weapon.

128 checkpoint: original HXM/player regression passed terminal0 in
128-camera-player-regression/controls.log after contact helper refactor. Final
formatter96406 terminal0. All handles terminal. Native weapon owner remains
FAILING specifically at Gauss splash gate; do not report weapon contact qualified.
Next turn must implement the real splash requirement, keep original radius and
Gauss test, and rerun complete native owner. Gauss source state comparisons alone
are insufficient. Camera admission stays closed. No commits/pushes.

### 129 - Source splash math correction and native radial consumer (in progress)

Audit found a reference-adapter defect in127/128: pinned maths.h active FixMul
uses double arithmetic with truncation toward zero, not signed right shift.
Camera reflection force construction and Gauss thrust used the latter. Corrected
production and changed the oracle to extract actual math macros and vector
multiplication. Prior receipts do not prove those arithmetic details; rerun needed.

Also correct the128 zero-distance note: source FixDiv(a,0) returns1, so exact-center
splash adjustment produces target+(1,1,1), not division-by-zero failure. Preserve
that convention and test it. New source owner compiles full ExplodeSplashDamageWeapon
and actual radial geometry block with exact source vector math, comparing the
new shared camera_splash geometry. Native Gauss radial path retains source impact
adjustment, precise distance/direction, float force scaling, source rotation rate
and turn calculation using the already qualified XL angle conversion, plus native
damage callbacks. Original Gauss radius unchanged. Main native contact controls
now use independent hit-point storage and exercise actual player splash damage
and force in a controlled empty room; center-hit convention remains covered.

Native splash is still WIP, not admitted. Missile blast/lightning, general flash/
child-projectile splash profiles, full explosion visual lifecycle and robot damage
creation-time semantics still need qualification; do not claim full source combat
parity from geometry tests or native callback reuse.

129 verified checkpoint: corrected GCC75478/MSVC43709 source owners terminal0.
Archived129-camera-splash-{msvc,linux}-reference.log:1008 force states (90 nonexact),
270 Gauss states,80 full splash function calls,1680 radial/visibility cases.
Native41011 failed min/max macro expansion; local undef fixes, formatter4594
terminal0,129-camera-splash-retry-native-build.log PASS no new warnings. Native
projectile129-camera-splash-native/controls.log PASS terminal0 with original Gauss
radius and actual player splash damage/force. Player/HXM regression129-camera-
player-regression/controls.log PASS terminal0, three resource cycles. Android87197
still live at writing. Added explicit maths/vecmat.cpp hash to source owner.

Important scope: radial source geometry and native player force/damage are now
verified; full rotational-response oracle, robot newborn invulnerability, source
explosion moving visual lifecycle, missile blast/lightning and other splash
profiles remain. Do not conflate native damage callback reuse with complete source
ApplyDamageToRobot equivalence. No original camera level admission or gameplay.

129 final: Android87197 terminal0, BUILD SUCCESSFUL1m31s, no warnings in
129-camera-splash-android.log. Documentation/source guard formatter52630 terminal0.
Final MSVC source configure/build/test with added vecmat.cpp hash passed terminal0
in129-camera-splash-final-msvc-{build,test}.log. GCC prior pass reused because only
an explicit same-file hash guard was added afterward. All handles terminal.
Source and native player/weapon owners pass, original addon and mission hashes
unchanged by runners. Camera admission stillclosed. Next independent rotation
oracle/native radial actor controls, robot creation-time semantics and explosion
visual lifecycle; missile blast/lightning and full monitor runtime still required.

## 130: Native splash rotation source comparison

The native production rotation entrypoint now has 27648 comparisons against
actual pinned PhysicsSetRotVelAndSaturate, TurnTowardsVector and ApplyRotForce
bodies, with original vector-to-angle functions, trigonometric tables and fixed
math macros. GCC and MSVC both agree with the exported native receipt. Cases
cover player and original normal/thief/attack robot definitions, physics gates,
morphing, zero/positive mass, zero and positive frame time, AI skip thresholds,
console/automap gates, zero/tiny/axis/oblique/large forces and three forward axes.
Angular velocity, AI skip count and RNG consumption are compared. Existing 24
camera contact cases still exercise the real splash call path with Gauss radius
unchanged. The first MSVC build needed a missing standard string include; the
failure log is retained.

This qualifies the rotation calculation at supplied physics time and normal
slow-motion speed only. It does not establish full simulation step timing,
nearby-robot damage lifecycle, camera movement/controller integration, or monitor
rendering. Original camera admission remains closed. Receipts are under
`temp/xl-overnight-20261101/130-camera-rotation-native/`, with GCC comparison in
`130-camera-rotation-linux-native-comparison.log`.

130 pending final formatted native build and Android build; source owners and
native comparison passed. Next: real nearby-robot radial coverage and source
robot damage creation-time semantics. No commits, no admission changes.

130 final native checkpoint: formatted native build39944 terminal0, rerun of
projectile owner terminal0 including all27648 source rotation comparisons and
24 actual camera contacts. GCC56142 and MSVC source owners terminal0; GCC native
receipt comparison terminal0. Code formatter69773 and docs formatter68123 both
terminal0. No warnings in native/Linux/Android logs at inspection. Android56710
still alive after assembleDebug task; continue polling the same handle, do not
restart. Source owner logs archived as130-camera-rotation-{msvc,linux}-reference.log.

Next source finding: CObject::ApplyDamageToRobot in physics/collide.cpp1362 checks
gameData.timeData.xGame - CreationTime() < I2X(1), before damage. Reset in
objects/object.cpp408 initializes m_xCreationTime=0; objects/createobject.cpp65
SetCreationTime defaults to current game time; Create/Clone paths set it. Native
objects currently have no corresponding general creation timestamp. Need inspect
loaded-object/reset/save semantics before introducing a signature-keyed runtime
sidecar. Do not use weapon laser creation time or bypass newborn immunity. Nearby
robot splash integration can expose other damage callback differences; no full
robot parity claim yet. Deadline remains18:00UTC/10:00PST.

130 Android56710 terminal0, BUILD SUCCESSFUL2m12s. All130 handles terminal; original mission/addon untouched and camera admission closed. Continue robot radial/damage lifecycle work toward18:00UTC.

## 131: Nearby-robot camera splash integration

The existing native projectile owner now adds 240 real radial robot cases in an
isolated room of original Neptune with the original camera addon installed.
Original normal/thief/attack robot definitions, five difficulties, zero/tiny/
three-unit/exact-radius offsets, and healthy/exploding/negative-health/
pending-deletion states cover actual Gauss splash dispatch. Tests check damage,
force and radius gates. The 240 actual radial rotation outputs are appended to
the native receipt and compared with original ApplyRotForce/TurnTowardsVector;
both MSVC and GCC pass all 27888 rows. Thus these are real radial-call-path
rotation checks in addition to checkpoint 130's direct production-function cases.
No original asset or weapon radius was changed. The existing 24 camera contacts,
projectile combat, stock isolation and 14 malformed-save controls also pass.

Scope: actors are created at game time zero and impacted at two seconds. This
qualifies aged, nonboss, noncompanion robot splash behavior, not newborn immunity,
lethal damage, object damage triggers, reactor/mine splash or all robot behavior.
The source still has a one-second creation-time gate missing from the native
camera splash path. Camera admission remains closed. Native/source receipts are
in `temp/xl-overnight-20261101/131-camera-radial-native/`; GCC comparison is
`131-camera-radial-linux-comparison.log`. Only native test and test-owner files
changed, so the successful 130 Android runtime build remains applicable.

131 all handles terminal: native initial42866 and formatted92374 terminal0,
formatter47936 terminal0, native projectile runner terminal0, GCC comparison
terminal0. No new native compiler warnings. The test saves its actor only after
creating the weapon, preserving segment links during cleanup.

Further lifecycle source findings: objects/createobject.cpp168 stamps Initialize;
SetCreationTime65 defaults negative argument to current time. Loaded objects are
zero-cleared by gameio/loadobjects.cpp ReadObjectInfo356. SaveState/LoadState in
objects/objectio.cpp535/337 do NOT serialize m_xCreationTime (only laser/powerup
union creation fields). Savegame.cpp1781 InitObjects(false) releases objects;
AllocObject(requested,false) restores without zeroing. ReleaseObject leaves the
private timestamp intact. Need follow full level reload and initialization
before claiming source save timestamp semantics. Existing Redux XL trailer owner
is runtime/trigger_save.cpp: projectile capture163, validate283, publish414.
Do not infer source time preservation just because our trailer can store it.
Next bounded work: qualify robot damage preconditions and dynamic creation
tracking, including checkpoint/reset semantics, then remaining visual explosion
lifecycle/camera consumers. Deadline remains 18:00 UTC (10am PST).

131 documentation formatter41425 terminal0. All processes terminal. Source SaveGame LoadGameData calls PrepareLevel at1719 before InitObjects(false), so trace that level reset to establish private timestamp origin. Indestructible is strength<=0 (collide.cpp1354); AttacksRobots object.cpp2080 requires robots-hit-robots setting and cheat or reprogramming. These are additional robot damage guards, not covered by the aged player-owned Gauss cases.

## 132: Camera splash robot damage gates and creation time

The camera splash path now applies the source pre-damage guards: valid robot
definition, exploding/negative-health exclusion, strength-based indestructibility,
one-second birth protection, and ordinary robot-killer exclusion except guidebot
kills. The current camera projectile profile excludes robot-hit cheats; runtime
therefore supplies the normal attacks-robots setting. This is a scoped camera
splash consumer change, not a replacement of Redux's general robot damage routine.
Boss/death bookkeeping and object damage triggers still require further work.

The actual prefix of pinned CObject::ApplyDamageToRobot and actual Indestructible
body compile in the existing camera source owner. GCC and MSVC pass 4320 guard
combinations, including the one-tick boundary, future creation times, missing and
indestructible definitions, target state, killer types and guidebot exception.
The adapter terminates at the first boss-bookkeeping branch; it does not claim
qualification of the rest of ApplyDamageToRobot.

A signature-bound object birth table records native obj_create's game time.
Mission/object initialization clears the table; absent loaded identities resolve
to zero. Five guarded lines in d2/main/object.c add the creation/clear hooks.
The source restore semantics are deliberate: gameio/savegame.cpp1719 calls
PrepareLevel; gameio/loadgame.cpp1789 calls LoadLevel; LoadLevel972 calls
CleanupBeforeGame, which clears all object storage at930. Object SaveState and
LoadState in objects/objectio.cpp omit m_xCreationTime. No new save field is
invented for this private timestamp. The native disk-save test creates objects at
seven seconds, saves, passes through a stock level, restores, and confirms zero
private birth times with full projectile histories retained.

Native camera radial controls now cover 960 cases: age zero, one tick before one
second, exactly one second, and two seconds, with the prior robot/difficulty/
distance/state matrix. Newborn robots retain health while receiving the same
force, rotation and AI stun as source. Actual radial outputs plus direct rotation
cases pass 28608 source comparisons under both compilers. Deletion and slot reuse
also check identity-bound birth handling. The remaining 24 camera contacts and
existing projectile/save-malformation/stock controls pass.

Receipts: `temp/xl-overnight-20261101/132-robot-damage-native/`, source owners
`132-robot-damage-{msvc,linux}-reference.log`, and GCC native comparison
`132-robot-damage-linux-native-comparison.log`. Original missions/addon unchanged;
camera admission remains closed. Native timestamp arithmetic uses the engine's
64-bit clock; source comparison covers ordinary fixed-time ranges, not overflow
behavior of the original 32-bit game clock.

132 current handles: source MSVC15598/GCC72077 terminal0, initial native29226
terminal0, formatter17555 terminal0. First Linux wrapper failed only due CRLF;
corrected wrapper then passed, no source failure. Native first runner terminal0
with960 radial cases and disk birth reset; final formatted build14175 running
(includes explicit deletion/reuse assertions), Android73962 running. Source-owner
logs need archiving after this checkpoint. No commits/admission changes.

Next source consumer gaps: full robot nonlethal object-trigger dispatch, lethal
robot callbacks/boss paths, source moving explosion visual lifecycle, missile
blast/lightning. Camera CT18/FVI/runtime monitor rendering and saved manager state
remain unfinished. Generic Redux robot damage has not been globally replaced by
these camera-specific preconditions; do not imply general combat parity.

132 final: formatted native14175 terminal0, repeat projectile owner terminal0
with960 radial cases, explicit deletion/reuse assertions, and disk-reset witness.
GCC native receipt comparison terminal0,28608 source matches. Android73962
terminal0, BUILD SUCCESSFUL2m17s; no C/C++ compiler warnings/errors in inspected
logs (Gradle reports its general deprecated-features notice). Documentation
formatter75099 terminal0. All handles terminal; original assets unchanged.

Next concrete source consumer: effects/fireball.cpp CreateExplosion sets move
extent2*size and move durationlife except morph/powerup-disappearance/afterburner
clips (lines160-164). DoExplosionSequence at693 computes viewer-minus-Origin,
source precise normalization, double elapsed fraction, source fixed vector scale,
and updates Position. Source CreateObject line289 initializes Origin. These are
private fields and absent from objectio SaveState/LoadState. Redux fireball.c
1478 do_explosion_sequence has no such motion; a guarded frame hook plus explicit
camera-impact initialization can implement this consumer while camera admission
remains closed. Need independently compiled source movement block and native
frame/save/reset tests. Runtime static_explosion.cpp is a different authored
particle renderer; do not conflate it with projectile-created OBJ_FIREBALL.

Correction to broad remaining-work phrasing: Redux apply_damage_to_robot already
calls xl_object_trigger_event(robot,1) for surviving player damage atcollide.c1704.
It needs coverage in camera splash rather than a duplicate new dispatch hook.

## 133: Camera impact fireball motion

Camera player-contact flashes, primary-weapon attached flashes and Gauss splash
fireballs now initialize the original viewer-facing explosion motion. A guarded
native do_explosion_sequence hook advances only explicitly initialized XL
fireballs, preserving stock behavior and unrelated effects. Motion keeps the
original creation position, uses twice the initial size as travel extent and the
initial life as duration, and follows the current Viewer on every frame. The
morph, powerup-disappearance and afterburner clips remain excluded exactly as in
source. Death clamps remaining life before applying the final offset.

The existing source owner compiles actual CreateExplosion initialization and the
actual DoExplosionSequence prefix through the movement block, using pinned clip
and render constants and original vector math. Native tests drive real
camera-impact fireballs through do_explosion_sequence with five viewer offsets
and six life values, including zero and negative life. Both GCC and MSVC match
all 720 positions, final life values and death flags. The receipt includes three
excluded clips and an ordinary explicit effect, plus the actual attached and
unattached camera weapon effects. Slot reuse without explicit initialization does
not inherit motion. A disk round trip through a stock level confirms source-style
reset of unsaved motion state while preserving the saved fireball position.

This is motion/lifecycle qualification, not GPU camera-monitor rendering or
complete missile effects. Source private motion fields and origin are omitted
from SaveState/LoadState and cleared by level reload, like private birth time;
no new save payload was invented. Camera admission remains closed. Receipts:
`temp/xl-overnight-20261101/133-explosion-motion-native/` and
`133-explosion-motion-linux-native-comparison.log`. Source owner logs archived as
`133-explosion-motion-{msvc,linux}-reference.log`.

133 verification status: MSVC91779 and GCC57910 source owners terminal0; native
initial11553 terminal0; native projectile runner terminal0,720 source motion
matches plus28608 rotation comparisons and960 robot radial cases. Code
formatter95618/player test formatter89611 terminal0. GCC native motion receipt
comparison terminal0. Final native11791 and Android87189 still running; poll
those handles. Added independent player-contact flash motion assertion in
existing camera_resource_test.h; run133-camera-player-regression.ps1 after final
build. No new compiler warnings in inspected native/GCC/Android logs. No commits,
assets untouched, camera admission closed.

133 final verification: initial HXM player-motion assertion failed because
camera_resource_controls selects addon metadata after an inactive-world reset;
the owner is a resource/stock-isolation fixture, not active XL gameplay. Failure
retained in133-camera-player-stock-fixture-failure. The fixture now explicitly
asserts stationary stock behavior, and the same helper runs against real active
Neptune in the projectile owner. Final logs show xl_motion=0 across three HXM
cycles and xl_motion=1 for the active eight-case player-contact matrix. This
corrects the coverage boundary rather than enabling XL effects in stock worlds.

Final native8178 terminal0; HXM and projectile owners both terminal0. All720
motion and28608 rotation source comparisons pass;960 robot radial cases and disk
birth/motion reset pass. Android87189 terminal0, BUILD SUCCESSFUL1m13s. GCC57910,
all source/native runner handles, code formatters and docs27040 terminal0. No new
compiler warnings. Only tests changed after the successful Android runtime build.

Next independent camera consumer: objects/updateobject.cpp CT_CAMERA dispatches
CObject::RotateCamera -> cameraManager.Rotate(this); render/cameras.cpp705 uses
GetObjectCamera(objectIndex), the last-created monitor entry, then CCamera.Rotate.
Source scheduler/binding/rotation scalar functions are already qualified, but
native controller dispatch and actual matrix composition/repair remain. Either
finish this path next or continue missile/full explosion profiles; retain all
original camera admission blockers until actual render/checkpoint/device gates
pass. Deadline remains18:00UTC/10amPST, no commits or original asset changes.

133 closed checkpoint at09:37UTC: final formatter9749 terminal0; owner rerun with explicit xl_motion=1 and720-sample gates terminal0. All handles terminal. Continue active goal toward18:00UTC; no admission/commit changes.

## 134: Full camera matrix composition and repair

`camera_orientation.h` now supplies the full original-orientation times heading
matrix calculation and CFixMatrix::CheckAndFix/CreateFU repair. It uses original
double/truncate dot products, source sine/cosine math, precise fixed normalization
and the existing source-qualified cross product with negative-fraction floor.
The zero/parallel up-vector fallbacks are preserved, including vertical forward
sign handling. A zero forward vector is rejected rather than propagating an
invalid camera orientation.

A new translation unit in the existing source owner independently compiles actual
FixSinCos, matrix Create/Mul/operator*, CheckAndFix/CreateFU, vector Dot/Normalize/
Cross, FixQuadAdjust, and the complete CCamera::Rotate body. This replaces the
previous matrix-call-only evidence with real arithmetic comparisons while keeping
the older scalar cases. GCC and MSVC pass 72849 comparisons: 57840 heading sweeps
from pitched/banked bases, 15000 full controller frames with speed changes, and
nine degenerate up-vector repairs. Original Pumo contributes 1161 additional
comparisons from all nine authored camera orientations (129,387,645 in the first
three levels). The full original archive run also matches14 levels,3182 object
ends/identities and795 effect payloads. Original HOG SHA256 remains
6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043.

Receipts: `temp/xl-overnight-20261101/134-camera-matrix-{msvc,linux}-original.log`
and source owner build/test logs. Initial configure failure was a regex escape;
initial compile failure exposed the extractor's inability to stop at a one-line
operator body. Both failures are archived. The one-line operator is now extracted
verbatim with an explicit guard. No runtime caller changed in134, so the133 native
and Android checks remain applicable. Native controller dispatch, manager state,
monitor rendering, FVI eligibility and device/save qualification remain incomplete;
original camera admission stays closed.

134 verified: formatter28588 terminal0, GCC69419 terminal0, final formatted MSVC
wrapper terminal0, original GCC runner terminal0, original MSVC44237 terminal0.
All handles terminal. Source owner logs archive after checkpoint. No new compiler
warnings, no commits/assets/admission changes. Reuse133 Android build because134
only adds unused portable orientation code and source-owner tests.

135 intended integration: native camera controller with source camera_bindings
last-monitor map, original captured object orientation, angle/delta/t0 state,
CT_CAMERA18 object_move_one dispatch, and per-frame SDL millisecond clock. Source
CCamera::Setup cameras.cpp119 initializes orient=current object matrix, angle/delta
zero,t0=0, updates owner mapping. CCameraManager::Rotate cameras.cpp705 uses last
GetObjectCamera. updateobject.cpp162/327 is the actual control dispatch (hash
c4f3b9f1cd225f4dda99bb2a4e42af8546bc37ae36cc0bd19091dad6f48b01ac).
Original default speed5000 verified in gameoptions.cpp280/gamedata.cpp2484 and
playerprofile.cpp1536. Native object_move_all starts atobject.c2711; capture one
wall-clock tick per frame rather than drifting per-camera reads. object_move_one
is a public void(object*) at2352. Preview load sets source atpreview.cpp792 then
loads runtime consumers; add controller load near other bindings, but leave all
admission checks closed. Existing object_trigger_load rejects type16; a manager
binding record is not an ordinary fired trigger. Do not just whitelist/drop it.
Use controlled active Neptune test objects and derived binding metadata to verify
native CT dispatch without enabling an unqualified original camera level. Actual
full camera disk save still needs its own admission/type-range and restore audit.

134 documentation formatter49989 terminal0. All handles terminal at09:46UTC; goal stays active toward18:00UTC. Proceed to native camera-controller integration using the verified matrix path.

## 135: Native CT_CAMERA controller integration

The native object frame switch now dispatches CT_CAMERA18 to a controller that
uses the qualified source binding map and orientation math. It stores one entry
per monitor, captures the original object matrix at binding, starts angle/delta/
t0 at zero, and rotates the last monitor associated with each object. The normal
source speed5000 is explicit; no user-facing speed setting has been added. One
SDL millisecond value is captured at the start of object_move_all and shared by
camera updates, matching the source frame-clock model. Still/static behaviors
remain unchanged. Unexpected bound identity/profile changes fail closed.

The existing native projectile owner drives10800 real object_move_one calls with
three original-orientation patterns and behaviors128/129/135, 1200 frames each.
It checks unchanged position, lifetime, type/control and RNG; duplicate-monitor
last binding; transactional invalid target/control/matrix rebinds; and an unbound
camera. The exported native matrices, angle/delta/t0 and brakes match the complete
pinned CCamera::Rotate body under both GCC and MSVC. These are controlled single-
object native frames, not original-level gameplay or a qualification of FVI.

A load-order audit caught that the preview loader returns before native camera
resource initialization. Binding is therefore called from load_level_robots_file
immediately after xl_initialize_camera_objects, using the retained level
inventory; it is not attempted on serialized, uninitialized camera controls.
Mission/object initialization clears controller state. This is not yet a complete
camera checkpoint implementation: type-range restore, source manager rebuild and
monitor render-state restore still need dedicated coverage. Ordinary object
trigger loading still rejects camera records; no admission blocker was removed.

Receipts: `temp/xl-overnight-20261101/135-camera-controller-native/` and
`135-camera-controller-linux-native-comparison.log`; source owner logs archived as
`135-camera-controller-{msvc,linux}-reference.log`. Original assets unchanged.
Camera monitor rendering, FVI collision eligibility and device/save qualification
remain incomplete; original camera admission stays closed.

135 status: initial MSVC source owner terminal0, GCC72595 terminal0, native63479
and formatted52128 terminal0; native owner and GCC native comparison terminal0
with10800 controller/28608 rotation/720 motion source comparisons plus960 radial
robot cases. Formatter1725 and load-order formatter58112 terminal0. First
Android85741 terminal0 BUILD SUCCESSFUL1m58s before binding-order correction.
Final corrected native73933 and Android50525 running at writing. Run both
135-camera-controller-native.ps1 and135-camera-player-regression.ps1 after native
completion. All camera admission blockers remain closed; no commits.

Next concrete work: route type16 camera binding metadata separately from normal
object-trigger actions (source manager consumes it at creation, not a fired action),
then monitor rendering and camera save/type-range behavior. Do not merely drop or
whitelist the trigger. Native controller binder currently requires normalized
OBJ_CAMBOT16/CT_CAMERA18/MT_NONE identities; rendering/FVI support is still absent.
Controller bind is in gameseq load_level_robots_file after camera initialization;
xl_camera_controller_bind_level in preview.cpp owns access to retained inventory.
Native frame clock starts in object_move_all, tests explicitly call the same
begin_frame then object_move_one. Speed5000 matches source defaults. Source clamp
and time-limit profile remains the already-qualified CameraRotation helper.

135 final: corrected native73933 terminal0; native projectile runner and HXM
player/resource regression both terminal0. Native CT_CAMERA10800 exact source
matches remain; corrected Android50525 terminal0 BUILD SUCCESSFUL1m35s. Docs
formatter53172 terminal0. All handles terminal. No compiler warnings/errors in
final native/Android logs. No commits or admission changes.

Important136 caution: do not simply omit camera records from object_trigger_load.
They occupy sorted source trigger indices, which master/enable/disable actions
may reference. Source CTrigger::Operate has no TT_CAMERA case, but pre-switch
flags/one-shot bookkeeping can still matter. Inspect actual Operate and
ExecObjTriggers (trigger.cpp1542) before choosing an inert record representation.
Retain authored indices and checkpoint identities while camera manager consumes
bindings. Camera metadata can be qualified without opening original camera-level
admission; renderer/lifecycle gates remain independent.

## 136 Camera object-trigger identity (2026-11-01)

Pinned main/trigger.cpp CTrigger::Operate rejects OBJ_CAMBOT in its actor gate
before tOperated, one-shot or alternate bookkeeping. ExecObjTriggers and the
object autoplay loop retain records; autoplay sets TF_DISABLED even when Operate
rejects the actor. Camera manager bindings consume the same sorted records.
There is no TT_CAMERA operation case. General type16 actions on non-camera actors
remain unqualified and rejected by the native loader.

The native object-trigger table now retains type16 records attached to actual
camera objects. Bound-state queries preserve their signature and owner rather
than treating a non-teleportable camera as a missing actor. Operate explicitly
rejects camera actors before its operated flag or action dispatch. The existing
source-order quicksort and checkpoint representation remain in use; no records
are dropped or renumbered. Renderer/admission gates are unchanged.

The existing native camera owner checks two equal-key records (including their
source quicksort reversal), seven flag profiles including disabled, one-shot,
alternate and autoplay, save-state restore, reused-signature rejection and a
non-camera-owner negative control. Validation results follow after completion.

136 verification: scoped formatter43959 terminal0, final native build79749
terminal0, existing projectile owner136-camera-trigger-native.ps1 terminal0.
Receipt136-camera-trigger-native/controls.log records7 camera identity profiles,
10800 controller frames,960 radial robot contacts and existing disk-save controls.
Pinned-source comparisons remain exact for controller10800, rotation28608 and
motion720. Android26788 terminal0, BUILD SUCCESSFUL1m31s. Original Neptune and
camera-addon hashes were checked unchanged by the integration owner. No admission
changes, commits or live processes. This qualifies trigger-table identity restore,
not full camera-object disk restore or rendered monitor lifecycle.

Next: audit the source camera manager's save/load rebuild order and native object
serialization type bounds, or begin the actual monitor render consumer. Do not
mistake table identity tests for full camera save qualification. Current loader
accepts type16 only on native camera owners; non-camera TT_CAMERA remains rejected.

## 137 Camera restore source audit (2026-11-01)

Read-only runtime audit found a concrete restore mismatch, so no speculative
serialization change was made. Exact source excerpts and file hashes are in
`temp/xl-overnight-20261101/137-camera-restore-source.txt`.

- savegame.cpp844's orientation replacement applies to OBJ_CAMERA6, not authored
  OBJ_CAMBOT16. Do not reset authored orientation during save.
- PrepareLevel at savegame.cpp1719 precedes saved object restoration1789.
  LoadLevelData loads authored objects and builds geometry (loadobjects.cpp1303);
  triangle-mesh construction creates cameraManager (createmesh.cpp1588).
  CCamera::Setup captures authored orientation and zero angle/delta/t0.
- Source InitObjects(false) releases slots without clearing their complete
  payload. ReleaseObject/FreeObject change attachment links, identity and lists;
  AllocObject(saved_index,false) also leaves the initial behavior byte intact.
  Its attachment writes begin after the first two fix fields in the control
  union and do not overwrite that byte. Source LoadState/SaveState omit CT_CAMERA
  AI payload; InitCamBots(1) normalizes model/control but not individual behavior.
  Thus authored behavior survives in original slots instead of being serialized.
- Redux reset_objects(1), object.c2841, memsets slots. Its state object conversion
  also omits CT_CAMERA AI payload, so the authored camera behavior would become0.
  Existing controller state also carries fresh-level signatures, which need
  validated reconciliation with saved identities. Neither issue is qualified by
  checkpoint136's trigger-table-only restore test.
- Camera manager ReAlign rebuild exists, but its only external caller in this
  pinned source is the render options menu. No savegame cameraManager rebuild
  was found after object restoration. A native restore must preserve the initial
  level orientation as controller base, not bind a new base to saved orientation.

Next implementation: preserve/recover authored camera behavior and controller
base through real native disk restore, validate saved actor identity/profile,
reset scalar rotation as original level setup does, and compare subsequent frames
against the full extracted source Rotate body. Keep original camera admission
closed. Do not add CT_CAMERA to generic AI serialization merely to compensate for
Redux slot clearing. Full monitor/FVI/device qualification remains outstanding.

137 changed evidence only; no runtime changes or fresh builds were necessary.
The previous136 integration and Android results remain the latest compiled state.

## 138 Native camera object restore consumer (2026-11-01)

camera_controller now captures the freshly loaded authored camera slots,
including unbound cameras. Its two-phase restore validates the full camera count,
owner slot, native type/control/movement, nonnegative signature, addon robot/model,
render type, immortal lifetime and segment before publishing anything. It restores
the authored behavior and unsaved MT_NONE physics union, preserves saved position
and orientation, reconciles controller signatures and resets angle/delta/t0.
Controller orientation bases remain those captured from the level. No stock save
format or XL trailer payload was added. The helper participates in trigger_save's
validation phase before any restored event state is published.

The existing native camera controller integration now writes and reads actual
stock object_rw records for nine combinations of original matrix and authored
behavior128/129/135. It calls the production state_object_to_object_rw and
state_object_rw_to_object functions around binary disk I/O and a cleared native
slot. It proves omitted behavior/brakes are zero before the new restore consumer,
checks a malformed control-type rejection without publication, restores changed
saved signatures, preserves saved orientation, and then runs10800 native frames.
The existing full pinned CCamera::Rotate comparison passes for all10800 frames,
including the first frame whose saved orientation differs from the level base.

This is object-record disk integration plus real frame dispatch, not complete
state_restore_all_sub coverage with an authored camera mission. The existing
original-Neptune full save/load regression still runs and passes with zero camera
records. Camera admission remains closed pending full original-level restore,
monitor render lifecycle and collision eligibility. No synthetic camera mission
was substituted for original Pumo gameplay.

138 checks: formatter30292 terminal0; native build51233 terminal0; integration
138-camera-restore-native.ps1 terminal0. Source controller10800, rotation28608,
motion720 comparisons and all existing projectile save controls pass. Android
57691 is still running at this entry; await that handle rather than restarting.

138 final: Android57691 terminal0, BUILD SUCCESSFUL1m28s. All build/test handles
terminal. Original Neptune and camera addon hashes unchanged by integration.
No new compiler warnings were reported. Next work should move toward actual
monitor render/FVI consumers while keeping full original-camera save qualification
explicitly open; do not repeat object-record tests as a substitute for that gate.

## 139 Source-faithful model hitbox bounds (2026-11-01)

FVI audit: simply extending native CollisionResult would expose Redux sphere-only
queries for cameras, while pinned CheckVectorObjectCollision selects hitboxes
under the default collision model and for static actors. Source UseHitbox allows
polyobjects except Gauss/Vulcan weapon models; UseSphere includes nostalgia,
monsterballs, hostages, powerups and mines. No camera collision-table admission
was added. Native FVI still needs actual hitbox construction/transforms/queries.

ModelGeometry now exposes combined index0 and per-submodel1..N bounds, accumulated
offsets and half-extents. The existing extracted CPolyModel::Size and
GetPolyModelMinMax reference now compares every component as well as radius.
The original cambot.hxm model58 matches on MSVC and GCC (radius116018), as does
original Apocalyptic alarlof.hxm model6 (radius325446). Nested offset and both
uniform sort-facing branches pass on both compilers.

The original Apocalyptic comparison caught two differences invisible to the old
rounded-radius test. First, source uses else-if extrema updates, producing maxZ
249035 instead of the mathematical262144 in its combined box. Collision bounds
now retain separate source extrema; the existing bounded custom-radius path is
unchanged. Invalid/inverted source boxes are rejected rather than overflowing.
Second, reversing source sort traversal changes per-submodel box order. The
reader exposes a reversed_sort option, defaultfalse, and both uniform directions
are compared against source. This does not yet qualify arbitrary per-node facing
selection in a live renderer; that remains part of the hitbox integration work.
Initial failure evidence:139-hxm-hitbox-initial-failure.log. No assets rewritten.

139 validation: scoped formatter70480 terminal0; MSVC/GCC xl_hxm_geometry_reference
and both original HXM comparisons terminal0. Native rebuild and existing
139-camera-hitbox-native.ps1 integration terminal0, preserving9 object restores,
10800 source-compared controller frames and the prior collision/save controls.
No new runtime hitbox consumer yet; latest Android package is verified138.
Receipts139-camera-hitbox-{msvc,linux}-test.log and139-_-hitbox-original-_.log.
All processes terminal. Next: build source-derived hitbox vertices/normals and
world transforms for original cambot, then qualify vector/hitbox intersections.
Do not use the new bounds as a sphere or axis-aligned world-box approximation.

## 140 Actual source hitbox construction (2026-11-01)

Added model_hitbox.h, constructing eight ordered vertices and six fixed-point
face normals from the qualified model bounds. Coordinate addition is checked;
inverted bounds and normal arithmetic overflow reject instead of wrapping.
The face topology matches the pinned hitBoxOffsets/hitboxFaceVerts tables.

The HXM source test no longer stubs ComputeHitbox. It extracts the real source
body and both topology arrays, plus actual vecmat Mag/Check/Cross/FixQuadAdjust,
operator subtraction, Normalize/Perp/Normal and pinned math macros. The complete
source path now fills vertices and normals. Every native vertex and face normal
is compared for every combined/submodel box in both sort directions, including
original cambot.hxm and alarlof.hxm. No native helper is used inside the reference
math. Only legacy math-format0 branches remain deliberately unavailable; this is
the already-established default math-format2 profile.

Initial compilation caught a missing CFixVector::Set storage adapter; preserved
140-hitbox-adapter-failure.log. Added that storage assignment and reran both
compilers. Final MSVC/GCC source controls and original camera/Apocalyptic model
comparisons all terminal0. Formatter85633 terminal0. No compiler warnings in
final build logs. No live processes or admission changes. This adds a portable
consumer with independently compiled source coverage, not native FVI integration;
latest runtime integration remains139 and Android package138.

Next world-transform audit: CObject::View(0), object.cpp1898, uses the transpose
of object orientation. TransformHitboxes multiplies that view by each vertex and
adds the requested position, then recomputes normals from transformed corners.
CollisionModel1 transforms box0 AND box1 (nBoxes=1, inclusive loop). Other modes
transform1..nHitboxes. The release branch also caches by model nFrame against the
global object frame, not by object identity: preserve/audit this before claiming
full source TransformHitboxes lifecycle parity. Do not silently select DBG to
hide cache behavior. Neither sphere substitution nor world-axis-aligned boxes
would implement this source path.

## 141 World-space hitbox transforms and release cache (2026-11-01)

ModelHitboxTransform owns one cache per model. It transforms local vertices using
the audited transposed View(0) convention, preserves source double Dot truncation
before position addition, builds the ordered world-space quad faces and recomputes
normals. Combined mode1 updates boxes0 and1 inclusively; other modes update1..N.
A repeated model/frame returns retained geometry even if position, orientation or
collision mode changes. Initial cache frame is explicit (default0 matching the
zero-initialized test storage). Arithmetic overflow is rejected transactionally.

Extended the existing HXM source owner with the real release TransformHitboxes
body (DBG0), actual matrix/vector multiplication, Dot and vector addition. View(0)
is a storage adapter supplying the independently audited orientation transpose;
this does not claim qualification of the complete CObject view-cache subsystem.
The actual source model-frame cache and transformed output storage are exercised.

MSVC/GCC original cambot.hxm and Apocalyptic alarlof.hxm runs each pass648
transform/cache comparisons (including existing synthetic models). Cases cover
identity/cyclic/skewed bases, positive/negative translations, mode0/1/2, repeated
frames, both implicit object position and explicit position override, both source
sort orders, and unchanged boxes excluded by the selected collision mode. All
world quad vertices and normals match exact source output. Both compiler source
ctest owners pass. Formatter79376 terminal0; all builds/tests terminal0; no new
compiler warnings. Receipts141-camera-hitbox-_-test.log and141-_-original-*.log.

No native FVI caller or camera admission change yet. Runtime validation/package
receipts remain139/138 respectively. Next: source CheckVectorHitboxCollision and
its line/quad helpers, including radius and normal selection, then native query
integration. Do not replace transformed quads with world-axis-aligned bounds.

## 142 Source line/quad collision primitive (2026-11-01)

Added model_hitbox_query.h with projected point-in-quad, clamped edge distance,
float line/plane intersection and radius-aware line/quad tests. Matches source
axis selection, fixed multiply truncation, float parallel threshold1e-6,
endpoint tolerance[-0.001,1.001], truncation before adding start position, and
zero-radius endpoint dot rejection. The source does not offset the plane by the
moving radius: it applies radius to distance from the plane hit to the quad.
Misses preserve the caller's output point. Unsupported arithmetic overflow rejects.

The existing HXM source owner extracts the actual PointIsInQuad, DistToQuad,
FindLinePlaneIntersection, FindLineQuadIntersection, FindPointLineIntersection,
float conversion/dot/scale and fixed distance bodies, plus source projection axes.
Added SHA pins for physics/hitbox_collision.cpp506e0d824dc2c1bd87e6cb69d84a021ab1a54773bc42fb09b9e3dda5bb1d84f7
and sphere_collision.cpp499873929ce6606f00b504e59624595e6bee57f9f4e225f53c85ebd0c1a80b60.
Reference functions do not call native query helpers.

MSVC and GCC both pass original camera and Apocalyptic runs, each103680 line/quad
comparisons including synthetic models. Camera run records13716 hits; Apocalyptic 15000. Tests require both hits and misses and compare exact distance, intersection
and unchanged miss output. Coverage includes transformed/skewed faces, translations,
three line axes, transverse offsets, crossing/endpoint/zero-length/receding shapes,
radii0/1/16384/65536, and both source sort directions. Prior648 transform/cache
comparisons and geometry controls remain passing in each original-model run.
Formatter47183 terminal0; all source ctest/build/original-model commands terminal0;
no compiler warnings. Receipts142-camera-query-_-test.log and142-_-original-*.log.

Next: RegisterHit nearest-face/tie behavior, FindLineHitboxIntersection and
CheckVectorHitboxCollision submodel selection, preserving model/frame cache and
position override behavior. The new primitive is not yet native FVI integration
or complete swept object-object collision. Camera admission remains closed;
runtime/Android receipts remain139/138. No live processes or asset changes.

## 143 Complete vector/hitbox selection (2026-11-01)

Added hitbox_vector_query, using the qualified model transform cache and line/quad
primitive. It queries combined box0 in collision mode1 and submodels1..N otherwise,
measures candidate distance from line start, preserves later-face/later-box wins
on equal distances, updates selected model only for an accepted hit, and leaves
caller hit/normal/model outputs unchanged on a miss. This matches the source's
actual ineffective static intersection.Create call rather than assuming that
call initializes a sentinel point.

Reference owner now compiles actual RegisterHit, FindLineHitboxIntersection and
CheckVectorHitboxCollision. Added collision_math.h SHA256
06512749bd4db99f49d55c9ff1155d524b629b3fb5b7fecf6e03c7ba38b2a3a1.
The test runs the full source query body, including TransformHitboxes cache calls,
and compares return distance, point, normal and model index. Source storage uses
named tBox/CModelHitboxList adapters; no native query helpers enter the reference.

MSVC/GCC source ctest and original cambot/Apocalyptic runs all terminal0. Each
original-model run passes38880 vector/hitbox comparisons, plus103680 line/quad and
648 transform/cache comparisons. Camera run18800 vector hits; Apocalyptic19102.
Cases retain multiple radii, ray axes/offsets, modes, frames, original/skewed bases,
position overrides and both sort orders. Both hits and misses required. Formatter
84510 terminal0; no new compiler warnings. Receipts143-camera-query-_-test.log and
143-_-query-original-*.log. No live processes, runtime admission or asset changes.

Next polygon-vs-polygon source requirements from CheckHitboxCollision: transform
moving object at p1 and stationary object at its own position; iterate both model
ranges; filter face pairs by normal dot; average penetrating edges before registering
hits. Its fallback condition tests the LAST nHits value (not nTotalHits), and the
returned model is iModel1 (range start), not loop i. Zero hit distance returns1;
miss returns0. These source details must survive independent comparisons. Complete
that branch before enabling player/modeled-weapon camera queries in native FVI.
Runtime integration and Android package receipts remain139/138 respectively.

## 144 Polygon-vs-polygon hitbox contacts (2026-11-01)

Added polygon hitbox query using ordered opposite-normal face pairs, both edge
penetration directions, average-of-edge-hit points and shared nearest-hit/tie
selection. It deliberately uses the second face normal for both directions,
checks the LAST box-pair count to decide line fallback, returns the source range
start as model index, maps a real zero-distance contact to1 and returns0 on miss.
The moving model is transformed at the swept endpoint; stationary at its own
position. Passing one cache for both models retains source shared-model behavior.

Extended the existing reference owner to compile actual FindQuadQuadIntersectionSub,
FindQuadQuadIntersection, FindHitboxIntersection and CheckHitboxCollision, plus
actual fixed vector division. Model storage supports two model IDs. Tests compare
full return distance, point, normal and model index, including unchanged miss
outputs. Both separate and shared model caches are exercised with identity,
cyclic/skewed orientation, translations, three sweep axes, seven endpoint offsets,
stationary/swept movement and combined/per-submodel collision modes.

Final MSVC/GCC source ctest and original camera/Apocalyptic runs all terminal0.
Each original-model run passes4032 polygon comparisons (camera2436 hits;
Apocalyptic2480),38880 vector queries,103680 line/quad queries and648 transforms.
Both hit and miss witnesses required. Formatter64076 terminal0; no new compiler
warnings. Receipts144-camera-pair-_-test.log and144-_-pair-original-*.log.
All processes terminal. No camera admission or original asset changes.

Next: bind original native model geometry to the qualified model cache and route
camera object queries through these consumers, then exercise actual native FVI.
Do not merely enlarge CollisionResult and retain sphere-only detection. Source
CheckVectorObjectCollision also has a ModelRadius broadphase and RegisterHit side
effects after geometric selection; those require audit during integration.
Static135 contact response remains unqualified even though its geometry can now
be constructed. Runtime integration/Android receipts remain139/138 respectively.

## 145 Native level-owned model hitbox caches (2026-11-01)

XlPreparedResources now provides model_hitboxes for an installed level model. The
cache is lazy and unique per model index, owns qualified source bounds/transforms,
and reads the resource owner's retained payload bytes and header submodel count.
Invalid/uninstalled indices and missing/truncated payloads reject. Reinstall clears
all caches; level/mission resource ownership destroys them naturally. No stock
save format, asset or camera admission change was made.

Existing native camera integration obtains the original addon camera and retail
player model through this production resource API. It runs25 polygon contact
queries with15 hits, verifies per-model cache identity, and performs a real HXM
reinstall followed by a changed-position query at the SAME frame number to prove
stale transformed geometry was discarded. It exports camera-model.bin and
player-model.bin from the owned dependency definitions. New --model mode in the
existing HXM source reference validates those exact bytes; projectile.cmake runs
both when HXM_REFERENCE is supplied. No new top-level test runner was added.

The final native integration and both source compilers pass. Each native payload
comparison includes648 transforms,103680 line/quad,38880 vector and4032 polygon
queries plus bounds/normals. Native camera payload yields18800 vector/2436 polygon
hits; native player18884/2484. Original camera and Neptune hashes remain unchanged
under the existing runner. Native build and integration96331 terminal0; formatter
45710 terminal0. GCC payload receipts145-{camera,player}-native-model-linux.log.
Native receipts145-camera-model-native/ including per-model reference logs.

Source audit: CObject::RegisterHit early-returns for OBJ_CAMBOT, so camera-target
queries do not require its robot/player/reactor critical-hit state. ModelRadius
uses source polyModel.Rad(0) for polygon objects; broadphase integration still needs
that radius contract audited, not a substitution of custom_radius or object.size.
The queried geometry now reaches a real native resource consumer, but FVI still
needs routing, broadphase, object-type table safety and native query receipts.

Initial Android5850 terminal0 BUILD SUCCESSFUL1m57s. Recompilation reported existing
warnings in particle_objects.cpp345 (structured-binding capture C++20 extension)
and skybox_collision_test.h21 (writable string), outside this change. Final Android
52445 is running after the last test-owner update; await this handle. No other
live build/test processes. Full source per-node sort-facing/cache integration and
actual original-camera gameplay admission remain unqualified.

145 final: Android52445 terminal0 BUILD SUCCESSFUL1m23s; all handles terminal.
The installed source CPolyModel::Rad(i) prefers rad[i], falling back to rad[!i],
while custom HXM loading sets Size() into slot1. Audit native model.rad against
that exact preference during broadphase routing. No commits or admission changes.

## 146 - native camera query wrapper and finite-segment broadphase

XlPreparedResources::camera_query now binds installed original camera definitions
to the qualified vector/polygon hitbox consumers. It validates target identity,
control/movement/render type and authored behavior; player and weapon actors are
accepted, while all four source mine IDs reject pending sphere routing. Polygon
actors use retained model.rad (source preferred base radius), nonpolygons use
object.size. Gauss/Vulcan use vector geometry even when rendered as polygons.
The finite-segment broadphase rejects using both actor radii. Hit and normal are
published only on contact. Static-camera geometry is allowed; its unsupported
contact response remains separately blocked.

The extracted fixed-point FindPointLineIntersection and CFixVector::Dist match
245 new closest-point/distance cases under MSVC and GCC, including degenerate,
reversed and endpoint-clamped segments. Existing geometry comparisons still pass.
Native original camera/player resource tests cover 81 wrapper cases and 45 hits,
checking exact distance/point/normal against the already source-qualified inner
queries plus distant segments on both sides with unchanged miss outputs. The
existing full camera/projectile owner and per-native-payload source references
pass. No new runner, admission change or source-asset edit.

An initial test compile exposed the stock vm_set_identity macro's unparenthesized
argument; replaced test setup with vmd_identity_matrix. Failure archived in
146-camera-query-native-build-first-failure.log. Scoped formatter81973 terminal0;
MSVC/native87751 terminal0; GCC source build/test terminal0. Receipts are under
temp/xl-overnight-20261101/146-camera-query*. Android92696 still finishing; await
terminal status before calling this checkpoint complete.

This is the resource-owned native query API, not FVI routing. Remaining work:
active installation ownership, source frame/cache lifecycle, safe object-type
collision table dispatch, actual native traversal integration and qualification.
Full per-node sort-facing selection remains unqualified. Original camera mission
admission stays closed, with no commit/push/merge.

146 final: Android92696 terminal0 BUILD SUCCESSFUL in 1m36s; documentation
formatter21773 terminal0. All checkpoint processes terminal at 11:11 UTC.

## 147 - installed camera query ownership and physics-tick cache bridge

Added xl_camera_query as the C/native bridge to the last successfully installed
XlPreparedResources, rather than assuming the pending prepared-level owner owns
live model tables. Successful HXM installation resets its frame epoch to1 and
claims ownership. Destruction clears only its own association; baseline clear or
capture also invalidates ownership. Older owners are never revived implicitly.
The current-frame C++ overload rejects use after replacement. Explicit-frame
geometry access remains available for existing source-comparison controls.

xl_hitbox_begin_frame advances the installed owner's epoch. The existing camera
begin-frame hook at object_move_all invokes it once per native physics step.
INT32_MAX rollover clears transforms before restarting at1. Pinned source
main/game.cpp GameFrame increments gameData.objData.nFrameCount before each
DoGameFrame physics iteration; 3d/hitbox.cpp caches by that counter. This hook
qualifies normal native object simulation, not source demo playback cache timing.

The existing full native projectile/camera integration passed:75 C-bridge queries
match explicit-frame contacts exactly; same-tick camera pose changes retain cached
geometry; the next camera physics tick refreshes it. A second prepared owner is
installed, the previous owner rejects, the bridge uses the replacement, destruction
does not revive the old owner, and explicit reinstall restores valid queries.
Native83614 terminal0. Scoped formatter79449 terminal0. Android24296 still live
at this entry; await its terminal status. Artifacts147-camera-owner-*.

FVI remains unrouted. Further source audit found ProcessObjectCollision passes
the hitbox point/normal directly to CollideTwoObjects and then UnstickFromObject.
Redux instead recomputes a sphere-weighted contact point. Source unstick also
performs reverse object queries. Those are concrete remaining consumers, alongside
safe collision type dispatch and original model per-node sort-facing fidelity.
No original mission admission, asset, save-format, commit or remote changes.

147 final: Android24296 terminal0 BUILD SUCCESSFUL in 1m37s; documentation
formatter53909 terminal0; scoped diff check passed. All checkpoint handles terminal.

## 148 - source-faithful post-contact unsticking search

Added portable camera_unstick with the pinned CObject::UnstickFromObject search,
including zero-old-velocity direction setup, saved old/new positions, source
fixed-point halving, per-iteration actor position updates when originally unmoved,
and the source continuation condition (at least8 iterations while offset remains,
and beyond8 while the last query still intersects). It accepts the geometry query
as a callback so the exact search can be qualified separately from reverse-query
routing. This header is not yet connected to native physics.

The existing xl_hxm_reference owner extracts the ACTUAL UnstickFromObject body
from physics/physics.cpp, hash2ca8d5c939f31da9d4686637ed3e4305bf89e208a575f2f7f6ea2b4d2e6fab76.
It also extracts source fixed-vector zero/multiply operations. The query adapter
checks source argument direction, actor radius, default collision model and
visibility flag; controlled query-result patterns compare each start/end pair and
all final position/offset state.900 cases include moved/unmoved, stopped/nonzero
velocity, signed/short offsets, three radii, three target positions and five hit
patterns.4828 query steps match, longest22. MSVC/GCC source geometry tests pass.

The first comparison exposed a real port error: treating fixed multiply as a
signed right shift rounded negative products down, while source FixMul/XRound
truncates toward zero. Corrected the portable operation and retained both first
failure logs under148-camera-unstick-*-first-failure.log. Format41822 terminal0;
final adapter-contract check still to be rerun at this entry.

No Android/runtime source changed in148;147 Android verification remains current.
No new top-level runner. Full reverse-query geometry is still outstanding:
UnstickFromObject queries the physical actor as stationary and the camera as the
moving object. CheckVectorObjectCollision can call actor.RegisterHit; unlike a
camera target, a player target updates critical/shield-hit state (criticalhit.cpp).
This side effect and reverse sphere/poly handling must be audited before routing
native gameplay. FVI/contact-point handoff and per-node model sort facing remain
open. Original mission admission stays closed; no commits or remote writes.

148 final: final MSVC and GCC tests terminal0, including reverse-query argument
checks; formatter53542 terminal0. All checkpoint processes terminal at11:23 UTC.

## 149 - outer hitbox query branches and reverse geometry

Added HitboxQueryActor/hitbox_object_query as the shared source hitbox branch,
including finite-segment/model-radius rejection, polygon pairs, stationary polygon
versus moving sphere, and stationary sphere versus moving polygon. The latter
preserves source's v0/v0 zero-length line and moving hitbox position override at
the sweep endpoint; it does not substitute the computed-but-unused radius line.
The installed native camera query now consumes this shared implementation.

Existing xl_hxm_reference extracts actual CheckVectorObjectCollision and
VmLinePointDist. Only the outer function symbol is renamed to avoid the separate
unstick query adapter. Geometry helper bodies remain extracted source. Adapters
select hitbox flags, no mines/co-op/robot attack shrink, model radii, and default
geometry modes1/2; PolyObjPos supplies origin (the intermediate projection is
replaced on actual hits). RegisterHit is a COUNTING adapter, not a qualified
implementation of player damage/shield effects. This distinction is intentional.

MSVC/GCC synthetic tests pass3024 outer comparisons with576 hits and504 registration
calls, plus all previous geometry and unstick tests. Exact returned distances and
successful point/normals match. The visibility flag suppresses registration.
Original native camera/player payload references each include4032 outer cases;
camera gives768 hits and672 calls. Existing native integration11116 terminal0,
including75 C-bridge comparisons and81 wrapper cases. Format70318 terminal0;
GCC2161 terminal0; MSVC command terminal0. Android3671 is still packaging at this
entry; await terminal status. Artifacts149-camera-object-*.

Source observation changes the next action: polygon-pair misses can still reach
RegisterHit because CheckVectorObjectCollision only rejects INT_MAX, while
CheckHitboxCollision returns0 on miss. Thus reverse player handling cannot update
hit state only on positive query results. Its registration argument and side
effects need qualification. Default playerprofile.cpp entry1166 sets nDamageModel
0, so advanced critical RNG/damage is not the default path, but shield hit records
and repair timestamps still update. No original admission, source asset or save
format changes, no new runner, commits or remote writes. FVI/contact-point/unstick
integration and per-node sort-facing fidelity remain outstanding.

149 final: Android3671 terminal0 BUILD SUCCESSFUL in1m35s; documentation
formatter88720 terminal0; scoped diff check passed. All checkpoint handles terminal.

## 150 - default player hit-registration state

Added CameraHitState implementing the pinned default nDamageModel=0 RegisterHit
path: supported object-type/nostalgia gates, repaired timestamp, critical cooldown,
shader-dependent shield duration, normalized size-scaled point, three-slot nearby
hit merge/history and return value. It preserves two source quirks: a merge writes
the matched slot's timestamp but the current slot's point, without advancing;
a new insertion advances the index and returns the next slot's point. Advanced
critical damage/RNG is outside this default profile, not disabled from authored
mission content (source playerprofile.cpp default is0).

The existing HXM reference now extracts the actual CObject::RegisterHit body from
objects/criticalhit.cpp, SHA eafa19e6b881a5370e5ba48ead2c52dcd8ac02a664c42dc9ed58161a371c90e2,
plus source vector averaging. Method symbol is renamed RegisterHitSource so the
outer-query call-count adapter remains separate. EGI_FLAG is default0; advanced
Damage/DamageRate/RandShort adapters throw if reached. Both shield durations500/2000,
nostalgia flags, object types2/4/5/9/16, three radii, critical timestamps, starting
history slots and24-step streams compare returned point and every retained field.
12960 transitions match on MSVC and GCC. Existing900 unstick cases,245 segment
cases and all geometry/outer-query tests remain passing. Formatter31336 terminal0,
MSVC command terminal0, GCC24927 terminal0. Artifacts150-camera-hit-state-*.

No native runtime source changed in150, so149 Android build remains current.
The state header is not yet attached to actor ownership/save/load/rendering.
Registration-after-miss arguments still need routing: source PolyObjPos in
descent.h4337 rotates gameData.modelData.offsets[ModelId] via View(0), then adds
object position. The outer query projects THAT center before a pair query, so
using object origin for RegisterHit on a miss would be wrong. Existing149 tests
qualified actual contacts only and used an origin PolyObjPos adapter; they do not
prove these registration points. This is the next concrete integration dependency.
FVI/contact point/unstick integration and per-node model sort-facing fidelity
remain open. No admission, original asset, save format, commit or remote changes.

150 final: documentation formatter38838 terminal0; scoped diff check passed.
All checkpoint handles are terminal. No new Android/runtime build was needed.

## 151 - rotated model centers and registration arguments

HitboxQueryActor accepts a model-center offset; hitbox_actor_center applies the
qualified View(0) transpose/dot convention and adds object position. The source
owner now extracts actual PolyObjPos from descent.h (SHA
f8c34f569c51541dc4d1e69ae3ac04972401b4c6cad99ac450bbc57a245f83e4), replacing its
origin-only adapter. Identity, axis permutation and nontrivial orientation controls
compare exact rotated centers. Offsets are supplied fixtures here, not inferred
or claimed as the final rendered-model centers.

hitbox_object_query now optionally reports HitboxRegistration (reached, point,
model). It initializes the source center projection, preserves it when a polygon
pair misses, distinguishes early broadphase/vector rejection from reaching the
registration stage, and publishes geometry contact outputs only on a positive
result. Visibility suppression remains caller-owned.3024 synthetic outer cases
compare reachability and exact registration point/model, including misses, against
the extracted outer function. Native camera/player payload tests each cover4032
outer cases. Both compilers and the complete native integration pass. Source hit
state12960 transitions and unstick900 cases continue passing.

Format47029 terminal0; MSVC command terminal0; GCC88129 terminal0; native81713
terminal0. Android18984 is still packaging at this entry; await terminal status.
Artifacts151-camera-registration-*. No new runner or source asset changes.

A material fidelity dependency was found in source render model construction:
3d/pof2model.cpp363 invokes Size(pObj,0) and stores its radius in slot1.
RenderModel::CModel::Size in buildmodel.cpp rewrites hitboxes/offsets and calls
Radius, which computes a center from filtered face vertices. Shift is compiled
to return0, but bounds/center still change. G3BuildModel's low-resolution path
calls BuildFromPOF. Prior classic CPolyModel::Size/source query comparisons remain
valid for that input stage; they do NOT prove final post-build gameplay geometry.
The evidence opening now states this explicitly. Qualifying this actual default
POF build path is the next priority before native collision admission.

FVI/contact/unstick, hit-state ownership/save/visual consumers and monitor effects
remain open. No camera admission, save format, commit or remote changes.

151 final: Android18984 terminal0 BUILD SUCCESSFUL in1m36s; documentation
formatter80482 terminal0; scoped diff check passed. All handles terminal at11:40 UTC.

## 152 - bounded POF render-construction input

Added ModelRenderGeometry for the original low-resolution POF build stream.
It tracks the shared vertex table across branches, snapshots face-used vertices
in source order with fixed-to-float conversion, records each part's parent/offset,
visits SORTNORM+28 then+30 independent of facing, and numbers SUBCALL children by
traversal independently of their animation-angle index. Rod/glow byte extents are
consumed without inventing polygon faces. Counts reject source uint16 overflow;
undefined face vertices, truncation, excessive nesting/recursion, traversal budget
and hitbox-capacity excess reject before use. Runtime does not consume this parser
yet; face geometry/bounds remain pending source emission comparisons.

The existing HXM owner extracts actual CModel::CountPOFModelItems from
3d/pof2model.cpp, hash34f9c3a91872bd3fddcc271571291948c71775e501567b9e6f1271678055a26f,
renaming its class to a minimal count adapter. Counts match synthetic clouds,
face-backed geometry, nested calls, sort records and both exact native-owned
original payloads. Camera:1 part,35 vertices,27 faces,104 face vertices. Player:
1 part,415 vertices,284 faces,1135 face vertices. Undefined-index, truncated-record
and self-recursion controls reject. Both MSVC/GCC source tests and both original
payload comparisons pass; existing geometry/unstick/hit-state checks also pass.
This validates source counts and bounded ingestion, NOT full GetPOFModelItems or
AddPOFFace emission, final bounds, radius/center or gameplay.

Formatter72973 terminal0; MSVC build/test/payload command terminal0; GCC66005
terminal0; GCC original-payload command terminal0. Artifacts152-pof-render-* and
152-{camera,player}-pof-{msvc,linux}.log. No runtime source changed, so151 Android
verification remains current. No new runner, commits, admission or asset edits.

Source audit details for next work: AddPOFFace snapshots m_vertices[index] and
updates min/max only for referenced face vertices. CSubModel::SetMinMax uses
independent comparisons, unlike the legacy min/max walker. BuildFromPOF sorts
faces then calls Size; source Shift returns0. Radius sorts/filter vertices with
uint16 counts, computes a tentative farthest-pair radius, shifts the center and
expands the radius. Its exact iteration bounds and face ordering need comparison;
do not replace it with a mathematically cleaner enclosing sphere. buildmodel.cpp
hash5bf1b3ee660b45e50da2d4516d59665f654f8964ec2738cb7ccd92738e36d7d9.

152 final: documentation formatter92397 terminal0; scoped diff check passed.
All checkpoint processes are terminal.

## 153 - actual source POF face emission and local bounds

The existing HXM source owner now includes reference_pof_render.h storage adapters
and generated extraction of CModel::GetPOFModelItems, CModel::AddPOFFace,
CSubModel::InitMinMax and CSubModel::SetMinMax. These are actual pinned bodies,
not a duplicate test parser. Adapter arrays/colors/bitmap storage let the geometry
path run without a renderer; object color is null, so palette/image-color consumers
are inactive and throw if unexpectedly reached. Face texture/color/UV rendering
is not being qualified by these geometry comparisons.

ModelRenderGeometry now retains per-part float bounds from face-used vertices.
Every emitted coordinate, parent, local offset, face count, defined-vertex count,
and min/max is compared with the source emitter. Synthetic tests include ordinary
faces, parent-and-child faces, shared point tables, point-only/nested/sort records,
and malformed rejection. Both exact original camera/player payloads match on
MSVC and GCC, including104/1135 emitted face vertices respectively. This removes
the152 uncertainty about GetPOFModelItems/AddPOFFace emission; final hierarchical
hitbox construction, radius/center and model-build installation are still open.

Final MSVC and GCC build/test/original-payload commands are terminal0. Scoped
formatter86516 terminal0. Prior initial GCC88909 also terminal0. Artifacts
153-pof-emission-* and153-{camera,player}-pof-{msvc,linux}.log. No native runtime
source changed;151 Android verification remains current. No new test runner,
admission, source asset, save format, commit or remote changes. Next: extract and
compare source CSubModel::Size/CModel::Size/Radius using these qualified emitted
records, preserving floating-point conversion, filtered vertex bounds and cache
behavior instead of using legacy polymodel boxes as a replacement.

153 final: documentation formatter73082 terminal0; scoped diff check passed.
All checkpoint processes are terminal.

## 154 - render-model hierarchical hitbox sizing

ModelRenderGeometry::hitbox_bounds converts face-used float extrema to fixed,
accumulates parent offsets, builds submodel and combined boxes, and preserves
RenderModel::Size's FULL combined span versus half-spans for individual parts.
Face-empty parts and overflowing bounds reject; ingestion may still inspect a
point-only stream, but it cannot become qualified collision geometry.

The existing POF reference extracts actual CSubModel::Size, CModel::Size,
CModel::Shift and ComputeHitbox. Source Shift is compiled to return0. Radius is
an explicit no-op adapter at this boundary: the test proves the hitbox sizing
phase, not the enclosing sphere/final center. Arrays and parent/submodel indices
represent source construction storage; all returned bounds, offsets, size fields,
eight vertices and six normals are compared. Both original camera/player payloads
produce two matched boxes. The hierarchical control produces three matched boxes
for two parts with faces.

An evidence audit caught a fixture coverage gap from153: the three-vertex flat
face incorrectly included a fourth zero index. That extra word acted as EOF,
so the appended child was never reached. Corrected the fixture, added an explicit
two-part/two-face assertion, and reran BOTH compilers and both original payloads.
The prior153 claim of parent-and-child FACE coverage is superseded by this actual
154 proof; its original payload emission checks were unaffected. Preserved the
pre-fix count receipt154-camera-pof-msvc-before-fixture-fix.log.

Final MSVC and GCC build/test/original-payload commands terminal0. Final formatter
90850 terminal0; prior initial GCC1516 terminal0. Receipts154-pof-sizing-* and
154-{camera,player}-pof-{msvc,linux}.log. Existing900 unstick and12960 hit-state
transitions remain passing. No native runtime source changed;151 Android remains
current. No new runner, admission, original asset, save format or remote edits.
Next: qualify actual CModel::Radius/FilterVertices and its cached result behavior,
then bind post-build bounds/centers to the native resource consumer. No commits.

154 final: documentation formatter43186 terminal0; scoped diff check passed.
All checkpoint processes are terminal.

## 155 - source enclosing sphere, center and count cache

ModelRenderGeometry::bounding_sphere now follows the source normal-allocation
radius path from emitted face vertices plus accumulated submodel offsets. It
sorts all coordinates, omits the final sorted entry when filtering duplicates
(the actual source count-1 call), computes the farthest-pair radius, initializes
the center from the fixed combined-box midpoint, shifts it in filtered order,
and expands the radius. Float operation order and fixed conversion are retained.
A4096 unique-vertex comparison budget rejects larger quadratic work. Allocation
failure throws; the source allocation-failure fallback is not qualified here.

The reference now extracts actual CModel::Radius, FilterVertices, CmpVerts and
used CFloatVector3 arithmetic. Its arrays allocate normally. Lexicographic sort
uses a std::sort storage adapter with the extracted comparator; equal coordinate
entries are interchangeable. Faces are grouped by part before radius evaluation,
which sorts their complete position multiset; this does not qualify renderer face
sort order. Source Size is now followed by the real Radius, replacing154's explicit
radius stub. Bounds/vertices/normals continue matching.

Original camera sphere: radius108769, center(33,16350,16284). Original player:
radius297534, center(953,-34355,-16331). Both are exact under MSVC and GCC. The
camera's legacy radius116018 is therefore not the final render-built value.
RenderModelSphereCache preserves source part/face/face-vertex count-only reuse and
two type slots. Controls change a vertex without changing counts, then switch to
an unfilled type slot, matching source cached results rather than silently
recomputing. The no-cache type-1 profile remains the default portable call.

All final MSVC/GCC source tests and exact original-payload comparisons terminal0;
GCC84982 terminal0. Formatter22211 terminal0. Artifacts155-pof-sphere-* and
155-{camera,player}-pof-{msvc,linux}.log. No native runtime source changed, so151
Android build remains current. Next bind render-built bounds/center and source
radius preference into the resource owner, preserving admission closure until
actual native traversal/contact/unstick and remaining consumers are qualified.
Source loadmodeldata.cpp sets custom ordinary POF Type(-1), SetRad(Size(),1);
BuildFromPOF later updates slot1 again, while Rad(0) prefers an existing base slot0.
No new runner, original asset/save changes, commits or remote writes.

155 final: documentation formatter81265 terminal0; scoped diff check passed.
All checkpoint processes are terminal. The uncached profile uses type=-1.

## 156 - native render-built collision geometry

XlPreparedResources collision caches now own ModelRenderGeometry-derived hitbox
transforms AND the qualified sphere/center. The legacy ModelGeometry bounds are
no longer used by model_hitboxes. The original raw owned bytes remain the input.
Camera query descriptors carry final center offsets; radius selection uses the
separately retained mission base radius when nonzero, otherwise the render-built
sphere radius. MissionDefinitions preserves those base radii across replacements
and records zero for appended camera models, keeping source Rad(0) preference.
This does not rewrite the global stock object-size/custom-projectile radius APIs;
those have their existing lifecycle contracts and need separate build-timing audit.

Native integration asserts camera radius108769/center(33,16350,16284), player
radius297534/center(953,-34355,-16331), camera fallback to its built radius and
player base-radius preference. Existing25 model queries/15 hits,75 bridge
comparisons,81 wrapper cases/45 hits and replacement/tick/reinstall checks pass.
The source owner now also runs world transforms, line/vector/polygon and outer
queries using the ACTUAL source POF sizing boxes, after its legacy-stage checks.
Each original payload run now covers1215 transforms,194400 line/quad,72900 vector,
7560 polygon and7560 outer object cases across the combined fixtures/stages.
MSVC and GCC both pass; exact native payload bytes are exported and source-checked
by the existing integration owner.

Native/MSVC49824 terminal0; GCC3418 terminal0; formatter39136 terminal0; scoped
diff check passed. Android99910 is still packaging at this entry; await terminal
status. Receipts156-native-render-geometry-* and156-{camera,player}-pof-linux.log.
The evidence opening now reflects the resolved low-resolution render-build phase.
Source POF traversal uses fixed sort-branch order, so final bounds no longer rely
on choosing a legacy min/max facing branch. High-resolution replacements remain
outside this profile. Next native FVI/contact-point/unstick and hit-state lifecycle
integration; original mission camera admission is still closed. No new runner,
original asset/save-format edits, commits or remote writes.

156 final: Android99910 terminal0; documentation formatter74897 terminal0.
All checkpoint processes are terminal.

## 157: native camera collision traversal

Camera object type16 exceeded the stock MAX_OBJECT_TYPES=16 collision matrix.
The new camera pair dispatch runs before stock indexing: source CollideInit enables
only fireball1, robot2, player4 and weapon5 against cameras, symmetrically. All
non-camera pairs still use the existing table; camera-wall lookup is short-circuited.
Stationary camera traversal now calls the installed source-qualified hitbox query
instead of the stock sphere query. Its winning contact normal occupies hit_wallnorm
and follows the existing recursive normal save/restore alongside the hit point.
No-actor camera queries and moving-camera queries fail explicitly. Robot/fireball
eligibility is retained, but their geometry still rejects through the qualified
resource wrapper rather than silently disabling those source collision pairs.

The existing native projectile integration owner checks all289 pair decisions and
153 direct FVI traversals in an isolated room in real Neptune geometry: player,
Gauss and Vulcan profiles, hit/miss, ignore lists and object-query flag controls.
The33 contacts preserve exact qualified query points and normals. This bounded
room suite does not yet qualify competing contacts across recursive segments.
Native/MSVC27944 terminal0; original camera/player payload reference comparisons
and existing projectile/contact/controller/restore controls passed. Code formatter
96120 and final77520 terminal0; scoped diff check passed. Initial native compile
caught a missing camera_collision.h test include, fixed before the passing run;
failed build log retained with before-include suffix. Android60668 is still live.
Receipts157-camera-fvi-* under the retained overnight artifact root.

This is geometry traversal integration only. Native physics still computes a stock
sphere-weighted contact point before response and does not call source camera
UnstickFromObject. Reverse queries, RegisterHit ownership/save/render lifecycle,
monitor rendering and remaining effects remain open. Original camera admission
stays closed. No new runner, original asset/save-format edits, commits or remote
writes. Next qualify the physics contact point and reverse-query/unstick path.

157 final: Android60668 terminal0, BUILD SUCCESSFUL in1m36s. Documentation
formatter7560 terminal0. All checkpoint processes are terminal. Next physics work
must audit the source CollisionModel/IsStatic branch before changing contact
point selection, including sphere-profile projectiles.

## 158: runtime collision selection correction

The157 follow-up audit found an important qualification gap. Prior geometry
fixtures replace source UseHitbox with actor.polygon and CollisionModel with a
forced1/2 adapter. They qualify the geometry branches, not their runtime selection.
The native camera wrapper also hardcodes mode1. Do not treat139-157 as proof of
source-default camera FVI behavior; the newly connected native route must change
before admission. Existing original camera admission remains closed.

New source extraction in the existing xl_hxm_reference owner runs actual EGIFlag,
CollisionModel, UseHitbox and UseSphere. Network.h and playerprofile.cpp hashes
are pinned along with the already pinned collision source.2016 single-player
profiles cover local settings0/1/2, mission override0/1, nostalgia, seven types,
Gauss/Vulcan exclusions, render/model validity and the supplied IsMine predicate.
The mine predicate adapter is an input, not a newly qualified mine classifier.
Explicit requested CollisionModel0/1/2 and default camera profile assertions pass.

Source playerprofile.cpp1165 sets extraGameInfo[0].nHitboxes=2. EGIFlag returns
that local value in single-player. UseHitbox requires !EGI_FLAG(nHitboxes...), so
it is false at that setting. CollisionModel returns2, but outer query still uses
the sphere branch when neither actor passes UseHitbox. Static behavior does not
override the latter condition. With local setting0 and mission override1, hitboxes
are enabled and the global model is2, not1. CMissionConfig::Init does not reset
m_nCollisionModel; static initial zero and later config loading need their own
lifecycle audit before supporting alternative profiles. No inferred repair of
these source choices is authorized by the fidelity requirement.

MSVC source build/test terminal0, 2016 policy profiles plus existing geometry
suite passed. GCC5006 is live at this entry. Formatter4586 terminal0; scoped diff
check passed. Receipts158-camera-policy-*. No native or Android source changed
in158, so157 build receipts remain applicable but their default-behavior claim is
explicitly withdrawn. Evidence opening corrected. No new top-level runner.

Next159: extract and port source CheckVectorSphereCollisionf and its fixed wrapper
(FLOAT_COLLISION_MATH=1 in collision_math.h49), qualify source-selected outer
queries and bind that default to native FVI. Then process physics contact and
unstick; CollisionModel is global, not a method depending on Gauss/Vulcan type.
Retain forced hitbox fixtures as branch evidence rather than deleting them or
using them to claim default selection. No original assets or admission changes.

158 final: GCC5006 terminal0, 2016 policy profiles and existing geometry suite
passed. Final mixed formatter14366 terminal0; scoped diff check passed. All
checkpoint processes terminal. Native runtime selection correction remains next.

## 159: source-selected default camera sphere queries

Ported source FLOAT_COLLISION_MATH=1 CheckVectorSphereCollisionf and its fixed
wrapper. Fixed endpoints become floats before subtraction; normalization and
projection use source float operations. Strict tangent rejection, zero sweeps,
inside-start return of distance to center, valid zero returns and unchanged miss
outputs are preserved. Source fixed wrapper does not expose the float routine's
mutation of its start argument. The outer source query maps negative misses to0,
publishes nonnegative point/zero normal, and reaches RegisterHit even for zero
results. Actual camera RegisterHit does not store player/robot hit history.

The existing source owner now extracts the actual float vector operations and
sphere functions.14700 comparisons include3996 contacts and3996 outer registration
calls; zero returns are1302 on MSVC and1296 on GCC. These counts differ in the
pinned source too, so this is per-compiler source fidelity, not a cross-platform
bitwise determinism claim. GCC initially exposed contact x=-2 versus source -1 at
case4194. Matching source global sqrt instead of std::sqrt preserves intermediate
precision and fixes exact equality. The diagnostic and pre-fix failure logs are
retained; no test tolerance was added.

Native xl_camera_query now selects this source default sphere branch, passing the
actual FVI radius and using camera object size/origin. Negative-radius semantics
are retained. It validates installed resource ownership/camera identity and rejects
multiplayer. Camera query supports player/weapon actors; other profiles remain
explicitly rejected. Forced branch APIs/tests are renamed camera_hitbox_query,
retaining their earlier geometry/cache evidence without calling them defaults.
The existing real-room FVI checks now assert source sphere selection and preserve
point/zero normal, ignore lists and query flags. First native run passed289 pair
checks,153 traversals and51 contacts; final native27936 still live at this entry.

Final source MSVC and GCC builds/tests terminal0. Code formatters48618/34014
terminal0; scoped diff check passed. Initial Android25407 terminal0 before the
sqrt fix; final Android20965 is live and must be awaited. Receipts159-camera-sphere-*.
No new top-level runner, original asset changes, admission changes or remote writes.
Next: qualify source physics contact-point choice (global CollisionModel=2), reverse
sphere queries and UnstickFromObject, then native hit registration ownership and
restore. Current native physics still uses its stock weighted contact point and
has not yet integrated source unsticking. Original camera admission remains closed.

159 final: native27936 terminal0, 289 pair decisions/153 traversals/51 contacts
passed. Final Android20965 terminal0, BUILD SUCCESSFUL in21s. Documentation
formatter90110 terminal0; scoped diff check passed. All checkpoint processes
terminal. Next source physics contact and reverse sphere query/unstick integration.

## 160: composed default sphere unsticking and hit history

Mapped source physics inputs before native wiring: vOldPos is the pre-query actor
position, vNewPos remains the attempted endpoint, and vOffset is the attempted
movement. Native counterparts are save_pos, new_pos and frame_vec, not the hit
point or distance actually moved. Source ProcessObjectCollision uses the query
point whenever global CollisionModel is nonzero (default2), including Gauss and
Vulcan, and calls UnstickFromObject even when response has killed the projectile.

Added camera_sphere_unstick as the composition of already qualified source
unsticking, default floating sphere query and default hit-state update. The source
reverse query supplies the physical actor as stationary and also supplies that
actor's size as query radius. Therefore the default sphere radius is twice the
physical actor size; camera size is not part of this reverse sphere radius. Its
center and start point are both the physical actor's current position, including
the per-iteration update when old and attempted positions match.

The existing source owner now composes ACTUAL extracted UnstickFromObject,
ReferenceVectorObjectCollision, floating sphere functions and RegisterHitSource.
The existing RegisterHit adapter can opt into the actual source hit-state method;
forced geometry fixtures keep their prior argument-recording mode.576 cases cover
player/weapon actors, moving/stopped old velocity, equal/different attempted
positions, zero/tiny/ordinary sizes and offsets, shield durations500/2000 and
critical cooldown ticks. Both MSVC/GCC pass1920 reverse queries and1728 hit
registration calls with exact final positions, offsets, history slots/timestamps,
shield and repaired times. All reverse results are0 in this default profile;
positive-radius zero results still reach RegisterHit. Do not change these source
semantics into an intuitive collision-avoidance algorithm. Zero-size misses do
not register, and weapon hit-state remains unchanged by RegisterHitSource.

MSVC/GCC commands terminal0; formatter53894 terminal0; scoped diff check passed.
Receipts160-camera-unstick-*. No native runtime files changed, so no new Android
build was required;159 remains the latest native/Android check. This helper is
not wired into do_physics_sim yet. Original camera admission remains closed.

Next161 native physics should use query contact, preserve pre-response velocity,
call this composition using save_pos/new_pos/frame_vec and retain actor hit state
by object identity. Source objectio.cpp514-519 saves damage counters and restores
tRepaired using elapsed SDL ticks (version46+); full hit history/shield times are
not in those fields. Do not assume a sidecar can be discarded on save/restore
without auditing the remaining source lifecycle. SHIELD_EFFECT_TIME in
include/objeffects.h21 is2000 when shaders enabled/available, otherwise500.
Native hit-state ownership/save/render qualification remains required. No new
top-level test runner, original asset changes, admission edits or remote writes.

160 final: documentation formatter51925 terminal0; scoped diff check passed.
All checkpoint processes terminal. Next native physics wiring and hit-state
identity/lifecycle controls, with source save semantics still an explicit gate.

## 161: native camera player physics and hit-state identity

Native camera object contacts now pass FVI's source query point into response,
then invoke the160 sphere unsticking composition using save_pos, attempted
new_pos, frame_vec and pre-response velocity. The stock sphere-weighted contact
calculation remains for other objects. Source FixPosition only reconstructs
velocity after wall/bad-start collisions, so the native camera HIT_OBJECT path
now preserves its reflected response velocity instead of overwriting it from
movement at the end of do_physics_sim. Other stock collision paths are unchanged.

A native per-object hit-state array retains the reverse query's default hit history.
Entries require matching object signature and type. The existing object birth
creation/reset hooks clear slots or the whole array, including mission shutdown
and native object initialization. Getter controls reject signature/type changes;
creation and level-reset controls clear previous state. The current component
uses the qualified shader-enabled source duration2000. Renderer availability
mapping remains an explicit gate; source also supports duration500 without
shaders. Save/restore of source damage/elapsed repaired state is NOT yet wired.

The existing camera/projectile integration owner now runs24 actual do_physics_sim
player frames in real Neptune room geometry: behaviors128/129, three axes, both
directions, and two frame durations. Exact final positions and reflected velocity
match the source-qualified composition. All24 impact flashes spawn at the actual
query point; reverse hit state and identity/reset checks pass. This is an unboosted,
automap-off player profile. Projectile death/persistence/retry frames, competing
contacts across segments and full original gameplay remain separately unqualified.

Source MSVC/GCC suites pass, including576 composed cases/1920 reverse queries and
1728 registrations. Native final25873 terminal0. Formatters69341/10993/31493
terminal0. Initial compile found legacy false macro/int return deduction; callback
now explicitly returns bool. First physics fixture failed its wall-clearance
assertion:7-unit offset plus player radius exceeded the selected room's guaranteed
10-unit clearance. Starting offset5 keeps all guards and tests actual inside-start
unsticking. Failure logs retained with before-bool/before-clearance suffixes.
Initial Android27167 terminal0; final70734 still live at this entry. Receipts
161-camera-physics-*; scoped diff check passed before final documentation.

Next162 qualify primary projectile physics (including dead/persistent behavior),
then source hit-state save/load semantics and renderer policy. Original camera
admission remains closed. No new top-level runner, original asset/save-format
changes, commits or remote writes.

161 final: Android70734 terminal0, BUILD SUCCESSFUL in1m32s. Documentation
formatter41085 terminal0; scoped diff check passed. All checkpoint processes
terminal. Native24 player physics frames are qualified within the stated profile.

## 162: full native primary projectile camera physics

Extended the existing camera integration owner through real do_physics_sim for
laser, Fusion, Gauss and Vulcan.128 frames cover behaviors128/129, both directions,
harmless/non-harmless shots, two strengths and two RNG seeds. The original firing
path creates each projectile; tests retain its definition/size/mass/persistence
and use an explicit axis-aligned, drag-free movement profile in the real room.
Inside-start contacts keep source remaining simulation time unchanged, allowing
an exact expected retry displacement after the160 unsticking composition.

All128 frames pass:112 projectile deaths,16 persistent retries and16 additional
repeated-contact frames. Dead projectiles still receive source unsticking before
termination. Surviving Fusion shots consume strength once, retain one camera in
hit history, and do not repeat awareness, effects or RNG on the next physical
contact. Exact positions, response velocities, impact flash positions/counts,
zero weapon hit-state changes and deletion ownership checks pass. Gauss retains
its shifted splash impact and four draws; persistent history prevents duplicate
response on repeated contact. Existing player24-frame and general projectile
integration controls also pass, including two-robot physical piercing order.

The initial test incorrectly expected every Vulcan hit to create awareness.
Pinned ai/ai.cpp AddAwarenessEvent52/CreateAwarenessEvent75 instead draw a Vulcan
filter and reject rolls>3276; only accepted events consume an agitation draw.
Source hash3bb3a90c4bc3246d7df8fbc3798cce20b2290a15605057c950717e8c4152b946.
The corrected test checks that exact predicate/draw count. Initial seeds1/1234
both filtered with the native RNG, so the branch-coverage assertion failed after
the per-frame checks passed. A bounded raw-RNG seed search now chooses one seed
for each predicate branch before running the frames:16 accepted/16 filtered
Vulcan events pass. Failure logs retained as before-awareness/before-seed-coverage.
No runtime algorithm or test tolerance changed in162.

Final native70281 terminal0. Formatters63469/66037/11829 terminal0. Earlier Android
68241/13630 terminal0; final2628 is live at this entry. Receipts
162-camera-projectile-physics-*. Source geometry/composition code is unchanged,
and the integration owner reuses its existing extracted-source receipts. Scoped
diff check passed before final documentation. No new runner or original asset edits.

Next163 audit and implement camera hit-state save/restore. Source objectio.cpp
saves damage counters plus elapsed repaired ticks; full hit history is omitted.
LoadState itself does not clear those omitted fields. Follow PrepareLevel through
loadgame.cpp930 objects.Clear, InitObjects(false), Alloc(false) and LoadState;
verify the container Clear semantics before claiming reset behavior. Source
CreateObject explicitly clears HitInfo in createobject.cpp343. Native source
renderer-policy mapping and broader camera profiles remain qualification gates.
Original camera admission remains closed; no commits or remote writes.

162 final: Android2628 terminal0, BUILD SUCCESSFUL in1m31s. Documentation
formatter76541 terminal0; scoped diff check passed. All checkpoint processes
terminal. Native128 projectile frames plus16 repeated contacts passed.

## Checkpoint163 - source hit-state save lifecycle

Pinned objectio.cpp writes three default damage counters and elapsed repair ticks;
LoadState reads them and rebases repaired time against the current cached SDL
clock. PrepareLevel clears the object array through the actual CArray::Clear
memset before restoring objects. Hit vectors, hit timestamps, insertion index,
shield time and critical time are omitted from the save and therefore reset.
The default nDamageModel=0 camera contact profile leaves damage counters zero.

The existing HXM source-reference owner now compiles the exact source save/load
field statements and CArray::Clear body. Hash checks additionally pin objectio.cpp,
carray.h, loadgame.cpp and savegame.cpp. CameraHitState gains checked elapsed-time
capture and restore methods; restore validates arithmetic before clearing state.
These methods are not yet connected to the native save trailer.

MSVC and GCC each pass154 save/restore cases, including64 negative rebased repair
timestamps, cleared omitted history, and exact first post-restore contact state.
Signed overflow capture/restore is rejected without partial publication; wraparound
clock profiles are not qualified. Existing geometry, sphere and unsticking source
controls also pass. No compiler warnings in either final build. Initial extraction
failed because inline Clear has an indented closing brace; extraction now uses its
following source separator. Both failure build logs are retained as before-extraction.

MSVC wrapper terminal0; GCC81618 terminal0. Scoped formatters44101 (prior receipt)
and38346 terminal0, final logs report all checks passed. git diff --check passed
apart from existing CRLF normalization notices. Receipts163-camera-hit-save-*.
No Android/runtime behavior changed in this checkpoint; native integration is next.

Clock audit for164: source gameStates.app.nSDLTicks is global cached game time,
not camera-owned time. Native camera_controller currently resets frame_ticks on
clear, load and restore. Resolve clock ownership before rebasing hit state in the
trailer, then extend the existing projectile full-save owner with physics-earned
player history, stock-mission transition, restoration and malformed-state checks.
Source signed time arithmetic remains bounded; advanced damage models and renderer
policy are separate admission gates. Original Pumo camera admission remains closed.

## Checkpoint164 - preserve cached game time across camera lifetime

Native camera_controller no longer resets frame_ticks when cameras are cleared,
loaded or restored. The source uses global gameStates.app.nSDLTicks: gametime.cpp
GetSlowTicks421-424 updates it from SDL, game.cpp554/573 caches it for physics,
and game.cpp948-950 initializes it at game start. Camera/level lifetime does not
own that clock. Redux's existing object_move_all hook updates the cached tick on
every physics frame, including stock missions; this change preserves that value
between frames. It does not claim all source menu/startup clock update sites.

The existing camera integration owner checks clock preservation on all9 authored
camera loads and real object-record restores, plus camera clear with a verified
nonzero clock. Rotation state still resets independently. Native build12610
terminal0; integration wrapper terminal0. Its10800 native controller frames match
actual source, and player24/projectile128 plus16 repeated physical contacts and
existing full projectile disk-save/stock/restore checks pass. No compiler warnings
in the final native build. Scoped formatter47287 terminal0; diff check passed.
Android5566 remains live at this entry after assembleDebug; wait on that handle.
Receipts164-camera-clock-*.

Next165 connect the163 codec to the native save trailer. Capture eligible live
player/weapon owners with slot/type/signature and elapsed repair time, including
untouched owners so full source default-state rebasing is represented. Validate
the complete owner set and bounded arithmetic before publishing any restore.
Native state.c2744 serializes every object record; it does not cull dead-flagged
objects in that loop. Extend the existing projectile integration owner with
physics-earned player hit history and full disk restore through stock. Original
camera full-save remains gated separately: do not substitute a synthetic camera
save for original mission admission. Renderer policy and broader contact profiles
remain unresolved gates. No original asset changes, commits or remote writes.

164 final: Android5566 terminal0, BUILD SUCCESSFUL in1m41s. Documentation
formatter75061 terminal0, all checks passed. Scoped diff check passed. No live
checkpoint processes remain.

## Checkpoint165 - native camera hit-state disk persistence

Schema29 adds required camera_hits rows containing owner, signature, type and
elapsed repair ticks. The complete current player/weapon owner set is captured,
including untouched objects and dead-flagged objects still present in stock
object records. Restore validates sorted complete ownership, type/signature,
integer fields and signed clock arithmetic before any trailer state publication.
A heap-backed prepared table avoids a multi-megabyte stack allocation at the
32768-object XL capacity. Applying resets omitted hit history/shield/critical
state and rebases repaired ticks through the source-qualified163 codec.
No legacy pre-release trailer reader or migration was added.

The existing camera fixture now earns a nonempty player hit history through
real do_physics_sim after the shader-profile cooldown. It then deletes the
synthetic camera. The existing projectile owner saves that player state to disk,
loads stock D2, checks XL hit state is unavailable, and restores the Neptune save
at a later cached tick. A nonzero elapsed interval before saving and a later load
clock are required; the repaired time rebases and all omitted history resets.
This qualifies the hit-state component, not original-camera full-save admission.

The existing malformed-trailer owner now checks25 cases: its14 projectile controls
plus11 camera hit controls covering missing field, wrong owner/signature/type,
overflow, oversized unsigned values, explicit JSON boolean, missing/duplicate
owner, wrong row width and non-array table. Rejections preserve projectile history
and both presence and every field of live camera hit state. Valid recovery passes.
Existing full projectile disk restore, creation-time/explosion reset,24 player
physics frames,128 projectile frames plus16 repeated contacts,10800 controller
source comparisons and source geometry controls remain passing. The source codec
is unchanged from163; source receipts include154 round trips and64 negative rebases.

Initial compile failed because the newly included runtime header imported geometry
after the legacy min macro. A forward declaration keeps geometry out of the public
save interface. Initial disk restore failed at the strict trailer field count;
count21 and schema29 now change together. The first new boolean corruption was
actually integer1 because loadgl.h115 defines true as1; the final fixture explicitly
constructs json::boolean_t, also correcting the older projectile boolean control.
Failure logs retained with before-include, before-schema and before-boolean suffixes.
No runtime acceptance rule was weakened to pass those fixture corrections.

Final native build17621 terminal0; final integration wrapper terminal0. Android
45668 terminal0, BUILD SUCCESSFUL in1m30s; later edits only correct native test
boolean construction. No new compiler warnings in final native/Android logs.
Formatters2918/51417/3759/21729 terminal0. Receipts165-camera-hit-save-*.
No new top-level runner, original asset modifications, commits or remote writes.

Next166 audit source hit-state rendering and renderer-policy mapping, and qualify
the next actual consumer rather than opening admission from save evidence alone.
Current physics uses the shader-enabled source duration2000; source fallback500
and native renderer capability/setting correspondence remain unqualified. Other
camera actor profiles, static135 response, monitor rendering and original camera
full mission saves remain separate gates. Original Pumo camera admission is closed.

## Checkpoint166 - source hit shader scalar envelope

Added camera_hit_render.h for the scalar NeedEffect/SetupHitEffectShader consumer:
three hit timestamps, source size factor1+2/size, hardware versus software size
scaling, cosine/square-root decay, strict1e-6 threshold, inactive inverse-radius
sentinel1e6 and active-slot count. Duration500/2000 and positive physical size are
explicit bounds; signed elapsed overflow is rejected. Future timestamps follow
the source outcome: sqrt of a negative elapsed value produces NaN and the source
threshold comparison fails. The portable code skips those slots without computing
NaN. It does not discard cleared zero timestamps, which source treats as active
within the startup duration even though no hit has been recorded.

Existing HXM reference owner compiles the actual NeedEffect function and the
unchanged SetupHitEffectShader slot loop from pinned render/sphere.cpp. Scalar
wrapper setup reproduces source size scaling. Identity transform adapters permit
the loop to execute; their vectors are not compared and do not qualify view-space
or object-space hit directions. Added source hashes cover sphere.cpp, objeffects.h
and rendershields.cpp. No shader upload, sphere geometry, blending or GPU draw is
claimed by this checkpoint; the portable envelope is not yet wired into runtime.

MSVC and GCC each pass1014 cases,1064 active slots and336 future-time profiles,
with exact float outputs against their compiler's actual source. Inputs include
size1 through INT32_MAX, original camera/player radii, both duration profiles,
both transform scale branches, onset/expiry boundaries and cleared timestamps.
Three invalid-profile cases reject. Existing save, geometry and unsticking source
controls remain passing. No compiler warnings in final builds. MSVC wrapper
terminal0. GCC62944 ran all tests successfully but its appended blank Windows
newline produced a trailing shell error; normalized wrapper to LF and reran to
terminal0. Formatter60651 terminal0. Receipts166-camera-hit-render-*.
No native/Android behavior changed, so165 runtime build receipts remain applicable.

Rendering audit: sphere.cpp632 requires NeedEffect for non-appearing players/robots;
sphere.cpp722 uses the hit shader only with bUseShaders and shader availability.
CreateSphereShader additionally requires per-pixel-lighting capability. The
objeffects.h21 duration macro only checks bUseShaders and shader availability,
so those two feature gates must not be conflated. RenderPlayerShield separately
uses player bWasHit and bOnlyShieldHits; playerprofile.cpp1532 defaults OnlyShieldHits
to1 despite constructor defaults0. Camera contact does not itself mark player
bWasHit through the qualified bounce path. Rendering while other damage is active
and camera-monitor views remain relevant consumers; do not remove the gate.

Next167 qualify shader hit-vector transforms and/or full player-shield gating
against actual source, then connect a faithful native/GPU consumer with render
verification. Do not present the scalar envelope as completed shield rendering.
Original camera admission, static135 response, broader actor contacts, native
renderer-policy correspondence and original mission camera full-save remain closed.

## Checkpoint167 - source hardware hit shader vectors

Extended camera_hit_render.h with the actual hardware-path inverse transpose,
fixed matrix/vector multiply and float normalization sequence. The envelope now
retains per-slot activity so inactive vectors stay exactly zero without evaluating
unused hit points. Float xyz conversion happens after fixed dot truncation; w stays
zero. The inverse preserves source FixMul/Det/FixDiv operation order, including
FixDiv's fixed integer1 result for a zero divisor. Overflow outside bounded signed
source arithmetic rejects instead of relying on undefined integer behavior.

The existing HXM reference owner now extracts actual CFixMatrix::Transpose (the
single-argument in-place swap overload), Det and Inverse, and the exact three matrix
setup statements from SetupHitEffectShader. Its unchanged slot loop returns all
four vector components as well as scalar uniforms. Hardware transforms no longer
use the identity adapter from166. The software branch still has an identity view
adapter and only its scalar outputs are qualified; do not claim software vectors.

MSVC and GCC each pass784 full uniform cases and1029 active vectors with exact
float comparisons, plus all nine entries of seven inverse transposes. Matrices
include identity, axis permutation, signed axes, a perturbed orientation, nonuniform
scale, zero and rank-one singular inputs. Hit vectors include zero, fixed-unit
edges, original player radius and mixed signs; ages cover future, active and expired
slots for500/2000 durations. Two unsafe fixed-arithmetic controls reject. Source
166 envelope1014 cases,163 save lifecycle154 cases and earlier geometry/unsticking
controls remain passing. Both wrappers terminal0; GCC24083 terminal0. No compiler
warnings. Formatter43746 terminal0; receipts167-camera-hit-vectors-*.
No native/Android behavior changed;165 runtime build evidence remains applicable.

Source sphere.cpp647 selects ogl.SetTransform(1) before shader setup. The pinned
ogl_lib.h629-630 setter/getter directly access bUseTransform, supporting the hardware
path selected here. This is branch-selection source evidence, not GPU validation.
The normalization and uniforms are now ready for actual fragment-shader comparison.

Next168 extend the existing offscreen POG GPU owner (pog_gpu_egl.cpp, with existing
particle shader helpers) to compile the original sphere hit shader and a portable
shader, submit source-qualified uniforms and compare framebuffer output. Keep
sphere geometry, native submission/lifecycle, player-shield gating, texture asset
ownership and full monitor rendering explicit follow-on consumers. Original Pumo
camera admission stays closed; no original assets changed, commits or remote writes.

## Checkpoint168 - source hit shader framebuffer comparison

Added portable camera_hit_shader.h with the original three-distance/clamp/max
fragment equation. Explicit hitVertex/hitUV/hitColor varyings replace legacy
built-ins so later renderer integration can supply the same inputs. This is shader
code only; no game draw or admission path is wired. reference_pog.cmake pins and
extracts both original sphere shader arrays directly from render/sphere.cpp.

The existing xl_pog_gpu owner now compiles the original hit vertex/fragment pair
and the portable fragment with a matching local-coordinate vertex bridge. It uses
167 source-qualified uniforms, two sizes/orientations, six ages, isolated slots,
overlap, mixed slots, future timestamps and actual RegisterHit-generated state.
A4x4 RGBA texture exercises independent colors/alpha, interpolated vertex colors,
two primitive depths, model-view rotation/translation/scale and three blend modes.
Alpha/additive factors come from the existing actual-source SetBlendMode extractor.
Every channel of every16x16 framebuffer is compared within one byte. All three
slots need visible witnesses; fully inactive alpha/additive draws must preserve
the background exactly. This does not qualify sphere tessellation or original
shield textures; the test quad is explicitly a fragment-equation control.

Initial standalone target link exposed an existing new particle_renderer dependency
on xl_prepared_particle_image. This asset-free owner has no mission resource owner.
Added an explicit adapter that throws if invoked, matching the owner's other
unsupported-game-service adapters; no fake resource or changed production path.
The complete existing GPU owner runs without invoking it. Link failure log retained
as168-camera-hit-gpu-build-before-owner.log.

Final WSL/EGL software OpenGL run42747 terminal0:3456 hit framebuffer comparisons,
1960 visible cases,624 inactive blended cases and all three slot witnesses passed.
The whole existing GPU owner passed in23.92s, including original particle shader,
lightning blur,25001-quad split, mixed transparency, production atlas/depth/state,
sprite bitmap and real context-recreation controls. No compiler warnings in the
final build. Formatters23702/62349 terminal0. Receipts168-camera-hit-gpu-*.
No top-level runner added. Native/Android165 behavior is unchanged; this is not an
Android GPU receipt. No original mission/assets edited, commits or remote writes.

Next169 qualify actual source sphere geometry/UVs and quality selection. Source
include/sphere.h14 selects SPHERE_TYPE_QUADS; CreateShieldSphere uses the original
shield[0..2] assets and texture quality, and DrawShieldSphere queues through the
transparency renderer unless already in its transparency pass. Native submission,
original textures, player shield gating/glow and final Android/monitor integration
remain necessary. Keep original Pumo camera admission closed.

## Checkpoint169 - actual quad-sphere mesh and UVs

Added shield_sphere.h implementing the release source's cube-to-quad subdivision
and final four-triangle fan per quad. Vertex order and all per-face UV seams are
preserved. Centers average positions and UVs, normalize position only, and receive
the source's additional normalization during intermediate quad splitting. The
final triangle split normalizes corners again and uses the normalized original
quad center. No alternate sphere primitive or tessellation library is substituted.

Release SPHERE_MIN_QUALITY is2, SPHERE_TYPE isQUADS, and texture slider0-3 maps to
sphere qualities2,2,3,4. Cached nonzero quality is retained. Native portable APIs
bound the release profile to texture0-3 and sphere2-4, rejecting unsupported values.
Default playerprofile render quality2 is assigned to textures.nQuality at1839,
therefore the default shield sphere uses quality3 (384 triangles/1152 vertices).

Existing HXM source-reference owner extracts the original CSphereVertex class and
its arithmetic, Normalize, quad ComputeCenter and both Split overloads, Quality,
SetupFaces, both Tesselate overloads, CreateFaces, CreateBuffers and Create. Small
vector-backed buffer adapters supply normal successful allocation. CreateEdgeList
is explicitly stubbed: outline-edge generation is not part of the ordered triangle
position/UV output being qualified. Original sphere.h is additionally hash-pinned.
The actual source Create loop emits the comparison vertices, rather than a copied
reference mesh algorithm. Source float operations remain compiler-local evidence.

MSVC and GCC each pass6048 exact ordered positions/UVs across release qualities2-4,
plus16 cached/default quality combinations and five rejected tessellation bounds.
All previous hit shader, save, geometry and unsticking source controls remain
passing. Both wrappers terminal0; GCC53056 terminal0. No final compiler warnings.
Formatter66492 terminal0 and scoped diff check passed. Receipts169-shield-sphere-*.
No native draw or Android behavior changed;165 runtime and168 shader GPU receipts
retain their scope. No new top-level runner, original asset edits or remote writes.

Next170 connect actual sphere triangles and shader in the existing offscreen owner
and inspect original shield textures/animation/loading. Source addon_bitmaps.cpp
loads forcefield.tga, forcefield2.tga and forcefield3.tga (or debug alpha variant).
RenderPlayerShield uses shield[0]. CSphere::InitSurface animates/binds its frame,
disables depth writes and supplies color; DrawFaces uses interleaved vertices/UVs,
with back then front cull passes for hit effects. Pulse/color, glow/transparency
submission, player-damage gating, renderer policy and original monitor rendering
remain unqualified. Original Pumo camera admission stays closed.

## Checkpoint170: complete shield draw and original forcefield pixels

Extended the existing optional xl_pog_gpu_test owner with --shield-sphere PATH.
No runtime admission, renderer or original mission data changed. Pinned sphere.cpp
now supplies actual CSphere::DrawFaces and CQuadSphere::RenderFaces bodies. Minimal
GL/client-array adapters execute the hit-effect branch, verify BACK/FRONT/BACK cull
order and fail on unexpected pulse/non-hit color paths. Mesh inputs are the exact
source-qualified169 triangle/UV output; this is composition of that prior evidence,
not a second independent tessellator in the GPU reference. Portable submission uses
immediate triangles and the168 fragment port; it is not Android runtime submission.

Extracted textures/d2x-xl/forcefield{,2,3}.tga from the original1.18.77 Windows
installer with7-Zip, without executing it. Each original is524332bytes, raw32-bit
TGA128x1024. Existing extracted CTGA decoder owner matches all524288decodedRGBA
bytes per image. Source CBitmap::CreateFrames uses InitChild(0,i*w,w,w), and InitChild
advances the parent buffer by y*rowSize+x*BPP. Thus the GPU fixtures use eight
consecutive decoded128x128frames. Source Animate selects shield[0]/[1] at10fps;
frame timing and addon loading/missing-image behavior are not qualified here.
Inspected source ogl/ogl_texture.cpp SHA256
97ceec7d0c05661f5945bd198eab8b482cd69ee2cd8899ab09ddd055619e58ec,
2d/bitmap.cpp542b40e7af08883e88884408da811bf346c7d46c26c18c25e909b9efa12e4c21.

Original extracted file SHA256, unchanged after comparison:

- forcefield.tga cd1be56194881034068dfe1e8fdf6bba27c4a4216012304faad2b3e8efc17ca9
- forcefield2.tga991e03bfa634036aff2e98a999043cbd78891f420653c09ed3d61154b06ed146
- forcefield3.tga84482a72b3892911dfd90b36966d29225dc72ee42016ce3a56703395bd157047

Each image passes864whole-sphere comparisons: qualities2/3/4, eightframes,
nearest/linear sampling withGL_CLAMP, ages0/1000/2000, two modelview rotations and
three scene depths. Each has384visible,288fullyoccluded and288expired cases; every
frame has a visibility witness. RGBA differences are bounded by one byte, and
float32 scene depth remains exactly unchanged. Source-selected GL_LEQUAL and
extracted additive blend factors are used. The GL adapters supply a constant color
and null pulse, and exclude glow, negative-alpha GL_MAX and missing-texture paths.
This does not qualify full RenderPlayerShield gating, pulse/color, transparency
queue integration, glow, monitor views, renderer capability policy or Android.

Receipts: temp/xl-overnight-20261101/170-shield-*; final build has no warnings.
The existing asset-free GPU owner is rerun after the final depth/blend adjustment.
No new top-level runner, remote writes, device use or authored asset changes.

Next171 qualify source pulse/player shield timing and color before runtime wiring.
CPulseData::Update owns a function-static CTimeout(25), shared across pulse objects;
do not replace it with independent per-object clocks. Constructor Valid is-1
(default initialized, truthy). The source300ms bWasHit gate differs from the2000ms
shader envelope. These details need extracted-source evidence, not inferred policy.

## Checkpoint171: shared shield pulse clock

Added portable shield_pulse.h with the actual source pulse state and shared25ms
clock behavior. Every pulse caller must share one process-lifetime clock. The
function-static source timeout initializes on the first Update invocation, then
only the first eligible pulse in rendering order advances when it expires. Long
gaps advance by one step, not elapsed-step catch-up. Setup changes the pulse but
does not reset the shared timer. Constructor validity is-1 (truthy), not false;
scale/minimum start at0.5 and speed/direction at0.02. Upper/lower bounds clamp and
reverse only after strict overshoot.

The existing HXM source owner now extracts the complete original CTimeout class,
complete CPulseData class, Setup and Update bodies, supplying only SDL_GetTicks and
a fail-fast unused sleep adapter. timeout.h is pinned at SHA256
426a54d8313ba483ca32e360c7d5dbaa2577f1d5438b8c7df7f68bae9d525ee5;
sphere.cpp/descent.h retain their existing pins. MSVC and GCC each pass12020 exact
scale/validity comparisons across four pulse objects, with2002changes and10018
suppressed updates. Coverage includes source default validity, three used setup
profiles, setup mid-sequence, changing caller order,0/1/24/25/26/500ms intervals,
both clamps/reversals, first-call delay and uint32SDL tick wrap. The tested source
profiles use64-bit time_t: after wrap, its signed elapsed value prevents expiry.
No claim is made for a source build configured with32-bit time_t.

Both build/test wrappers exited0; Linux session35703 terminal0. No compiler
warnings. Scoped formatter56019 terminal0 and diff check passed. Receipts are
171-shield-pulse-* in the existing overnight temp directory. Earlier source
geometry, hit-state, save and shader-input comparisons remain passing in the same
owner. This header is not runtime-wired; no Android/runtime behavior changed, so
165 native/Android and170 GPU receipts retain their prior scope. No original data,
new top-level runner, remote changes or device work.

Next172 extract RenderPlayerShield and qualify its visibility gate, hit reset,
cloak/invulnerability/appearance colors and pulse setup transitions. The source
300ms bWasHit gate and2000ms shader envelope are different responsibilities. Full
runtime submission, glow and monitor views remain gates to original Pumo admission.

## Checkpoint172: player shield decisions and color

Added portable player_shield.h for release RenderPlayerShield decisions and state
transitions. Inputs explicitly represent resolved object-effect, shadow and shield
profile gates, appearance, cloak visibility/fade, invulnerability, shield ratio,
clock and500/2000ms effect duration. Outputs distinguish surface setup from actual
draw and carry unpulsed RGBA. New hits become active at the render tick and set
pulse(0.1,0.5). Active hits expire at300ms and reset pulse(0.02,0.4), before the
only-hits gate. Appearance bypasses that visibility gate and chooses orange;
invulnerability chooses pale orange; ordinary shield is blue. Cloak multiplies
RGB by(fade/31)^2. Existing hit alpha uses double sqrt/cos then float conversion;
shield ratio preserves source fixed-to-float operation order, without clamping.

Existing HXM owner extracts the complete hash-pinned RenderPlayerShield function.
Small object/player/profile/GL adapters record its sphere submission, surface,
stencil and pulse setup calls. CPulseData is the actual171source class, with setup
recording only. Resolved profile macros are supplied inputs; this does not qualify
the upstream profile resolution or GetCloakInfo implementation. It also does not
execute the downstream DrawShieldSphere queue or GPU path. DBG=0/RENDER_HITBOX=0
are the tested release branches.

MSVC and GCC each pass414720decision/color/state comparisons:512boolean profiles,
shadow passes0/1/3, fade0/15/31, hit states-1/0/1, ages-1/0/299/300/2000, shield
values0/50/150 and both effect durations. Each has71712draws,6048surface-only cases,
25920new-hit and10368expiry transitions. Finite colors compare exactly; the source
future-timestamp sqrt produces NaNs and the test compares that classification
explicitly. This does not authorize submitting NaN colors to a runtime renderer.
Source stencil restoration occurs only on drawing paths, leaving the surface-only
early return without StencilOn. This is observed source control flow, not a request
to introduce a native GL-state leak. Portable output contains no GL mutation.

Both final wrappers terminal0, no compiler warnings. Formatters92265/69450 terminal0
and scoped diff check passed. Receipts172-player-shield-* in the existing overnight
temp directory. Source owner retains previous geometry/pulse/hit/save coverage.
No runtime/Android, authored asset, admission, remote or device changes.

Next173 implement the real GL/GLES shield primitive using the qualified mesh,
fragment equation and uniforms, then replace the immediate-mode portable side of
170 with that production primitive and recheck original frames/context lifetime.
Use existing renderer state/resource patterns; preserve actual source cull/depth
and additive behavior. Player event wiring, full scene transforms, addon animation,
glow and monitor views remain separately required. Source bWasHit is set in both
CObject::ApplyDamageToPlayer and CollideWeaponAndPlayer, not generic camera bounce.

## Checkpoint173: production shield GPU primitive

Added runtime/shield_gpu.h: GL/GLES shader/VBO/VAO owner with cached quality2-4 mesh,
source hit uniforms and fragment equation, caller matrix/color/frame texture,
BACK then FRONT passes, LEQUAL, depth writes off and additive blending. It restores
caller program, buffers/VAO, texture units/bindings, blend/depth, cull mode/winding
and relevant enables using the existing state owner plus a cull-state guard.
Expired uniforms skip drawing. Nonfinite colors/matrices/hit vectors/radii and
unsupported geometry or texture mapping reject before GL mutation. Ordinary
resource discard/reprepare is covered; real context loss remains next174.

The existing original-shield GPU owner now calls this production primitive instead
of immediate-mode portable triangles. It compares both the original GL_CLAMP
texture and a portable transparent-border texture against extracted source draw
functions/shaders. All three originals pass2592comparisons each(7776total): eight
frames, qualities2/3/4, nearest/linear/trilinear filtering, ages0/1000/2000, two
rotations and three scene depths. Per original there are576visible,432occluded and
432expired scene cases; each compares two production texture paths. Every RGBA
channel stays within one byte; depth is unchanged. Deliberately different caller
texture unit, winding/cull, depth/write and blend factors restore exactly. Discard
and reprepare at frame4 also passes for each mesh quality.

Sampling investigation retained strict tolerances and failure receipts:

- Padded nearest at UV=1 initially selected transparent storage; source GL_CLAMP
  selects the final image texel. The padded nearest-only path now clamps to that
  texel center after interpolation.
- Padded RGBA8 linear filtering differed by two bytes, matching the precision issue
  previously observed for lightning. Optional RGBA32F border storage passes the
  original one-byte limit. Shared copy_particle_border_texture retains RGBA8 as its
  default for existing consumers. Its new explicit storage option is bounded to
  RGBA8/RGBA32F. This shield path requires future GLES render/filter capability
  verification and a resource owner; no admission change follows from desktop GL.
- Clamping filtered UVs before texture lookup changed mip derivatives at sphere
  seams (trilinear149vs151). Filtered UVs now retain original derivatives; only
  nearest padded sampling uses the endpoint clamp. The source shader's default
  texture expression remains unchanged through an optional shader-coordinate macro.

Final Linux wrapper21305 terminal0, no compiler warnings. Existing asset-free GPU
owner passed23.90s, including168hit shader and previous particle/lightning/sprite/
context controls. Scoped formatting and diff checks passed. Receipts173-shield-gpu-*;
failed before-border/nearest/float/derivative logs are retained. Android build
receipt173-shield-gpu-android-build.log covers the shared helper default path;
the new shield class is not yet included in Android runtime code. No claim of
Android shield drawing, full scene transforms/submission, glow or monitor views.
Original files remain read-only and original Pumo camera admission stays closed.

Next174 extend the existing protected lightning/POG GPU owner for actual GLES
shield compilation/drawing and floating-point border filtering, including context
recreation. Existing test_xl_lightning_gpu.jsonc is subordinate to
test_xl_pog_runtime; extend it and its owner rather than adding a runner. Source
pixels remain desktop reference evidence; device probes must distinguish their
own scope. Then add resource ownership/animation and normal scene integration.

Android shared-helper build73486 finished terminal0 after APK assembly. This does not extend the shield draw qualification to GLES.

## Checkpoint174: retained GLES shield renderer and real context recovery

Added shield_renderer.{h,cpp} as the retained production owner, built in desktop
arch_ogl, Android and the existing EGL owner. Its context discard is part of the
existing particle/effect context lifecycle, including abandoned-context handle
reset. Normal mission scene submission remains unwired. The protected existing
lightning/POG GPU test now exercises this owner, not a temporary local primitive.
No new top-level runner or admission path was added.

Shared shield_gpu_probe.h checks72 cases: qualities2/3/4, nearest/linear float-border
sampling, ages0/1000/2000 and fully occluded/near-half/far-half/full depth profiles.
It independently computes barycentric local positions/UVs, hit strength, constant
RGBA texture border response, depth and additive output on CPU. The comparison
excludes locations within one reported raster subpixel of a triangle edge and
within1e-5 of the depth split. More than200000pixels are checked per invocation.
Each isolated depth half must agree within one byte. Full images must also agree
within one byte with the sum of the independently checked half images; their CPU
reference sums must match exactly. Only overlap with contributions in BOTH halves
allows their two one-byte quantization errors to accumulate. Other pixels retain
the one-byte limit. The173original-source texture comparison still retains its
one-byte limit everywhere and was not weakened.

Two retained failed device receipts exposed the validation details. Initial full
output at quality2/pixel854 was162 versus CPU164. Both isolated halves subsequently
passed their one-byte checks. A source-qualified mesh diagnostic found FOUR actual
triangle contributions85/86/88/89 at that pixel, two in each depth half. Corrected
the analytical bookkeeping to use the independently tested halves, rather than
assuming a convex surface with exactly two triangle contributions. The source
mesh and production shader remained unchanged. Final device report counts608
channel comparisons using the qualified two-byte overlap allowance. This is a
bounded GPU/CPU numerical contract, not a byte-identical cross-device claim.

Final desktop owner70974 terminal0, all existing GPU tests plus72analytical cases
before/after actual EGL recreation pass28.53s. Windows32 native build passed with
the retained owner. Android final build38212 terminal0, BUILD SUCCESSFUL1m38s,
no compiler warnings. Fixed one pre-existing skybox diagnostic filename warning
encountered on recompilation by supplying a mutable local **FILE** copy to the
legacy char* API; diagnostic text is unchanged. Both automation catalogs passed.
Scoped formatters and diff checks passed.

Protected device run f5c1310e7556406b89eb57e5504c0a53 passed28/28 in49658ms on the
owned emulator-5556; process29089 terminal0. Final state reports shield_cases72,
checks2, shield_overlap_rounding608 and egl_recreate_count1. Complete retained GPU
pixel arrays match across the actual context loss/recreation. Existing28line,
108bitmap,96wall and other lightning/sprite controls also pass. Before/after
protected state manifests are identical, SHA256
59c4481debd389d8e987b75ce106fdf2ca6e6827c69c5a4237b39d7ae8c3da05.
Final result/state/manifests are copied to174-shield-device-* in the overnight
directory, alongside build/run/failure logs. The other emulator5554 was untouched.

This proves the tested GLES shader/VBO path, renderable/filterable RGBA32F border
storage and retained-owner recovery on this emulator. It does not establish all
Android GPU capabilities or original forcefield pixel parity on the device: the
device probe uses an analytical constant4x4image; original textures retain the173
desktop source comparisons. Normal resources, animation, scene transforms/queue,
player event wiring, glow, monitors and full original-camera save remain gates.
Original Pumo camera admission is unchanged.

Next175 qualify CAddonBitmap::SetFPS/Animate and CBitmap::NextFrame before adding
the owned original forcefield resource. The addon clock is per bitmap, unlike the
shared pulse clock. SetFPS10 starts a100ms timer; expiry is consumed before bitmap/
CurFrame availability checks. Frame preparation starts at0; expiry advances
one frame without catch-up. include/addon_bitmaps.h SHA256
9310ae945a30718aca25bc2a20622eaaf3dbf3e05d592cb8b0ee65d0f5269611.
The existing source timeout, TGA/frame-layout evidence and resource ownership
patterns should be reused. No user data edits, commits, pushes or remote writes.

## Tranche 175 - source-qualified addon animation clock

Added addon_animation.h with the per-addon SetFPS/Animate timer and bounded
NextFrame advancement. The existing HXM reference owner extracts the complete
CAddonBitmap class, actual constructor and CBitmap::NextFrame body from pinned
source. It reuses the actual CTimeout implementation qualified in171. Adapters
supply bitmap storage and SDL ticks; filename handling, loading and GL remain
outside this comparison.

Each MSVC64 and GCC run passed32032 comparisons,7633 frame advances,2246 wraps
and72 bitmap reloads across four independent addons with1/8/17/128 frames.
Coverage includes repeated calls at one tick, irregular elapsed intervals,
0/10/20/1000 FPS, missing bitmap/prepared frame, disable/restart, bitmap replacement
and uint32 SDL tick wrap. Source consumes timeout expiry before checking frame
availability, advances only once without catch-up and retains its clock when a
bitmap is replaced. Frame index belongs to the replacement bitmap. Signed64
source timeout arithmetic stalls across SDL wrap until restarted; no synthetic
wrap correction was introduced. Unsupported FPS and frame counts fail closed.

Both source reference tests passed; Linux owner completed in0.69s. Build logs
contain no compiler warnings. Scoped code formatting and diff checks passed.
Receipts are175-addon-animation-{msvc,linux}-{build,test}.log in the overnight
artifact directory. No Android/runtime changes in this checkpoint;174 remains
the latest protected device receipt. Original Pumo admission is unchanged.

Next176: reuse the existing owned addon lookup/TGA decoding path for original
shield frame resources, preserving per-addon clock lifetime across bitmap reload
and context loss. Normal scene/player-event/glow/monitor wiring and original
camera save admission remain unqualified. No commits, pushes or remote writes.

## Tranche 176 - owned shield frame resources and context recovery

Reused the existing mission/default/install addon loader through a small typed
read_addon_image helper. The unchanged ownership checks, case correction, bounded
read and CTGA-compatible decoder now also feed prepare_shield_image(index0..2)
for forcefield, forcefield2 and forcefield3. ShieldImageResource owns decoded
pixels and its ShieldTexture. Missing selected shield images fail closed; this
is not a qualification of the source's untextured half-alpha fallback. Existing
particle, corona and optional glare callers retain their behavior.

ShieldTexture validates bounded RGBA strips of square power-of-two frames and
lazily uploads one selected decoded band, generates source mips and copies them
to the qualified RGBA32F transparent-border storage. Callers explicitly select
nearest, linear or trilinear filtering. Repeated requests reuse the current frame;
frame changes invalidate its previous handle. Callers must not retain or leave
that handle bound across frame changes. The resource stores no animation clock:
the175 per-addon timer will remain outside bitmap/resource lifetime. A registry
joins the existing renderer context-discard hook, retaining CPU pixels and safely
recreating GPU names after real context loss. Invalid frame indices fail before
upload; upload restores active texture, binding, pixel-unpack buffer and unpack
alignment/row/skip settings. Unsupported GPU border storage still fails closed.

Extended the existing native effect/resource owner for all three shield names,
using decoded two-frame strips. It passed distinct startup installation, owned
mission-default/mod precedence, missing/malformed/shadowed resource rejection,
case-insensitive lookup, retained pixels and recovery. All five existing particle/
corona profiles also passed, along with the full effect inspection owner and
original Pumo admission witnesses. No new top-level runner or admission path.

The existing original whole-shield GPU comparison now obtains every production
border texture through ShieldTexture. All7776 comparisons passed for unchanged
forcefield/2/3, eight frames, qualities2/3/4, three filters, hit ages, rotations and
depth profiles, preserving the one-byte source comparison bound. Added invalid
frame controls, repeated lookup and upload-state checks with a deliberately tiny
bound unpack buffer and non-default row/skip/alignment. Frame4 discards/recreates
texture ownership. The full asset-free GPU owner passed30.90s, including72shared
analytical cases before/after actual EGL recreation. Shared probe CPU pixels and
ShieldTexture persist across recreation, rather than being reconstructed per call.

Final native build/integration23484 and Linux owner44954 terminal0. Android93221
terminal0, BUILD SUCCESSFUL1m48s, no compiler warnings. Recompilation exposed a
C++20-only capture of a structured binding in particle_objects.cpp; a local
reference to the same effect preserves behavior under C++17. Native effect/RNG
integration passed after that correction. Earlier Android warning log is retained.

Protected emulator-5556 run b85d9b9b1e41463980f23b0118196bb8 passed28/28 in52533ms;
process2866 terminal0. Final state reports checks2,shield_cases72,
shield_overlap_rounding608 and egl_recreate_count1. Retained pixels agree exactly
across actual context recreation. Protected before/after manifests both hash
59c4481debd389d8e987b75ce106fdf2ca6e6827c69c5a4237b39d7ae8c3da05.
Result/state/manifests and build/test logs are retained under176-shield-resource-*
in the overnight directory. Emulator5554 was untouched. This device control still
uses an analytical4x4texture; original forcefield pixels are desktop-qualified.

Next177: combine the qualified per-addon clock with resource/frame selection at
the actual sphere draw boundary, and exercise original forcefield pixels on the
protected device. Source CSphere::Animate only animates addon indices0/1 at10FPS;
index2 is not advanced there. InitSurface animates the parent before choosing its
CurFrame and Bind(1). The loader API is not yet installed into normal scene/player
resource selection. Profile/default filter choice, player event/scene queue/glow,
monitors and full original-camera saves remain gates. Original Pumo admission is
unchanged. No user data edits, commits, pushes or remote writes.

## Tranche 177 - retained shield draw binding and original GLES textures

Added ShieldAddon as one retained logical addon owner, separate from its replaceable
ShieldImageResource. It uses the175source-qualified clock at the hit draw boundary:
indices0/1 advance at10FPS, index2 does not advance in CSphere::Animate. Rebinding
the same resource preserves its frame; replacing/unloading resets the bitmap frame
without resetting the addon clock. Context loss leaves both frame and clock intact.
Inactive hit envelopes return before animation, matching NeedEffect's early exit.
The interface accepts resolved RGB only and forces surface alpha1, as the source
hit branch does in InitSurface. This remains the positive additive hit path; it
does not implement appearance, non-hit rings, negative-alpha blending or glow.

Added linear-nearest-mip filtering after inspecting the source renderQualities
array. Callers still choose filtering explicitly; upstream/default profile
resolution is not claimed. The source comparison now covers all four filter modes
for all eight frames of forcefield/2/3:10368framebuffer comparisons, preserving the
one-byte source bound. Each image has3456 comparisons,768visible,576occluded and
576expired cases. The first new-filter run failed because the test accidentally
mapped nearest-mip to production trilinear. An explicit-gradient experiment did
not change that discrepancy. Correcting the test mapping fixed it; the experiment
was reverted completely. Production GLSL100/120 and the prior hit equation remain
unchanged. Both diagnostic failure logs are retained; no tolerance was relaxed.

New shared shield_original_gpu_probe.h uses the actual ShieldAddon/ShieldTexture
owners. It verifies all524288decoded bytes of each original via fixed FNV64
witnesses (b762a08f7dddacee,3e3d91c43410cc4b,a6097408e2a5fc49), then draws the
source-selected17frames: eight each for addons0/1 and frame0 for addon2. Across
qualities2/3/4, four filters and visible/occluded/expired profiles,612cases include
204visible draws. It verifies frame indices after every draw, including elapsed
inactive calls and index2 calls at different ticks. Separate binding controls
check same-resource binding, replacement and unload without clock reset. Before/
after actual EGL context recreation, the complete612image arrays agree exactly.
This is repeatability and visibility/ownership evidence on GL/GLES, combined with
the desktop source comparison; it is not a cross-device pixel identity claim.

Extended the existing protected LightningGpu mode to require
-ShieldImageDirectory containing the three original forcefield TGAs. It verifies
all three original SHA256 values before staging, loads them through the normal
owned mission resource loader, and cleans the temporary device files. The original
Pumo mod package has no forcefield texture overrides; only ambient-forcefield.wav
matches that name. The original HOG/mod inputs were not rewritten. The existing
owned JSON script now asserts original_shield_cases612 before and after context
loss. No new top-level runner was added. Current runner usage is also updated in
lightning-simulation.md.

Final Linux83998 terminal0: all original source comparisons and612addon cases
before/after real recreation pass; full asset-free GPU owner passes29.44s.
Windows32 native build passes. Android24323 terminal0, BUILD SUCCESSFUL1m45s,
no compiler warnings. Scoped formatting/diff checks pass. Automation catalogs
passed:99standalone JSON,501support scripts,314standalone PowerShell,427top-level
runner entries. Those catalogs remain valid after the final C++ interface-only edit.

Final protected run268984c1f28f4cd89459087ea460254b passed28/28 in55936ms on owned
emulator-5556;55531terminal0. Final state reports checks2,shield_cases72,
original_shield_cases612,shield_overlap_rounding608,egl_recreate_count1. Complete
retained pixels match across actual context loss. Protected before/after manifests
both hash59c4481debd389d8e987b75ce106fdf2ca6e6827c69c5a4237b39d7ae8c3da05.
Final receipts are177-shield-binding-{result.json,state.json,before.sha256,after.sha256}
with build/source/GPU/device logs in the overnight directory. The earlier successful
device run before the RGB-only interface is retained in before-alpha-interface.log.
Emulator5554 was untouched. All work remains uncommitted, with no remote writes.

Original Pumo camera admission remains closed. Normal player-event/profile/scene
submission, glow and full original-camera saves remain unqualified. Next178 should
advance the authored monitor consumer: qualify source CCamera::Create/Render and
buffer lifetime before installing a native offscreen scene capture. The pinned
source uses full/half-screen ordinary camera targets, switches the viewer for
RenderFrame(0,0), marks successful captures valid, and restores the viewer; the
manager restores cockpit state. Existing scheduler, bindings and UV evidence can
be reused. Source paths are render/cameras.cpp,include/cameras.h,ogl/fbuffer.cpp
and include/fbuffer.h. Preserve admission gates while qualifying that consumer.

## Tranche 178 - source camera target sizing and capture transaction

Added camera_capture.h for ordinary camera target specifications and the actual
capture control flow. Target sizes preserve integer full/half-screen dimensions,
including odd sizes; ordinary source requests buffer type1 and two color buffers.
Inputs are bounded to positive signed-native canvas dimensions and an empty
half-size target is rejected. This is a specification, not a GL allocation or
permission to resize source-owned targets at arbitrary times.

CameraCapture follows CCamera::Render: activate camera rendering, switch to source
CM_FULL_SCREEN3 if needed, attempt ReleaseBuffer, choose attached or fallback viewer,
activate its canvas, RenderFrame(0,0), deactivate, restore the viewer, publish valid,
and DisableBuffer. Missing buffers retain an older valid capture. DisableBuffer's
return value is ignored by the source. The camera-active flag ends at0 rather than
restoring a previous1. The individual capture leaves cockpit restoration to the
already-qualified manager. Ordinary callback return flow is qualified here;
exception/fatal-error recovery and actual GL state restoration are not yet claimed.

Extended the existing effect source owner, extracting actual CCamera::Init,
CreateBuffer, Create and Render bodies from the pinned cameras.cpp, plus the pinned
CM_FULL_SCREEN macro. GL buffer allocation, canvas activation, renderer callback
and viewer storage use explicit data/trace adapters. Shadow camera execution throws
in the adapter and is outside this profile. Actual source Setup orientation was
not reimplemented by these adapters; its earlier separate evidence remains scoped.
New cockpit.h SHA25698a1627a9b7608ab481b82e53a36d4fb00829a69a35af4dfb84300cf3ccb59a3.

Both MSVC64 and GCC passed600target specifications and9216capture transactions:
4608render callbacks with an available buffer,4320retained-valid buffer misses and
7rejected dimensions. Cases cover attached/fallback viewers, null/non-null prior
viewers, all six cockpit modes, prior active/valid states, callbacks that alter the
viewer/cockpit, failed disable results and eight-call sequences with buffer misses.
Exact operation traces, observed renderer state, restored viewer, final cockpit,
active flag, canvas and validity match the source. Source creation treats a negative
FBO.Create result as nonzero success; this is observed, not authorization for the
native backend to accept incomplete GL targets. Future GL allocation must fail
closed outside complete, supported targets.

The complete existing effect reference suite passed5.66s on MSVC and2.00s on GCC;
both final wrappers terminal0 with no compiler warnings. The initial MSVC bool/int
comparison warning was fixed with an explicit conversion and its build log retained.
Scoped formatters and diff checks passed. Detailed source output is copied to
178-camera-capture-{msvc,linux}-reference.log, beside build/test logs, in the overnight
directory. No Android runtime path changed in178;177 remains the device receipt.
No new test runner, admission changes, commits, pushes or remote writes.

Next179: implement/qualify the ordinary camera GL target. Source ogl/fbuffer.cpp
CreateColorBuffers uses two RGB unsigned-byte nearest-filtered textures for type1.
Correction in179: the active source branch generates a mip chain at allocation,
but GL_NEAREST never samples it. The earlier RGBA/non-mipmapped note was incorrect. CreateDepthBuffer attaches one shared depth/stencil texture to
both depth and stencil; inspect COGL::CreateDepthTexture for its exact format before
allocation. CFBO::SelectColorBuffers defaults to color0, and can select the other
attachment or all buffers. The actual binding occurs through ogl/drawbuffer.cpp
ChooseDrawBuffer/SelectDrawBuffer during RenderFrame when cameras.bActive is set;
CCamera::Render does not itself call EnableBuffer. BlurBuffer can also select the
camera target. Preserve these distinctions when wiring the native scene callback.
Actual attachment formats, context recreation, captured scene state and monitor
surface sampling remain gates. Original Pumo camera admission is unchanged.

## Tranche 179 - ordinary camera framebuffer ownership

CameraTargetGpu now owns two nearest-filtered RGB8 color textures and one shared
DEPTH24_STENCIL8 texture attached to both depth and stencil. This follows the
active type1 source allocation branch: RGB/UNSIGNED_BYTE with a generated mip
chain that GL_NEAREST does not sample. The earlier178 RGBA/non-mipmapped note is
corrected above. GLES uses CLAMP_TO_EDGE for source GL_CLAMP under nearest
sampling. Logical buffer1 selection remaps physical attachments because GLES
requires fragment output locations to match attachment slots; both-buffer mode
restores natural0/1 attachment order.

Allocation is transactional and rejects unsupported/incomplete targets. Capture
publishes validity only after a successful callback and GL check, rejects nested
capture of the same owner, and restores framebuffer, viewport, shader/VAO,
texture/unpack, blend/depth/cull/scissor, clear and front/back stencil state.
Callback failure invalidates the partial capture and the same owner can recover.
The existing context-discard hook invalidates all retained camera images without
retaining stale GL names. Engine viewer/canvas/render globals are deliberately
still the future scene adapter's responsibility.

The existing desktop GPU owner extracts actual source CFBO::CreateColorBuffers,
CreateDepthBuffer, AttachBuffers and SelectColorBuffers plus
COGL::CreateDepthTexture. Its data adapter supplies texture entry points and an
explicit current FBO; it does not replace the extracted allocation operations.
Only ordinary type1 is qualified. Typed adapter cleanup does not reproduce the
source Destroy bug that also deletes the depth texture name as a renderbuffer.
Source hashes are pinned in tests/reference_camera_target.cmake.

Desktop passed48 exact source-versus-port allocation/render/sample comparisons:
eight full/half-resolution targets, two logical colors, and three sampling offsets.
A shared GL/GLES control separately checks48 complete images against CPU expected
RGB/opaque alpha, nearest edge clamping, MRT writes, depth rejection and stencil
rejection. It also tests callback failure/recovery and caller state restoration.
The same eight retained targets lose validity across actual EGL recreation and
produce identical pixels after rebuilding. The full existing GPU owner passed
32.78s; Linux and Windows native builds passed without compiler warnings. Android
x86_64 debug built successfully in2m12s. Both automation catalogs passed99standalone
JSON,501support scripts,314PowerShell owners and427top-level master entries.

The first Linux attempt exposed the inline source-function extractor's delimiter
assumption; the corrected explicit inline extraction passed. Its failure log is
retained as179-camera-target-extraction-failure.log. All other179build, test and
format receipts are in temp/xl-overnight-20261101. No new test runner or camera
admission change is introduced.

Protected emulator-5556 run38e66bbd88844471b343496bec336f5b passed28/28steps in57964ms.
Final state reports camera_target_cases48, checks2, shield_cases72,
original_shield_cases612 and shield_overlap_rounding608, with actual context
recreation. Before/after protected app manifests are byte-identical with SHA256
59c4481debd389d8e987b75ce106fdf2ca6e6827c69c5a4237b39d7ae8c3da05.
Receipts are179-camera-target-{result.json,state.json,before.sha256,after.sha256}.
This proves the target and controlled sampling, not a captured original level or
its actual monitor surface. Original Pumo camera admission remains closed.

Next180: qualify source SetupMonitorFace and its ordinary-camera selection of
base/overlay images, validity/availability gating, visibility notification and
aligned UV ownership. Then wire the native scene/canvas transaction and actual
monitor consumer, preserving authored textures when the source does. Full camera
scene capture, static135, remaining contacts/profiles/glow and original-camera
full saves remain gates. Reserve the final10minutes before18:00UTC for audit and
handoff rather than starting another implementation chunk.

## Tranche 180 - ordinary monitor image selection

camera_monitor.h implements the release SetupMonitorFace decisions for ordinary
cameras. A missing camera clears the face camera index. Disabled cameras preserve
all face state. Enabled existing cameras mark themselves visible even before a
valid image exists. Validity plus a texture or exact buffer availability1 enables
alignment and camera image selection: replace base if no overlay texture is
specified or the base bitmap is absent; otherwise replace overlay. Existing
unselected bitmap and UV state remain untouched. The enabled ordinary path clears
the face teleport flag. A valid image or fit-to-wall marks the helper handled;
fit-to-wall alone does not replace either bitmap. This matters because the actual
LoadFaceBitmaps caller returns immediately on a handled monitor, otherwise clears
its camera index and continues normal authored bitmap loading.

The selection takes resolved availability/resource inputs; it does not emulate
HaveBuffer GL queries or own their call order. Teleporter rendering is explicitly
rejected rather than silently receiving ordinary-camera behavior. Alignment is a
notification here; its actual math remains the separately source-qualified
CameraAlignment path. No actual scene rendering or level admission is enabled.

The existing effect reference owner extracts actual IsMonitorFace and
SetupMonitorFace from pinned render/renderlib.cpp SHA256
d3eacfaf28c9687e7e37c3e47333b11d4a026000839231e8a71b443bc86d4252.
MSVC and GCC each passed6144full face/camera-state comparisons:480aligned image
selections,160overlay replacements and5664retained-image cases. Controls cover
missing/disabled cameras, prior visibility/teleport state, absent base/overlay
bitmaps, overlay texture IDs, validity, independent texture/availability state
including statuses-1/0/1/2, fit-to-wall, UV ownership and unchanged fields. Another
48source lookup controls verify enabled/forced face lookup. Unqualified teleporter
selection is rejected. The source Align adapter checks exact face/UV/vertex pointer
inputs and records invocation; it does not stand in for an actual rendered face.

Both full effect reference owners passed, MSVC5.74s and GCC2.13s, with terminal0
wrappers and no new compiler warnings. Detailed outputs are copied to
180-camera-monitor-{msvc,linux}-reference.log beside build/test/format logs in the
overnight directory. Android code did not change;179remains the current GPU/device
receipt. No new runner, admission changes, commits, pushes or remote writes.

Next181: integrate native offscreen scene capture. Normal Android ogl_start_frame
currently invokes android_ogl_msaa_begin_frame and can replace the camera FBO;
viewport conversion also assumes the main drawable. The camera pass must preserve
its own target and canvas dimensions while restoring engine/shim state and keeping
main MSAA accounting balanced. Qualify real native rendering through the existing
GPU/device owners before connecting the scheduler and actual monitor faces. Source
SetupMonitor chooses its UV texture unit from the actual bmTop presence, not merely
which bitmap was replaced; retain that distinction in the eventual face consumer.
Actual camera scene/monitor rendering, static135, unqualified contact/profiles/glow
and full original-camera saves remain gates. Original Pumo admission is unchanged.

## Tranche 181 - native frame boundaries inside camera targets

CameraTargetGpu now exposes callback-scoped active dimensions through the existing
renderer bridge. The scope restores its predecessor on success or exception;
ordinary rendering observes no active target. D2 ogl_start_frame/ogl_end_frame
respect those dimensions and preserve the currently bound camera FBO. Android
camera passes bypass main-MSAA allocation/binding/resolving/nesting transitions,
pending main graphics-option application and main GPU timer starts. Readback
preparation also preserves the camera target. Camera viewport coordinates are
physical target pixels, independent of window scaling/keyboard offsets; viewport
cache invalidation forces the following main pass to apply its own viewport.
Offscreen GL errors are left for the capture owner's fail-closed check rather
than drained into main-MSAA diagnostics. Stock frame behavior remains on its
existing path when no camera target is active.

The existing shared GPU probe now exercises actual production ogl_start_frame,
readback preparation and ogl_end_frame with two half/full-resolution odd-sized
targets and offset canvases, sampling both color buffers. Four native-boundary
cases supplement179's48analytical target cases. The diagnostic restores its
canvas/screen dimensions, fixed-function or GLES-shim matrices and merged-shader
matrix uniforms; new read-only GLES matrix accessors support that restoration.
It checks framebuffer/viewport ownership and complete sampled-image repeatability
across actual context recreation. The draw between native frame boundaries is the
controlled RGB/depth/stencil pattern, not render_mine or a full level capture.

The Android support script now requests4xMSAA through the existing graphics
confirmation flow and requires an allocated target with at least2effective samples.
Each camera pass verifies unchanged main-MSAA generation, dimensions, effective
samples, bound flag, bind/resolve counters and scene-error count. This directly
exercises coexistence with enabled MSAA rather than relying on an inactive default.
The existing protected owner remains responsible for restoring all app data.

Windows native integration built successfully. The full desktop GPU owner passed
45.10s after final edits, including the retained targets and all prior shader,
particle, lightning and shield controls. Android x86_64 debug built in2m19s.
The first Android attempt found an unavailable short include path for oglprog.h;
it was corrected to the repository-relative header and the failed log retained as
181-camera-native-android-header-failure.log. The final build has no compiler
warnings; the initial shared-header rebuild also exposed existing unrelated D1/D2
warnings, which were not changed by this work. Both automation catalogs passed
99standalone JSON,501support scripts,314PowerShell owners and427master entries.

No camera level admission or scheduler/scene hook is enabled. Full native capture
still needs the source-aware engine transaction. Source render/rendersetup.cpp
BeginRenderMine excludes camera views from automap discovery; render/rendermine.cpp
DoRenderObject excludes them from rendered-object gameplay lists; SetupMineRenderer
sets bDoCameras false during camera capture. RenderEffects also applies the resolved
monitor particle/lightning options. Those conditions must be preserved before
using render_frame as a camera callback. Do not simply set Viewer and call the
normal main-view path. Original Pumo camera admission remains closed.

The first protected device run passed the initial MSAA-enabled camera checks but
failed the second probe because launcher resume reapplied msaa_level0. Logcat and
graphics_safety state identified the live option change; the main target had been
created successfully after context recreation before that option disabled it.
The script now restores the already accepted4xMSAA choice after resume and waits
for allocation again. No rendering code or graphics-safety bypass was needed.
Failed-run receipts are preserved as181-camera-native-resume-failure-*; its
protected-data manifest was also restored byte-for-byte.

Final emulator-5556 run 8f976a380d0a4a70a5dd654cfa9f81e7 passed33/33steps in58851ms, with
camera_native_pass_cases4, camera_target_cases48, checks2 and original_shield_cases612.
Both GPU passes require allocated main MSAA and unchanged accounting around each
camera pass. The same captured images match after actual graphics-context
recreation. Final receipts are181-camera-native-{result.json,state.json,before.sha256,
after.sha256}; the successful owner log is181-camera-native-device-final.log.
Protected manifests match SHA256
59c4481debd389d8e987b75ce106fdf2ca6e6827c69c5a4237b39d7ae8c3da05.
All181build/test processes reached terminal status; scoped format/diff checks pass.

Next182: qualify the source camera view/effects/gameplay-visibility policy and
apply it to the native capture transaction. Source SetupRenderView takes the
camera's authored segment and unmodified orientation/zoom, without player rear
view, stereo eye offsets, head angles or zoom controls. Source BeginRenderMine
preserves gameplay rendered-object history during camera views and excludes their
automap discovery; monitor lookup is disabled recursively. RenderEffects uses
resolved monitor/auxiliary-view flags: raw profile defaults alone are insufficient,
because DefaultSmokeSettings/DefaultLightningSettings resolve them from quality
and expert-mode policy. Then capture the actual native scene with preserved owner,
canvas and caller state; do not infer full scene support from181's controlled draw.
Reserve17:50UTC onward for final audit/handoff before the18:00UTC cutoff.

## Tranche 182 - camera view effect and visibility policy

camera_view_policy.h derives effect eligibility from resolved options. The source
RenderEffects dispatcher runs sparks, particles and lightning in that order when
the stereo/window/camera gate is open. Automap options take precedence in automap
views; ordinary views use quality for sparks and independent auxiliary/monitor
flags for particles and lightning. Transparency renders even when those passes
are gated out, and postprocessing prepares only for window0. The policy preserves
these distinctions rather than treating a camera as a generic disabled-effects view.
Separate predicates preserve the source exclusion of camera views from automap
discovery and recursive monitor rendering, including multiplayer option gating.

The existing effect reference owner compiles actual RenderEffects and verbatim
SetupMineRenderer/BeginRenderMine predicate statements. MSVC and GCC each passed
41472complete effect call traces, including20736camera views, and128automap/monitor
predicate comparisons. Cases cover stereo devices/separations, windows0/1/2,
quality0/1/3, all monitor/auxiliary/automap flag combinations, camera activity and
thread readiness waits. Transparency and postprocess call ordering is checked.
The data-only managers record calls; this does not qualify new effect rasterizers,
threads or full native scene behavior.

Source hashes: render/rendermine.cpp
f433b94da415fea1466c17af7feecb1bf05af84ec5831d2b741672fc70db50d8;
render/rendersetup.cpp
3632952f30e5f569793eb0fbcf1ea2b70154afaf5875fd6c2ece90275470cac6.
Full existing effect owners passed7.22s MSVC and2.14s GCC, terminal0 with no new
compiler warnings. Detailed outputs are182-camera-view-{msvc,linux}-reference.log
beside build/test/format receipts. No Android runtime code changed in182;181's
33-step MSAA/context device receipt remains current. No new runner or admission
change, and no commits, pushes or remote writes.

Next183: native scene capture callback using the qualified target and view policy.
Use source camera segment/orientation/zoom without player rear-view/stereo/head
adjustments; preserve prior rendered-object history and automap discovery; avoid
recursive monitor sampling. Restore viewer/canvas, CPU projection and GL/shim
matrices around the callback, and keep capture before the main view rather than
inside an unfinished transparency frame. Test actual native mine rendering through
the existing protected original-hub owner without admitting an original camera
level or claiming the current player's view proves authored camera integration.
Resolved monitor settings must be supplied explicitly: raw playerprofile values
are0, but actual DefaultSmokeSettings sets particles.bMonitors from quality>1 in
non-expert/setup mode and DefaultLightningSettings sets lightning.bMonitors1.
Their full profile resolution remains separate from182's resolved-input contract.
Keep remaining original-camera admission gates until the complete consumer works.

## Tranche 183 - native camera scene capture

camera_scene.cpp supplies the ordinary singleplayer scene callback for the retained
camera target. It uses the temporary viewer's authored segment, orientation and
zoom, invokes the actual native render_frame/render_mine path, and restores the
viewer, canvas, cockpit, CPU projection state, GL/shim matrices, merged-wall
uniforms and texture units zero through three. Capture is a before-main-view
contract, not an operation inside an unfinished renderer frame. It rejects nested
target/scene capture, an active transparency collection, unsupported demo/endlevel
or multiplayer state, invalid viewer segment/zoom and invalid canvas dimensions.
The legacy non-Android GLES path remains explicitly unsupported.

The native renderer excludes camera views from automap discovery, gameplay
rendered-object history, main-view introspection and robot draw diagnostics.
Transparency still drains; particle and lightning eligibility uses the qualified
182 policy with explicitly resolved monitor flags. Both are enabled in the device
control. No source profile defaults were used to disable effects or admit a level.
This callback does not install authored cameras, schedule them, bind monitor
surfaces, resolve full player profiles, or qualify original-camera save/load.

The existing protected original-hub GPU owner now calls the actual scene callback
at 160x90 and 320x180. Four captures per invocation check visible opaque mine
pixels, identical paused-scene pixels with rear view off/on, untouched automap and
all gameplay rendered-window records, unchanged simulation RNG, restored CPU/GL
view state, seeded secondary texture bindings, and unchanged main MSAA allocation,
bind/resolve accounting and errors. Retained camera targets must be invalid after
real graphics-context recreation. The temporary camera is copied from the player's
view; this is native scene qualification, not authored camera placement or monitor
image qualification. The diagnostic alone preserves/reseeds FX RNG for its repeat
comparison; production capture retains ordinary rendering behavior.

MSVC native build and existing effect-inspection lifecycle owner passed (22.12s).
Android x86_64 state-restoration build passed in 2m56s. Final strengthened protected
run 1138c874ab314e9294f8ea061aeebbaf passed 33/33 in 88374ms on emulator-5556,
including two GPU invocations across real context recreation. Each reports four
scene cases, four native frame-boundary cases, 48 camera target cases and 612
original shield cases. Main MSAA finished at 4x, generation 3, 54 binds/resolves,
zero scene errors. Protected before/after manifests are byte-identical with SHA256
59c4481debd389d8e987b75ce106fdf2ca6e6827c69c5a4237b39d7ae8c3da05.
Initial scene run fb381fe205ee40ae87226d8ede6595cc also passed 33/33 in 97777ms;
its separate receipts precede the strengthened binding/matrix/MSAA assertions.

A subsequent GCC syntax compilation found legacy min/max macros entering C++
standard headers. The adapter now undefines them like neighboring renderers.
Final GCC -Wall -Wextra syntax compilation and MSVC native rebuild passed without
warnings; the failed GCC log is retained as 183-camera-scene-linux-macro-failure.log.
This was a Linux header issue, not a change to scene execution. The final Android APK rebuild with that
cleanup passed in 2m27s, terminal 0. GCC syntax checks of the native render.c hooks
and transparency_scene.cpp also passed without diagnostics, using the existing
pinned JSON headers. The initial ad hoc transparency compile omitted that include
path; its failure receipt is retained. No full Linux native gameplay test is claimed. The unchanged desktop framebuffer owner
retains 181's completed receipt.

Scoped formatting and git diff --check passed. Both catalogs passed: 99 standalone
JSON tests, 501 support scripts, 314 PowerShell tests and 427 master entries. No new
runner, admission bypass, mission edits, commits, pushes or remote writes.
Receipts: temp/xl-overnight-20261101/183-camera-scene-* and 183-*-catalog.log.

Next implementation boundary: connect the qualified scheduler/target/scene owner
to authored monitor surfaces. Source ogl/ogl_fastrender.cpp SetupMonitor selects
(GL_TEXTURE1 when an overlay bitmap is present, otherwise GL_TEXTURE0), plus the
lightmap offset. This differs from choosing which bitmap the camera replaces.
It supplies camera UVs for that unit; ResetMonitor restores the authored base or
overlay stream. RenderFace keeps monitors out of its ordinary transparency queue
and still performs the selected lighting/shader path. Preserve independent UVs for
the other bitmap and do not substitute a flat unlit texture. Source hash:
72141594cffbdce35430e7d4cd74353aa5806dc314e30b2c11666dfae22d3d03.
The source's camera-UV-pointer-minus-global-face-index idiom must not be copied as
out-of-bounds C++ pointer arithmetic into the native local-vertex renderer.
Keep static behavior 135, remaining actor/weapon responses, full profile/glow and
original camera save consumers gated. Reserve 17:50 UTC onward for audit/handoff.
