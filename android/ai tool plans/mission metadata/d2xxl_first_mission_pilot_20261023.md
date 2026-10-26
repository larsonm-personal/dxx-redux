# D2X-XL first mission pilot

## Final pilot acceptance (2026-10-24)

The requested original Ahayweh Gate singleplayer pilot is verified playable in
the opt-in build, including its required authored XL extensions. See the final
[acceptance record](../../XL_compat/evidence/pilot-acceptance-audit.md) and
[19-category reconciliation](../../XL_compat/evidence/pilot-requirements.md).
These supersede historical open-gate statements below; earlier failures remain
preserved as evidence, not current status

Final host suite: 36/36, 162.27 seconds,
`temp/xl-pilot-current-full-suite.log`. Final Android rendering checks: 28/28
twice, runs `4bf6a9725f774329bd2169bd2c6761d0` (26252 ms) and
`84c203681f214ac9ae9a21e4a99c1c92` (25349 ms). The current test-only object
probe uses projected-body samples rather than assuming an opaque robot center;
9/10 of 25 initially visible samples become occluded by the real object pass,
with same framebuffer and zero GL errors. Witness positions are in the
`temp/xl-object-silhouette-report.json` and repeat-report artifacts. The earlier
center-only failure is retained. No production rendering change was needed

Android APK: `android/app/build/outputs/apk/debug/app-debug.apk`, SHA-256
`0c34652882800e4593f8b57d70034e4afd8748f5ddb89f3b21ab2913fe33e977`.
Build completed in 1m28s without new compiler warnings; scoped formatting/lint
and diff checks pass. Installed on emulator-5554, using the previously imported
original mission. Test runs changed no saves; app stopped after capture

Acceptance retains the user's exclusions for exact images and incidental classic
retuning. It does not certify other campaign levels, multiplayer, every optional
XL renderer, physical Android devices, or an uninterrupted Android all-secrets
run. Normal-input host routes, Android original import/persistence, native Android
exit/score/menu checks and real rendered extension tests provide the stated
bounded evidence. Ordinary builds remain off; campaign browser status remains
unsupported. No required work remains for this selected pilot objective

## Decision

Latest scope correction: the user excludes XL retuning of classic GAMEPLAY,
not visual changes. Visual tweaks are allowed when they help deliver the pilot.
Exact visual fidelity remains unnecessary; required XL engine extensions must
be faithful. Incidental XL retuning of classic gameplay, such as a wholesale
flare-speed change, need not be copied. This is not a requirement to preserve
every Redux gameplay choice. Authored warps, speed boosts and other extensions
must work wherever present. Earlier blanket exclusions of visual tuning and
blanket requirements to preserve Redux gameplay are superseded

Latest implementation constraint: the user explicitly permits ordinary guarded
hooks in d1/d2 and rejects generated-source CMake substitution. This supersedes
the earlier no-base-edits constraint and historical overlay checkpoints. The
runtime now uses opt-in `DXX_XL_PILOT` hooks with extension state kept in
`android/XL_compat`; default engine behavior must remain unchanged

Direct-hook verification: both `xl-render-preview` and `xl-preview-lifecycle`
build from the ordinary engine sources with `DXX_XL_PILOT=ON`; the lifecycle
suite passes. The normal `buildd2` game and metadata targets also rebuild with
`DXX_XL_PILOT=OFF`. Runtime configuration no longer generates or substitutes
engine C files. Scoped formatter/linter and diff whitespace checks pass. The
normal rebuild reports an existing C4113 warning in untouched `texmap/ntmap.c`;
the new hook sources add no compiler warnings

Mission isolation uncovered a real admission bug: retail Counterstrike and the
XL pilot share `d2leva-1.rl2`. Admission is now scoped to the `d2-xl` mission
(or the explicit private `xlprobe` test fixture), as well as the original hash.
The test renames its loose fixture copy before mission-level checks so it cannot
shadow retail HOG members, and verifies retail resolution from `descent2.hog`.
Pilot -> stock -> pilot passes fresh UVL/color/material checks and byte comparisons
of loaded robot, weapon, powerup and joint definitions. Intro skipping is a
consumed flag and must be set before each headless startup; failing to do so
entered the unsupported movie path. Custom override rollback remains separate

Latest completion scope, clarified by the user on 2026-10-23: a fully playable,
verified Ahayweh Gate port with faithful implementation of every XL engine
extension used by the pilot. Exact visual fidelity and XL author's changes to
incidental gameplay values already present in 1996 are explicitly NOT requirements.
The inventory, assisted route and browser are
intermediate deliverables only. This scope supersedes stricter visual-parity
requirements in the historical checkpoints below

## Current acceptance boundary after user clarification

Physical secret entry (2026-10-24, nine cases verified): extend
the existing normal-controls navigator with a segment-entry objective. Each
case first earns the red key (and the reactor objective for segment 199),
restores that unchanged earned checkpoint, then
flies/fights/opens barriers with ordinary inputs until the ship is physically
in the target segment, alive. Nine explicit cases use the original metadata's
capture segments: 4, 205, 206, 17, 81, 225, 14, 227, 199. The ship's runtime
segment and native point-in-segment lookup must both agree. No teleporting,
ghosting, health grants or robot removals are introduced. Individual entry
tests will not establish a single uninterrupted all-secrets playthrough

All 25 normal-navigation/lifecycle tests pass in 63.03 seconds, including the
nine new `xl_normal_secret_*` cases, existing key/reactor/escape and 12 branch
tests. Evidence: `temp/xl-secret-entry-regression.log` and
`temp/xl-secret-entry-summary.json`, which links each current native run log.
The eight pre-reactor entries retain 33-57 shields (segment 17: 51.56);
segment 199 is entered after the normally earned reactor fight with 86.87
shields and 27.48 seconds still on the unchanged countdown. Each frame rejects
life loss and countdown pausing; physical entry requires actual ship segment
and native point-in-segment agreement. No engine combat/physics changes

Preserved failures exposed controller limitations: the first segment-199 route
ignored the reactor and died; the retry kept firing at the destroyed reactor;
the next attempt stalled against its wreck until countdown death, which the
new life-count invariant caught. The side route reaches the secret doorway,
but a close-range reactor fight causes fatal blast damage. The final case uses
the existing normal reactor objective first, then flies the side loop around
the wreck under its earned timer. Logs retain `xl-secret-entry-tests`,
`xl-secret-entry-199-fixed`, `xl-secret-entry-final-tests`,
`xl-secret-entry-side-loop-tests` and `xl-secret-entry-earned-reactor-tests`
prefixes under `temp/`. A separate first contract-tools build used x86 setup
for an x64 build tree; the correctly selected x64 retry passes. Scoped
formatting/lint and diff checks pass. These are host tests; no Android build
or new Android traversal claim is made by this slice

Acceptance audit (2026-10-24): see
[`pilot-acceptance-audit.md`](../../XL_compat/evidence/pilot-acceptance-audit.md).
All 23 private tests pass in 129.64 seconds, including reference arithmetic,
full archive inspection, gameplay objectives and all trigger branches. A fresh
resource-backed ar12 run reaches level 1; read-only process inspection confirms
active coronas and successfully loaded corona/glare bitmaps. The old baseline
remains intact. This closes the missing reference-resource prerequisite, not
reference-image equivalence. A concrete remaining gameplay gap is physical
entry into all reachable branches/secrets: current branch assertions stop at
activation plus linked-target passability. Extend normal-input coverage rather
than treating those states as proof of actual visitation

Object-depth follow-up (2026-10-24, controlled check passed): an explicit
Android probe brackets the real object pass, before translucent geometry.
It positions small diagnostic halos behind existing robot centers along the
current view ray, retains only samples visible against wall depth, and requires
at least one to disappear after object rendering while remaining visible with
depth disabled. Color/depth writes are disabled for diagnostics; robot/world
state is untouched. The test uses a controlled segment-26 camera toward the
original robot in segment 36. This is rasterized object-depth evidence, not
normal gameplay progression or an authored emitter attached to that robot

Run `5ce89588721f4c4997deb399596be57f` passes all 28 steps in 24634 ms.
One robot-anchored test halo (sample object index 89) is visible before objects,
hidden afterward, and visible again with depth disabled; both phases use the
same framebuffer and GL errors are zero. The index identifies the halo's robot
anchor; the depth difference is attributable to the intervening object pass.
Evidence: `temp/xl-object-depth-report.json`, `temp/xl-object-depth-result.json`,
`temp/xl-object-depth-test.log`. The prior color/partial-depth/wall/preference
checks also pass in this run. Android builds in 1m12s with no compiler warnings,
APK SHA-256 `6b888da430cdc8e3aaaa28a80b56ffe60ce16fed9587ed263f9063dab4759c9f`.
Both desktop pilot targets rebuild, both catalogs and scoped formatting pass.
No saves were written, robots were not moved/replaced, and the app was stopped
after capture. This closes the concrete object-depth gap for the tested path;
it does not establish every model/device combination or reference equivalence

Actual-scene depth probe (2026-10-24, wall check passed): added an explicit
Android-only request consumed immediately before normal halo rendering. GPU
sample queries draw the authored texture-405 halo with color/depth writes
disabled against the current scene framebuffer. Require its visible position
to rasterize with depth both on/off; move the test halo farther along the
camera ray behind its opaque wall and require samples only with depth off.
This uses actual scene depth rather than an artificial cleared-depth mask.
It leaves the world/camera and framebuffer contents unchanged. Negative-control
draws are excluded from the production depth-state counters. Object silhouettes
and complete-resource reference observations remain separate follow-ups

Android run `37e68e95b5374cacbc596fc174de4f83` passes 24/24 steps in 26796 ms.
Both normal-position controls report rasterized samples. The behind-wall
control reports samples with depth disabled and none with actual scene depth;
GL errors are zero. Evidence: `temp/xl-scene-depth-report.json`,
`temp/xl-scene-depth-result.json`, `temp/xl-scene-depth-test.log`. This proves
wall occlusion for the controlled original-level view, not general object
silhouettes or every rendering configuration. Android build passes in 57s,
APK SHA-256 `fa2413319af2104897b3e2b30e8f62172b0ff5f461b1ffd4b8190171f0108d60`.
Both desktop pilot targets build, lifecycle passes in 2.14s, both automation
catalogs and scoped formatting pass. The only Android compiler warnings are
existing unused declarations in `gr.c`. The app was stopped after capture

Depth-pipeline follow-up (2026-10-24, verified selection/state): source review
confirms classic depth disables scene depth testing, so merely enabling depth
for the final halo pass cannot provide correct occlusion. The active pilot now
selects the existing depth-buffered mine/object pipeline through
`ogl_use_classic_depth`; the saved `GameCfg.ClassicDepth` preference is not
modified. Stock levels retain their requested pipeline. Both native custom-model
depth toggles use the same effective choice. Native pilot/stock lifecycle checks
cover both requested values, and the Android controlled-view test actually
draws with classic depth requested while counting disabled-depth halo draws.
Actual object silhouettes and scene-buffer contents remain a separate
verification step; this policy alone does not prove pixel occlusion

The extended Android test passes 22/22, run
`78b8caa470dd49c2a50b8f5d87411401` (28845 ms). Final state records 339 scene
draw submissions, 75 depth checks while classic was requested, and zero
disabled-depth draws. Requested preference is false again at completion;
the test's graphics setters do not persist configuration. Evidence:
`temp/xl-depth-policy-android-{test.log,result.json,state.json}`. APK SHA-256
`d3ecd43ddc35f33f1ccd9163a55103be31d0af5e020d96c4c1a2e77491cd0dcf`.
Both desktop pilot targets and stock `buildd2` with `DXX_XL_PILOT=OFF` build;
native lifecycle passes in 2.83 seconds, including actual pilot -> stock ->
pilot transitions. Both automation catalogs and scoped formatting pass.
Android build passes in 1m23s; warnings are in existing unrelated code, with
none introduced by this change. The emulator app was stopped after capture

Framebuffer regression and correction (2026-10-24): the initial pixel probe
disabled culling and missed a real defect. Matching normal clockwise front
faces/back-face culling made the old halo fan disappear despite successful draw
submissions and zero GL errors. Negative run `ad8f37814a494e46b402288c4c1d3561`
fails at step 12 with zero visible pixels. The renderer now winds clockwise.
The unchanged stricter test passes 16/16, run
`6d396ff0f87d461787764516f44160e6`, 19539 ms: visible halves 854/854 pixels,
nearer left-half depth 0/854, fully blocked 0/0. All lit pixels match authored
orange within quantization tolerance; depth writes are restored and GL errors
remain zero. See `temp/xl-corona-culling-{negative,fixed}-pixels.json`,
corresponding `-result.json` and `-test.log` files. Android build passes (52 s),
APK SHA-256 `72d9ae17a4aeb3885407d036e5d7666561fb3a711be50e3739a9ff49c60b8386`;
both desktop pilot targets rebuild successfully. This supersedes the older
pixel-output gap below, but actual scene/object occlusion, classic-depth mode,
and complete-resource reference observations remain open. The controlled FBO
does not prove those behaviors. The app was stopped after capture; no save
slots were changed

