# D2X-XL: sequential implementation queue for November 10, 2026

Purpose: turn the remaining original-content failures into working game behavior,
while continuing the recent reduction in test and implementation duplication.
This is an implementation plan for sequential Sol workers. No implementation in
this queue has been done merely by writing this plan.

## Working location and schedule

Work directly in `C:/local/dxx-redux`, on `xl_overnight_20261109`. The overnight
implementation is now commit `8fae81aa5d95cb6ef564a9abba7a11e42e346e31`, directly
on `cmake` commit `ada52ff949e988a2268fef6e920dfbd744f73a20`. It was rebased from
`59108b8e627d89faaac4432a4a395a1d0380ebed` without changing any file contents.
The test cleanup/private-asset repair is already included in `cmake` through #461.
The overnight tree is `83e81a57af4a7239b3a31c5f3ebdc6687ec23edd`.
Verify the checkout at worker startup; preserve any subsequent user changes.
Do not move the new implementation into another temporary worktree. Source and
the reviewable diff belong in this main checkout. Preserve existing stashes,
untracked plans, and historical worktrees. Follow repository commit/publication
instructions; ordinary implementation and validation need no extra approval.

The planning clock was approximately 09:15 PST on Tuesday, November 10. The queue
below budgets 9 hours 30 minutes from a nominal 09:30 start, finishing around
19:00 PST (03:00 UTC November 11). These are engineering estimates, especially
uncertain for the new particle kinds. They are not timeouts or deadlines. Start
when available, update the estimate after each item, and finish coherent pieces
if they run past 19:00. Do not rush validation, stop arbitrarily at 19:00, or wait
for a new time window if a worker starts late.

| Item | Work | Estimate | Nominal PST window | Dependency |
| --- | --- | ---: | --- | --- |
| W0 | Current-checkout builds and fresh introspection | 30 min | 09:30-10:00 | None |
| W1 | Warp 5 HXM animation normalization | 75 min | 10:00-11:15 | W0 |
| W2 | General stationary bubble profiles | 75 min | 11:15-12:30 | W0 |
| W3 | Shared waterfall behavior | 105 min | 12:30-14:15 | W2 shared particle boundary |
| W4 | Shared snow/rain behavior and genuine resources | 105 min | 14:15-16:00 | W3 emitter/resource structure |
| W5 | Combined original-level play and remaining bounded blockers | 75 min | 16:00-17:15 | W1-W4 outcomes |
| W6 | Consolidation, combined validation and deliverable builds | 90 min | 17:15-18:45 | All retained changes |
| W7 | Local review handoff | 15 min | 18:45-19:00 | W6 |

Use the listed order and one active worker at a time. W2 can proceed if W1 finds
a genuine external/content blocker; W5-W7 must cover the actual retained subset
even if a feature remains blocked. Fix an implementation defect before moving
on when the next item depends on that implementation. A diagnostic-only outcome
is not a completed feature. Do not broaden this into another exhaustive test
campaign if a piece finishes early: use the time to play the changed original
content, remove duplication encountered in the changed path, or fix its next
concrete blocker. Record scope changes and reasons in the ledger below.

## Established baseline and corrections

Read `temp/xl-implementation-20261109/HANDOFF.md` for historical receipts and
`build-artifacts.json` in that directory for binary identities. Its statement
that the source is uncommitted predates the local review branch above.

Already implemented: genuine XL resource import, slow-motion/bullet-time pickup
and controls, production frame clocks and typed saves, general camera owners,
lightning anchors, and several renderer repairs. Existing desktop and Android
Warp 6 feature checks include cold restore. They are assisted feature checks,
not proof of connected campaign traversal. Retain those implementations.

The previous combined candidate passed Windows D1/D2 host suites (89/100 passes,
one existing skip each), public Windows/Linux GPU/audio contracts (56/61), and
the private fast selection (113/113, no skips). Android's protected GPU owner
passed 33/33, but its final `state.json` was stale: W0 fixes that reporting path.
Original Sphere 2 and Warp 6 passed strict 120-frame rendered loading with XL
active. The unchanged eight-case headless readiness sample remains 4/8; its four
passing cases were XL-inactive. Keep those distinct measures and the same cohort.
The private-asset gate must remain passing; the known readiness result is not
permission to ignore a new failure or weaken either gate.

Test consolidation is already in the parent history: 77 files/34,020 lines were
replaced by five shared files/1,064 lines, and Gauss coverage was reduced from
240 to 24 cases. Do not rebuild that scaffolding. Prefer a few decisive cases in
existing reference/native owners and actual original-level demonstrations.

The remaining content diagnosis is more specific than the old handoff:

| Original content | Current evidence | Work selected |
| --- | --- | --- |
| Warp Shock 5, `volcano.hxm` | Robot 51 has invalid joint-list bytes in groups 6/7 despite only two guns; native animation uses groups 0 through `n_guns` inclusive | Prove all consumers' bounds, then normalize unused slots without relaxing live-data validation |
| Dinter, `dinter.rl2` | 17 findings cover 11 stationary emitters: five bubbles, five waterfall, one snow; six findings duplicate the subtype rejection in another category | General bubble profiles, waterfall, snow, then actual play |
| Boiling 2, `watrwrks.rl2` | Three bubble profiles plus one rain emitter produce five findings | Bubbles and rain, then actual play |
| Boiling 4/6, `canyon.rl2` / `canyon2.rl2` | Four bubble-profile findings per level | Bubbles, then actual play |
| Boiling 7, `control2.rl2` | Zero admission findings; actual loader cannot find authored `lava-fall.wav` | Preserve honest missing-resource failure unless an authentic file is located |
| Boiling 1/3/5 | Unsupported equipment/triggers or separate level-layout failure | Defer this wider campaign work |

All eleven Dinter emitters have control 0 and movement 0. The generic admission
message misleadingly mentions stationary/uncontrolled objects even when the
actual failed predicate is subtype. Do not begin a moving-emitter subsystem to
solve these levels. Expected Dinter finding counts are 17 -> 12 after bubbles,
12 -> 2 after waterfall, and 2 -> 0 after snow, if no other predicates change.
Those numbers guide diagnosis; zero findings alone is not gameplay acceptance.

## Shared execution rules and inputs

Read `AGENTS.md` and `.github/copilot-instructions.md`. Put shared XL behavior in
`android/XL_compat`, with minimal required engine hooks. Preserve stock D1/D2 and
Windows/Linux/Android builds. Do not introduce a general particle framework,
mission-name special cases, per-level runners, fabricated assets, modified
original levels, or test-expectation edits that disguise unsupported behavior.
Keep rejection of unsafe/unimplemented variants until their runtime is ready.

Use the pinned source at `temp/d2x-xl-reference`, revision
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. The implementation and existing
reference extractors are the starting point; read exact upstream branches before
changing their semantics. Do not update reference hashes merely to make a test
pass. Extend the existing reference/native owner only for meaningful new
behavior and the necessary invalid-data boundary.

Inputs already available locally:

- Original mission staging:
  `temp/xl-implementation-20261109/headless-readiness-final/`;
  case directories `0/extra` = Boiling, `2/extra` = Dinter, `6/extra` = Sphere,
  `7/extra` = Warp. Verify descriptors and hashes before use.
- Existing inspection reports:
  `temp/xl-implementation-20261109/{dinter-final,boiling-final,wshock-inventory}.json`.
  Use the existing native inspector and HOG member offsets, including extended
  `D2X` archives. Do not assume every HOG begins with `DHF` or write a second parser.
- Complete genuine desktop resources:
  `temp/xl-implementation-20261109/complete-runtime`.
  Retail staging: `temp/xl-stack-review-20261109/retail-data`.
- Original distribution: `C:/Users/first last/Downloads/d2x-xl-2-1-8.7z`,
  SHA256 `2cfc020fbf25a048f099511b71599c7be1b77836be8fc1112e5eb310c65beb9d`.
  `C:/Program Files/7-Zip/7z.exe` can list/extract it. Extract required members
  together; it is a large solid archive. Never run the installer.
- Private asset checkout: `C:/local/dxx-redux-test-assets`, pinned baseline
  `9dad21bd65ba05665ac6fe315f37fe7461e85725`. Follow existing catalog paths rather
  than guessing archive names or copying private content into source/APKs.
- Existing protected Android emulator was `emulator-5556`, AVD
  `DxxPhoneFog20261102`. Recheck device identity. Run Android owners serially,
  retain all profile/file guards, and restore changed device files exactly.

Use `temp/xl-day-20261110` for new logs, builds, and runnable packages, after
calling `android/helpers/retain-recent-artifacts.ps1 -Artifacts` with the planned
output. Keep a single handoff/progress trail, rather than a report per assertion.
Historical receipts are comparison inputs; do not overwrite them.

Critical build trap: `temp/xl-implementation-20261109/build-local.ps1` sets its
source to the old `source/` worktree. Its Linux private launcher does too. Do not
run them unchanged and attribute those binaries to today's root checkout.
Use `run-windows-build.ps1`, or adapt that one proven local launcher into the new
artifact directory with explicit `C:/local/dxx-redux` source and new build paths.
Reuse pinned dependency caches, not a stale source/cache association. Check
`CMAKE_HOME_DIRECTORY` and record the actual source/diff identity for every final
binary. Keep Linux build/output storage under persistent native `/var/tmp`, not
`/mnt/c` or a transient `/tmp` directory, for the existing timing-sensitive gate.

