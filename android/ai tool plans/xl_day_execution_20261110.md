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
| W0 | Not started | Root build/source association and fresh dump |
| W1 | Not started | Warp 5 HXM unused animation slots |
| W2 | Not started | General original bubble profiles |
| W3 | Not started | Waterfall behavior |
| W4 | Not started | Snow/rain and genuine resource import |
| W5 | Not started | Combined original-level play and cold restore |
| W6 | Not started | Consolidation and final combined validation |
| W7 | Not started | Local review handoff |