Corona rendering connected (2026-10-24): ordinary `DXX_XL_PILOT` hooks draw
procedural soft halos after the mine/object passes, using live emitter RGB and
state. The renderer preserves blend mode and depth-write state, uses the
existing depth test for partial occlusion, and rejects back-facing or
wall-occluded centers through native FVI. It does not alter baked vertex light
or require external artwork. Native lifecycle coverage sees all 112 emitters
from their own segment centers and checks 148 intervening solid-wall negative
controls. These geometric queries are distinct from framebuffer verification

Android `test_xl_pilot_coronas.jsonc` passes 15/15 steps on emulator-5554, run
`9bcb4879cf594c398e95137e5e111578`, 20446 ms. It performs normal imported-mission
startup, then deliberately repositions the camera to segment 40 facing authored
texture 405. This is a controlled rendering pose, not gameplay progression.
Introspection reports 157 corona draw submissions, active pilot, and zero
unresolved emitters. Evidence: `temp/xl-android-corona-final.log`,
`temp/xl-android-corona-result.json`, `temp/xl-android-corona-state.json`.
The screenshot `temp/xl-android-corona-final.png` was inspected, but the bright
lava texture makes it inconclusive about the halo's pixel contribution; an
isolated framebuffer comparison remains required before closing this gate

Preserved first attempt: `temp/xl-android-corona-test.log` failed on the wrong
assertion field `player.segment` (correct: `position.segment`), after native
rendering reported 98 submissions. Its delayed screenshot caught the player
after death and is not stable-view evidence. No engine behavior was changed
for that test correction

Both desktop pilot targets build, and all 14 lifecycle/escape/branch tests pass
(40.28 seconds). Android D1/D2 build passes in 52 seconds, installed APK SHA-256
`62ec2500c92631aabd22a480d64c798fd970dfefeddbb419baa8bb19a3a650e9`.
The unchanged `gr.c` unused declarations still warn; new code adds no warnings.
Classic-depth partial occlusion, object occlusion, framebuffer color, and
complete-resource reference observations remain unverified; no support status
is promoted

The ordinary `buildd2` target also rebuilds with `DXX_XL_PILOT:BOOL=OFF`
(`temp/xl-corona-stock-build.log`). Its first attempt was stopped by sandbox
denial of an existing vcpkg cache write; the authorized retry passes. Both
automation catalogs pass (374 top-level entries, 431 support scripts), scoped
formatting and diff whitespace checks pass. The emulator app was stopped after
the rendering test; no saves were written by that test

Live texture-emitter state implemented (2026-10-24): the opt-in query now uses
the source RGB, reference default brightness, current base/overlay textures,
bitmap names, native wall flags/types and light-subtraction mask. Its 94-entry
sparse brightness table reconstructs all 3640 bytes of the reference member
exactly. Native lifecycle coverage observes 112 emitters, 24 chromatic, checks
light switching and texture/wall removal for every eligible face, and rejects
emitters in inactive/stock state. Both desktop targets build, and lifecycle
passes (1.90 seconds). The query is preparation for the renderer: it does not
draw coronas or establish occlusion. See the linked audit for provenance and
the remaining reference-render prerequisite; admission remains unsupported

The complete 14-test host lifecycle/escape/branch sweep also passes (42.17
seconds). The new controlled emitter checks are required by the CTest wrapper,
so an older fixture executable cannot silently omit them. Scoped formatting
and diff whitespace checks pass

Android D1/D2 opt-in build passes in 1m 42s
(`temp/xl-android-emitter-build.log`), APK SHA-256
`f2d3d95d0bd5d0dc66d908d08d0e5e50d2955526028eb111d26a4dc3dc6ad24f`.
This APK was not installed or used for the earlier Android checks. Two existing
unused-declaration warnings in unchanged `d2/arch/ogl/gr.c` remain; the emitter
changes introduce no compiler warnings

Authored-color audit refined (2026-10-24): see
[the source/resource audit](../../XL_compat/evidence/pilot-authored-colors.md).
Side colors register the colored-level flag, already set by 440 fractional
vertex colors; no direct side-array rendering consumer was found in the pinned
source. Texture colors remain an implementation gap: seven chromatic texture
IDs yield 24 initial corona candidates after the reference brightness and wall
filters; texture 353's 12 uses are explicitly excluded by the reference.
These are static candidates, not verified render results

The audit found that the old reference screenshot has coronas disabled and
lacks the optional XL data pack. The reference's "found hog file" log is emitted
even if opening fails. Extracted the missing data HOG and corona/glare images
from the existing local 1.18.77 distribution without executing it. Exact hashes,
resource/version distinctions and failed historical downloads are retained in
the audit. Next: enable an explicit complete-resource reference comparison and
implement authored emitters with light state and occlusion, without double
counting the baked ambient light. No compatibility status is promoted

Android ZIP import/startup verified (2026-10-24): the 15-step import test passes
on emulator-5554, run `38ee731aef94466ab562f04a3825a4ee`, 23237 ms. The ZIP
contains the unchanged original HOG and descriptor. Manually staged mission
files were moved to `files/xl-manual-fixture-backup` before launch; the normal
`files/d2x-redux/missions` directory remains empty. The launcher imports and
enables the stored ZIP, discovers 24 normal and six secret levels, extracts it
under the active set's `.content/mod_support/mods/.extracted_mission_zips`,
and launches Ahayweh Gate through normal mission/difficulty menus. Assertions
confirm level 1, 38 robots, no invulnerability, active pilot shading and zero
unresolved emitters. This is import/startup evidence, not campaign qualification

Device-extracted SHA-256 matches the source for both files: HOG
`7491c45d217831509a840d934641bf1791e029ae6c1bcbe3e4efba2847d53d32`,
descriptor `cfd619db7eff6a37663188905aec1a1c96d50567613d07ac1dcfd97b621bb8ea`.
Evidence: `temp/xl-android-zip-import.log`, `temp/xl-android-zip-import-result.json`,
`temp/xl-android-zip-import-state.json`, `temp/xl-android-zip-manifest.json`.
The test is retained as explicit-only `test_xl_pilot_import.jsonc`; source ZIP
and mission bytes stay private. No compatibility status was promoted

The existing 34-step `test_xl_pilot_runtime.jsonc` then passes against that
imported mission, run `14987059725b4155b9548bbf83c36db2`, 81793 ms. It verifies
the controlled authored-light/opacity save round trip, switches to retail
Counterstrike with all XL state cleared, and restores the pilot save with the
same authored state. Evidence: `temp/xl-android-imported-runtime.log`,
`temp/xl-android-imported-runtime-result.json` and
`temp/xl-android-imported-runtime-state.json`. The app was stopped after the test
and the original quick slot restored from the private backup; both hashes match
`9100b0c247b7333c2fdb16ba0edd5d2348ab9b49d10293c6ec6bd86c8c519295`.
The imported ZIP remains enabled and the manual mission files remain preserved
outside the engine mission directory. These runs close import/startup and
imported-save/isolation checks, not the outstanding authored-color audit

Android interactive completion verified (2026-10-24):
`test_xl_pilot_completion.jsonc` passes 22/22 steps on emulator-5554, run
`710b245820084495831ed1dc81857b2a`, 25358 ms. It loads an earned checkpoint at
the actual exit approach; saved velocity crosses the native exit without an
endlevel command, teleport, resource changes or replay interception. The real
score screen identifies Ahayweh Gate, retains 50.9 shields/alive status and
awards the normal 5750 bonus (10850 -> 16600). After input suppression expires,
the normal advance request returns to the main menu. Options opens and closes
successfully. Final state: one window, no game, level 0 (normal Android cleanup),
XL inactive/completed, zero render diagnostics. Level 2 is not entered

Evidence: `temp/xl-android-completion-ready-test.log`,
`temp/xl-android-completion-result.json`, `temp/xl-android-completion-state.json`.
`temp/xl-android-score.png` visibly shows the title and bonuses; it was captured
from the preceding run that failed to wait for input readiness, not the final
passing run. The final installed APK SHA-256 is
`ae98fb2c174df49f3b40309dda49cd74c520ad0bac891741aade1e8fdf59529a`;
its build passes in 1m 55s (`temp/xl-android-checkpoint-build.log`). A warning in
the unchanged save-signature initializer at `d2/main/state.c:1516` remains

The checkpoint comes from
`temp/xl-compat-ninja/escape-tests/21cb2abcd971/preview-user/xl-normal-exit-approach.sav`,
SHA-256 `4a224622fd3b87cdaddb436937201e3c009ee7089eb616b96bc2b6ede3ab1f53`,
matched against the staged Android quick slot. The host fixture retains the
last naturally reached state in the exit's segment. Its private exporter uses
the real `mod_save_write` source-definition writer, captured from the loaded
mission before progression, plus the native effect-section offset. An ordinary
`DXX_XL_CHECKPOINT_EXPORT` hook is enabled only for the private host target.
It does not relax Android validation, alter saved world bytes or make desktop
saves generally Android-compatible. The original slot was preserved and restored
after the run; restored device SHA-256 is
`9100b0c247b7333c2fdb16ba0edd5d2348ab9b49d10293c6ec6bd86c8c519295`

Preserved failures: the first unannotated desktop save was correctly rejected
for missing source definitions (`temp/xl-android-completion-test.log`). The
next run reached the score/menu but incorrectly expected level 1 after Android
cleanup (`temp/xl-android-completion-with-sources-test.log`). The following run
sent advance before suppression elapsed (`temp/xl-android-completion-final-test.log`).
The final test explicitly waits for `screen_advance_can_activate`; no production
input behavior was changed to satisfy it

Desktop replay also now passes after loading the native exported `XLTest.plr`:
`temp/xl-escape-replay-result.json` reports terminal level exit at frame 407.
Every recorded result field matches except the expected exit-reporting fields:
recording stops before endlevel presentation (`endlevel_completed=false`),
while replay reports `terminal_exit=level_exit` and completion true. The source
recording is `escape-tests/0c58f275f059/preview-user/xl-normal-escape.dximdemo`.
This replay evidence is distinct from the Android score/menu test above

Both desktop targets and Android build; all 14 host lifecycle/escape/branch
tests pass (46.50 seconds), scoped lint passes, and both automation catalogs
pass (372 top-level entries, 431 support scripts). The mod-save generated table
was split into MSVC-compatible literals: its 28836 payload bytes are unchanged,
SHA-256 `3356bfba0b9b5a2c948d7dc03d5a4abd751a3e77798c8d33f8dac7a55e986a10`.
The later ZIP import check above supersedes this run's import gap;
authored side/texture-color semantics remain open;
production mission admission remains unsupported

Interactive-completion preparation (2026-10-24): the normal escape fixture now
records the earned reactor checkpoint and all 407 ordered flight-input frames
through the standard quick recorder. It verifies mission, checkpoint start mode
and full frame count, and exports `preview-user/xl-normal-escape.dximdemo` in
the private escape fixture. Recording does not replace gameplay: the same native
timed exit must still complete alive, with no resource or transform assignment.

This exposed an MSVC fixture defect: including `physfsx.h` before the metadata
entry point's JSON header applied engine byte packing to shared JSON types.
Staging the first recording operation crashed. Including JSON before engine
headers fixes it; the standard recorder and timed escape then pass. The initial
debug capture is `temp/xl-escape-debug.log`; the passing standard recording is
under `temp/xl-compat-ninja/escape-tests/d07c5e337daf/preview-user/`.

Do not claim interactive completion from this export alone. Earlier desktop replay attempts
exit with status 1 before writing an actual result. A PTY run identifies the
startup rejection: recorded missile view is disabled, while the desktop profile
enables it. Other projection fields match (640x480, aspect 65536, zoom 36864,
depth 9000, classic depth off). The subsequent passing run above matches the
real native profile without rewriting the recording header. Windows `inferno.c`
reopens stdout/stderr on `CON`, so redirected log files did not expose the error
Also, normal replay intercepts level exit before interactive score presentation,
so replay success alone cannot close that gate. The rendered score/menu path
is separately verified by the Android checkpoint run above. No production admission changed