## W0 - Make new work and its evidence point to this checkout

Inputs: current branch, the build trap above, and the stale GPU `state.json`
documented in the old handoff. Primary files:
`android/helpers/test_helpers.ps1`, `android/helpers/run_test.ps1`,
`android/tests/test_xl_pog_runtime.ps1`.

1. Capture branch, HEAD, tracked/untracked status, and existing device identity in
   the new handoff. Preserve the eight pre-existing untracked plans. Establish
   one Windows D2 build of the root checkout with `BUILD_TESTING=ON` and
   `DXX_XL_DEVELOPER_TESTS=ON`. Prepare reusable D1/Linux configuration without
   rerunning every baseline suite. Verify source cache paths explicitly.
2. Inspect `Get-GameIntrospection`: it already has `-Fresh`, which removes the
   owned stale JSON, requests a new dump and polls for it. The final dump in
   `run_test.ps1` currently omits that switch and explicit serial; the POG owner
   then reads `files/introspect.json`. Route the final diagnostic through the
   existing fresh helper on the same serial. Inspect the failure path too.
3. Keep durable `automation_result.json` as the test result. A missing fresh dump
   must be reported as missing, never replaced silently by an old successful
   snapshot. Check current mission/level and available run/generation identity.
   Only touch the producer in `game_introspect.cpp` if a reproduced producer race
   remains; do not add another polling service or receipt framework.
4. Exercise the existing owner after a mission/level change so an old snapshot
   would be distinguishable. If root Android code has not changed, the existing
   matching APK can serve this helper-only check; identify it honestly.

Done: a root-sourced developer build works, a final dump describes the current
run, and the next worker has exact build/artifact paths. This is a small enabling
fix, not a new testing project.

## W1 - Load the original Warp 5 HXM through the normal path

Primary files: `d2/main/d2_ham_validation.c` and `.h`, `d2/main/bm.c`,
`d2/main/robot.c`, `android/XL_compat/runtime/resource_validation.cpp`,
`runtime/hxm_test.h`, `runtime/hxm_boss_test.h`, and `tests/reference_hxm.cpp`
under `android/XL_compat`. Read `evidence/hxm-gameplay.md` there as context.

1. Reproduce the original failure with today's executable. Original
   `7/extra/missions/wshock20.hog` SHA256 is
   `2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c`.
   Its `volcano.hxm` member begins at offset 8460823, length 13100, SHA256
   `e132d223d44349742889658f501eaa52d28256319833551973f8754d431f8d99`.
   Confirm these from the existing inspector instead of changing the asset.
2. Trace `bm_validate_level_hxm_internal`, `d2_ham_validate_xl_hxm`,
   `d2_ham_read_robot_meta`, normal replacement loading, and resource validation.
   The original has 27 replacement robot records and no model=-1 inert records;
   do not start by relaxing the unrelated inert-record rejection.
3. Diagnose robot 51: model 75, two guns, gun submodels 2/4. Groups 0/1 have valid
   two-joint lists, group 2 has empty lists, and groups 6/7 contain garbage,
   including a negative count. `robot_get_anim_state` asserts gun <= n_guns;
   `set_robot_state` and reference `SetRobotState` visit 0..n_guns inclusive.
   Verify all actual consumers and serialized layout, including resource closure
   and normal replacement loading, before treating the other groups as unused.
   Reference anchors: `objects/robot.cpp`, `gameio/loadmodeldata.cpp`.
4. If that reachability argument holds, normalize inactive slots at the shared
   validated XL loading boundary and retain strict checks on active counts,
   offsets, model/submodel indices and replacement tables. Scope policy so stock
   HXM behavior is preserved. Avoid separate preflight-only and runtime fixes.
   If the invalid bytes are reachable, retain rejection and report the exact
   record/offset and source evidence instead of whitelisting this file.
5. Extend existing HXM coverage with this semantic distinction and one decisive
   malformed-active-list control. Run the configured `xl_hxm_native` and
   `xl_hxm_geometry_reference` owners. Inspect `ctest -N` for exact availability.
6. Load original Warp 5 with full genuine resources, render it, and exercise a
   replacement robot actually used by that level with live AI/combat. Verify
   animation and normal save/load, a fresh-process restore, and return to stock
   content. Disclose position assists; do not change mission bytes or claim a
   whole-level playthrough from a short probe.

Done: the original HXM loads through ordinary production code and its active
models behave correctly, with invalid live data still rejected. If another
original-content dependency appears, record it and the extent actually verified.

## W2 - Admit general bounded stationary bubbles

Primary files under `android/XL_compat`: `particle_admission.h`,
`particle_effect.h`, `particle_bubble.h`, `fire_emitter.h`, `particle_geometry.h`,
`particle_world.h`, `runtime/particle_objects.cpp`,
`runtime/particle_resources.cpp`, `runtime/resource_validation.cpp`,
`runtime/particle_objects_test.h`, and `tests/reference_particles.cpp`.
Use `evidence/bubbles.md` and `evidence/particle-effects.md` as existing owners.

1. Replace the assumption that bubbles must match the single Pumo 27/40 profile
   with source-derived supported numeric/geometry constraints. Read reference
   `objects/effectobject.cpp::CObject::SetupSmoke`,
   `effects/objsmoke.cpp::DoStaticParticles`, and bubble branches in
   `effects/particles.cpp` / `effects/particle_emitter.cpp`.
2. Original Dinter objects 1486,1493,1495,1496,1504 all carry
   `[2,3,3,5,4,9,0,0,0,0,3,2,1]` in version 21/40. Boiling waterworks objects
   58/59 use `[3,6,2,5,3,9,0,0,0,0,2,2,1]`, object 61 differs in speed/drift;
   canyon/canyon2 include face-selector 0 profiles. Check the native face/point
   interpretation, geometry, color defaults, scale/lifetime and density directly.
   Do not replace a single profile allowlist with another list of mission tuples.
3. Preserve bounded arithmetic, valid segment/face geometry, water interaction,
   bubble sound ownership, 40 Hz source timing and FX RNG ordering. Support only
   actually implemented stationary, uncontrolled, single-player shapes. Improve
   the generic rejection reason when it conceals a subtype failure.
4. Reuse the existing emitter lifecycle and native object binding. Cover one
   original 21/40 profile, existing 27/40 behavior, and a meaningful invalid face
   or unsafe arithmetic boundary in existing owners. Avoid a Cartesian product
   of every numeric field or new per-mission fixture runners.
5. Reinspect Dinter and Boiling. Expected findings: Dinter 12, waterworks 2,
   canyon/canyon2 0. Actually render and observe bubbles in at least one newly
   admitted canyon level, including source enable/lifetime and warm/cold save
   behavior. A newly exposed missing resource is a blocker, not a passed play test.

Done: original stationary bubble profiles share one bounded behavior and the
existing Pumo bubble path still works. Record whether both Boiling 4/6 truly load.

## W3 - Add waterfall to the existing particle runtime

Primary files: W2 particle files plus `particle_render.h`, `particle_buffer.h`,
`runtime/particle_renderer.cpp`, `runtime/particle_scene_probe.h`, and the existing
`tests/reference_particle_{world,render,buffers,geometry}.cpp` owners.

1. Follow serialized subtype 4 through `particle_effect.h` into source internal
   type 3 and image index 6. Decoder support already exists; admission currently
   stops at subtype 3. Native `particle_objects_frame` currently sends unknown
   non-bubble/non-smoke kinds into the fire branch. Never just widen admission.
2. Read source waterfall creation, drift/gravity/collision, lifetime, opacity,
   orientation and emission rules. Reuse `AuthoredParticleEmitter`'s allocation,
   ring, age/retirement, object binding and RNG lifecycle. A small explicit kind
   or behavior policy can replace the current boolean template parameters if
   that simplifies these three additions; do not copy the whole emitter into a
   waterfall-specific implementation or build a plugin system.
3. Add the actual waterfall motion/render policy, resource preparation and native
   dispatch together. Image 6 uses `smoke.tga` in the pinned source, but with a
   different animation policy from ordinary smoke image 1. Reuse decoded pixels
   where sensible while preserving those image behaviors. Genuine resources
   have `smoke.png`; no invented `waterfall.png` is needed.
4. Preserve transparency ordering, portal/segment clipping, buffer limits,
   context recreation and enable/disable cleanup. Reuse the owner snapshot
   model: source behavior rebuilds transient particle clouds on restoration.
   Do not serialize a second particle engine or admit unsupported moving owners.
5. Dinter waterfall objects are 1483,1488,1508,1518,1520 in segments
   2549,2598,245,2551,259. Their serialized type is 4, face selectors vary,
   color is 180/200/255 with alpha 60. Use the existing report for exact payloads.
   Add a few reference/native observations that distinguish waterfall from fire
   and smoke, then inspect actual native rendering of original authored geometry
   using the shared scene probe/owner. Full Dinter loading may still await snow.

Done: the five waterfall owners no longer fail because of subtype, the native
runtime emits and draws the source behavior, and old fire/smoke/bubbles retain
their behavior. Expected Dinter findings fall to two, both from the snow owner.
Keep any incomplete behavior gated; no admission-only declaration of support.

## W4 - Finish snow and rain using the same lifecycle

Primary files: W3 shared files, `runtime/particle_resources.h`, and
`android/app/src/main/java/com/dxxredux/app/{ResourceModCatalog,ResourceModImport}.kt`.
Use existing importer tests and `android/tests/test_xl_addon_import.ps1`.