Verification for the earlier recording slice: scoped formatting/lint and whitespace checks
pass, both desktop targets build, and all 14 lifecycle/escape/branch tests pass
in 36.36 seconds. The recording export assertion is required by the escape
CTest. Android runtime sources are unchanged by this slice

Current slice extends Android persistence to changed authored state. An explicit
automation-only fixture in `XL_compat/runtime/persistence_probe.cpp` switches
off an actual pilot light, snapshots every UVL and light-subtraction mask, and
changes wall 6 opacity. After quick-save it deliberately changes those values
again. Both immediate restore and pilot -> stock -> saved pilot must recover
the snapshot and deliver the restored opacity through the material adapter.
The helper is compiled only into the opt-in Android runtime; it is not called
by normal gameplay. This remains controlled persistence evidence, not a claim
that the navigator operated that light or wall naturally

That slice passes 34/34 steps on emulator-5554, run
`d8be973a6ce24757bb664894512324a9`, 73237 ms. Both `verify` operations pass,
covering 5,472 UVLs, 228 subtraction masks and wall 6's restored 20/31 material
alpha after immediate restore and the stock-mission round trip. The build and
scoped formatter/linter pass. Logs: `temp/xl-android-extension-build.log` and
`temp/xl-android-extension-test.log`; durable result:
`temp/xl-android-authored-result.json`. The actual copied save is
`temp/xl-android-authored-save.sg3`, verified against the device SHA-256
`3684e89cc74b2903e35211fefaf14a9e2012f5afacc25c9854c7c54929068670`.
APK SHA-256 is
`99d974df0a5db1f3e53d8f92dc49782f30e04961e5af176b1bab35a1846f7f46`

Infrastructure qualification: the first launcher startup timed out and the
harness recovery could not write its SDK lock; the second normal launch
succeeded and ran the full test. Preserve that log rather than interpreting
the pass as performance certification. The emulator and quick-save retain the
explicitly altered fixture state after this test

Admission boundary fixed: the loader now rejects both original pilot admission
and the explicit translated-preview path in multiplayer before translation or
stock loading. It clears extension state first. The real-loader lifecycle test
checks network and cooperative modes, stock-format loading in both modes, and
single-player recovery with authored colors/materials. The rebuilt host passes
all 14 lifecycle, timed-escape and branch tests (36.60 seconds), including the
required admission marker. This verifies the loader boundary, not multiplayer
UI presentation or support for playing the pilot cooperatively

Android follow-through for the guard: opt-in x86_64 APK build passes in 1m 33s,
SHA-256 `2644b45e8dc9344417ae90ac649e34ffe67f102ef4a920d1d0dca5c62aada823`.
Installed on emulator-5554; the unchanged runtime/persistence script passes
34/34 steps, run `c5a73780261e47cbb1da4f9c71f88b4c`, 67003 ms. Evidence:
`temp/xl-android-admission-build.log`, `temp/xl-android-admission-test.log` and
`temp/xl-android-admission-result.json`. Final state is unpaused Ahayweh Gate,
XL active/not completed, 2600 shaded vertices and zero unresolved emitters.
This rechecks single-player startup, authored-state restore and mission
isolation on the guard build; multiplayer rejection itself is host-tested.
The controlled modified light/opacity save remains on the emulator. Interactive
completion, ZIP import and authored texture-color semantics remain open

Android persistence follow-up: the expanded runtime test passed 18/18 steps
(run `f8e299cf00d54f799a24e30a64a581fa`, 33877 ms;
`temp/xl-android-restore-test.log`). It creates a real quick-save, changes
difficulty from 2 to 3 through the existing diagnostic API, then requires
quick-load to restore difficulty 2, Ahayweh Gate/level 1, active XL shading,
zero unresolved emitters, 38 robots, no invulnerability and an unpaused game
window. This is a controlled persistence test, not normal-input progression

The full 30-step extension passes (run `a9ea352c512542e585e4796bbcf0255f`,
68493 ms; `temp/xl-android-isolation-test.log`). It switches to retail
Counterstrike, requires all XL admission/render diagnostics to clear, then
quick-loads the pilot again. This specifically exercises the shared
`d2leva-1.rl2` filename on Android. The actual save is
`Players/save_sets/single/player/d2-xl/player.sg3`, SHA-256
`b489f910900592f5931fcf927195aaf95a944925288d59802db0b5e893525f21`.
The copied save and final result/state are preserved as
`temp/xl-android-pilot-save.sg3`, `temp/xl-android-isolation-result.json` and
`temp/xl-android-isolation-state.json`. Retain separate evidence for import, timed completion and authored
wall/light state changes; the initial-room restore does not establish those

Android opt-in slice implemented: the same ordinary `DXX_XL_PILOT` hooks now
build into the D2 shared library via explicit Gradle `-PxlPilot=true`, leaving
the default OFF, D1 unflagged and the JNI library name intact. The startup JNI
passes the exact-hash pilot flag only in that build. Guarded introspection
exposes admission, completion, shaded vertices, unresolved emitters and trigger
activation. Production launcher admission remains unsupported

Android x86_64 debug build passes. APK SHA-256:
`cbfbd07ed402f16c24b88f5cf6694027958d747a319d64c98a34794909c7f3a3`.
Original HOG/descriptor were staged in the emulator's private
`files/d2x-redux/missions`; this tests ordinary native mission selection, not
the ZIP-import workflow. A manual startup smoke test passed, then the reusable
`test_xl_pilot_runtime.jsonc` passed all 12 steps through `run_test.ps1`
on emulator-5554 (run ID `f17884367541404e92ae52b04c5a3ed1`, 25945 ms).
Assertions include Ahayweh Gate, level 1, 38 robots, no invulnerability,
XL active/not completed, shaded vertices > 0 and zero unresolved emitters

Evidence: `temp/xl-android-build.log`, `temp/xl-android-runtime-test.log`,
`temp/xl-android-introspect.json`, `temp/xl-android-smoke-result.json`,
`temp/xl-android-smoke.png` and `temp/xl-android-smoke-after-wait.png`.
The first capture contains a System UI ANR dialog, which was dismissed with
Wait; the later capture shows the unobstructed mine. Preserve that distinction:
this establishes startup and rendering-hook execution, not performance or
full Android qualification. Persistence, completion and import still need tests

Validation for this slice: scoped formatting/lint and whitespace checks pass;
the rebuilt host lifecycle, timed escape, inventory and lighting arithmetic
checks pass. Automation catalog validation passes with 371 top-level entries
and 431 support scripts. The Android smoke test is registered as explicit-only
because it requires the opt-in APK and private mission fixtures

The authored-material lifecycle now additionally checks native AI visibility,
object-to-object visibility and weapon-object FVI queries across all 14 opacity
walls. Each uses controlled objects and a solid-boundary negative control.
The full 14-test lifecycle/escape/branch sweep passes (35.21 seconds). These are isolated targeting/collision-query
fixtures, not evidence of live projectile damage or an uninterrupted playthrough

Further lighting audit: pinned XL `effects/glare.cpp:317-334` directly reads
texture RGB for coronas when ambient color is enabled. Thus texture colors are
not globally unused even in the legacy profile. `3d/fastmodels.cpp:752` gates
model light-manager use on `SHOW_DYN_LIGHT`; `lighting/light.cpp:737` similarly
gates segment-color use for non-polygon objects. Baked vertex lighting remains
valid for the legacy mode, but corona/texture-emitter presentation remains a
separate gap; do not silently certify the texture table from vertex tests

All 13 trigger actions now have normal-input coverage: 12 branch tests plus
the native timed exit. The latest complete sweep ran `xl_preview_lifecycle`,
`xl_normal_escape` and all 12 `xl_normal_branch_*` tests twice, with all 14 tests
passing both runs (72.96 seconds total). Each branch also saves and restores its
actual earned state, comparing linked-target texture IDs, wall type, flags,
state, hitpoints, opacity, doorway flags and player shields/energy. Diagnostic
activation history resets on restore rather than being mistaken for saved
gameplay state. This supersedes the failing branch sweep below

The controller now keeps firing distance from intact blastable walls and aims
at multiple patches as damage exposes transparent holes. It limits supply
detours during branch exploration. For the segment-52 puzzle, planning can
consider a firing position across transparent walls, but actual firing still
requires the native per-pixel projectile trace. It samples the switch face and
physically crosses the opened barriers for triggers 5/6 after activating 4.
No player transform, wall state, health, ammunition or trigger is assigned by
the navigator. The ordinary key/reactor/escape input policy remains separate

Latest passing artifacts (each directory contains `preview.log` and its earned
`preview-user/xl-normal-branch.sav`) under `temp/xl-compat-ninja`:

| Trigger | Branch frames | Artifact |
| --- | --- | --- |
| 1 | 5496 | `branch-1-tests/9275fca97d81` |
| 2 | 5410 | `branch-2-tests/83a7356aaab4` |
| 3 | 2968 | `branch-3-tests/9f786b71f994` |
| 4 | 14626 | `branch-4-tests/a8b5e223ff6a` |
| 5 | 14873 | `branch-5-tests/90e8145d8004` |
| 6 | 14912 | `branch-6-tests/a5d6ad3653b5` |
| 7 | 3319 | `branch-7-tests/c39bea15c74f` |
| 8 | 2863 | `branch-8-tests/bc3ef49d467d` |
| 9 | 2317 | `branch-9-tests/af1c95c2b2d7` |
| 10 | 1808 | `branch-10-tests/bfb7fc98897f` |
| 11 | 2956 | `branch-11-tests/f0b7587555d1` |
| 12 | Already exercised on key route | `branch-12-tests/fc58be3f147b` |

This proves activation, observed target passability and immediate save/restore
for all non-exit actions, not physical traversal of every linked portal or a
single uninterrupted all-secrets playthrough. Interactive completion, remaining
authored color/material semantics and Android qualification are still open

Lighting audit lead checked against local XL commit
`11038eb53dd86f3bcdc027dddd3f93c60f02536e`: `ReadColor` at
`gameio/loadgeometry.cpp:396` always reads RGB; its last argument only controls
registration of `bColored`. `LoadTexColorsCompiled` passing lighting method 0
does not discard texture colors. `CLightManager::AddGeometryLights` reads them
for static emitters (`lighting/dynlight.cpp:961,970`). The exact
`color.lights` member search finds only its side-table loader, but consumer and
profile semantics still require a bounded audit. Do not waive texture-color
support based on the registration flag or equate pixel parity with this issue

Trigger coverage is now observed from the real `check_trigger` path, separately
from linked-target passability sampled during normal frames. The guarded hook
records successful native activations and resets on load; it never activates a
trigger. Route evidence aggregates across the earned key/reactor saves. The
original key/reactor/escape route covers only triggers 12, 9 and 0

Registered `xl_normal_branch_1` through `xl_normal_branch_12`. Each uses the
normal earned key route and restores that save to explore a specific branch.
Trigger 12 is already exercised on the key route, so its test retains that
evidence instead of attempting to shoot an already destroyed switch again.
The controller can shoot across a transparent barrier from a reachable firing
position, sample different points on a switch face, and collect energy during
branch exploration. These are input policies, not changes to engine rules

Earlier sweep, superseded above: timed escape passes unchanged (407 frames, countdown
2504568, shields 3333686; `escape-tests/db733127f8f0`). Eight branch tests pass
with both native activation and linked-target passability; four fail. Earlier
passes from other controller revisions did not replace those failures:

| Trigger | Earlier result | Artifact under `temp/xl-compat-ninja` |
| --- | --- | --- |
| 1 | Pass, 5513 branch frames | `branch-1-tests/ee39d933aefb` |
| 2 | Pass, 5460 branch frames | `branch-2-tests/66f9114e67ce` |
| 3 | Player dies before activation, frame 8754 | `branch-3-tests/7d9dddbba817` |
| 4 | Alive at 36000-frame limit, no activation | `branch-4-tests/883dda3cc281` |
| 5 | Alive at 36000-frame limit, no activation | `branch-5-tests/a4ad4bcb12b2` |
| 6 | Player dies before activation, frame 16944 | `branch-6-tests/c83e9ac49e2f` |
| 7 | Pass, 4206 branch frames | `branch-7-tests/54d091099323` |
| 8 | Pass, 2763 branch frames | `branch-8-tests/86c33683d235` |
| 9 | Pass, 2317 branch frames | `branch-9-tests/a20ec063ed05` |
| 10 | Pass, 2555 branch frames | `branch-10-tests/6f438b5ad094` |
| 11 | Pass, 4422 branch frames | `branch-11-tests/2d0c5847f640` |
| 12 | Pass on the earned key route | `branch-12-tests/43cdebe63d83` |

That earlier sweep was deliberately failing, not qualified. Trigger 4's host in
segment 52 is behind the two closed walls it opens; approach planning now tests
projectile visibility rather than assuming the player can fly to every host.
Triggers 5/6 are on those barriers. Their actual activation sequence remains
unproven. Target logs distinguish door opening from the later flyable flag;
trigger 8 now demonstrates both. These checks do not yet prove physical player
traversal of every opened target or branch-specific save persistence

Branch exploration initially changed the established reactor route and caused
it to fail. Its additional supply/search policies are now confined to branch
exploration, and the ordinary key/reactor/escape policy passes again. Investigate
remaining failures from current position/goal/switch logs rather than selecting
a favorable random seed or altering combat rules. Lifecycle checks, opt-in and
default D2 game/metadata builds, scoped formatting/lint and whitespace checks
pass; the four branch failures remain visible

Timed native escape now passes in an initial run: `preview-tests/819174b41b20`
restores the earned reactor save without changing shields, energy or countdown,
then reaches native completion in 407 frames with countdown 2504568 fixed units
(about 38.2 seconds) and shields 3333686 (about 50.9). The input controller skips
supply/combat detours during escape; simulation, damage and the countdown remain
normal. No trigger/completion handler is called by the controller

The first run crashed after reaching the exit because headless startup has no
game window and `start_endlevel_sequence` unconditionally dereferenced it. The
ordinary live-exit visibility calls now check for a window. The guarded pilot
completion hook runs from `AdvanceLevel`, awards normal bonuses, and ends the
single-level session instead of trying unsupported level 2. Only the headless
pilot skips the modal score presentation. Interactive score/menu behavior still
needs runtime verification. `xl_normal_escape` is registered separately and
requires both earned key/reactor checkpoints and native completion evidence

Repeatability now passes: `escape-tests/656546d46f1e/preview.log` and
`escape-tests/90b01acb943b/preview.log` report identical 407-frame results.
Each then verifies completion/admission state clears through stock and returns
fresh in the pilot. `xl_normal_escape`, `xl_preview_lifecycle` and
`xl_stock_loader_probe` pass twice. Both opt-in game/metadata targets and normal
XL-disabled D2 game/metadata targets build. Scoped formatting/lint passes;
existing `weapon.c` return-path warnings are outside these changes

This closes the timed-route gate, not all branch, authored extension,
interactive or Android acceptance gates

Next reconcile coverage against all 13 inventoried triggers, with
activation and target-state evidence per branch. Keep authored side/texture
color semantics, material targeting/projectile behavior and Android deployment
as independent open gates. The Dot survey supplies fixture priorities and
requirement leads; neither its parser agreement nor an exit run can close those
gates by implication

Normal reactor milestone now passes twice. `xl_normal_reactor` first earns the
red key, restores that exact save without replenishment, then uses normal inputs
to collect reachable supplies, cross authored door triggers and destroy the
reactor. The controller now uses portal-aware waypoints with full player-radius
checks; a recenter fallback that oscillated in segment 35 was replaced with a
destination-center fallback. A secret-area detour required physically crossing
an authored open-door trigger to get back out. The navigator reads trigger links
to choose the route but never calls the trigger handler or assigns wall state

Two consecutive runs agree: red key at frame 4887 with ten kills, then reactor
destroyed after another 6819 frames, player alive with shields 3333686 fixed
units (about 50.9) and countdown 2949120 (45 seconds). This updates the earlier
red-key controller timing below. Evidence: `reactor-tests/9d0acf156549/preview.log`
and `reactor-tests/70e9c8528af9/preview.log`. Each contains the earned
`preview-user/xl-normal-reactor.sav` (322815 bytes). Lifecycle, red-key and reactor
tests all pass twice; build and scoped formatting/lint pass. The countdown is
not paused or extended. Timed exit and remaining branches are still unverified

Normal red-key milestone now passes reproducibly with seed 1. The input
controller's earlier failures below exposed slow target tracking, wasted firing,
and a missing input-loop step: `do_weapon_n_item_stuff` handles missile/item
inputs outside `GameProcessFrame`. The controller now uses that normal handler,
aims ahead of moving robots, engages visible threats before advancing, strafes,
and seeks nearby reachable shields/laser upgrades. All changes are input policy;
the engine still enforces turn/thrust limits, collision, firing delay, ammunition,
damage and pickup rules

Registered `xl_normal_red_key` separately from the assisted route and lifecycle
test. Two consecutive runs both finish at frame 4356 (72.6 seconds), with nine
robot kills, red key held, player alive, 12 shields and level-2 lasers. The
actual player checkpoint is `preview-user/xl-normal-key.sav`, 314487 bytes in
both runs. Evidence: `navigation-tests/b20dccdcb87b/preview.log` and
`navigation-tests/752c2e2ab257/preview.log`. The lifecycle test also passes twice;
build and scoped formatting/lint pass. This supersedes the failing red-key
controller result below, not the remaining reactor/exit/branch requirements

Normal-input navigation investigation: an optional `NAVIGATION=ON` invocation
of `tests/preview_lifecycle.cmake` adds `-xl-test-navigation`. It starts the real
pilot with 38 robots and difficulty 2, checks combat cheats/invulnerability,
finds a path across currently passable or normally openable doors, and drives
only `Controls` through the normal frame loop. It uses ship rotation and thrust,
aims at visible robots, and fires ordinary lasers. No transform, health, enemy,
weapon damage, door or key state is assigned by the navigator

The first untuned run reached the red key at frame 2487 with only 25804 fixed
shield units remaining (`preview-tests/a5120ec209cc/preview.log`). That run did
not explicitly seed the RNG and is not the reproducible acceptance baseline.
With seed 1, the controller dies before the key. The current lead-aim/dodge
version kills four robots but dies at frame 2943
(`preview-tests/eb6360b2550b/preview.log`). The diagnostic returns failure and the
CMake runner requires a living key-pickup result; it cannot silently pass by
skipping navigation. Ordinary lifecycle CTest remains separate and passes.
Improve controller combat/navigation from the logged aim, range and damage
evidence; do not select a favorable seed or change gameplay rules to qualify
the level. Reactor, exit and all required branches remain unverified

| Required for the pilot                                                           | Not a completion blocker                                                                            |
| -------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| Correct 15/33 decoding and guarded admission, no silently dropped XL fields      | Pixel-identical rendering, resolution, filtering, contrast, cockpit appearance                      |
| Authored XL colors and opacity with their actual visibility/gameplay semantics   | XL changes to classic player thrust light, headlight cone, flare RNG or explosion palette averaging |
| XL-only wall/trigger/segment/object/resource behavior if required by these bytes | Reproducing XL tweaks to classic AI, physics, damage, weapons or ordinary resource presentation     |
| Normal Redux combat/progression through every required pilot branch and exit     | Bit-for-bit matching of incidental classic runtime normalization to XL                              |
| Persistence of extension state and stock-state isolation                         | Identical random sequences or rendered frames between different engines                             |

Do not confuse this exclusion with permission to discard extension data: a wall
that changes targeting, a trigger that changes progression, or an authored color
table still needs correct support. Classify discrepancies by their cause before
waiving them. Keep evidence scoped to this level and single-player; campaign and
multiplayer claims remain separate

Guarded original-source admission and controlled save/load now have the evidence
described below. Next work is required extension semantics, resource closure and
normal unassisted progression. Stop adding classic renderer-parity
adapters or aligning screenshots solely for visual matching. Existing experimental
adapters and tests can remain evidence, but must not become mandatory production
behavior merely because they were implemented before this clarification

### Survey reconciliation and remaining qualification work

Rechecked the supplied Dot conversation and downloaded evidence ZIP against the
preserved bundle. The archive hash is identical. The repository verifier passes
manifest, JSON/CSV and aggregate checks and the native same-hash pilot comparison.
This rerun uses the existing native inventory; it is not a fresh corpus scan,
new native parse or gameplay test. See `XL_compat/evidence/README.md` for provenance
and the two excluded object sections

Dot's ten areas map to the catalog's 19 rows. Apply them to the pilot as follows;
corpus prevalence determines later fixture priorities, not pilot requirements:

| Survey area | Pilot decision and next evidence |
| --- | --- |
| Formats and packaging | Keep exact-hash 15/33 admission; test original descriptor/HOG selection and reject unsupported sources. No blanket version admission |
| Capacities and widths | Current serialized counts fit. Check reference bounds and runtime spawn headroom; widening all XL tables belongs to later fixtures |
| Events and object triggers | Inventory has 13 classic wall actions and no object triggers. Exercise every action and branch; classic action IDs alone do not prove semantics |
| Movement transitions | Do not implement corpus teleports/boosts for a pilot that does not require them. Confirm absence across decoded actions before marking not required |
| Segment rules | Classify each serialized property using v15 semantics; distinguish required XL behavior from classic loader normalization |
| Generators and spawning | No pilot matcens are inventoried. Validate contained-item/runtime spawn dependencies; do not infer all spawning is absent |
| Wall materials | Test all 14 authored-opacity walls for rendering, actor/projectile passage and targeting independently; alpha and doorway flags alone are insufficient |
| Lighting | Account for vertex, side and texture tables and changing-light persistence. Trace consumers in the pinned reference/profile; unused in one mode is not universally unused |
| Effects and audio | Preserve object identity and validate referenced resources. Corpus effect counts do not establish pilot requirements or make missing cues cosmetic |
| Resources and custom data | Close transitive dependencies and test failed-load rollback plus pilot -> stock -> pilot isolation; direct object references alone are insufficient |

Maintain a requirement ledger with source hash, serialized location/value,
reference consumer, implementation path, behavioral test and unresolved scope.
For each non-neutral extension field, either demonstrate the behavior or document
why the selected profile does not consume it. Do not turn an unexplained field
into a silent default or a `not required` result

Qualification order: finish the authored-material/color and dependency audit;
exercise required semantics in the real engine; then complete ordinary combat,
keys, all required branches, reactor, countdown and native exit with normal rules.
Keep the existing assisted-route artifact explicitly separate. Controlled save
mutation tests prove persistence only. Follow with stock-state isolation and
the intended Android launch/import path before claiming Android support

Single-player pilot qualification does not require implementing XL multiplayer,
all 30 campaign levels or modern hub persistence. Those claims remain unverified
and unavailable until separately tested. Grayscale or omitted cues may be useful
diagnostic modes, but do not fulfill this pilot's authored-extension requirement

### Original-source admission checkpoint

Authored-material collision follow-up: the real-loader lifecycle test now traces
from each of the 14 nonzero-opacity illusion walls' segment centers into their
adjacent segments using `find_vector_intersection`. Point, small-radius and
loaded-player-radius traces all reach the expected adjacent segment. Each trace
also has a negative control: temporarily closing that same wall produces a hit
on the exact tested side, then its original type is restored. This verifies wall
passage independently of alpha and doorway bit assertions, for both serialized
opacity values (21 and 31). The rebuilt Windows lifecycle test and scoped
formatter pass. Objects are excluded from these geometric traces; weapon/object
collisions, AI targeting and ordinary combat remain separate requirements

Added `-xl-pilot` to the dedicated runtime target. The wrapper reads original
`d2leva-1.rl2` through PhysicsFS, checks its exact size/hash, runs the bounded
15/33 translator and verifies the private translated bytes before loading.
A generated file-open hook substitutes only the level stream; it preserves the
original filename used by engine mission/save logic. Extension state is enabled
only after successful loading and geometry checks. This is opt-in pilot admission,
not qualification or acceptance of other levels/versions

Real-loader coverage passes for the original source, rejection of a one-bit
source change, restoration, preserved Gamesave_current_filename, and original HOG
resolution (verified with PHYSFS_getRealDir), followed by color/material checks
and archive unmount. Existing unrelated-level isolation still passes. Evidence:
`temp/xl-compat-ninja/preview-tests/09706a3229a9/preview.log`. Both runtime targets
build; base d1/d2 sources remain unchanged. The startup helper can select the
original d2-xl mission with -xl-preview-start -xl-pilot, but that full mission
startup and ordinary gameplay are still to be exercised

Next verify mission descriptor/resource precedence and run original-source
progression/save checks without the assisted route's robot removal, invulnerability,
boosted damage, paused countdown or position commits. Authored side/texture color
semantics and opacity/collision interactions remain extension obligations. Classic
renderer tuning and matched pixel images are not blockers under the latest scope

### Original-mission save/restore checkpoint