1. Read source branches for serialized snow 6 and rain 5. Keep serialized kind,
   internal particle type and image index distinct. The pinned image table maps
   snow to image 4, `simplesmoke.tga`, and rain to image 3, `rain.tga`. Do not pick
   `snow.png` merely because its filename sounds suitable. Verify source quality
   selection, dimensions, alpha, atlas/animation and motion policy.
2. Implement each kind's creation, movement, visibility, collision and rendering
   through the shared emitter and transparency pipeline established in W3. Retain
   source ordering and bounded counts, with explicit kind dispatch and rejection
   of unimplemented cases. Share lifecycle/plumbing; keep genuinely different
   motion/image rules explicit rather than disguising everything as smoke.
3. The original distribution contains `D2X-XL/textures/d2x-xl/rain.png` and
   `simplesmoke.png`. Extend the existing owned-resource catalog/import path for
   required genuine members and native case-correct preparation. Verify hashes,
   normal enable/restart/mount and missing-resource diagnostics. Do not manually
   preload private textures only in a probe and call the product path complete.
4. Dinter snow object 1492, segment 6743, payload
   `[4,2,5,4,1,10,0,0,0,0,2,6,1]`; Boiling waterworks rain object 60, payload
   `[10,6,10,6,1,10,255,255,255,128,5,5,1]`. Use these actual owners as integration
   witnesses. Add focused cases to existing reference/native/import owners,
   including absence of a required texture. Avoid a runner for every subtype.
5. Reinspect originals: expect Dinter and Boiling waterworks to have zero of the
   currently known particle findings. Run the strict actual renderer with full
   resources, then observe the snow/rain in real production frames. Exercise
   disable/enable if authored, level teardown and save restoration. Record any
   newly exposed dependency for W5 instead of claiming full compatibility.

Done: both kinds run with genuine resources through normal desktop/Android
ownership. Shared emitter behavior is understandable, and existing particle
types still work. W5 supplies combined play and platform integration evidence.

## W5 - Play the combined candidate and resolve its next concrete blockers

1. Rebuild from the root checkout after W1-W4. Run strict original rendered
   load/tick for Warp 5, Dinter 1, Boiling 2/4/6 and the existing Sphere 2/Warp 6
   controls. Read mission identifiers from their unchanged descriptors. Use
   `android/tests/xl_rendered_readiness.py`; require its existing 120 simulation
   and 120 rendered frames, no GL error, correct level identity, and XL active
   for these feature claims. Do not change its 45-second limit.
2. Go beyond admission and load/tick: observe newly supported effects from a
   suitable real viewpoint, move through the nearby original geometry with
   ordinary input and live AI, save through the existing path, quit, restart and
   restore. Reuse existing native automation, scene probes and protected owners.
   Disclose positioning/unlock assists and distinguish them from normal travel.
   Do not create a new protocol or repeated fixture chain to describe each step.
3. Run a compact Android scenario on original newly supported content, including
   real import of the added resources, restart, visible effects, and cold save
   restoration. Generalize data selection in an existing owner if needed rather
   than adding another large top-level PowerShell runner. Preserve its file/AVD
   guards. With renderer changes, reuse the existing protected GPU/context owner.
4. If actual loading reveals another shared dependency, diagnose and fix a
   bounded production defect and its existing coverage. Keep the queue estimate
   current. Do not expand into unrelated campaign systems or alter original data.
5. Boiling 7 needs authored `lava-fall.wav`, absent from the two already checked
   original packages and distribution. Spend at most roughly 10 minutes checking
   available authentic manifests/locations not already searched. If unavailable,
   leave that content blocked with a precise filename/search record. Do not alias
   a different waterfall/lava sound or generate a replacement.
6. Fresh final-binary audibility remains unverified. If the host output is
   usable, do one real sound check. Bound recording calibration to 10-15 minutes:
   previous independent WinMM and WASAPI tones both recorded all zero, so repeated
   harness debugging is not a good use of today. If calibration still fails,
   record the external limit and keep game-side mixer checks; no system audio
   changes, microphone use, or claim that an old recording proves this binary.

Done: each targeted level is classified as actually demonstrated, failed with
reproducible cause, or unverified. No full campaign or physical ARM claim follows
from these checks. Actual gameplay failures stay visible in the final handoff.

## W6 - Consolidate and validate the combined source once

1. Review the complete diff against `8fae81aa5`. Remove temporary logging, unused
   helpers, duplicate subtype dispatch and repeated test setup introduced by this
   queue. Preserve comments that explain source semantics. Update the existing
   particle/HXM evidence and README with actual supported behavior and limits;
   do not rewrite every historical report. Favor one clear owner for validation,
   resource preparation, emitter lifecycle and renderer integration.
2. Check test additions for real failure detection. Keep compact source/negative
   controls and one shared integration sequence. Remove superseded checks when
   coverage is demonstrably redundant; do not delete unrelated valid coverage to
   hit a numeric target. Avoid new top-level runners. Record net test/code changes
   and remaining duplicated owners that genuinely justify follow-on work.
3. Run the repository scoped mixed-language formatter once on changed code. Wait
   for it to exit. Build the final Windows D1/D2 and run their configured full
   host tests. Run canonical public contracts on Windows and Linux GPU/audio, and
   the canonical private fast selection on fresh native Linux outputs. Expected
   baseline counts are context, not forced totals; explain legitimate changes.
   The private gate's names, criteria, selection and timeouts remain unchanged.
4. Re-run the frozen eight-case readiness with its same original catalog/data and
   current engine, recording exact per-case changes. Keep supplemental full-
   resource rendered results separate. Do not shrink the cohort, switch data
   silently, or relabel the historical 4/8 as full XL playability.
5. Run affected Android unit/import checks and the shared particle/GPU integration
   against the final source. If renderer changes affect the protected full GPU
   owner, run it through completion/context recreation, not only a tiny subcase.
   Run both automation catalog checks if test wiring changed and before any
   authorized test commit. Preserve accurate optional-asset and hardware skips.
6. Build a universal debug APK and Windows runnable folder from that final tree.
   ARM ABIs must build; only claim execution on the devices actually used. Verify
   final package hashes, current source/diff identity and resource manifests.
   Smoke-launch the exact packaged EXE/APK when feasible. If code changes after
   a failure, rebuild and repeat affected checks; otherwise do not keep rerunning
   passing suites merely to consume the remaining schedule.

Done: no newly introduced failure is hidden; all final receipts identify the
delivered source and binaries. If a required check cannot run, explain the actual
environment blocker, what was attempted and what remains unverified.

## W7 - Leave a straightforward local review

Keep source changes in `C:/local/dxx-redux` on the review branch. In
`temp/xl-day-20261110/HANDOFF.md`, give the actual branch/base/HEAD and working-diff
identity, a short behavior summary, exact changed-source review commands, runnable
Windows launcher and universal APK paths/hashes, ordinary resource/import/control
steps, final check results, and the precise implemented/blocked/deferred list.
If no new commit was requested, say explicitly that today's changes are the root
working diff; do not imply that naming a branch commits or publishes them.

Put durable feature/support notes in the existing Git-visible evidence docs and
the progress ledger here, since scratch artifacts can be cleaned. Do not leave
the only implementation in a patch or hidden worktree. Do not announce the whole
stack merge-ready merely because local checks pass. Update the ledger and give a
short final response with clickable source/plan/artifact paths.

## Command anchors

These are entry points, not instructions to rerun every suite after each item.
Use fresh owned output directories and inspect supported options first.

```powershell
# Root checkout; default host helper supports both games
.\run-windows-build.ps1 -Target both -BuildType RelWithDebInfo
# For the cached developer build, adapt the old build-local.ps1 as described
# above, keeping BUILD_TESTING=ON and DXX_XL_DEVELOPER_TESTS=ON for D2
ctest --test-dir temp/xl-day-20261110/build-d2 -N
ctest --test-dir temp/xl-day-20261110/build-d2 --output-on-failure

python android/XL_compat/tests/public_contracts.py `
  --work temp/xl-day-20261110/public-windows `
  --reference-root temp/d2x-xl-reference
# On native Linux, use a fresh /var/tmp output and add --gpu --audio

python android/tests/xl_rendered_readiness.py `
  --engine temp/xl-day-20261110/build-d2/main/d2x-redux.exe `
  --data temp/xl-implementation-20261109/complete-runtime `
  --extra temp/xl-implementation-20261109/headless-readiness-final/6/extra `
  --work temp/xl-day-20261110/sphere-rendered `
  --mission sphere --level 2 --level-file sp-02.rl2
# Other original levels use their actual descriptor names and new work paths
# New particle textures must also be present in the owned data root when needed

$env:JAVA_HOME='C:/local/jdk-21'
$env:Path="$env:JAVA_HOME/bin;$env:Path"
Push-Location android
.\gradlew.bat assembleDebug assembleDebugAndroidTest --offline --max-workers=2
Pop-Location
# An x86-only iteration build is fine; the final APK must include all normal ABIs