Resource traversal now also follows every side's base/overlay textures,
destroyed-texture replacements, linked effects (including critical/destroyed
variants), model texture effects and wall animation frames. Combat, pickup,
effect and door sounds resolve through `digi_xlat_sound` to nonempty loaded
samples. Passing totals are 136 textures, 25 effects, nine wall animations,
56 vclips, 780 paged bitmaps and 51 resolved sound samples. An initial failure
on sound 0 was a verifier error: effect playback consumes `eclip.sound_num`,
not the sound field of its embedded vclip; `gameseq.c` ambient setup and
`collide.c` effect destruction confirm that distinction. The check now follows
the actual consumer. Headless dummy audio does not establish audible playback
or spatial mixing. Override precedence/rollback and resource isolation still
need their own evidence; these counts are not full mission qualification

First normal-input simulation: after the persistence checks, restart the original
mission and drive the real player for 600 frames at 60 Hz through `Controls`
forward thrust and primary fire plus `calc_game_time`/`GameProcessFrame`. No route
actor is started and no objects, positions, damage values or countdowns are
changed. Reject combat cheats/invulnerability and require player movement,
energy expenditure, player projectiles, robot projectiles and shield damage.
The run passes: 82 observed player projectile signatures, 22 robot projectile
signatures, player alive in segment 193 with shields 5916348 fixed-point units.
This is live simulation under normal rules, not a rendered playthrough or route
completion. Input navigation, all required branches and the reactor/exit remain
unverified. Fixed forward/fire inputs alone are not a competent completion policy

Loaded dependency follow-up: after real `StartNewGame(1)`, the lifecycle test
traverses placed robots, contained robots/powerups, robot weapons and smart blobs,
weapon children, the conservative full player arsenal, reactor shots, model
variants, animation joint ranges and vclip frames. Referenced object bitmaps are
actually paged in and checked for resident payloads and nonzero dimensions. A
negative control replaces a reachable robot's weapon reference with an invalid
ID and requires rejection, then restores the original field. Build, lifecycle
test and scoped formatting pass. This closes those loaded-data reference checks,
not the entire resource gate: audio, wall/effect texture chains, custom override
precedence and failure rollback remain distinct obligations. It does not prove
that all attacks, loot paths or animations have executed during ordinary play

Added optional XL_PILOT_DESCRIPTOR coverage using the outer mission descriptor
and original HOG. The embedded descriptor differs in displayed title and text
line endings; the regular/secret level lists agree. Runtime checks confirm the
outer D2-XL: Counterstrike! title wins, with 24 regular and six secret entries

The real StartNewGame(1) path loads the original pilot, retains all 38 robots
and does not grant invulnerability. The persistence test switches one light off,
changes a live authored-opacity record, writes a normal engine save, changes the
in-memory values, then restores. Mission/level identity, energy/flags, robot count,
all UVLs, live wall opacity and XL ambient/material activation pass. These are
controlled state changes, explicitly not combat/progression evidence

The test uncovered that the included metadata initializer ignores -userdir.
Selecting a write directory after initialization also fails because the console
log is already open. The isolated test now selects and mounts its private
directory immediately after PhysicsFS initialization, before console startup.
CTest requires the save artifact in that directory as well as the final success
message. The corrected run passes. Remaining gameplay and extension obligations
are unchanged; classic renderer parity remains out of scope

Rendering investigation: the pinned local XL source loads vertex RGB into
`gameData.renderData.color.ambient` in `LoadVertLightsCompiled`. The static face
path runs `SetVertexColor`, `SetVertexLight`, then `AdjustVertexColor`. It includes
face-color blending, dynamic-light/color mixing, brightness and reactor/seismic
scaling. `ScaleColor` caps the largest RGB component to the supplied light value;
it does not simply multiply every component by that value. An implementation
that multiplies raw vertex colors into stock lighting is not reference parity

Stock `render_face` already supplies per-vertex RGB to `g3_draw_tmap` and
`g3_draw_tmap_2`, with vertex identities represented by pointers into
`Segment_points`. Investigate an opt-in XL runtime target with a draw adapter
confined to XL compilation before considering any base-source changes. This is
an integration candidate, not an implemented or verified rendering path. The
adapter must retain face context and XL light state; matching one static image
cannot substitute for dynamic lighting, reactor and save/restore checks

An isolated vertex-lighting calculation now exists in `XL_compat/lighting.cpp`.
Windows x64 and Android x86_64 analytical cases and 8,192 comparisons against
functions extracted unchanged from the hash-pinned XL `renderlib.cpp` pass. The oracle uses substituted data and
vector containers; it establishes arithmetic agreement only. No renderer hook or
production admission was added. Light generation, runtime state mapping, texture
and face-color setup, reference visual checks and normal gameplay remain open

The existing reference profile `temp/xl-reference-ar12/user/profiles/xlref.plx`
has SHA-256 `702102254d6905a89b96e41a2bcf89af3327480dc841922cd99e87ff0247e514`.
Its saved normal-profile settings are lighting method 0, color level 2 and
lightmaps disabled; contrast is 8. These are the saved settings associated with
the prior startup experiment, not newly chosen settings to make a comparison pass.
They still need runtime confirmation. XL's `SetMethod` enables ambient colors at
color level 2; other methods can regenerate ambient colors instead of retaining
the file tables. Do not generalize vertex-path parity to per-pixel/lightmap modes

Added `vertex_brightness` to reconstruct XL's initial shared-vertex brightness
from the maximum decoded face UVL light, using pinned side corner ordering. Tests
cover unsigned high-bit values, shared vertices, open portals and portal walls.
This supplies a required input for the lighting calculation; it is not a renderer
integration or proof of dynamic-light generation

Runtime integration checkpoint: a separate Windows x86 `xl-render-preview` build
now compiles using `XL_compat/runtime/configure.cmake`. Its generated source
overlay adds one checked drawing hook, and an isolated loader wrapper resets XL
state on every load and verifies complete fixture bytes against the source
translation before enabling colors. Normal source files/builds remain unchanged.
This is a static-light preview only: dynamic lights, reactor/seismic flash and
special face colors remain unintegrated. Build and scoped formatting pass;
runtime/lifecycle rejection tests and images are still pending. No gameplay or
production compatibility claim follows from this build

Lifecycle integration now passes against the real stock loader with the isolated
wrapper: inactive state, preview activation, unrelated-level reset, reactivation,
same-size one-bit coordinate mismatch rejection, and restoration/recovery. The
test exposed stock MSVC headers leaking one-byte packing into XL color structs;
XL public C++ headers now bracket their own eight-byte layout without changing
stock packing. Inventory and both lighting tests still pass. This exercises the
loader and direct color hook, not the render loop or normal gameplay; those
acceptance gates remain open

Use **D2-XL: Counterstrike!**, starting with **d2leva-1.rl2 only**, as the first
XL format and gameplay pilot. This is a candidate for implementation, not a
claim that the mission currently works. Full campaign support remains unverified
until all 24 regular and six secret levels pass their own feature and behavior
checks. Do not mark the entire mission supported after completing the pilot

The immediate engineering milestone should be a read-only feature inventory in
the native engine, with explicit unsupported-feature diagnostics, before enabling
XL levels for play. Keep game-format knowledge there rather than adding a second
production parser in Kotlin or a tooling script

## Existing notes

- [Archive analysis](d2xxl_mission_archive_analysis_20260823.md): original 35-archive
  investigation, duplicates, variants, standard versus extended HOGs and import results
- [As-is import work](plan_d2xxl_as_is_mission_import_20260823.md): archive retention,
  cache omission and import/metadata checks
- [Extended-HOG diagnostic](plan_d2xxl_extended_hog_diagnostic_20260823.md): six
  rejected XL archives and D2-XL as an importable negative control
- [PO2 study](plan_po2_engine_feature_compatibility_study_20260823.md): example of
  apparently usable content with missing weapon/robot data and executable behavior
- `game_data/mission_files/d2xxl_downloads/po2_compatibility_notes.md`: PO2 handoff
- `android/outstanding_bugs.md`: outstanding XL archive analysis item

Those historical import and metadata results do not establish gameplay support.
In particular, the archive analysis's wording that extended-HOG missions need a
converter describes a packaging prerequisite, not sufficient engine support

## Fresh inspection on 2026-10-23

Source archive:
`C:/Users/first last/Desktop/d2x-xl levels/d2x-xl/D2-XL.7z`

- Archive SHA-256: `24060d3addb78bdafb8691e7c3a3137543f92b1f4b2f68d694c28703a3d14f3c`
- Two outer files: `d2-xl.hog` (9,871,510 bytes) and `d2-xl.mn2` (514 bytes)
- Standard `DHF` HOG, ordinary `name` descriptor, 24 regular and six secret levels
- All 30 RL2 headers are level version **15**, game-data version **33**
- Current `d2/main/gamesave.c` accepts level versions 1 through **8** through
  `level_envelope_is_valid`; its `GAME_VERSION` is 32
- Therefore successful archive import is not evidence these levels can load today
- HOG inventory includes palettes, fonts, briefing/credits, PCX art and music,
  plus one POG for `d2levd-s`; no HAM, HXM, PMH, PTI or external mod tree was found
- Outer descriptor and embedded `d2-xl.mn2` differ in size (514 versus 521 bytes);
  descriptor precedence and contents still need reconciliation

First-level raw inventory, obtained without loading it into the engine:

| Item                         | Value                                                              |
| ---------------------------- | ------------------------------------------------------------------ |
| Level SHA-256                | `5e835e76e984c658531175640294a4b05f6127904884ba7acfd65c93165be5b2` |
| Level bytes                  | 149,235                                                            |
| Vertices / segments          | 828 / 228                                                          |
| Objects / walls              | 110 / 86                                                           |
| Geometry triggers            | 13                                                                 |
| Trigger types                | Five open-door (0), seven open-wall (9), one exit (3)              |
| Trigger flags                | All zero                                                           |
| Maximum trigger target count | Four                                                               |
| Object triggers              | Zero                                                               |
| Robot centers                | Zero                                                               |

These counts and trigger types make this a bounded first candidate. They do not
prove object behavior, wall semantics, geometry or lighting compatibility. Object
types/IDs, wall types/flags, segment functions/ownership, every link, colors and
asset resolution remain to be inventoried. No runtime test was performed

Local XL source inspected at `C:/local/d2x-xl`, clean checkout commit
`11038eb53dd86f3bcdc027dddd3f93c60f02536e`:

- `main/segment.cpp`, `CSegment::Read`: XL owner/group bytes precede segment data
- `main/side.cpp`, `CSide::ReadWallNum`: wall references become 16-bit at version 13
- `gameio/loadgeometry.cpp`: embedded vertex, side and texture color tables;
  version 15 uses different component encodings across these tables
- `gameio/loadobjects.cpp`, `ReadTriggerInfo`: game version 33 adds object triggers
- `main/trigger.cpp`, `CTrigger::Read`: checked serialized trigger layout against
  the pilot's raw records

Simply increasing the version limit would misinterpret data. Reading wider wall
IDs into the existing 255-wall/sentinel representation would also be unsafe
without range checks. The pilot has only 86 walls, so it should not require a
general wall-capacity expansion if all references validate

## Why this mission first

It exercises real XL level data while avoiding extended-HOG handling, a new
descriptor dialect and a separate mod-asset tree. Familiar Counterstrike content
provides a useful stock comparison, though the XL version must be treated as its
own authored content rather than assumed identical

Other candidates are less bounded for this first step:

- Sphere: historical unsupported `d2x-name` descriptor plus its large cache-heavy package
- Anthology: freshly listed extended HOG plus OOF models and external textures
- Boiling Point: freshly listed extended HOG plus external replacement textures
- PO2: historical evidence of unloaded custom gameplay tables, custom executable
  objectives and wingmen; a particularly poor candidate for an easy success claim
- A vanilla-format mission merely downloaded from the XL collection would be a
  useful control, but would not demonstrate an XL level-format implementation

## Acceptance contract

Keep separate results for archive import, format decoding, gameplay, presentation
and campaign coverage. Use verified / unsupported / unknown / not applicable for
each feature, with evidence attached. Unknown is never equivalent to supported

1. **Inventory before admission**
   - Pin archive/level hashes and engine/source revisions
   - Produce a native per-level manifest covering versions, counts and limits;
     object types/IDs and behavior data; wall types/flags/keys; trigger types,
     flags and targets; segment functions/ownership; lighting and asset dependencies
   - Classify every extra field and asset as implemented, verified neutral,
     unsupported or unknown. Do not silently drop records or substitute stock data
   - Report exact level, feature and record index for blockers. Preserve existing
     rejection until supported decoding and feature checks are available
   - Run inventory across all 30 levels, but select only level 1 for the first
     runtime milestone. Other levels retain their own blocked/unknown status
2. **Decode faithfully**
   - Implement only the explicitly audited version combinations; do not blanket
     accept every format through version 15
   - Check section extents, record counts, IDs, references and representable limits
   - Compare normalized geometry, objects, walls, triggers and colors against an
     independent XL reader/reference dump, including values rather than just counts
   - Test rejection of truncation, unknown semantics and unrepresentable wall IDs
   - Verify stock D1/D2 loading and metadata remain unchanged
3. **Verify level 1 gameplay**
   - Establish XL-extension behavior from the pinned reference/source for these
     exact bytes; retain Redux's behavior for classic features
   - Exercise every trigger and target, keyed doors, wall changes, robot combat,
     pickups, reactor/countdown and normal exit through gameplay actions
   - Check all reachable branches and secrets, not just a route to the exit
   - Save/reload before and after representative state changes and verify persistence
   - Use introspection assertions and reusable automation; warping to the exit or
     directly setting completion state cannot prove playability
   - Verify leaving the mission restores stock assets and behavior
4. **Verify authored extensions separately from visual fidelity**
   - Account for embedded colors, palette, textures, briefing, music and sounds
   - Verify authored colors, opacity and required resource semantics. Do not
     require matched images/audio for changes to classic 1996 features
   - Disclose visual differences, but do not block completion on filtering,
     resolution, classic emitter tuning or other non-extension presentation choices
5. **Expand only with evidence**
   - Level transitions, secret entry/return, all remaining levels and their assets
     need separate checks before a campaign claim
   - Coop, demo compatibility and other XL missions remain unverified until tested
   - If no reference runtime is available, retain static-analysis-only status and
     do not promote the pilot to gameplay verified

## Work status

### Isolation requirement (2026-10-23)

User requires XL code not to pollute the base game. All XL-specific parsing,
policy, conversion, diagnostics and tests belong under `android/XL_compat/`.
Start with a standalone native CMake library/tool, with no dependency from the
stock game and no changes to `d1/` or `d2/`. Future integration must preserve this
boundary; do not insert XL format branches into the stock loaders or weaken their
validation. Determine from the inventory whether faithful translation into stock
data is possible; unsupported behavior or presentation must remain explicit

- [x] Locate original notes and downloads
- [x] Select a concrete pilot and inspect its actual archive and level headers
- [x] Check key serialization differences against local XL source
- [x] Record evidence limits and explicit acceptance criteria
- [ ] Native feature inventory and diagnostics
- [ ] Independent semantic comparison and XL reference run
- [ ] Guarded version-15/game-33 decoder and rejection coverage
- [ ] Level 1 gameplay and presentation validation
- [ ] Remaining campaign and secret-level validation

## Isolated implementation checkpoint

`android/XL_compat/` now contains a standalone native library, inspection CLI,
synthetic tests and optional pinned-archive CTest integration. It has no dependency
from the stock engines, and `git diff --name-only -- d1 d2` is empty

The reader decodes all 30 levels' geometry, objects, walls, triggers, embedded
colors, header data, model-name tables, reactor links, generators and delta lights.
It accepts only the audited 15/33 version pair and never grants gameplay admission.
It explicitly retains unresolved dependencies and runtime semantics in the report

Fresh level 1 findings:

- 392 of 828 vertex colors, 36 of 1,368 side colors and eight of 910 texture
  colors have unequal RGB components. A grayscale conversion would lose content
- 110 objects decoded, including 38 robots (one Guide-Bot), four hostages, eight
  player starts, four coop starts, one reactor and 55 powerups
- The two special segments beyond fuel/reactor are existing blue/red goal types,
  not repair centers. All segment flags are existing ambient-water/lava bits
- No object triggers; the retained 700-entry legacy head table is entirely `-1`
- 70 light-index records and 3,960 delta-light records decoded

Verification completed: MSVC CMake build, synthetic CTest, 30-level private-corpus
CTest, Android x86_64 build and emulator execution of the synthetic test and full
archive inspection. Host and Android JSON reports are byte-identical. Scoped
format/lint checks pass. The Visual Studio CMake generator hit sandbox FileTracker
access restrictions; the Ninja/MSVC build succeeded. The installed formatter
required an approved elevated scoped run

Next: independent XL semantic/reference validation and asset-aware dependency
checks, then isolated gameplay/presentation integration. No converter or loader
has been enabled; colored lighting must be preserved rather than silently omitted.
The full acceptance contract above remains open, including runtime verification

### Reference runtime preparation

Downloaded the source repository's pinned Windows release `v1.18.74-ar12` into
`temp/xl-reference-ar12/`, verifying the GitHub release asset's SHA-256:
`1fb635000a0c12b6497915fff81b48b0b57c7fd902f1d2d616fc84b8d00fb7d4`

- Release: `https://github.com/arbruijn/d2x-xl/releases/tag/v1.18.74-ar12`
- Local source tag: `d2b3e3f1f1594d1d7da2ec545f6fc75d18043866`
- Executable SHA-256: `77db2a39a2293b10554986eeb0c6a4b664621cfc9839bf5686ad9bd402505e18`
- No diff between that tag and the inspected source HEAD for `loadgeometry.cpp`,
  `loadobjects.cpp`, `segment.cpp`, `side.cpp` or `objectio.cpp`
- Original local retail data and the XL mission have been copied into the scratch
  `game/data` and `game/missions` folders, with data hashes in `data-manifest.json`
- Separate scratch `user` and `cache` directories are prepared for the reference
  executable's `-datadir`, `-userdir` and `-cachedir` flags
- The reference executable loaded level 1, Ahayweh Gate, and saved an in-engine
  starting-room screenshot. Log: `temp/xl-reference-ar12/level1-reference-load.log`;
  screenshot: `user/screenshots/Ahayweh_Gate-0000.tga` under that reference folder
- Screenshot SHA-256: `a010ee9b8819920f72702b9951fed66e5761c0967b79ef3fd7e54e98d9fce399`
- Optional XL enhancement assets were missing and audio was disabled. No gameplay
  route was exercised. This is startup evidence only, not full presentation parity

### Developer stock-loader probe

The isolated `xl_probe_fixture` tool creates a deliberately unverified gameplay
fixture, requiring `--incomplete-presentation-probe` on its command line. This is
not production conversion or supported mission admission. Embedded colors remain
in the source report but are not applied by stock rendering

The unchanged Windows D2 headless metadata executable loaded the fixture and
reported 228 segments, 86 walls, 13 triggers, 110 objects, 38 robots, four hostages,
55 powerups and one reactor. The isolated `xl_stock_loader_probe` CTest reproduces
this check and verifies the original source remains unchanged. Synthetic and
stock-loader CTests passed after the probe was added

These counts prove only a bounded loader milestone. Independent value comparison,
asset validation, trigger/progression/save tests and faithful lighting remain open.
No base engine files have been modified and no mission has been marked supported

### Stock field audit findings

An opt-in separate D2 build using `XL_compat/stock_audit/configure.cmake` now
compares 22,295 loaded values against the original XL inventory. Geometry,
textures/UVLs, wall fields and trigger values/targets agree except for four
ambient flags: segments 51, 81, 82 and 111 change from 0 to 2. The audit exits
nonzero and preserves these differences as unresolved evidence

Stock `gameseg.c::set_ambient_sound_flags` recomputes ambient sound reachability;
the pinned XL source uses `main/findsegment.cpp::SetAmbientSoundFlags`, with
XL-specific passability checks. Source similarity alone does not establish that
the same segments are audible in the reference runtime. Do not waive the audit
failure without that comparison. Wall controlling-trigger reconstruction, object
normalization and color rendering remain outside this audit's coverage

The audit was then expanded to object identity, positions/orientation, size and
shields, contained items, physics/spin, AI, pickup counts and render records. It
now compares 29,120 values, with six mismatches: the four ambient flags above and
two player-start object 4 orientation components changed by one fixed-point unit.
Stock loading calls `check_and_fix_matrix` for players; XL calls its own matrix
normalizer. Keep the mismatch open until the reference values are established

All 225 checked direct object dependencies resolve against the retail tables.
This does not prove complete transitive asset coverage. A negative source with
robot ID 255 was rejected using its original ID, before a loader substitution
could be accepted. Physics, AI and rendering-record agreement is static loaded
state evidence, not a test of combat or dynamic rendering

### Actual draw-path checkpoint

The isolated Windows preview now has an explicit `-xl-preview-start` mode that
starts the private pilot fixture with ordinary enemies and no route assists.
The generated startup and graphics overlays remain under the dedicated build
directory; no `d1/` or `d2/` files are edited. The capture runs after 30 swaps
with successful calls to the XL vertex-color hook

The first post-event/front-buffer capture was entirely black and is rejected
as visual evidence. Capturing the back buffer immediately before swap produced
the starting room, cockpit and textured geometry at 640x480:
`temp/xl-runtime-engine/main/screenshots/scrn0001.png`, SHA-256
`08c92ff4151347bd391ee0b7efe21461ec2badd82cdf9b34822ef67adb4b66ad`.
It was visually inspected. This is not a matched reference-camera comparison,
and does not verify dynamic lighting, special face effects, combat or progression.
The pilot remains unsupported in the compatibility report. The four lighting,
inventory and real-loader lifecycle tests pass

Windows desktop automation did not discover the CLI-launched window; the helper's
own launch attempt timed out on app approval. Reproducible native startup and
engine screenshot capture allowed this check without relying on UI interaction

### Assisted route and checkpoint diagnostic

The fresh stock route build completed the translated pilot's red-key, reactor and
exit objectives in 1,739 simulated frames. Two fresh JSON reports were identical.
A separate red-key run saved a checkpoint at frame 713; another process restored
it with key flags 4 and completed reactor/exit objectives in 1,005 frames.
`xl_route_checkpoint_probe` reproduces this under the isolated CTest project

This is explicitly assisted diagnostic coverage: ordinary robots are removed,
the player is invulnerable, projectiles have boosted damage, the countdown is
paused and the diagnostic actor's position is committed for checkpoint saving.
The generated coverage report retains `playable: false`. None of the combat,
every-trigger/branch, ordinary playthrough, native-transition, presentation or XL
reference acceptance gates is marked complete by this result

## Survey reconciliation and revised execution order (2026-10-23)

See [evidence audit](../../XL_compat/evidence/README.md) and the revised
[extension catalog](../../XL_compat/EXTENSIONS.md). All supplied manifest hashes
match; the 131 distinct payloads/56 HOGs and principal feature counts were
recomputed from the supplied records. The original corpus was not rescanned.
The survey's pilot has exactly our source hash and agrees with native counts,
offsets, wall/cloak histograms and chromatic colors. Its wider v11-v23 coverage
is research evidence, not an extension of our native 15/33 parser

Ahayweh Gate remains the first pilot. The Apocalyptic Factor XL is a later
candidate family, not a replacement or a shortcut to completion. Do not require
unrelated teleports/generators to be ported before the pilot; do require evidence
that the pilot does not need each omitted feature. Static field agreement alone
does not establish behavior, even for classic-looking trigger types

Next acceptance work, in order:

1. Reconcile source/reference profiles, descriptor precedence and resource closure.
   Retain the six loader mismatches until reference values explain them; no waivers
2. Verify changing lights and material behavior together. Specifically inspect
   pilot walls 6-17 (type 3, cloak 21) and 81-82 (type 3, cloak 31), in segments
   99-105, 84 and 226. Compare visibility, targeting, collision, projectile passage
   and activation. These fields may be classic or XL-authored semantics; their
   presence does not prove either a new feature or equivalence. Include the
   existing 70 light-index records and 3960 delta-light records in on/off,
   flicker, shot-light, reactor-flash and restore checks
3. Match camera, assets and actual runtime lighting settings in the reference and
   preview. The real starting-room capture proves drawing only. Integrate dynamic
   light generation, face colors and state transfer before claiming fidelity
4. Run normal combat and every trigger/branch, red key, reactor countdown and exit
   without route assists. Save/reload before and after state changes; verify robot,
   wall, trigger and lighting state. Complete production loading, not just fixture
   translation. Neither grayscale nor assisted completion satisfies the user goal
5. Exercise pilot -> stock -> pilot and, when a second fixture exists, custom A ->
   stock -> custom B. Verify failed-load rollback and audio/model/texture/light
   cleanup. Save/replay/network representations must preserve XL state; record a
   single-player scope explicitly while multiplayer remains unverified. Do not
   admit XL multiplayer based on single-player tests

Broader backlog after the pilot: versioned parsing and widened references;
finite event interpreter/scheduler (masters, enable/disable, delayed/repeating,
object damage/destruction); teleports/boosts and route edges; old/new segment
properties; equipment generators; authored materials and effect/audio lifecycles;
mission resource precedence. Each subsystem carries persistence, replay and
multiplayer-consistency gates, rather than adding them at the end. Gameplay data
and cues move ahead of presentation work whenever progression depends on them