pwsh -NoProfile -File android/tests/test_validate_automation_catalog.ps1
pwsh -NoProfile -File android/tests/test_run_all_tests_catalog.ps1
```

For private validation, the canonical owner is
`android/tests/run_xl_originals.py --repo <current-root> --work <fresh-native-path>
--reference-root <pinned-source> --fast`. Its staged assets normally live under
`<repo>/game_data/retail-ci`. Reuse the previous native-Linux launcher's physical
asset-path adaptation only after correcting its hardcoded source and build paths.
Preserve canonical tests, names, timing limits and behavior. The old launcher is
`temp/xl-implementation-20261109/run-private-linux-nativefs.py`.

For frozen readiness, inspect `android/tests/xl_readiness.py --help`, the existing
catalog and prior `headless-readiness-final/results.json`; use the same catalog
and staged original input. The available commands are `inventory`, `prepare`,
`run`; do not regenerate a different selection to obtain a better headline.

## Explicit follow-on work outside this queue

- Omega/input-demo render-clock timeline: a separate cross-cutting determinism
  effort; retain the current replay guard until that work is actually complete
- Boiling equipment IDs 7/8/9/24/26/27/46/47, trigger 22/object trigger 21, and
  `subzero.rl2` layout failure at object 4/movement 255: inspect individually in a
  later tranche rather than broadly admitting unused/multiplayer semantics
- Moving particle owners, unsupported control modes, multiplayer, full campaign
  traversal and physical ARM runtime qualification
- Missing authentic sound assets that cannot be obtained from the available
  originals; these remain content requirements, not synthetic fixtures to invent

## Progress ledger for sequential workers

Update this table after each item and append only a concise handoff entry: actual
files/functions changed, commands/results and source identity, original-content
outcome, remaining blocker, next item, elapsed time and revised finish estimate.
Do not equate an elapsed estimate, admission-only success or missing asset with
implementation completion. A blocked item must state the evidence and dependency;
independent later work can continue without pretending that item succeeded.

| Item | Status | Actual result / handoff |
| --- | --- | --- |
| W0 | Done (Linux config pending) | `run_test.ps1` now uses the normalized serial and `-Fresh` for final/failure diagnostics; `test_xl_pog_runtime.ps1` re-requests fresh state, checks level/mission identity and permits a new output directory. Root D2 developer build and D1 configure point at this checkout. Protected secret-level owner passed 31/31 after level transitions: final Neptune level 1, mission generation 4, request generation 3, rather than seeded stale level 999; exact device restoration. Scene-only visual assertion failed and protected GPU owner hit its unchanged 480s timeout; both were restored and remain failures. WSL access denied/hung, so native Linux configuration is pending. Branch `d2xxl_nov10`, HEAD `ebf245302`, same file tree as reviewed overnight through merged parent `b3bb12cbe`; no original target level played in W0. See `temp/xl-day-20261110/HANDOFF.md`. W1 next; at 12:30 PST revised queue finish about 21:30 PST. |
| W1 | Done | `d2_ham_validation.{c,h}` normalize only inactive XL HXM joint groups in a validated copy; `runtime/resource_validation.cpp` installs the same normalized native lists, leaving stock HXM strict. Extended `test_d2_ham_validation.c`, existing `xl_hxm_native` with a rendered original actor witness through `route_confirmation_desktop.cpp`, and `evidence/hxm-gameplay.md`. Pinned XL source `2576a98b5de310e3ac7c9777df2677d1627b8bae`; unchanged Warp HOG SHA256 `2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c`, original volcano HXM SHA256 `e132d223d44349742889658f501eaa52d28256319833551973f8754d431f8d99`. Original Warp 5 failed admission before, then passed a strict 120-frame unassisted rendered load; original robot 51 object 226 passed active animation/gun transforms, assisted live AI weapon 21, damage, normal warm save/restore, separate-process cold restore, and stock return in a real OpenGL renderer. Source authored segment 528 becomes runtime segment 1040 after normal relink. `xl_hxm_geometry_reference` and `xl_hxm_native` passed 2/2 (79.58s), root `test_d2_ham_validation` passed; `git diff --check` clean. This is a short controlled actor witness, not a whole-level playthrough. No external blocker for W1; W2 next. At 13:00 PST revised queue finish about 20:45 PST. |
| W2 | Runtime implemented; original canyon play blocked by external audio names | `particle_admission.h` replaces the Pumo-only bubble tuple with source-derived decoded/geometry/emitter bounds while retaining stationary owner restrictions and subtype 4-6 guards; `admission.cpp` uses that shared preparation. `particle_effect.h` now matches the pinned source's MSVC FX point-jitter draw order. Extended existing `tests/reference_particles.cpp`, `tests/effect_layout_contract.h` (invalid subtype 7 instead of now-supported bubble 2), `runtime/particle_objects_test.h`, and `evidence/bubbles.md`. Pinned source `2576a98b5de310e3ac7c9777df2677d1627b8bae`; original Boiling HOG SHA256 `f0c7363c992bdcf0222873e63bcf7fff699c6f53ca28ab8ea9437f953b190ce2`, descriptor `a9e35d1ca3e77693c1bee2eab8e353a08ee108f9da0ac2b1e348b9107722433db`. Inspector findings Dinter 17->12, waterworks 5->2, canyon/canyon2 4->0. Existing `xl_effect_decode_reference`, `xl_effect_payloads`, `xl_effect_inspection_gate` passed 3/3 after rebuild (34.72s), with 21/40 face/point and 27/40 controls; invalid face remains rejected. Actual strict rendered Boiling canyon (descriptor level 3, despite the earlier plan's level-4 label) and canyon2 (level 6) both failed `level_load`: authored `dripping-water3.wav` / `dripping-water2.wav` absent from original HOG and owned folders; the genuine distribution has differently named `dripping-water-3.wav` / `dripping-water-2.wav`. No original canyon render or warm/cold save result. This is an external content-name blocker, so W3 proceeds without claiming the play acceptance. At 13:15 PST revised queue finish about 20:00 PST. |
| W3 | Runtime/native fixture complete; original authored-geometry render pending | `particle_waterfall.h` adds source waterfall creation, decay-one directed movement, fade-2 opacity, collision/outside retirement and 8x8 animation; `fire_emitter.h` now shares the existing ring/lifecycle through an explicit `ParticleKind` policy. `runtime/particle_objects.{h,cpp}`, `particle_render.h`, `runtime/particle_resources.cpp`, `runtime/transparency_scene.cpp` dispatch serialized subtype 4 to image 6 and genuine owned smoke pixels with distinct drawing. `particle_admission.h` admits only stationary subtype 4 after dispatch; 5/6 remain gated. Extended existing `tests/reference_particles.cpp`, `tests/reference_fire_emitter.h`, and `runtime/particle_objects_test.h` with original Dinter face payloads, source image/fade checks, native frame/scene, disable and fresh-cloud restore. Pinned source `2576a98b5de310e3ac7c9777df2677d1627b8bae`; unchanged Dinter HOG SHA256 `b4dc0423e781de5b667093313e92ffdb582c5b75c45ec119f680b2613b31fb13`. Inspector Dinter findings 12->2, both snow owner 1492. Root D2 developer build passed; `xl_effect_decode_reference` and `xl_effect_inspection_gate` passed, with native waterfall receipt in `build-xl/effect-inspection/controls.log`; `git diff --check` clean. The existing native test uses original object 1488 payload on live Neptune geometry. Full original Dinter render cannot start until W4 snow behavior/resource is present, so original geometry and gameplay remain unproved here. W4 next; at 13:25 PST revised queue finish about 19:45 PST. |
| W4 | Runtime and genuine import complete; production snow/rain frames pending | `particle_weather.h` supplies distinct snow/rain creation, decay-one motion, water/lava/collision retirement and source image policy through W3's shared emitter; `fire_emitter.h`, `runtime/particle_objects.{h,cpp}`, `particle_render.h`, `runtime/transparency_scene.cpp`, `particle_world.{h,cpp}`, `runtime/particle_resources.cpp`, `runtime/resource_validation.cpp`, `particle_image.h` own dispatch, draw, weather image preparation and lifetime. `particle_admission.h` admits stationary 5/6 only after runtime support. Existing reference/native owners use actual Dinter snow object 1492 and Boiling rain object 60 payloads, check scene/disable/fresh-cloud restoration, and `xl_effect_decode_reference` plus `xl_effect_inspection_gate` passed. Genuine distribution 2.1.8 `rain.png` SHA256 `2fa050b3cf433a363595a41e6604336d053b7d9d7e9e938540a844d7cde110d3`, `simplesmoke.png` `8c39135fddbc35dc85e2132ece9fb0449625ae9b86df3e3eae8cf7fa44bf8c48` are selected by `ResourceModCatalog.kt`; existing `ResourceModStoreTest.kt` passed and `XlAddonPickerChecks.kt` verifies imported hashes. x86 Android app/test builds passed; guarded `test_xl_addon_import.ps1 -DistributionName d2x-xl-2-1-8.7z` passed with exact profile hash restoration after fixing that owner's original-distribution adb wait from 180 to 420 seconds. Pinned source `2576a98b5de310e3ac7c9777df2677d1627b8bae`; unchanged Dinter/Boiling HOGs as W3/W2 hashes. Reinspection Dinter and Boiling waterworks has zero particle findings, which is admission only. Strict Dinter missing-snow control failed with explicit `simplesmoke` diagnostic; genuine-image attempt advanced to internal `dinter.POG` six-frame true-color animation rejection (three strips in that POG have 6/9/9 frames). Strict Boiling waterworks level 1 fails on absent authored POG despite `custom_textures=yes`; no original snow/rain frames or play claim. First guarded import run hit its old 180-second adb timeout, restored hashes, then the 420-second rerun passed. `git diff --check` clean. W5 next; at 13:59 PST revised queue finish about 20:00 PST, with POG investigation and required final validation determining actual finish. |
| W5 | Combined candidate classified; original Warp 5 Android play passed; new-particle Android proof pending | Root binary `build-d2-reference/main/d2x-redux.exe` SHA256 `e8feecdf2c72346f3b7d85099878813660967fcf1076a2a2ace2b943b3f40a11` passed strict unassisted 120 simulated/rendered frames each for original Warp 5, Sphere 2 and Warp 6 (`temp/xl-day-20261110/w5-*/result.json`). Original Dinter with genuine snow/smoke images still rejects at `dinter.POG` bitmap 1333, a six-frame true-color animation (other strips 9/9); the installed POG registry has no animated-frame owner. Original Boiling waterworks needs absent authored POG, Boiling 2 rejects equipment IDs 7/8/9/24 plus two, Boiling 4 rejects movement layout 255 at object 4, and Boiling 6 lacks `dripping-water2.wav`; no rain/snow/waterfall original frames or gameplay claimed. Bounded Boiling 7 `lava-fall.wav` search checked additional authentic `d2x-xl-libraries.7z`, `lor-xl.7z`, older `d2x-xl-1.18.74-ar12.zip` and local manifests; absent. Existing native Warp 5 actor witness from W1 supplies assisted AI/combat/save/cold evidence. Extended the existing guarded `test_xl_addon_import.ps1` with original Warp 5 data selection and controller movement, and its Android picker wait to 240s for real 1.6 GiB import. On x86 emulator-5556, authentic import plus warm 20/20 and fresh-process cold 8/8 passed; actor object 226/ID 51/model 75/guns 2/weapon 21 survived, 64 imported resource hashes matched native owner, warm/cold save hashes matched, PIDs differed, profile hashes restored (`temp/xl-warp5-android/owner-result.json`). First generated script used fire axis and failed movement; corrected `LS_Y` passed. Next warm script passed but the shared final fresh introspection arrived after its five-second window; bounded owner retry recovered it. Endpoint loopback calibration with authentic `slowdown.wav` had nonzero 405 packets/correlation .997; original Warp 6 time-equipment game proof completed with five disclosed position assists and nonzero 1359-packet host audio capture, but named wave correlation was weak, so only game audio at output is claimed. Pinned source `2576a98b5de310e3ac7c9777df2677d1627b8bae`; original HOG hashes remain W1–W3. W6 next; at 14:39 PST revised queue finish about 20:30 PST, contingent on final platform checks. |
| W6 | Prior-source validation done; affected gates need refresh after further production edits | Root diff reviewed; removed temporary diagnostic and duplicate admission checks, corrected HXM null-check order and source-reference particle count, and fixed MSVC Debug source comparison in `particle_effect.h`. Scoped mixed-language formatter/check and `git diff --check` pass; formatter's PSScriptAnalyzer `-Fix` crashed internally, so the changed import owner was formatted/check-run directly. Current tracked code/test diff: 29 paths, +771/-120 lines, plus two new policy headers (259 lines); nine existing test paths changed (+226/-33), with no new runner or lifecycle. Root-source developer Windows D2/D1 builds passed using prepared local dependencies after the normal vcpkg launcher could not fetch dependencies. Full D2 CTest 100 passed/one connectivity skip; D1 89 passed/one same skip; standalone affected native 3/3 and canonical Windows public 56/56 pass. Frozen original eight-case headless readiness, pinned asset commit `9dad21bd65ba05665ac6fe315f37fe7461e85725`, unchanged catalog/input/hashes: exact historical 4/8 `loaded_ticked`, four `load_failed`, all loaded rows `xl_active=false`. Strict original Warp 5, Sphere 2, Warp 6 rendered results remain separate; the exact packaged D2 EXE re-passed Warp 5 120/120 frames. Universal APK (arm64-v8a/armeabi-v7a/x86_64) built, genuine distribution Android unit owner 5/5 and both automation catalogs pass. Final APK protected original Warp 5 import/warm20/cold8 passed with original resource hashes and exact profile restoration. Final APK shared original Pumo smoke/skybox/GPU/save/context/stock owner passed 68/68 and exact 285-entry profile restoration. Full lightning/GPU owner passed its first GPU probe and context recreation but exceeded its unchanged 480s limit at step 29/33 second probe; no durable pass, exact restoration. Native Linux GPU/audio and private fast gates remain unverified: WSL denied/hung and Docker CLI absent. Source pinned to XL `2576a98b5de310e3ac7c9777df2677d1627b8bae`; original assets unchanged. Final package hashes and receipts in `temp/xl-day-20261110/HANDOFF.md`. W7 next; at 15:40 PST revised finish about 16:15 PST, with no deadline cutoff. |
| W7 | Provisional handoff; refresh after remaining work | `temp/xl-day-20261110/HANDOFF.md` now names branch `d2xxl_nov10`, reviewed base `8fae81aa5d95cb6ef564a9abba7a11e42e346e31`, current HEAD `ebf245302817dc4bcdbd1b319b5412a2d204f5c9`, exact root review commands including both new untracked headers, and Windows D1/D2 folders plus universal APK paths/SHA256. Source remains visible as 34 modified tracked files and two new headers, uncommitted and unpublished; three pre-existing stashes and original bytes untouched. Ordinary `-hogdir` Windows launch and Android Setup import/menu steps are documented. Final receipts: packaged D2 original Warp 5 120/120 rendered, final APK original Warp 5 warm20/cold8 and Pumo shared particle/GPU/context 68/68 with protected restoration; full lightning/GPU owner timed out and native Linux/private fast gates remain unverified, clearly distinct from the unchanged 4/8 frozen headless cohort. Pinned XL source `2576a98b5de310e3ac7c9777df2677d1627b8bae`; genuine resource hashes, blocked Dinter/Boiling causes and deferred work are in the handoff/evidence. W7 elapsed about 5 minutes; final review at 15:45 PST, no estimate cutoff or merge-readiness claim. |

### W3–W5 acceptance re-audit, 16:12 PST

This correction supersedes the Dinter POG blocker in the historical rows above.
The unchanged Dinter HOG (`b4dc0423e781de5b667093313e92ffdb582c5b75c45ec119f680b2613b31fb13`)
contains a 92-entry POG with unclocked RGBA strips at bitmaps 1333, 1417 and
1421 (6/9/9 frames). The pinned source `2576a98b5de310e3ac7c9777df2677d1627b8bae`
initializes `CreateFrames`' current frame to zero. `runtime/pog_resources.{h,cpp}`
retains the whole bounded pixel arena and frame metadata; `runtime/pog_install.h`
exposes the first-frame native view and rejects strips referenced by live
effects, wall clips or Vclips before changing resources. The preflight uses
that same clock check. The existing `runtime/pog_test.h` owner passed with the
original Dinter POG, first-frame dimensions/receipts, Arena restoration and a
clocked negative control in both admission and direct installation
(`temp/xl-day-20261110/pog-dinter-static-final.log`).

| Item | Current acceptance status | Original-level outcome and next dependency |
| --- | --- | --- |
| W3 | Shared waterfall runtime and native fixture complete; original authored-geometry frame still unproved | Strict Dinter now passes POG preparation, then the existing generated-boss lifecycle guard rejects before a frame. The original object 1488 payload remains tested on Neptune geometry only. |
| W4 | Shared snow/rain runtime and genuine image import complete; production frames still unproved | Dinter's genuine `simplesmoke.png` reaches the same boss guard. Boiling waterworks still lacks its authored POG. Zero particle inspection findings remain admission only. |
| W5 | Target originals classified; Warp 5, Sphere 2 and Warp 6 rendered proofs and Android Warp 5 play retained | `temp/xl-day-20261110/dinter-static-pog-rendered/engine.log` shows `generator-created bosses require dynamic boss lifecycle support`. This is an unrelated campaign dependency and its admission guard remains. No original Dinter/Boiling new-particle frame, ordinary traversal or cold restore is claimed. |

At 16:12 PST, the revised finish estimate is about 18:15 PST for affected W6
validation and W7 handoff; 19:00 remains a soft estimate. No source asset or
mission byte was altered. Android newly supported particle play remains
unverified because these original levels stop before rendering.

### W6–W7 final-source closeout, 16:49 PST

| Item | Final status | Concrete handoff |
| --- | --- | --- |
| W6 | Consolidated and validated with explicit failed/unavailable gates | After the bounded POG correction in `runtime/pog_resources.{h,cpp}`, `runtime/pog_install.h`, `runtime/resource_validation.cpp` and the existing `runtime/pog_test.h`, root D2 full CTest passed 100 with one existing connectivity skip; Windows public XL contracts passed 56/56; the original Dinter POG native owner passed with clocked rejection, first-frame receipts and Arena cleanup. Scoped clang-format and `git diff --check` pass. The packaged D2 EXE SHA256 `4ce802729a0e54a6834b21784d871712ee42cf7a25e33c164dbbbf2a2a2927db` passed strict original Warp 5 120/120 rendered frames, then rejected original Dinter only at `generator-created bosses require dynamic boss lifecycle support` (`w7-package-{warp5,dinter}-pog`); no new Dinter particle frame is claimed. Frozen headless cohort reran unchanged at 4/8, all four loaded rows `xl_active=false`, exact same eight per-case results. The rebuilt universal APK SHA256 `ae09a674c498d7b7c327b1deb1145c207a28f7c8fba1d60306c49288d6c32c2d` contains arm64-v8a, armeabi-v7a and x86_64 D2 libraries. Final APK original Pumo hub owner passed 68/68 with EGL recreation and exact 285-entry profile restoration; original Warp 5 import/warm20/cold8 passed with 64 imported resource hashes, different PIDs and exact profile restoration. An unintended general Pumo mode failed its scene pixel assertion at 20/78 and restored exactly. The full lightning/GPU owner repeated its 480-second timeout at step 29/33 after first capture and EGL recreation; no full pass, fresh failure introspection unavailable, exact 285-entry restoration. Final Android unit owner ran four passes and one optional distribution skip because the host environment lacked `DXX_XL_DISTRIBUTION`; the earlier genuine-distribution 5/5 run remains a prior-source result. D1 executable unchanged by this D2 POG edit, with its prior 89-pass/one-skip CTest; native Linux GPU/audio and private fast remain unverified after WSL denial/hang and absent Docker CLI. Pinned XL source `2576a98b5de310e3ac7c9777df2677d1627b8bae`; original bytes untouched. W6 final re-audit elapsed about 48 minutes; W7 handoff next, revised finish about 17:00 PST. |
| W7 | Local review handoff complete | `temp/xl-day-20261110/HANDOFF.md` names root `C:/local/dxx-redux`, branch `d2xxl_nov10`, reviewed base `8fae81aa5d95cb6ef564a9abba7a11e42e346e31`, actual HEAD, exact root diff commands, two untracked policy headers, packaged D1/D2 EXE and universal APK paths/hashes, normal Windows launch and Android import steps. It separates implemented runtime behavior, original-level blockers, the failed full GPU owner and unavailable Linux/private gates; no whole-stack merge-readiness claim. Source is left as the uncommitted root working diff, with no PR or merge, three pre-existing stashes preserved. Pinned source and original-level outcomes are as in W6. W7 closeout took about 3 minutes; final review at 16:51 PST, earlier than the nominal soft 19:00 estimate. |

### Original geometry follow-up, 17:00 PST

The W3–W5 rendered acceptance gaps remain. The Dinter generated-boss guard is
not a harmless preflight check: `runtime/boss_spawn.cpp` registers a fixed set
of authored bosses at level load, while its state and route restoration require
the same owner count and identities. A boss created later by a matcen is not
registered or restorable. The guard stays in place; dynamic boss lifecycle is
a separate campaign dependency, not a stationary emitter change.

Extended the existing `tests/reference_particle_geometry.cpp` owner to run
`prepare_authored_particle` on the same original face geometry that it compares
against pinned source face area, corners and random sampling. Using pinned XL
source `2576a98b5de310e3ac7c9777df2677d1627b8bae`, the unchanged Dinter
HOG SHA256 `b4dc0423e781de5b667093313e92ffdb582c5b75c45ec119f680b2613b31fb13`
passed 41/41 face comparisons and 41/41 stationary native preparations;
unchanged Boiling HOG SHA256
`f0c7363c992bdcf0222873e63bcf7fff699c6f53ca28ab8ea9437f953b190ce2`
passed five face comparisons/preparations on waterworks. The existing reader
excluded `subzero.rl2` for unsupported movement 255. Receipts are
`temp/xl-day-20261110/{dinter,boiling}-original-face-reference.log`.
`xl_effect_decode_reference` CTest passed 1/1; scoped clang-format and
`git diff --check` passed. This is original geometry and production preparation
without loading the native Dinter mine or rendering a particle frame. Original
Dinter/Boiling waterfall, snow and rain play, Android newly supported particle
scenario and cold restore remain blocked/unverified as above. Only host test
and evidence/ledger changed, so the prior EXE/APK bytes and hashes remain valid.
Refresh W7 working-diff identity after this entry. Elapsed about nine minutes;
the bounded follow-up is complete at 17:00 PST. The nominal 19:00 estimate is
still soft and is not used as a completion claim.

### Original Dinter runtime follow-up, 18:01 PST

The independent generated-boss dependency is now implemented in
`runtime/boss_spawn.{h,cpp}`, `runtime/trigger_save.cpp`, `runtime/preview.cpp`
and `d2/main/fuelcen.c`: a genuine matcen-born boss acquires bounded per-owner
state and routes, retires when its object disappears, and is rebound from the
saved object table before trailer validation. Source identity hashes authored
bosses only. Trailer rejection rolls the generated set back. The exact
stationary Dinter admission guard was removed only after this lifecycle was
working; contained-boss and unrelated guards remain. A derived Neptune
fixture, not original mission bytes, supplies the boss-generating matcen.
The existing `boss_spawn_test.h` and `boss_groups_test.h` owners retain all 50
pinned reference scenarios, mixed reactor objective samples, malformed-save
checks and their 80-second inner limits. The complete source reference and
native controls pass 5/5 in 72.10 seconds total (`xl_effect_decode_reference`,
`xl_effect_payloads`, `xl_boss_spawning`, `xl_boss_groups`,
`xl_effect_inspection_gate`). The new generated case covers live creation,
independent route/state, invalid trailer rollback, cold disk restore and stock
isolation. The reference sample reset is once per difficulty with explicit
scenario cleanup; all 50 rows still match the pinned executable exactly.

Original unchanged Dinter HOG SHA256
`b4dc0423e781de5b667093313e92ffdb582c5b75c45ec119f680b2613b31fb13`
and pinned source `2576a98b5de310e3ac7c9777df2677d1627b8bae` now load
past the boss guard. Its original fire objects exposed the pinned MSVC
`ChangeDir` 32-bit intermediate wrap; `particle_fire.h` now preserves that
arithmetic, with a boundary case in the existing `particle_objects_test.h`.
`temp/xl-day-20261110/dinter-manual-120/load.json` records a separate normal
engine run with 120 simulated and 120 rendered frames, XL active, original
`dinter.rl2`, 6,744 segments, no assist and no GL error. The unchanged
45-second supplemental script timed out at phase `loaded` on the same large
level (`dinter-generated-boss-rendered-4`); the ordinary 120/120 run took
about 71 seconds. This is a rendered load, not traversal, visible waterfall/
snow proof, Android proof, or cold save proof. Boiling content blockers and
the frozen headless 4/8 result remain unchanged. No original mission bytes
or private criteria/timeouts changed. W6 revalidation, APK/EXE repackaging,
Android guarded owners and W7 handoff are next; current estimate is about
20:00 PST, with the nominal 19:00 time still soft.

### Final W6 validation and W7 delivery, 18:39 PST

This closeout supersedes earlier W6/W7 package hashes and the historical
Dinter guard status above. Root checkout `C:/local/dxx-redux` remains on
`d2xxl_nov10` at `ebf245302817dc4bcdbd1b319b5412a2d204f5c9`, with all
source edits visible in its working diff; no commit, push, PR or merge was
made. The reviewed base is `8fae81aa5d95cb6ef564a9abba7a11e42e346e31`.
Pinned XL source is `2576a98b5de310e3ac7c9777df2677d1627b8bae`.
Three pre-existing stashes and all original mission bytes are unchanged.

| Item | Actual source and validation | Original outcome / remaining limit |
| --- | --- | --- |
| W6 | Final production changes are `runtime/boss_spawn.{h,cpp}`, `runtime/trigger_save.cpp`, `runtime/preview.cpp`, `d2/main/fuelcen.c`, `particle_fire.h`, `runtime/particle_objects.cpp`; existing `boss_spawn_test.h`, `boss_groups_test.h`, `particle_objects_test.h`, `tests/boss_{spawn,groups}.cmake` and `CMakeLists.txt` carry source-aligned controls. The five combined native/source owners passed 5/5 in 72.10 s; after the last deletion assertion, `xl_boss_groups` separately passed 1/1 in 35.12 s under its unchanged 90 s CTest limit. Root D2 CTest passed 100 with one documented connectivity skip and zero failures. Final-source Windows public contracts passed 56/56 (`public-windows-ninja-final.log`). The rebuilt universal APK contains arm64-v8a, armeabi-v7a and x86_64 D2 libraries. Its guarded original Pumo hub owner passed 68/68 and matched all 285 protected hashes. Its guarded original Warp 5 owner passed warm 20/20 and cold 8/8 in distinct processes, with exact protected-profile restoration. The first final-APK Warp 5 attempt timed out at its unchanged 240 s archive-import wait while a concurrent host build was active; it restored its profile exactly, and the serial retry imported the authentic archive and passed. | The exact final packaged D2 executable passed strict original Warp 5 level 5 at 120 unassisted simulated plus 120 OpenGL frames, XL active (`w7-package-warp5-generated-final/result.json`). Original Dinter now loads with XL active and 6,744 segments; a separate ordinary engine run reached 120/120 rendered frames (`dinter-manual-120/load.json`). The unchanged strict supplemental 45 s Dinter probe timed out at `loaded` before frames (`dinter-generated-boss-rendered-4/result.json`); this is not a strict pass or traversal proof. Frozen headless readiness remains 4/8, with loaded cases XL inactive. |
| W7 | `temp/xl-day-20261110/HANDOFF.md` gives root review commands, package paths/hashes, ordinary run/import steps and precise implemented/blocked/deferred status. Packaged D1 EXE SHA256 `d59136a095e25574146ed6006e58a97c202c522c07ae43affa7e8036e784ac49`; D2 EXE `2f258dd123c11e5b0aa13824d43601791d4a3228d0dd302fdec6ee2e9483ad10`; universal APK `921f855b599fa0b51d27545a1eb81e52061757fd13c2c5a07c0ad8aaeb076243`; test APK `62e0ff8cc8e237774213b2bd0cfdcdd821ab0df24479df3dc1f4b7bd1d1c60fc`. D1 remains prior-source and previously passed 89 tests with one connectivity skip. | Dinter waterfall/snow scene frames, gameplay traversal and cold save remain unproved. Boiling waterworks lacks its authored POG; canyon/canyon2 and Boiling 7 lack exact authored wave names; Boiling 2 equipment IDs and Boiling 4 movement 255 remain unsupported. The full lightning/GPU owner timed out at its unchanged 480 s gate after initial capture/recreation, despite exact restoration; native Linux GPU/audio and private fast remain unavailable after WSL denial/hang and absent Docker. No whole-stack merge-readiness claim follows from local checks. |

W6's final-source revalidation and W7 packaging/handoff completed around
18:39 PST. The revised estimate is closeout by about 18:50 PST after the
working-diff identity and handoff are verified; the nominal 19:00 time remains
soft. No replay/admission guard was relaxed for a feature whose runtime is
still unsupported, no private test timeout was changed, and no resource was
fabricated.

### W5 runtime follow-up and W6 revalidation, 19:39 PST

The original Dinter strict rendered probe exposed a production performance
defect after level load: `escort_route_monitor_completion()` repeatedly
computed route certificates for a level with no live GuideBot. A one-off
timing run measured 56.24 seconds in that function out of 56.67 seconds of
simulation (`temp/xl-day-20261110/dinter-pre-timing/engine.log`). The bounded
shared fix in `d2/main/guidebot_route.c` returns when `Buddy_objnum` is not a
live companion object. The existing `android/tests/test_guidebot_live_navigation.cpp`
owner now verifies that route work is deferred with no companion and resumes
when a live one returns. Its two genuine mission cases passed twice
(`android/temp/guidebot_live_navigation/run_e4dfbe500b234f359c31e9472e1d3e8e`).
Temporary diagnostic timing and route-assist hooks were removed before the
final builds. The source reference remains pinned at
`2576a98b5de310e3ac7c9777df2677d1627b8bae`.

The exact newly packaged Windows D2 executable SHA256
`d4b673ee94a77b1769a6d1d2033e63cb0e774823ce76ba89e2e7e01748406c98`
passes the unchanged 45-second supplemental rendered probe on original
Dinter level 1 and Warp Shockwave level 5: both report 120 unassisted
simulated frames, 120 OpenGL frames, XL active, and no GL error
(`w7-package-dinter-companion-guard-authentic/result.json` and
`w7-package-warp5-companion-guard/result.json`). Original Dinter HOG SHA256
remains `b4dc0423e781de5b667093313e92ffdb582c5b75c45ec119f680b2613b31fb13`.
Its spawn is separated from the authored waterfall/snow segments in the raw
segment graph, so this load-and-tick result does not show those scenes or
ordinary traversal and cold save. The failed preliminary probes in
`w7-package-dinter-companion-guard*` used incomplete asset roots and failed
source-aligned missing-sound/image checks; the passing probe uses the genuine
retail data, unchanged original mission, and authentic XL sound and image.

The full root D2 build passed, and final D2 CTest passed 100 with one
existing connectivity skip and zero failures
(`temp/xl-day-20261110/dinter-guard-full-ctest.log`). Scoped mixed-language
code quality and `git diff --check` passed. The offline Android build passed
in 1m49s (`dinter-guard-android-build.log`), and the universal APK SHA256
`79a132a4e62b4e7fa669657193af1683078f7efacdc97ea7ccbf7ef8213f39bf`
contains D2 libraries for arm64-v8a, armeabi-v7a and x86_64. It installed on
emulator-5556. The guarded original Pumo hub owner passed 68/68 in 385502 ms,
including EGL recreation and saved resources, then restored all 285 protected
profile hashes exactly (`android-final-pumo-owner-after-guard`). The Warp 5
Android owner is running serially after that restoration. W7 package and
working-diff handoff will be updated after its result; estimate about 20:00
PST. The frozen headless cohort remains 4/8 with loaded rows XL inactive.

The user requested the repository cleanup utility as ordinary development
practice. `AGENTS.md` now points to `android/clean-workspace.ps1`, preview,
active-run/authentic-asset exclusions, the 50 GiB target, emulator workspace
preservation and `android/CLEANUP.md`. With active assets and current runs
excluded, the utility recovered 50.18 GiB free
(`temp/xl-day-20261110/cleanup-narrow.log`). After final builds consumed
scratch space, a second preview and guarded run recovered 50.04 GiB free
(`cleanup-final-preview.log`, `cleanup-final.log`), with final packages,
authentic assets, pinned source, owner receipts and emulator workspace excluded.

### W7 final package and review closeout, 19:48 PST

The rebuilt final APK's guarded original Warp 5 owner passed authentic
archive/resource import, warm gameplay and save 20/20, and cold restore in a
different process 8/8 (`android-final-warp5-owner-after-guard.log`,
`temp/xl-warp5-android/owner-result.json`, attempt
`8a8035fde9544658a377e5514edb3a0c`). All protected profile hashes were
restored. This followed the serial Pumo 68/68 owner and its exact 285-hash
restoration. No concurrent host build ran during either owner. Both packaged
Windows D2 strict original Warp 5 and Dinter probes passed 120/120 at the
unchanged 45-second gate. The final root D2 cache identifies
`C:/local/dxx-redux/d2` as `CMAKE_HOME_DIRECTORY`.

Runnable package SHA256 values: Windows D1
`temp/xl-day-20261110/windows-runnable/d1/d1x-redux.exe`
`d59136a095e25574146ed6006e58a97c202c522c07ae43affa7e8036e784ac49`;
Windows D2 `temp/xl-day-20261110/windows-runnable/d2/d2x-redux.exe`
`d4b673ee94a77b1769a6d1d2033e63cb0e774823ce76ba89e2e7e01748406c98`;
universal APK `temp/xl-day-20261110/android-universal/app-debug.apk`
`79a132a4e62b4e7fa669657193af1683078f7efacdc97ea7ccbf7ef8213f39bf`;
test APK `temp/xl-day-20261110/android-universal/app-debug-androidTest.apk`
`62e0ff8cc8e237774213b2bd0cfdcdd821ab0df24479df3dc1f4b7bd1d1c60fc`.
The D1 result is prior-source; the D2 EXE and APK include the new no-companion
route guard. The normal Windows run and Android import steps, exact root review
commands, implemented/blocked/deferred limits and final source identity are in
`temp/xl-day-20261110/HANDOFF.md`. The branch remains `d2xxl_nov10`, HEAD
`ebf245302817dc4bcdbd1b319b5412a2d204f5c9`, with 53 tracked paths and
two untracked policy headers visible in the main checkout, three pre-existing
stashes preserved, no commit/PR/merge, and `git diff --check` clean. The work
closed around 19:48 PST, past the nominal 19:00 estimate; no criterion was
weakened for the schedule.

### W3-W4 original Dinter geometry follow-up, 20:03 PST

The first unfinished original-scene acceptance item was W3's actual authored
geometry witness. `android/app/src/main/cpp/shared/route_confirmation_desktop.cpp`
now adds read-only live particle owner and native scene-quad diagnostics after
the unchanged 120 simulated/120 OpenGL frames. It uses the loaded mine's
actual object position and segment, prepared resource dimensions, and the
existing shared `particle_scene_items`/`particle_scene_quad` path. Each native
scene calculation is marked `assisted_owner_position_segment_only` so it is
not confused with the unassisted GL view from the normal player spawn. No new
runner, resource substitute, admission relaxation, frame criterion or
45-second timeout was added. The `NOMINMAX` include guard is required because
the desktop Windows socket header otherwise defines `min`/`max` macros that
break the shared particle headers.

The exact packaged Windows D2 executable SHA256
`997eb6e2f2fb20aa75a2eaac1c58403a61f3d3a4ce97a095191fc3db8b3b9c6e`
passes the strict original Dinter level-1 rendered probe with XL active,
6,744 segments, 120 unassisted simulated and 120 OpenGL frames, no GL error
(`temp/xl-day-20261110/dinter-original-geometry-final/result.json`). Its
proof records positive live particle counts and positive-alpha native scene
quads in the authored segments for waterfall objects
1483/1488/1508/1518/1520, bubbles 1486/1493/1495/1496/1504 and snow 1492.
These are original content object identities, not synthetic payload copies.
The same executable passes strict original Warp 5 120/120 as a regression
control (`warp5-original-geometry-final/result.json`). Original Dinter HOG
SHA256 remains `b4dc0423e781de5b667093313e92ffdb582c5b75c45ec119f680b2613b31fb13`;
the pinned XL source remains `2576a98b5de310e3ac7c9777df2677d1627b8bae`.
Scoped code quality passed; the root D2 build passed; root D2 CTest passed
100 with the existing connectivity skip and zero failures in 70.96 seconds
(`dinter-original-geometry-ctest.log`). This desktop-only diagnostic source
is not part of the Android native library, so the prior Android APK/hash and
its guarded owner results remain valid.

W3's real original mine geometry and W4's snow native scene-quads are now
observed, but this is not a GPU-visible waterfall/snow frame from a player
camera, ordinary traversal, Android Dinter import/play, or a cold Dinter
save. W2's canyon bubble play remains externally blocked by exact missing
authored wave names, and Boiling waterworks still lacks its authored POG.
W5's real-viewpoint and Android original-content steps remain the next work.
At this checkpoint, revise the estimate to at least another 1-2 hours for
one coherent W5 assisted-view/Android owner attempt and W6-W7 revalidation;
the 19:00 nominal finish remains soft.

### W5 original Dinter scene and Android follow-up, 20:47 PST

The desktop-only rendered probe in
`android/app/src/main/cpp/shared/route_confirmation_desktop.cpp` now accepts
an optional `-xl-readiness-scene-object` for an assisted camera after its
unchanged strict 120 unassisted simulated/120 OpenGL frame gate. It uses the
loaded original object's actual position and segment, saves one particle frame,
clears only transient particle systems, and saves two same-pose control frames.
The output explicitly labels the camera assist. No original mission byte,
private gate, frozen readiness cohort, production renderer or timeout changed.
The exact packaged D2 executable SHA256
`de40e9695e3b6f6fa5e0bc690af121f16b6a4919796488e0813b5d08114f2174`
passed the strict original Dinter and Warp 5 120/120 probes with XL active and
no GL error (`dinter-w5-camera-final-strict/result.json`,
`warp5-w5-camera-final-strict/result.json`). In original Dinter level 1, the
assisted object 1488 waterfall, 1492 snow and 1486 bubble scenes each retained
positive live particles and yielded respectively 102,896, 1,825 and 9,992
changed framebuffer pixels against their cleared-particle controls. Repeated
controls changed zero pixels in each case. Exact captures, screenshot hashes
and region `[100,20,540,280]` are in
`temp/xl-day-20261110/dinter-scene-comparison.json`; source runs are
`dinter-{waterfall,snow,bubble}-package-final`. This is particle-attributable
GPU evidence with a position assist, not ordinary traversal from spawn.

The existing protected Android import owner was extended in
`android/tests/test_xl_addon_import.ps1`,
`android/app/src/androidTest/java/com/dxxredux/app/RecoveryInstrumentation.kt`
and `XlAddonPickerChecks.kt` to select and hash-check the unchanged original
`dinter.hog` SHA256 `b4dc0423e781de5b667093313e92ffdb582c5b75c45ec119f680b2613b31fb13`
and `dinter.mn2` SHA256
`89ea5005ce971a3c7462dbfe727bbfa9b17b8766432e5f418e93c50c76ec72b1`.
It reuses the same launcher/picker, retail D2 input list, XL resource owner,
pilot, controller/save script, timeout and exact profile backup/restore. The
first attempt reached Dinter's briefing before the shared Warp difficulty
selection, failed at step 11, and restored every protected profile hash. The
Dinter-only script order was corrected from that actual menu observation.
The second serial run `temp/xl-dinter-android/owner-result.json` attempt
`2557aeec0b3d4315ab8e318d29861e70` passed warm 19/19 on original Dinter
level 1 with XL active, controller movement and normal save; cold 8/8 loaded
the same saved level in a new game process (PIDs 20932/22042). It verified
64 authentic imported XL resources, matching warm/cold save hashes and exact
protected profile restoration. The temporary picker inputs were removed only
after their hashes were reverified. The final formatted Android test APK was
rebuilt offline; its SHA256 is
`cdb5461d8bcb994f9c44d44b334e7bbea59500fa2b1e40a9e8c052ec4ba147b3`.
The production universal APK remains SHA256
`79a132a4e62b4e7fa669657193af1683078f7efacdc97ea7ccbf7ef8213f39bf`.

W5 now has original Dinter bubble/waterfall/snow GPU scene evidence and Android
warm/cold original-level play. Dinter's authored effect segments are disconnected
from the spawn component in the raw segment graph, so ordinary traversal to
those particular effects is still unproved. Boiling canyon/canyon2 are blocked
by exact missing authored wave names, waterworks by its missing authored POG;
no rain original-level frame is claimed. The pinned XL source remains
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. W6-W7 final-source checks
and review identity follow; estimate 21:00 PST. The 19:00 finish was soft.

### W6-W7 final-source validation and package checkpoint, 20:47 PST

Root D2 full CTest on the final desktop source passed 100 with the one existing
connectivity skip and zero failures in 70.36 seconds
(`temp/xl-day-20261110/dinter-assisted-scene-final-ctest.log`). The scoped
Kotlin/PowerShell code-quality run passed; the offline Android test APK build
after formatting succeeded (`dinter-android-code-quality.log`,
`dinter-android-test-final-build.log`). The protected Dinter owner ran the
same logic before formatting; the final test APK was rebuilt from formatted
source. Existing final production APK protected Pumo 68/68 and Warp 5 warm
20/20, cold 8/8 results remain valid because only Android test and desktop
probe sources changed after those runs. `git diff --check` is clean. The full
lightning/GPU Android owner remains an unchanged 480-second timeout, and
Linux GPU/audio and private fast gates remain unavailable. The frozen headless
readiness result remains 4/8 with four loaded rows XL inactive. Do not infer
whole-stack merge readiness from these local results.

### W5 Android waterfall viewpoint and W6-W7 closeout, 21:30 PST

The previous checkpoint's Android Dinter owner proved original play/save but
its spawn screenshot did not show a particle scene. The existing desktop probe
now records the exact assisted camera position and angles in
`android/app/src/main/cpp/shared/route_confirmation_desktop.cpp`, using its
loaded original object 1488/segment 2598. The existing guarded Android owner
in `android/tests/test_xl_addon_import.ps1` uses that pose for a Dinter-only
view after a normal save; it backs away from the authored face under ordinary
controller input with the unchanged `position.speed > 0.05` threshold and
10-second wait. The final owner preserves original-file hash checks, 240-second
host runner setting, 64 imported XL resource hashes, fresh-process cold load,
save equality and exact profile restoration. No per-level runner, production
particle hook, fabricated asset, original byte edit or readiness relaxation
was added. Two intermediate assisted attempts failed respectively at spawn
movement and at a save request issued in the dense effect segment. Both
restored protected profile hashes exactly; their results remain in
`dinter-android-assisted-owner.log` and `...-third.log`. The final script saves
before the assist, so cold restore is independent of camera placement.

The final serial x86_64 owner attempt
`976e72945e3743c9bc5fe849f16ddbad` passed original Dinter warm 22/22,
cold 8/8 (`temp/xl-dinter-android/owner-result.json`). Warm process 26551
loaded original level 1 with XL active, saved normally, then posed at authored
waterfall segment 2598, passed ordinary thrust and captured
`temp/xl-dinter-android/warm.png` SHA256
`d420c8d1d8cb16d8dbe906f7a73b8c6105b317a64f2496b65532ff580a1338fa`.
The screenshot visibly shows the blue original waterfall view, but it has no
Android cleared-particle control, so pixel attribution relies on the separate
desktop differential. Cold process 28319 restored the same saved level with
XL active; warm/cold save hashes match, 64 native resource hashes match, and
all protected profile hashes match. Warm automation elapsed 333264 ms in the
dense segment despite the unchanged 240-second host runner setting; do not
report it as a strict 240-second completion. The durable 22/22 result and
owner PASS show that the existing runner did not abort this completion.
Temporary picker inputs were removed after rechecking original hashes.

The final root-associated D2 executable SHA256 is now
`5fc562dfddb178d289291db56623598a996005d63e64b211e0963659abb64c9a`.
It passed unchanged strict original Dinter and Warp 5 120/120 full-resource
probes (`dinter-final-camera-pose-strict/result.json`,
`warp5-final-camera-pose-strict/result.json`), XL active, no GL error. Its
assisted original Dinter waterfall capture has 97,221 changed pixels versus
the cleared-particle same-pose control, zero pixels between repeated controls
(`dinter-camera-pose-probe/load.json` and screenshots). The same root D2
source passed full CTest 100 with one existing connectivity skip and zero
failures in 69.44 seconds (`dinter-camera-pose-final-ctest.log`). Scoped
C++/PowerShell quality and `git diff --check` pass. Production universal APK
SHA256 remains
`79a132a4e62b4e7fa669657193af1683078f7efacdc97ea7ccbf7ef8213f39bf`;
the final formatted Android test APK SHA256 remains
`cdb5461d8bcb994f9c44d44b334e7bbea59500fa2b1e40a9e8c052ec4ba147b3`.
The guarded assisted owner used the previously built test APK with the same
logic before Kotlin-only formatting; the final test APK was built but not
reinstalled for another full import.

The pinned D2X-XL source remains
`2576a98b5de310e3ac7c9777df2677d1627b8bae`; original Dinter HOG remains
`b4dc0423e781de5b667093313e92ffdb582c5b75c45ec119f680b2613b31fb13`.
W0-W7 accessible implementation, consolidated validation and review handoff
are complete. Dinter effect-area traversal from spawn is still unproved
because the raw authored graph disconnects it; Android particle-only pixels
are not isolated. Boiling rain/waterworks/canyon blockers, the full GPU owner
timeout, unavailable Linux/private gates and physical ARM execution remain
blocked or deferred exactly as above. The final package/review identity is in
`temp/xl-day-20261110/HANDOFF.md`; revised estimate is complete at this
21:30 PST checkpoint, after the soft 19:00 target.