Smaller fixture progression: Apocalyptic Factor/D2-XL additional levels;
Lunar Outpost Reloaded/Speed! movement; Glass House/Crownpoint X materials;
Sphere/Boiling Point event/environment/generator slices; finally Monolevel and
Anthology capacity stress. Resolve Subzero/playgdxl object-parser uncertainty
before claiming complete inventories. Modern official 2.0 hubs and non-cubic
geometry require a separate scope/specification decision

Current-state clarification: earlier 'no renderer hook' and 'images pending'
paragraphs above describe historical checkpoints. The isolated preview now draws
and has a verified image artifact; dynamic/material fidelity, normal gameplay and
production admission remain open. The survey changes priorities, not support status

## Pilot wall-material implementation checkpoint (2026-10-23)

Source comparison found a concrete gap: XL `CWall::IsPassable` considers authored
cloak values when deciding render-through, and `WallAlpha` applies textured
opacity to ordinary XL walls. Stock Redux does neither for these pilot illusion
walls. XL `palette.h` sets FADE_LEVELS to 31, unlike stock GR_FADE_LEVELS=34.
Thus cloak 21 means alpha 10/31; cloak 31 means full opacity on this ordinary
textured path. It must not be mapped to stock cloaking fades

The isolated runtime now wraps doorway classification and textured face alpha,
using current live wall state. A checked generated OpenGL overlay supplies the
alpha to the three textured draw paths; per-face state clears after drawing and
on every load. It does not modify `d1/` or `d2/`. The wrapper supplements the
classic subset needed here; it is not a complete implementation of XL colored
walls, player/key-specific passage, camera surfaces or all door-state semantics

The real-loader lifecycle test passes with the added pilot assertions: twelve
cloak-21 walls use 10/31 alpha and retain fly/render-through flags; two cloak-31
walls remain opaque; per-face and stock-level state isolation holds. Both Windows
preview targets build. This proves the tested numeric/flag paths, not full visual,
collision or projectile equivalence

`-xl-preview-wall 6` places the diagnostic camera at the wall's segment center,
facing the side. It is explicitly repositioned visual evidence, not gameplay.
The default material capture is `temp/xl-runtime-engine/main/screenshots/scrn0002.png`
(SHA-256 f8921e835884735ae771e6e3fcc53448067280979929797ac02cb68ed205e9ae).
`-xl-preview-stock-materials` disables both material adapters for investigation;
its capture is `scrn0003.png`
(SHA-256 10c3d22f8e300ab33a94eda33c0bbd19424078c097a0109056b60b2be3172b47).
Both images were inspected. Animation/simulation timing differs, so their pixel
difference is not a clean material-only metric. A matched-time XL reference scene
and deterministic capture are still required. Both diagnostic processes stopped

Remaining priorities: independent material/reference capture, changing-light
integration, normal combat/progression, saved-state fidelity and production
admission. Compatibility remains unsupported

## Changing-light integration checkpoint (2026-10-23)

The preview now passes live reactor/seismic state and the engine flash scale into
XL vertex shading; previously its replacement RGB discarded those inputs. The
reactor zero-scale case follows XL FlashFrame's minimum one fixed-point unit.
This wires state into the arithmetic already compared against pinned XL source;
it does not establish phase/timing parity, automap behavior or dynamic-source
color generation. Stock and XL FlashFrame have additional lifecycle differences
that still need runtime comparison

Extended the real-loader test to exercise every one of the pilot's 70 light
index records through stock subtract_light/add_light. Expected corner values are
computed from the original source deltas using pinned XL light.cpp ChangeLight's
legacy-mode formula: ordered +/- byte * 2048, clamped at zero after each update.
All 70 sources change UVLs and pass exact comparisons, including repeated off/on
no-op checks. Feeding the changed live UVLs to the XL hook changes 5878 tested
colored corners. Add-back uses the post-clamp state; the test does not falsely
assume subtraction/addition must recover the initial values after saturation

The reactor/seismic test observes normal RGB 32426/32767/32305 and half-flash
16213/16383/16152 on its selected pilot vertex. A one-unit fixed truncation is
allowed at the expected 16384 peak. Both preview targets build; the lifecycle,
analytical lighting and pinned-source arithmetic tests pass. Logs are in
`temp/xl-compat-ninja/preview-tests/cb780d3f7acf/preview.log`

Coverage limits: scalar UVLs and direct color-hook output, not a matched rendered
on/off scene; not runtime reference light-manager behavior, object illumination,
dynamic colored-light generation, save/restore, or normal combat/progression.
All production compatibility gates remain open. This supersedes the earlier
statement that the preview does not consume reactor/seismic flash state

## Dynamic-light accumulation checkpoint (2026-10-23)

XL legacy lighting does not store the same state as Redux's final RGB light sum.
Its ApplyLight accumulates scalar fixed-point intensity separately; SetDynColor
keeps a color-present flag and blends successive source colors by pairwise average.
Unforced white colors are skipped; forced white colors participate. This is
order-dependent and not recoverable by averaging or normalizing Redux RGB output

Added `DynamicVertexLight` in the isolated lighting module with checked scalar
accumulation and the XL color rule. This receives an already-attenuated contribution;
it is not an emitter-intensity or distance calculation. A new optional
`XL_REFERENCE_LIGHT` source oracle extracts unchanged InitDynColoring/SetDynColor
from hash-pinned `lighting/light.cpp`:
`ca95b9183729c7f3665ef0706206afce093b53b0d97aa6ba11379dd5fdf5f36f`.
The test substitutes data containers, not color code, and compares 12288 steps
across fresh accumulation sequences, source colors, force flags and missing colors.
It passes on Windows x64 and Android x86_64 emulator-5556. Existing analytical and
pinned vertex-lighting tests also pass

The new state is not yet fed by the live emitter loop. Do not mark dynamic lights
implemented or turn the preview's static output into a supported fallback. Next
port the legacy ApplyLight path and its per-frame reset/visible-vertex selection,
then feed the separate scalar/color state plus original vertex brightness into
shade_vertex. Emission and attenuation must be ported from pinned XL rather than
inferred from stock final RGB

Specific source differences to preserve during that port:

- XL uses CFixVector::Dist where stock uses quick distance estimates
- XL robot emission uses lightcast as a gate and lighting as intensity; stock uses
  lightcast itself as intensity
- XL player emission uses thrust magnitude, unlike stock velocity-smoothed thrust
- Weapon colors use XL's weapon color table; missiles use 1/0.3/0; flare emission
  uses random intensity rather than stock's time/XOR pattern
- Fireball colors average resolved animation-frame colors and normalize by their
  maximum channel; resource overrides therefore matter to lighting
- Powerup color/enable rules, dim-source segment-local attenuation, source order,
  muzzle flashes and headlight cones must remain explicit

These findings change the next implementation action; they do not certify a
reference runtime, renderer, combat sequence or save. Dynamic generation and the
pilot's existing gameplay/production-admission gates remain open

## Partial live dynamic-light checkpoint (2026-10-23)

The isolated render overlay now feeds robot, debris and explicit OBJ_LIGHT
sources into separate scalar/color state each frame, then into XL vertex shading
with original vertex brightness. It selects visible vertices for ordinary point
sources and segment-local squared attenuation for dim sources. Both Windows
preview targets build. The real-loader lifecycle test passes for an explicit
point light reaching shading, state clearing on the next frame, and a weapon
source being reported unresolved. This supersedes the previous checkpoint's
statement that the accumulator has no live emitter connection

This is a partial port, not dynamic-light qualification. Player, weapon, fireball
and marker emitters are counted as unresolved. Muzzle flashes and headlights
are not covered by that counter. Powerup lighting follows the saved reference
profile's disabled setting; running-reference confirmation is still required.
Distance math mode, attenuation boundaries, object illumination and matched
frames remain unverified. Stock lighting still executes for stock consumers;
avoid duplicate random-number draws and lifetime changes when porting emitters

Before implementing player emission, preserve XL's distinction between local
pre-scale player thrust and remote object thrust. Stock final physics thrust is
not equivalent. Trace the raw controls, XL speed/damage multipliers and frame
reset together, then test the adapter against pinned source. Weapon/fireball
colors require resolved resources, so keep resource closure ahead of any claim
that combat lighting is faithful. Follow with matched normal-play scenes and
save/restore checks; none of the earlier gameplay gates is waived

Rechecked the user-supplied survey ZIP using the repository verifier and the
original pilot bytes. Manifest/CSV/JSON checks and selected native pilot fields
pass; the report remains playable:false. EXTENSIONS.md now states an explicit
evidence ladder from inventory through experimental loading, arithmetic, assisted
routing and per-level qualification, preventing any one checkpoint from silently
becoming a broader support claim. No full corpus rescan or new reference
playthrough is implied by this reconciliation

## Weapon and marker emission checkpoint (2026-10-23)

Ported ordinary weapon emission into the isolated frame loop: resolved weapon
intensity, forced source color, and the pinned CObject::InitTables missile
classification including robot missiles. CWeaponData::Create initializes weapon
colors to white; the inspected consumers do not assign a replacement table.
Missiles use 1/0.3/0. Flare randomness and multiplayer omega suppression remain
explicit unresolved cases rather than silently borrowing stock behavior

Markers now use XL's 8 * abs(0.5 - fractional lifetime) intensity, 0.1/1/0.1
color and segment-local squared attenuation regardless of intensity. Stock
compute_light_emission already adds one second to their lifetime; that preserves
the fractional bits used for emission. The XL hook runs afterward and does not
repeat the mutation. This dependency must be preserved or replaced explicitly
if the stock lighting loop is removed later

The real-loader integration checks orange player/robot missiles versus white
laser/mine sources, forced-white color mixing, strict point-source radius,
visible-vertex selection, marker pulse, single lifetime mutation ownership and
state clearing when dynamics are disabled. The checks use controlled weapon
intensities, not proof of retail weapon-table or complete resource parity

Next emission work remains local player thrust, headlight cones, flares, fireballs
and muzzle flashes. A specific fireball resource distinction is now confirmed:
XL CBitmap::GetAvgColor uses AvgColorIndex's nearest palette entry for indexed
frames, then averages resolved animation frames and normalizes their maximum.
Redux's avg_color_rgb is not equivalent. Decode/resolve the actual animation
frames and compare this path before claiming explosion-light fidelity. No
normal-combat, reference-image or production-admission gate is complete

## Indexed explosion emission checkpoint (2026-10-23)

Implemented indexed fireball frame resolution in the isolated preview. Source
inspection of XL gameio/piggy.cpp confirms that nonzero average palette indices
come from bitmap headers; GetAvgColor uses those before calculating a fallback.
The fallback ignores pixel indices 0/254/255, averages palette channels with
integer division, and selects the nearest palette entry among 0..254 with the
first equal-distance entry retained. Stock's closest-color helper excludes 254,
so it cannot be substituted. RLE and RLE_BIG frames are decoded by row through
the stock scanline decoder. Colors are averaged across the actual animation
frame IDs, normalized by maximum channel, and forced into the dynamic color
accumulator. Lifetime attenuation follows the pinned ComputeLightIntensity path

Controlled real-loader checks cover stored-index precedence over pixels,
noncontiguous frame IDs, raw and RLE_BIG fallback, index 254 selection, and
full/half intensity. Three retail clips (small explosion, player hit, volatile
wall hit) also exercise resource resolution after restoring the real palette
and bitmap tables. These tests do not prove exact reference resource resolution
or visual parity. No persistent bitmap-color cache is introduced, avoiding stale
palette/override state across loads while correctness is being established

This path only handles indexed GameBitmaps; true-color/OOF/TGA overrides and
XL-only explosion render modes remain outside its evidence. Player/headlight,
flare and muzzle-flash emission remain incomplete. Keep resource closure,
reference runtime comparisons and normal combat/progression gates open

## Muzzle-flash integration checkpoint (2026-10-23)

The dedicated build now generates a lighting.c overlay with a checked unique
timestamp hook inside cast_muzzle_flash_light. It snapshots queue contributions
before stock expiration, using exactly that loop's timestamp, then applies them
before object lights in the XL accumulator. Flash sources use visible vertices,
exact distance and minimum-distance clamping, with no source color. The stock
queue remains the sole owner of expiration; the adapter only reads it and clears
its private snapshot on load. No base d1/d2 files were edited

Pinned XL CastMuzzleFlashLight and stock share the one-third-second duration,
integer scale and signed 16-bit elapsed narrowing. Preserve that behavior,
including its historical wrap, rather than silently improving one engine during
a fidelity comparison. A runtime/reference timing comparison is still required

Real-loader tests exercise zero-age, half-age, last-live-tick and expired
contributions through colored shading, check that snapshotting does not mutate
the queue, and run stock set_dynamic_light to verify actual expiration reaches
the XL hook. Player thrust, headlights and flare randomness remain unfinished;
normal-play, resource, image, save/replay and production-admission gates stay open

## Local-player thrust light checkpoint (2026-10-23)

Added checked generated controls.c/object.c overlays for raw thrust capture and
player-reset clearing. The preview uses max(magnitude(raw thrust)/4, 2) + 0.5,
with unforced white color, for the normal single-player ship. Load and death
clear the vector. Headlight and multiplayer/hoard branches remain explicitly
unresolved rather than using this formula for those modes

The saved reference profile selects ship 0 and nDamageModel=0. XL's shipModifiers
entry has speed 1, and SubSystemDamage returns 0.5 with damage disabled, making
ApplyFlightControls' 2*DriveDamage multiplier one. This supports direct raw-vector
capture for that profile, not general alternate ship or damage-model support

Correction to the earlier investigation: updateobject.cpp's playerThrust zeroing
is in StopPlayerMovement, not a per-frame reset. Do not clear this state merely
because another render frame occurs. The adapter resets on lifecycle/control
events. Reference pause, guided-missile, respawn and save behavior still require
comparison; this checkpoint does not certify those paths

The real-loader test invokes read_flying_controls with controlled input and ship
thrust. It checks that final thrust doubles while the light uses the original
vector (expected peak 28672, one-unit truncation tolerance), then checks death
clearing reaches the 20480 baseline at the selected distance. Both preview
targets build and the lifecycle test passes. Player reset overlay compilation
does not itself prove respawn/save parity. Headlight and flare implementation,
runtime profile confirmation and all ordinary gameplay gates remain open

## Local headlight cone checkpoint (2026-10-23)

Implemented the legacy single-player, non-darkness headlight branch from pinned
ApplyLight. Its intensity is 10 fixed units; the radius test shifts distance by
three; source color is forced white. Normalized forward dot below 0.5 uses
intensity/(10*clamped distance); the cone uses fixed dot squared * intensity / 8.
No multiplayer maxHeadlightDist restriction is applied to this single-player
path. Multiplayer remains unresolved rather than borrowing local behavior

The saved reference profile has headlight available, no loadout device, darkness
off and lighting method zero. HeadlightIsOn therefore requires both possession
and on bits. Tests exercise front/back/side, a 3-4-5 angled direction, a point
beyond ordinary point-light range, the exclusive shifted radius, and each flag
missing independently. Both preview builds and lifecycle checks pass

This is face-light generation, not proof of reference rendering or object
illumination. Darkness cone sizes, alternate lighting modes, remote obstruction
tests and headlight energy-drain parity are not covered. Flare emission remains
the immediate missing single-player emitter; normal gameplay and all original
qualification gates remain open

## Flare formula and RNG isolation checkpoint (2026-10-23)

Inspection of pinned include/maths.h revealed that RandFloat(scale) divides by
scale, rather than multiplying. Therefore ComputeLightIntensity's flare factor
is 1.75..2.0. Added checked flare_intensity arithmetic to the standalone module
and connected it to one 15-bit Redux FX sample per evaluated flare. Do not use
the apparent but incorrect 1.75..5.75 range or stock's time/XOR flicker

The analytical tests cover multiplier endpoints, lifetime capping and zero life.
The real-loader test seeds the FX stream, verifies live color/intensity against
the sampled result and checks exactly one FX call with unchanged simulation RNG
state and call count. It restores FX state/count afterward. Multiplayer omega
remains an explicit unresolved emitter; it replaces flare in that negative test

Using Redux's FX stream is intentional integration, not a claim of exact XL libc
random-sequence parity. XL's platform RAND_MAX and shared global random consumers
can differ. Capture comparisons must control the random samples or assess the
specified flicker range/behavior, and ordinary combat/replay must confirm that
rendering does not perturb simulation. The emitter loop now has implementations
for the investigated normal single-player cases, but object illumination,
resource closure, reference profiles/scenes, normal gameplay, save/replay and
production admission still require evidence. A zero unresolved count certifies
none of those gates

## Linked-portal crossing checkpoint (2026-10-24)

Reconciled increasing-frame segment transitions in the existing escape, branch
and nine secret-entry logs against all 17 authored trigger-target records.
The original audit covers 10 records. Seven missing records share four physical
portals. Added private CTests for 176 -> 182 (trigger 1), 193 -> 194 (trigger 2),
136 -> 137 (trigger 3), and 5 -> 185 (trigger 5; target also linked by trigger 6).
Each restores the branch checkpoint earned with ordinary controls, verifies
unchanged resources, and requires the exact adjacent segment transition during
GameProcessFrame. Arrival must be alive with unchanged lives and no paused timer.
This is undirected portal coverage, not both directions or every shared-trigger
sequence, and not an uninterrupted all-secrets playthrough

Initial run: 3/4 passed; 136 -> 137 died at frame 220. Preserved evidence:
`temp/xl-portal-tests.log`, portal fixture `62b4b1e9157c`. Targeted damage logging
in fixture `2cf04b5f8d60` showed incoming damage while engaging a defender about
80 units away. The controller stopped forward motion during all fights, leaving
it in a narrow passage. For portal-crossing objectives it now continues toward
its waypoint at up to 15 units/second while the defender is over 60 units away.
It still uses normal thrust, rotation and firing; no world/resource/AI/damage
changes. The crossing then finishes at frame 126 with its earned shields intact

Final validation: all 29 navigation/lifecycle tests pass in 74.64 seconds,
`temp/xl-portal-regression.log`. Explicit crossing frames: 150, 242, 126, 3049.
`temp/xl-portal-crossing-final.json` links all target records to prior transition
logs or explicit crossing assertions and includes arrival/resource reports.
The original incomplete audit remains `temp/xl-portal-crossing-audit.json`.
Scoped formatting/lint and diff whitespace checks pass. Host engine and standalone
contract builds pass without new compiler warnings. No Android engine changes,
APK rebuild, production support promotion, or completion claim in this slice

Next: reconcile remaining extension/reference behavior requirements against the
current acceptance audit; keep image matching separate from required extension
semantics. Public asset-free CI remains insufficient for pilot qualification

## Ambient runtime comparison checkpoint (2026-10-24)

The prior six serialized-versus-loaded differences were not a reference runtime
comparison. Captured all 228 segment flags from the exact ar12 executable using
read-only process introspection, with offsets established from its retained
SetAmbientSoundFlagsCommon disassembly. gameData is read through RVA 0x445a60;
segment pointer is +0x450, last index +0x1ee8, stride 0x5f0, flags byte +0x5ae.
The reader requires the pinned executable hash and last segment 227. It never
writes process memory. The isolated reference has the previously verified
1.18.77 resources; its options and corona resources are captured again. The
owned process 44128 was stopped after capture. No baseline profile was changed

Evidence: `evidence/pilot-ambient-flags.json`, private reference files
`temp/xl-reference-complete/{ambient-runtime-state.json,read-ambient.ps1,
ambient-disassembly.txt,ambient-level-load.log}` and native diagnostic log
`temp/xl-compat-ninja/preview-tests/6e680d6b72e8/preview.log`.
The current host lifecycle test passes in 1.95 seconds and reports all native
segment flags. Build and scoped formatting pass

Results: 226/228 runtime flags agree. Serialized-zero segments 51 and 111 become
2 in BOTH runtimes. Segments 81 and 82 are 2 in Redux and 0 in ar12. Bit 2 is
ambient lava (not water); bit 1 is ambient water. Preserve this as unresolved
sound behavior. The two player-start orientation rounding differences remain a
separate question. Do not hardcode these segment flags or waive the difference

Next investigate source selection, transparency and resource/load timing around
the 82 <-> 83 closed portal (wall 3/2, texture 269) and nearby sound sources.
The XL transparency routine consults override bitmaps, layered base/overlay
transparency and optional automatic transparency; the stock routine differs.
No causal conclusion or production change is justified by the flags alone.
The test was run with sound disabled: sound-flag evidence is not audible playback

## Ambient discrepancy resolved as reference paging artifact (2026-10-24)

Added read-only reference diagnostics for nearby faces, texture-info flags and
bitmap flags, retaining exact-binary transparency disassembly. Both runtimes
have the same nearby lava-source textures and the 82/83 boundary texture 269 is
transparent after startup. Native recomputation after activating XL materials
still produces the same flags, rejecting the hypothesis that late material
activation caused the two differences

A fresh startup sampled the reference every 2 ms without writing memory.
`temp/xl-reference-complete/ambient-startup.json` records:
- 833 ms: texture 269 maps to bitmap 1498, bitmap flags 16 (paged out only)
- 2632 ms: last segment 227, segment 82 flags 0, bitmap still flags 16
- 3299 ms: bitmap flags become 1 (transparent), segment 82 remains 0

Source explains the timing: PiggyInitPigFile initializes resident bitmap flags
with BM_FLAG_PAGED_OUT and stores the PIG transparency elsewhere. LoadLevelData
calls SetAmbientSoundFlags before later palette/mod loading and texture caching.
CSide::CheckTransparency reads the resident/override bitmap flags. Redux's
piggy_bitmap_get_flags deliberately uses saved PIG flags while paged out

The controlled native experiment temporarily substitutes non-emitting opaque
texture 175 for texture 269 and recomputes ambience. Exactly segments 81/82 lose
the lava flag; every resulting segment matches the pinned ar12 observation
(serialized flags plus lava in 51/111). The test restores all original textures
and flags and asserts that active-material recomputation does not change them.
This causally reproduces the boundary effect without changing production code

Classification: classic bitmap-paging/ambient-reachability artifact, not an XL
mission extension. Retain Redux's transparency-aware ambience under the user's
permission to omit incidental classic-behavior changes. Do not hardcode segment
flags or suppress real sound cues to match the artifact. This closes the loader
flag discrepancy; it does not claim audible output or exact reference sound timing

Private readers: read-ambient-detail.ps1 and read-ambient-startup.ps1 beside the
captured JSON and transparency-disassembly.txt. Owned reference processes were
stopped; a first non-force Stop-Process attempt errored, and explicit Force plus
process inventory confirmed cleanup. No other process was stopped. Native
negative-control log: preview-tests/6176327d4832/preview.log (lifecycle passed in
2.00 seconds). The checked-in pilot-ambient-flags.json retains original differing
values, startup observations, scope and explanation

Final regression: all 29 navigation/lifecycle cases pass in 74.42 seconds
(`temp/xl-ambient-regression.log`), including the new all-228 reference flag
comparison under the opaque-boundary control. Scoped formatting/lint and
whitespace checks pass. The controlled native changes remain test-only

Next acceptance review should explicitly reconcile object illumination under
the selected legacy lighting profile and the category-by-category required-data
closure, rather than treating old historical TODO text as current blockers.
Pinned lighting/light.cpp:737 selects segment RGB for non-polygon objects only
when nLightingMethod is nonzero; the tested reference profile uses zero. Confirm
the rest of that consumer chain and record the supported profile explicitly

## Current requirement reconciliation and Android repeat failure (2026-10-24)

Added evidence/pilot-requirements.md mapping all 19 support categories to exact
pilot requirements and current evidence. Rechecked original inventory, archive
members and legacy object-light consumers. The reference method-zero path does
not apply authored face/texture RGB to polygon models: objrender clears the
object color index, SHOW_DYN_LIGHT is false, and the fallback is scalar classic
lighting with white base color. Alternate renderer modes remain unclaimed.
No need to port unrelated object-light tuning under the user's scope

Fresh full host suite passes 36/36 in 162.27 seconds:
`temp/xl-pilot-current-full-suite.log`. A new Android 28-step corona run fails
at step 16: run eb9cd0d5073142d8b9f7b56757c8fdc1, 23630 ms. Original APK hash
6b888da430cdc8e3aaaa28a80b56ffe60ce16fed9587ed263f9063dab4759c9f unchanged.
Evidence: temp/xl-pilot-final-coronas.log and corresponding result JSON.
Owned-FBO color/partial/full depth tests still pass. Object probe has one
visible-before sample, zero fully blocked samples, same FBO and zero GL errors

Hypothesis: robot bounding-sphere centers need not fall inside their animated
silhouette. Revised test-only probe samples a 5x5 projected body grid with small
halos behind existing robots. Every retained halo must be visible before objects;
a witness must be hidden afterward and visible again with depth disabled in the
same FBO. Record witness coordinates/radius/object ID. No robot, world or production
renderer change. Build/test still required; do not treat this hypothesis as proof
