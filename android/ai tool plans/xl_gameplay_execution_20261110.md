# D2X-XL gameplay and durable input demos: next sequential tranche

Planning checkpoint: Tuesday November 10, 2026, approximately 21:55 PST
(Wednesday November 11, 05:55 UTC). Budget approximately 24 hours of sequential
work, nominally through Wednesday November 11 around 21:55 PST. This is an
estimate, not a cutoff or permission window. Check the real clock on entry and
after each item; finish coherent work and revise estimates when it overruns.

## 1. Outcome and priority

The next body of work is to close the **single-player gameplay and input-replay
contract**. A player must be able to navigate, fight, operate authored mechanisms,
earn resources, save/resume, die/restart and finish levels with the required XL
rules. New input demos recorded after qualification must remain useful regression
evidence while graphics and sound are subsequently improved.

This follows the existing single-player XL scope. Multiplayer/network XL remains
explicitly unsupported. Here "demo" means the existing input-based .dximdemo
recordings, including level-start and save-start recordings. Preserve classic
.dem behavior, but do not turn this tranche into a classic-demo converter or a
promise to replay old XL-engine .dem files. Old desyncing development recordings
remain evidence, not a reason to preserve bugs.

The highest-priority gaps are:

1. Repair the actual Linux compilation regression in the current prerequisite.
2. Make source-significant time and input events reproducible, especially Omega,
   slow motion and bullet time, without depending on the replay machine's display
   rate, wall clock or audio device.
3. Preserve and compare all XL state that affects subsequent movement, combat or
   progression, including across cold saves and campaign transitions.
4. Let gameplay proceed without optional presentation output where independence
   has been proved. Maintain collision, visibility, definition and asset identity
   requirements that really affect gameplay.
5. Close remaining valid single-player gameplay gaps and demonstrate connected
   original-content play, then freeze a small, honest replay corpus.

Do not pursue pixel parity, every particle animation, exact audio output, or a
larger rendering test framework. These can remain open **only if** they cannot
change qualified gameplay. A texture affecting weapon passage, a model affecting
hitboxes, view-dependent AI, or an animation driving a gun transform is gameplay.
Headless means no graphics/audio device, not a separate simplified simulation.
The normal interactive build must still show readable geometry, enemies,
projectiles, doors and the HUD well enough to navigate and aim. Invisible objects,
unusable frame latency or broken controls cannot be excused as missing polish.

Twenty-four hours is not evidence of completion. If any required gameplay
category remains unimplemented or unverified, report it and continue the next
coherent fix. Call the result a bounded milestone until the closure gate in
section 10 is met. Do not describe a few passing levels as all XL support.

## 2. Authority, workspace and prerequisite identity

Read AGENTS.md and .github/copilot-instructions.md. Work sequentially in
C:/local/dxx-redux; keep source changes visible in this main checkout. No parallel
agents or independent workers sharing it. A coordinator may assign one G item;
then complete that item and its acceptance, update this ledger, and return its
handoff. Otherwise continue through the queue. Within an item, complete one
coherent change before beginning another.

The user explicitly authorizes the implementing Sol worker to create commits,
create/push branches, and create/update reviewable PRs **in
larsonm-personal/dxx-redux**, the user's fork. Never push to upstream
dxx-redux/dxx-redux. Never merge PRs, enable auto-merge, enqueue merges, or run
gh stack merge. This authorization supersedes the older plans' no-publication
instructions. It does not authorize rewriting unrelated branches or publishing
private mission assets.

Observed baseline:

| Item                         | Identity at planning                                                                 |
| ---------------------------- | ------------------------------------------------------------------------------------ |
| Root branch                  | d2xxl_nov10                                                                          |
| Local HEAD                   | 90a748b0c64b23169bb17e96a671759c1208a273                                             |
| Open prerequisite PR         | [#464](https://github.com/larsonm-personal/dxx-redux/pull/464), d2xxl_nov10 -> cmake |
| PR head / cached remote head | cce790379079d8673850424e95344179bed76ce2                                             |
| Both heads' identical tree   | 3828501abd12c10e803ea8b215bc12a991c037bc                                             |
| cmake prerequisite           | b3bb12cbeaa64cb17b87fbaf70cd06efaa25bcf0, including #461 and #463                    |
| Pinned XL reference          | temp/d2x-xl-reference at 2576a98b5de310e3ac7c9777df2677d1627b8bae                    |
| origin                       | https://github.com/larsonm-personal/dxx-redux.git                                    |
| upstream                     | https://github.com/dxx-redux/dxx-redux.git; read-only for this work                  |

Refresh these facts before implementation. The local and published commits have
different ancestry but the same reviewed implementation tree. Do not force-push
local 90a748b0c over the user's remote branch or accidentally include its duplicate
implementation diff in a new PR.

If the heads are still tree-identical, preserve local d2xxl_nov10 and create the
first new branch from the refreshed origin/d2xxl_nov10. Carry these new plan files
as explicit additions. If #464 has since been integrated into cmake by the user,
verify the integrated tree and use the refreshed cmake as trunk. If later changes
exist, preserve and incorporate them; stop only an operation that would overwrite
someone else's work, while continuing safe investigation.

The three existing stashes, other branches, untracked plans and original assets
are not disposable. At planning there is an independent, untracked
android/ai tool plans/pr464_ci_review_20261110.md. Read its latest findings when
available; do not edit or stage it as part of this work. The old W0-W7 ledger and
temp/xl-day-20261110/HANDOFF.md are historical evidence, with some earlier
uncommitted/status wording superseded by the commits above.

## 3. What is already known, and what is not

### Concrete baseline findings

- Linux public save/replay run
  [42967564629](https://github.com/larsonm-personal/dxx-redux/actions/runs/42967564629),
  job 130348267371, fails compiling
  android/app/src/main/cpp/shared/route_confirmation_desktop.cpp:249:
  GCC cannot bind packed Objects[index].segnum to short int& in the JSON
  initializer. Copy the scalar to an ordinary value before forwarding it, then
  rerun the affected real targets. Do not change packing or remove the receipt.
  Replay jobs skipped after this build failure have supplied no replay evidence.
  Other PR checks were still running at planning; refresh before attributing them.
- xl_omega_input_demo_error in
  android/XL_compat/runtime/slow_motion_runtime.cpp rejects an active XL level
  with prepared/owned time equipment, and separately rejects Omega ownership.
  xl_slow_motion_request also rejects recording/playback activation. These are
  real missing real-time/render-clock timelines, not stale guards to delete.
- The current format-5 ordered-input path exists already. Reuse
  input_demo_fixture.{h,cpp}, input_demo_controls.*, input_demo_dispatch.h,
  input_demo_recorder.cpp, input_demo_replay.cpp, input_demo_hooks_shared.c,
  input_demo_start_shared.c and input_demo_newdemo_shared.c under
  android/app/src/main/cpp/shared/. Do not create a second XL replay format or
  playback loop.
- slow_motion_runtime.cpp reads timer_query for transition time and SDL_GetTicks
  for pickup animation; Omega reads a render counter advanced from d2/main/
  gamerend.c, automap.c and render.c. Normal frames, automap, extra views and
  paused/menu frames need an explicit ordering audit.
- Existing saves retain XL trigger, object-event, producer, boss and weapon/time
  state through runtime/trigger_save.cpp and associated owners. The generic
  input-demo world trace does not by itself prove complete XL-state coverage.
- Dinter, Warp 5, Warp 6 and Sphere 2 have supplemental full-resource rendered
  load/tick evidence. Dinter has desktop assisted effect views and Android
  spawn/assisted-waterfall save evidence. This is not connected traversal or
  full-level completion. Dinter's spawn and effect areas are in separate raw
  segment-graph components: inspect authored teleports/events before diagnosing
  that as broken geometry.
- The dense Dinter Android scene was slow, and an earlier scene/save attempt
  timed out. Keep gameplay latency meaningful; do not "solve" it by increasing
  all owner timeouts, shortening play, or relying on position assists.

### Specific remaining content predicates

| Case                         | Known predicate                                                                                                  | Required decision                                                                                                                                           |
| ---------------------------- | ---------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Boiling 2, control.rl2       | Equipment selections include IDs 7/8/9/24, plus additional unsupported IDs                                       | Inspect complete masks and source behavior; do not invent pickups                                                                                           |
| Boiling 1/5 and wider survey | Smoke wall/object trigger types, including 22 and 21                                                             | Pinned main/trigger.cpp explicitly does nothing for TT_SMOKE_LIFE/SPEED/DENS/SIZE/DRIFT (20-24); qualify complete dispatch/flags semantics before admitting |
| Boiling 4, subzero.rl2       | Parser reports object 4 movement 255                                                                             | Trace exact prior record boundaries against reference reader; distinguish malformed source, inactive payload and decoder misalignment                       |
| Boiling 3 canyon / 6 canyon2 | Authored dripping-water3.wav / dripping-water2.wav absent; genuine package has hyphenated names                  | No aliases or renamed files; evaluate explicit optional ambient-output behavior                                                                             |
| Boiling 7 control2.rl2       | Authored lava-fall.wav missing in bounded authentic searches                                                     | No repeated scavenger hunt; prove optionality or retain an exact blocker                                                                                    |
| Boiling 1 watrwrks.rl2       | Descriptor requests custom textures; authored POG absent                                                         | Cannot assume graphics-only: transparency/collision and switch/door semantics must be audited                                                               |
| Dinter                       | Shared stationary effects, generated boss lifecycle, static POG strips now implemented                           | Demonstrate ordinary navigation/combat and actual event connections; no moving-emitter project                                                              |
| Wider original catalog       | Remaining trigger 35, geometry/layout, object, generator, custom-definition or mission-flow predicates may exist | Inventory and rank by actual gameplay reach; do not infer absence from known sample passes                                                                  |

Source anchors for these decisions:

- temp/d2x-xl-reference/include/powerup.h: 7 Hoard orb, 8 Monsterball,
  24 Turbo, 27 Megawow, 46/47 flags, 48/49 time equipment. IDs 9 and 26
  are not defined there. objects/powerup.cpp has null release pickup handlers
  for Turbo/Megawow. main/producers.cpp, objects/createobject.cpp and the loaded
  powerup/vclip tables determine whether an authored maker can still create an
  inert item. Never equate "no reward" with "safe to erase the selected object."
- temp/d2x-xl-reference/objects/objectio.cpp, CObject::Read:
  unknown movement logs an invalid type and consumes no known movement payload.
  That observation alone does not qualify normalizing 255 to MT_NONE.
- temp/d2x-xl-reference/include/trigger.h and main/trigger.cpp:
  smoke 20-24 are explicit no-op action arms; 27 needs its own dispatch audit;
  35 invokes DoReprogramRobots and is gameplay. Follow CObject::Reprogram and
  its AI/ownership effects if the survey requires it.
- temp/d2x-xl-reference/physics/slowmotion.cpp,
  weapons/omega.cpp, main/game.cpp and render/renderframe.cpp are authoritative
  timing references. Existing extracted-reference owners pin consumed files.
  Extend their adapters/pins rather than copying the reference algorithm into
  a second expected implementation.
- android/XL_compat/{inventory.cpp,admission.cpp,EXTENSIONS.md} and
  runtime/{preview.cpp,resource_validation.cpp,equipment.cpp,trigger.cpp,
  object_trigger.cpp,boss_spawn.cpp} are the production boundaries.
  README/EXTENSIONS prose includes stale statements: code, receipts and the latest
  ledger take precedence; correct relevant prose in the owning item.

### Assets and evidence to preserve

Use authentic local retail data plus
temp/xl-implementation-20261109/complete-runtime. The original-readiness trees
under that same directory and temp/xl-day-20261110/readiness-assets contain
original mission inputs. Re-resolve exact paths and hash them; do not reconstruct
an archive from a translated fixture.

| Original HOG    | SHA-256                                                          |
| --------------- | ---------------------------------------------------------------- |
| dinter.hog      | b4dc0423e781de5b667093313e92ffdb582c5b75c45ec119f680b2613b31fb13 |
| wshock20.hog    | 2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c |
| Boiling HOG     | f0c7363c992bdcf0222873e63bcf7fff699c6f53ca28ab8ea9437f953b190ce2 |
| d2x-xl-2-1-8.7z | 2cfc020fbf25a048f099511b71599c7be1b77836be8fc1112e5eb310c65beb9d |

The checked-in xl_readiness_inventory.json catalogs 1,111 level entries in 23
archives at private asset commit 9dad21bd65ba05665ac6fe315f37fe7461e85725.
That is an inventory, not 1,111 promised playthroughs. Use existing surveys and
one bounded metadata pass to build the missing-feature map. The frozen eight-case
measurement and its historical 4/8 headless result remain a separate series.
Do not shrink or reroll it. Preserve improved results without rewriting history.

Previous local results include D2 CTest 100 pass / 1 existing skip, public Windows
contracts 56/56, Android Pumo 68/68, Warp 5 warm20/cold8 and Dinter warm22/cold8.
They are baseline evidence, not validation of future edits. Native Linux/private
final-source checks were unverified locally; the full lightning/GPU owner timed
out. A new gameplay pass neither retroactively fixes nor needs to hide that
presentation limitation.

## 4. Six reviewable layers, published in the user's fork

Keep six feature-sized layers, with a few coherent commits within each. Do not
recreate the former 58-PR micro-stack. Add another layer only when a newly
discovered independent feature genuinely improves review, and record why.

| Layer / proposed branch               | Work                                                                | PR base                                             |
| ------------------------------------- | ------------------------------------------------------------------- | --------------------------------------------------- |
| xl_gameplay_20261111_01_baseline      | G0: plans, portability and trustworthy baseline                     | refreshed d2xxl_nov10, or verified integrated cmake |
| xl_gameplay_20261111_02_timeline      | G1: ordered gameplay clock/input replay                             | 01_baseline                                         |
| xl_gameplay_20261111_03_continuation  | G2: complete simulation state and cold continuation                 | 02_timeline                                         |
| xl_gameplay_20261111_04_runtime       | G3-G4: simulation/presentation boundary and valid authored gameplay | 03_continuation                                     |
| xl_gameplay_20261111_05_progression   | G5: connected navigation, combat and mission lifecycle              | 04_runtime                                          |
| xl_gameplay_20261111_06_qualification | G6-G7: durable corpus, platform proof and delivery                  | 05_progression                                      |

Each branch builds and has meaningful scoped tests before publication. A draft
can honestly carry an unmet external/platform gate; it cannot claim implemented
behavior with a known implementation failure. Complete the current layer before
creating its child, so lower-layer rewrites are uncommon.

Installed gh 2.102.0 supports native stacks. The GitHub connector can create/edit
ordinary PRs but does not expose native stack linking; use gh for that step.
Use structured connector PR bodies where available. For CLI bodies write the
exact text to a UTF-8 file and use --body-file. Always name the fork explicitly.

Recommended publication procedure:

1. Refresh origin and PR #464, record local/remote heads, tree hashes, working
   diff, stash list and current native stack metadata. Never assume an old cache
   matches this checkout.
2. Create each new branch in the root checkout from its recorded parent.
   Stage explicit owned paths only; never git add -A or gh stack add -A.
3. Format once, run scoped gates, inspect staged diff, commit and push that branch
   to origin. Record pushed commit and tree. Preserve local d2xxl_nov10 unchanged.
4. Create the PR in larsonm-personal/dxx-redux with the table's immediate base.
   State the concrete behavior, dependency PR, tests, current limits and source
   identity. Do not create six independent cmake-based PRs with cumulative diffs.
5. Once two layers exist, link only this tranche's PRs using native stacks.
   With GH_REPO set to larsonm-personal/dxx-redux, the verified CLI shape is:

       gh stack link --remote origin --base <verified-trunk> <first-PR-URL> <second-PR-URL>

   Append new PRs using the returned stack number and full fork PR URL. Inspect
   gh stack view --json and server PR metadata afterward. Do not include #464
   itself or unrelated PRs in a new stack without a reason and explicit ledger
   entry. If native linking fails, retain ordinary parent-based PRs and explicit
   dependency links; coverage must run conservatively.

6. Check actual native stack metadata. .github/scripts/stack_ci.py permits an
   expensive-job skip only for a complete valid non-tip record. Unknown metadata
   runs tests. The current tip must receive full required aggregate coverage,
   including lower-layer changes. Do not add skip labels or change eligibility
   merely to get green checks.
7. Review exact-head CI after each published layer while doing safe local work.
   Fix deterministic failures before adding dependent work. A run for an old
   head, a PR merge ref, a queued job or a skipped job is not current-head success.
   Record which source identity a workflow actually tested.
8. Prefer fixup commits over history rewriting after publication. If a rebase is
   necessary, preserve all later fixes, record old refs and use explicit
   --force-with-lease=refs/heads/<owned-branch>:<observed-old-sha> only for these
   worker-created branches. Revalidate/restack descendants. Do not force-update
   the user's trunk, cmake, unrelated refs, tags or stashes.

Useful explicit commands, with placeholders resolved from the ledger:

    git fetch origin
    git rev-parse HEAD
    git rev-parse 'HEAD^{tree}'
    git rev-parse 'origin/d2xxl_nov10^{tree}'
    git switch -c xl_gameplay_20261111_01_baseline origin/d2xxl_nov10
    git push -u origin xl_gameplay_20261111_01_baseline
    gh pr create --repo larsonm-personal/dxx-redux --base <parent> --head <branch> --draft --title <title> --body-file <body-file>

These are sequential examples, not a script to run blindly. If branch names
already exist, inspect and continue them. Commit this plan and the goal prompt
with the first layer; thereafter ledger updates belong to their owning layer.
All stack instructions are for implementation, not evidence that this planning
session has created or published branches.

## 5. Execution queue and time budget

| Item                                            | Nominal hours | Cumulative hours | Layer |
| ----------------------------------------------- | ------------: | ---------------: | ----- |
| G0 - Baseline, Linux repair and capability map  |           1.5 |              1.5 | 01    |
| G1 - Reproducible clocks and ordered controls   |           5.0 |              6.5 | 02    |
| G2 - State completeness and cold continuation   |           3.0 |              9.5 | 03    |
| G3 - Gameplay without presentation dependencies |           3.0 |             12.5 | 04    |
| G4 - Remaining authored gameplay predicates     |           3.0 |             15.5 | 04    |
| G5 - Connected original progression and combat  |           3.5 |             19.0 | 05    |
| G6 - Qualify and freeze new demos on platforms  |           3.0 |             22.0 | 06    |
| G7 - CI closure, packages and review handoff    |           2.0 |             24.0 | 06    |

Estimates include focused tests and publication, not arbitrary idle waiting.
G1, G3 and G5 carry the largest uncertainty. Re-estimate as soon as the first
trace exposes a larger problem. Do not use the estimate to replace gameplay with
an inventory report. Genuine content/platform blockers permit independent work
to continue; code regressions in retained work require repair.

### G0 - Establish a runnable and reviewable baseline

Read the current PR464 review, latest W0-W7 closeout, XL evidence index,
EXTENSIONS.md, regression_demos/README.md, tests/INPUT_DEMO_ASSETS.md and current
CI workflows. Refresh all identities in section 2 without disturbing unrelated
work. Record actual start time and revise the queue estimate.

Repair the packed-field forwarding compilation failure in the existing receipt.
Check nearby newly added JSON construction for the same actual problem; do not
globally rewrite packed structures. Compile the failing Linux native D2/save
target, D2 headless target and a Windows target. Use hosted fork CI if local Linux
is unavailable. Refresh all other failed jobs and integrate relevant findings
from the independent review, rather than duplicating its investigation.

Build a concise gameplay capability table in this plan or the existing XL
evidence directory, using the 19 EXTENSIONS categories and existing native
inventory. For each unmet predicate record the exact source condition, affected
original level/object/action, whether it can affect SIM, owning runtime code and
the next meaningful test. Classify supported/proved, implemented/unproved,
missing gameplay, presentation-only, invalid content and unavailable genuine
content separately. Inspect all available XL requirements once; don't spend
hours regenerating metadata that is already present.

Select a fixed gameplay witness set before changing admission. Start with:

- Pumo original ordinary start and its existing controlled combat/progression
  cases, keeping assisted and connected outcomes separate.
- Warp Shock 5 volcano.rl2 for original HXM combat.
- Warp Shock 6 warplab.rl2 for time equipment and normal-input continuation.
- Original Dinter for large-level navigation, generated actors and density.
- One Boiling canyon for the optional-ambient decision; control.rl2 for makers;
  subzero.rl2 only after its valid/invalid layout decision.
- Existing Ahayweh normal key/reactor/escape evidence as a retained progression
  baseline, plus stock D1/D2 compact recordings as non-XL controls.

This is a witness set, not a replacement for the frozen readiness cohort.
Add a case only to cover a distinct missing gameplay category, using existing
owners. Do not add a per-level runner.

Acceptance: baseline build regression fixed with exact source receipts; failed
and skipped checks classified; fork/trunk/stack lineage verified; source/cache
associations correct; capability table and witness set recorded. Publish layer 01.
The capability audit is not a claim of runtime completion. The active G0 map and fixed witness set are in [gameplay-capabilities-20261111.md](../XL_compat/evidence/gameplay-capabilities-20261111.md).

### G1 - Make Omega and time equipment recordable and replayable

Audit every input and clock consumer before changing the format. Trace normal
gameplay, automap, rear/extra views, camera rendering, pause/menu, frame stalls,
level reload and shutdown through the existing ordered dispatch phases.

Required implementation:

1. Specify one small extension to the existing input-demo contract for all
   gameplay-significant external time/event inputs. Record the elapsed transition
   clock and the render-event sequence/count at the exact points the simulation
   consumes them. An end-of-frame count is insufficient if operations interleave.
   Use bounded integer fields, defined ordering, overflow/wrap handling and
   explicit required capabilities. Keep build SHA as provenance, not an
   exact-build playback requirement.
2. Preserve source timing rules: slow-motion updates at most once when strictly
   more than 25 ms has elapsed; no catch-up loop. Preserve factor/rounding order,
   drain, activation/reversal, reactor force-off and world/player clock separation.
   Omega charge suppression depends on last and preceding render frames. Do not
   replace it with one synthetic render per simulation frame merely to pass.
3. Extend ordinary ordered controls for desktop Alt+S/Alt+B and Android Extra
   actions, including press/release, repeated/two-in-one-batch commands and the
   record-stop tail. Use the same dispatcher in normal play and playback.
4. Playback consumes the recorded environmental timeline; it does not query the
   current wall clock, audio callback or GL redraw rate for simulation decisions.
   Rendering may use a separate presentation clock. No replay-specific state
   patches, forced RNG values, reissued controls, or expected-state injection.
5. Keep existing format-5 stock playback and immutable public fixtures working.
   Decide whether a capability extension or a new top-level format is necessary
   after auditing every reader. If semantics require a version bump, make it
   explicit and retain the supported stock reader; do not silently reinterpret
   old frames or recapture unrelated stock oracles.
   Inventory all simulation-affecting settings: difficulty, Guide-Bot routing,
   input scaling, physics/weapon options, RNG/FP mode and XL time profile.
   The present slow-motion adapter qualifies default speedup=6 and difficulty
   0-4; record that boundary and reject unsupported settings explicitly until
   their real behavior is implemented. Do not let playback inherit launcher
   defaults or claim every XL option from this one qualified profile.
6. Inventory every existing Omega/time guard call in game.c and shared demo
   start/hooks/newdemo owners. Relax each only for a fully validated supported
   timeline. Legacy/malformed/unsupported timelines must reject before mutation.
   Picking up Omega/time equipment during recording must work too; ownership at
   frame zero is not sufficient coverage.

Tests, extending existing owners:

- Extracted reference clock cases: 25/26 ms boundaries, long stall, reversal,
  depletion, reactor transition, two factors, Omega fired/idle/recharge/wrap.
- Real ordinary recording with Omega firing/damage and recharging, and one with
  actual time-pickup acquisition, activation, reversal and combat. Derived setup
  controls can isolate cases, but label them; retain original Warp proof in G5.
- Compare live capture with immediate replay and two clean-process replays using
  per-frame semantic state and SIM RNG. Alter display rate, extra view and replay
  pacing without changing the recorded timeline. Headless proof follows G3 if
  the current loader cannot yet execute it.
- Negative missing/duplicate/nonmonotonic/out-of-range/unknown timeline fields
  and truncation reject atomically. Ensure unsupported future capabilities do
  not silently default.
- Existing public stock recording/replay suite remains unchanged and passes.

Acceptance: normal player controls can produce a faithful XL Omega/time demo;
supported playback is deterministic on the currently qualified backend;
source-derived timing assertions pass; capability guards are honest; format and
forward-compatibility policy are documented. Publish layer 02. Do not claim
cross-platform or graphics-independent qualification before G3/G6.

### G2 - Preserve complete gameplay state across save and replay boundaries

Audit the actual state owners, not only fields already emitted in result JSON.
Use existing trigger_save.cpp, slow_motion_save.h, weapon_clock_snapshot,
equipment/boss/object-trigger owners and generic object/world/RNG traces.

Required state includes:

- Time factors/directions/ages, Omega charge/fire markers, primary/secondary
  deadlines, fusion/guided state and ordered-control state still pending.
- Player resources, keys/equipment, death/respawn, collision-delay state,
  boost/water/protection effects and all persistent projectile hit history.
- Object slots/signatures, allocator/segment lists, AI awareness/targets/timers,
  generated bosses/robots, spawning queues, loot/ownership and Guide-Bot state.
- Earned wall/door/material passability, trigger enablement/one-shot/delay queues,
  master/object actions and their source-bound owners, generators pending phase,
  reactor/countdown, secret/base return and level completion.
- Relevant persistent static/global state across level -> stock -> XL transitions.

Create the smallest semantic XL extension to the existing detailed observer.
Prefer existing typed snapshots rather than duplicating save decoding. Compare
frame zero, each simulation boundary, save-before/save-after, restored and
terminal state. Preserve SIM RNG draw counts/order as well as final RNG state.
Separate proven presentation state with an explicit rationale; do not exclude
unknown fields or entire XL objects to obtain a match.

Verify uninterrupted execution versus a real disk save and fresh-process load
with identical future inputs, not just a replay checkpoint that repairs omitted
state. Exercise a pending trigger/generator, active time transition, guided or
projectile state and an earned progression change. Use few combined cases.
Pause/menu elapsed-time treatment must be defined; loading must not import the
new process's arbitrary timer epoch.

Validate all extension data before publishing any restored state. Bad counts,
source identities, object signatures, timers, factors or unknown required
capabilities fail without partially changing the world. FX clouds may rebuild
where already supported, but allocator/SIM effects of rebuilding must be neutral.

Acceptance: detailed XL coverage map has no unknown gameplay state in the
qualified features; cold and uninterrupted continuations agree; invalid restore
is atomic; repeated stock/XL transitions clear transient owners correctly.
Publish layer 03 and update schema documentation without a compatibility shim
for every historical development save.

### G3 - Separate required simulation from optional presentation

This is a runtime boundary change, not a blanket "ignore missing resources" flag.
Start with actual compilation/activation paths:

- android/XL_compat/runtime/configure.cmake and preview/resource validation.
- d2/CMakeLists.txt, d2/main/CMakeLists.txt,
  cmake/dxx-headless-targets.cmake.
- android/app/src/main/cpp/headless/d2_headless_runtime.c and
  input_demo_headless_main.cpp.
- particle, camera, model, POG and named-audio owners, plus all uses of
  DXX_XL_RUNTIME/OGL that mix simulation with renderer capability.

The current configure.cmake already attaches DXX_XL_RUNTIME and its sources to
dxx-redux-d2-prefixed targets, including headless targets in an OpenGL-configured
build. These targets can link GL libraries without creating a graphics device.
First reproduce the actual activation/resource/GPU-call blocker; do not assume
that every headless target lacks XL code or undertake a broad build-system
rewrite. A genuine no-device run is the acceptance condition, not absence of
all linked graphics libraries.

Extract/share only the necessary existing simulation ownership so the real
headless D2 loop can load and replay supported XL with XL active. Keep renderer
entry points behind capabilities and avoid a second loader, trigger interpreter,
resource registry or fake GL device. Preserve explicit software-renderer UI
limitations; a successful console replay does not certify software-rendered play.

For each resource/effect, decide from its consumers:

| Must remain gameplay-required                                                                             | May be optional after proof                                                               |
| --------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------- |
| Geometry, topology, object/weapon/robot definitions, gun transforms, hitboxes, required source identities | Ambient sample output, particle sprite pixels, cosmetic lighting/camera output            |
| Texture transparency/supertransparency used by collision or AI, destructible switch/door semantics        | A visual skin only when native gameplay geometry/material information is already complete |
| Event ordering/flags/timers and any resource metadata selecting a gameplay branch                         | Backend mixer/GL handles and presentation-only animation phase                            |

Do not replace a missing model with an unrelated stock model, fabricate a POG,
rename a wave, or alter mission bytes. Optional absence is represented explicitly
and reported in normal diagnostics. Resource-present paths retain genuine owned
assets. Resource lookup errors and malformed payloads must not be swallowed
generically. Preserve save/source ownership and known asset mismatch rejection.

For the missing Boiling waves, trace sound-trigger and ambient behavior through
runtime/named_audio.cpp and pinned DoPlaySound, including tOperated/playing flags,
loop cancellation and trigger reuse. If output success currently affects saved
event state, define a deterministic logical event separate from device playback
and validate its gameplay consequences. Do not invent successful audio playback.
Any deliberate user-scope presentation policy must be documented against source.

Audit slow-motion transition_sound and mixer/sink success as well. Audio-device
availability must not change movement, energy, firing or future trigger state.
Do not merely omit such fields from the comparator without tracing consumers.
Missing watrwrks POG stays blocked unless its gameplay semantics can actually be
resolved from authentic data/source; "graphics is out of scope" is not that proof.

With a fixed recorded timeline, compare the same simulation with full effects,
effects disabled and headless/no audio device. Investigate render visibility
lists, animation updates, FX RNG, shared object allocation and audio callback
paths that alter SIM. Fix the owner; do not copy rendered state into headless.
Budget dense optional effects in their shared owner if required for responsive
play, with no skipped simulation steps or per-level special cases.

Acceptance: an XL navigation/shooting recording runs in the true headless target
with XL active and matches rendered SIM/RNG; Omega/time recordings from G1 do too.
Ordinary rendering remains functional. Missing optional ambient assets no longer
block only where the source/consumer audit proves the policy, with honest
presentation warnings. Gameplay-critical missing/invalid resources still reject.
Keep the frozen readiness measurement separate, including its failures.

### G4 - Close the remaining valid authored gameplay predicates

Work from G0's complete feature map, not only Boiling's first error. Rank actions
by original levels unlocked and deterministic gameplay impact. Finish each
predicate's real runtime, persistence and replay before relaxing admission.

1. Qualify source no-op smoke actions 20-24 through full trigger dispatch:
   activation eligibility, one-shot/permanent/alternate flags, delay/master
   traversal, operated state and object-owner lifetime still apply. These are
   source-defined inert leaves, not silently discarded unknown actions. Do not
   start a particle-parameter feature absent from the pinned implementation.
2. Resolve equipment IDs through actual source selection/creation/pickup behavior.
   Keep selection RNG and occupancy/allocation semantics. Multiplayer-only
   rewards remain inert/rejected as source and single-player policy require;
   undefined IDs cannot index missing powerup/vclip resources. Add no speculative
   Turbo or Megawow behavior. If genuinely missing source resources prevent valid
   item creation, report the blocked generator rather than claiming support.
3. Investigate subzero object 4 from the first serialized object onward using the
   pinned reader and exact section offsets. If it is a decode bug, repair the
   shared parser and adjacent/truncated cases. If it is an invalid authored tag
   with no safe runtime semantics, retain rejection with the exact byte evidence.
   Never mutate original bytes or normalize all unknown movement tags.
4. Implement remaining required single-player gameplay leaves found by the map,
   including robot reprogramming if present. Extend the current trigger/AI owner;
   prove real changed targeting/firing, lifecycle, save and replay. Audit water,
   boosts, environmental damage, custom HAM/HXM, capacities and boss behavior
   for any actual unimplemented predicate exposed by the wider inventory.
5. Re-run original affected level entry after each predicate. A new next error is
   progress in diagnosis, not original-level acceptance. Record the whole chain.

Use existing inventory/reference/native cases plus a few combined original
witnesses. Keep malformed/unknown feature guards, capacity checks and multiplayer
guards intact. Correct misleading README statements for the now-implemented
features. Publish layer 04 only after both G3/G4 changes are coherent; split a
large newly discovered independent feature into a named extra layer if needed.

Acceptance: every G0 gameplay gap is implemented and meaningfully tested or has
an exact external/invalid/out-of-scope classification. Valid unimplemented
single-player behavior remains open, not "deferred graphics." Original outcomes
are recorded separately from parser/source controls.

### G5 - Prove connected navigation, shooting and mission lifecycle

Use the existing shared Pumo/native navigation/automation owners and input
recorder. Extend data-driven scenarios, not per-action/per-level frameworks.
Use original assets and ordinary runtime load/import. Derived cases remain
explicitly derived.

Required observations:

- Move from a fresh level start through actual connected geometry. Cross doors
  after earning keys/activating switches; confirm physical segment transitions.
  For Dinter, trace actual teleport/master/object-event links between components
  and activate them normally. A shortest-path graph, camera pose or teleport
  assist does not establish this route.
- Fire ordinary primary/secondary weapons, hit/miss actual geometry, take damage,
  kill a hostile actor, receive normal loot and consume/replenish resources.
  Include Omega/time equipment, HXM actors and generator/boss output where those
  features occur. Retain causal no-fire/blocked-wall controls in shared tests.
- Operate boosts/teleports/water, relevant object actions and delayed chains in
  the same live world. Validate AI/projectile visibility and material collision,
  not just wall flags or submitted draw counts.
- Save an earned state, close the process, load it normally, continue fighting
  and navigating. Exercise death/respawn and authored spawn setting in a compact
  control, with inventory/lives/reset semantics verified.
- Finish at least one suitable original XL level by ordinary key/objective,
  reactor/boss and exit actions; reach the real score screen and next-level or
  mission-end state. Test secret entry/return and next-level preflight in existing
  lifecycle controls. Unsupported next content must reject clearly without
  corrupting the earned save or claiming a completed campaign.

Reuse the already earned Ahayweh route/checkpoints for retained regression, but
do not use them as the sole proof of newly implemented features. Require new
connected evidence on at least Warp 5 and a time-equipment route, plus a real
Dinter route crossing its relevant authored event connection if such a route
exists. Do not demand a Dinter whole-level completion before every independent
fix; record unfinished sections and continue solving the concrete blocker.

Assisted starting poses are permitted for isolated mechanics and diagnosis.
Every receipt must identify the assist and the route portion it cannot certify.
Ordinary progression acceptance uses no position writes, key grants, invulnerability,
artificial boss deaths, timer pauses, or expected-result edits. An earned
checkpoint is valid for continuation; it is not a fresh-start full playthrough.

Record demos during these runs, but call them candidates until G6 verifies live
capture fidelity. Fix any retained regression at its shared owner. Source
comparisons prove the relevant mechanics; actual route/combat traces prove their
integration. Neither substitutes for the other.

Acceptance: documented connected route/combat witnesses and at least one real
original completion, normal cold continuation, death/secret/transition controls,
all newly required gameplay categories exercised. Every remaining route or
campaign limit is explicit. Publish layer 05.

### G6 - Freeze forward-compatible gameplay recordings and platform proof

Reuse android/regression_demos/README.md, tests/test_input_demo_regressions.ps1,
test_input_demo_regressions_graphics.ps1, test_auto_level_demos.ps1, existing
compact/private owners and detailed state/RNG comparators. Do not add a parallel
XL harness. Original/private recordings and checkpoints remain in private/local
artifact storage with hash manifests; do not commit retail mission bytes or
embedded proprietary checkpoints to the public fork. Public fixtures must use
already permitted public data.

Choose approximately four to six compact combined scenarios from G1-G5, covering
all new mechanics without an exhaustive Cartesian product. Record:

1. Ordered ordinary flight/shooting with Omega and real hostile consequences.
2. Actual time-pickup/control sequence, factor transition, drain and combat.
3. Earned door/trigger/generator state plus cold continuation.
4. Original HXM/boss encounter or Dinter event-connected traversal.
5. Completion/transition or secret/death lifecycle where not already covered.

For each accepted recording require live capture versus immediate playback,
two independent clean-process playbacks, detailed semantic state and SIM RNG
through terminal state, asset/source identities and natural clean exit.
Repeatability of two wrong replays is not recording fidelity. Re-record a
candidate after a real engine fix, preserving the failed evidence; never copy a
replay result into the expected recording.

Platform matrix:

| Path                          | Required evidence                                                                                           |
| ----------------------------- | ----------------------------------------------------------------------------------------------------------- |
| Windows normal GL             | Live original capture, immediate/repeated playback, responsive movement/shooting                            |
| Linux true headless           | Same accepted inputs/state/SIM RNG without DISPLAY/WAYLAND or a video/audio device                          |
| Linux rendered/software GL    | Same inputs with actual rendering; do not relabel Xvfb as headless                                          |
| Android x86_64 owned emulator | Normal import/launch, touch/controller time actions, real save/cold restore and recording export            |
| Android -> host               | At least one Android live recording matches its captured state on host; record ABI/FP settings              |
| Android other packaged ABIs   | Universal APK contains the normal configured libraries; build evidence only unless hardware is actually run |
| Stock D1/D2                   | Existing unchanged public gameplay, save and replay controls pass                                           |

Run Android owners serially with existing device/profile guards and exact
restoration. Start by checking adb devices and app/profile ownership. Reuse
test_xl_addon_import.ps1 and its shared scripts for authentic resource/mission
setup, and test_auto_level_demos.ps1 for recording/export where applicable.
Keep malformed data, actual timeout and restoration failures as failures.
Record attempt IDs and measured elapsed time: the prior Dinter owner's successful
333-second warm run was not evidence of a strict 240-second completion.

Forward contract:

- Stable semantic feature/capability identity, ordered inputs/timelines, initial
  state, required settings/asset identities, terminal expectations and diagnostic
  schema are documented. No exact commit whitelist.
- Later graphics/audio-only changes must pass the immutable gameplay corpus.
  Proven FX differences are reported separately from SIM; unknown differences
  are investigated. Do not redesign asset verification to allow arbitrary mods.
- A future gameplay fix that changes valid behavior requires an explicit
  compatibility decision/version boundary and retained old evidence. This
  tranche creates a maintainable contract, not a promise that every future bug
  fix can preserve an incorrect historical trajectory.
- Demonstrate the forward policy with different supported presentation settings
  on the same accepted recordings. A version label alone is not stability.

Acceptance: all selected new recordings satisfy the matrix or precisely named
platform blocks; stock fixtures unchanged; forward policy enforceable through
the existing owners; Android protected files exactly restored. Missing required
platform proof prevents the full qualification label even if code is implemented.

### G7 - Close CI, package and deliver a reviewable stack

Run one final scoped quality invocation, relevant complete native suites and
required automation catalog checks. Do not keep rebuilding everything without
new changes or unresolved concerns. Test the final committed implementation;
if a follow-up changes source, refresh affected packages/tests and provenance.

Review the native stack and all exact-head checks in the fork. Required series
include XL compatibility contracts, public gameplay/save/replay checks, affected
engine/script checks and the trusted original-asset owner. Use existing
xl-asset-request.yml / xl-pr-assets.yml / nightly controller boundaries.
The controller requires the owned repository/actor and approved asset environment.
If the current publishing identity is ineligible, report it; do not impersonate
the owner, broaden token permissions or weaken the trusted controller.

The run_xl_originals.py --fast historical/reviewed membership, original assertions,
timeouts and immutable input verification remain intact. An asset-request
workflow success is not the downstream original-test result. Retain the full
frozen readiness sample and distinguish its load/tick result from the new
rendered/gameplay qualification. Report skips by exact predicate and dependency.

Build runnable Windows D1/D2 folders through the root build helper or verified
root-bound cache. Build the normal universal Android APK, not just the x86_64
iteration variant. Confirm packaged library ABIs and executable/cache source
identity. Do not bundle proprietary retail/mission/resource assets. Hash the
final executables/APKs and retain the test APK identity if used.

Final handoff must contain:

- Fork URL, trunk, every branch/parent/PR URL and pushed commit/tree, native stack
  number, exact tested identities and uncommitted working diff if any.
- Review commands for the root checkout, per-layer parent...branch differences
  and aggregate trunk...tip differences. Explain any prerequisite #464 still
  awaiting user merge. No merges performed.
- Final Windows folders and D1/D2 EXE paths/SHA-256; universal APK and test APK
  paths/SHA-256; build commands/cache source, logs and install/run commands.
- Ordinary Windows -hogdir/mission steps and Android retail/addon/mission import,
  enable and launch steps; normal time controls, save/load and demo recording/
  replay steps. Do not require developer pose hooks to play.
- Results split into public/source, private/native, original play, input replay,
  Android, hosted CI and readiness. Explicit implemented, blocked, unverified and
  deferred-presentation lists. No whole-stack merge-readiness claim from local
  checks alone.
- The exact next unfinished gameplay item if the full closure gate is not met,
  with evidence, source anchors and revised work estimate.

Acceptance: coherent stack published to the fork with accurate descriptions,
final-source packages and verification, honest unresolved gates, clean handoff.
This closes delivery, not an unfinished gameplay category by renaming it deferred.

## 6. Shared validation and command anchors

Read each owner's current help before invoking it; paths below are anchors, not
permission to use a stale binary. Use root helpers, verify CMAKE_HOME_DIRECTORY,
and record the build commit plus working patch/tree. The old scratch launchers
may point at another checkout.

    .\run-windows-build.ps1
    python android/XL_compat/tests/public_contracts.py --work <new-output> --reference-root temp/d2x-xl-reference
    python android/tests/run_xl_originals.py --repo . --work <new-private-output> --fast --reference-root temp/d2x-xl-reference
    .\android\tests\test_input_demo_regressions.ps1 -Corpus public
    .\android\tests\test_input_demo_regressions_graphics.ps1 -Corpus public
    pwsh -NoProfile -File android/tests/test_validate_automation_catalog.ps1
    pwsh -NoProfile -File android/tests/test_run_all_tests_catalog.ps1
    git diff --check

On native Linux add --gpu --audio to the public reference owner where those
existing gates are required; their output checks are distinct from headless
gameplay. run_xl_originals requires verified game_data/retail-ci/xl inputs and
the existing staging contract. Do not replace that contract with scratch files
having the expected names.

Supplemental original rendered load/tick remains:

    python android/tests/xl_rendered_readiness.py --engine <root-built-D2> --data <authentic-retail-root> --extra <owned-resources-and-missions> --mission <descriptor-name> --level <number> --level-file <authored-member> --work <new-output>

Keep its existing 45-second/120-simulation/120-rendered-frame criteria. The
runner's generic acceptance does not itself require xl_active to be true for
every possible input; explicitly assert and report true for an XL gameplay
claim. Do not modify it to substitute for the ordinary route/combat owner.

For Gradle set JAVA_HOME=C:/local/jdk-21 first. An x86_64 iteration build is fine;
the final :app:assembleDebug must omit ciTestAbi narrowing. Use installed pinned
dependencies/offline caches when available. Run the scoped mixed-language
android/run-code-quality.ps1 -Fix -Paths invocation once and await completion
before further edits. Add top-level tests only when unavoidable; register them
and their timeout, and mark child scripts non-standalone with their existing owner.

## 7. Runtime discipline and bounded external investigations

- Keep shared owners under android/XL_compat and existing shared/native/Android
  test facilities. Make small D1/D2 hooks; preserve stock and cross-platform builds.
  No copied emitters, per-level runners, per-action wrappers or new test framework.
- Invalid data checks remain source-aligned. Distinguish source no-op semantics
  from unknown/unsupported behavior. Source adapters must execute pinned source,
  not an expectation copied from the candidate implementation.
- No original mission-byte edits, fabricated resources, audio filename aliases,
  shortened tests, reduced frozen cohorts, or expected-state replacement.
- Missing-wave content searches are already exhausted for the named cases.
  Spend at most 15 minutes total only if a genuinely new authentic package/index
  appears. Spend no time on external audio recording unless a simulation defect
  requires it; output fidelity is outside this tranche. Record the concrete
  content limit and continue gameplay work.
- Spend at most 20 minutes diagnosing unavailable local Linux/WSL before using
  the fork's normal hosted checks and continuing independent implementation.
  Do not leave hung WSL jobs consuming resources. Missing private credentials or
  hosted approvals remain explicit external gates, not local passes.
- Free space is ordinary development maintenance. Follow AGENTS.md and
  android/CLEANUP.md: preview android/clean-workspace.ps1, wait for active
  builds/tests, exclude current runs, authentic inputs, source reference and final
  evidence, then use -TemporaryOnly -TargetFreeSpaceGB 50. Include
  -PreserveEmulatorWorkspaces when applicable. Do not hand-delete caches or ask
  for a new schedule merely because cleanup is needed.
- Before new output generations use android/helpers/retain-recent-artifacts.ps1
  -Artifacts <planned-output>. Preserve current source/reference/assets and
  evidence from cleanup explicitly; ignored temp contents are otherwise disposable.
  Keep compact durable handoff facts in Git-visible evidence, private bytes private.
- No tests may share an Android device concurrently. Restore the exact protected
  profile even on failure; preserve failed attempt receipts and validate current
  save/resource ownership before the next run.

## 8. Shared ledger

Update this table and append a concrete handoff after every item. "Implemented"
and "accepted" are separate facts. A blocked item names the exact dependency and
what independent item can proceed. An internal regression is repaired before
retaining the change. All statuses below are planning statuses, not completed work.

| Item | Status                                                                                   | Branch / commit / PR                                              | Evidence, limits and revised estimate                                                                                       |
| ---- | ---------------------------------------------------------------------------------------- | ----------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------- |
| G0   | In progress: baseline repairs implemented; Linux validation and capability audit running | xl_gameplay_20261111_01_baseline; parent e87f4bfae; not published | Windows 100 pass / 1 expected skip; targeted original Warp checks pass; WSL recovered, fresh root-bound GCC builds required |
| G1   | Not started                                                                              | 02_timeline                                                       | Omega and time-equipment record/replay guards still active                                                                  |
| G2   | Not started                                                                              | 03_continuation                                                   | Complete XL observer/cold-continuation audit required                                                                       |
| G3   | Not started                                                                              | 04_runtime                                                        | True-headless XL and optional presentation independence unqualified                                                         |
| G4   | Not started                                                                              | 04_runtime                                                        | Source no-op smoke, makers, subzero and wider gameplay predicates require decisions                                         |
| G5   | Not started                                                                              | 05_progression                                                    | Existing assisted views do not prove connected original play                                                                |
| G6   | Not started                                                                              | 06_qualification                                                  | No newly frozen forward-compatible XL corpus yet                                                                            |
| G7   | Not started                                                                              | 06_qualification                                                  | No new packages/branches/PRs created by this plan                                                                           |

Per-item handoff template:

    Item and status:
    Actual start/end/time zone; elapsed; revised remaining estimate:
    Root branch; parent/head/tree; dirty paths; pushed SHA; PR/stack:
    Files changed and concrete behavior:
    Pinned source functions/files consumed:
    Authentic input hashes and unchanged-byte verification:
    Build paths/cache source; executable/APK hashes:
    Tests/commands; pass/fail/skip counts; logs; source tested:
    Original-level outcome; assists; connected/combat/completion limits:
    Live capture vs replay; SIM/state coverage; cold continuation:
    CI run/head and skip/blocker reason:
    Android device/attempt IDs; exact restoration result:
    Implemented vs accepted vs external/invalid/presentation limits:
    Next unfinished item, exact next command/investigation:

## 9. Review instructions to retain in the final handoff

Substitute the actual recorded trunk/tip and layer parents. Quote Git revision
expressions containing braces in PowerShell.

    Set-Location C:/local/dxx-redux
    git status --short
    git branch --show-current
    git log --oneline --decorate <trunk>..<tip>
    git diff --stat <trunk>...<tip>
    git diff <parent>...<layer-branch> -- android/XL_compat android/app/src/main/cpp/shared d2
    git diff
    git diff --cached
    gh stack view --json
    gh pr view <PR> --repo larsonm-personal/dxx-redux
    gh pr checks <PR> --repo larsonm-personal/dxx-redux

Use git diff --no-index -- /dev/null <new-file> only where the host supports it,
or ordinary file review for untracked files; git diff alone omits them. State
whether all implementation is committed and pushed, and identify anything that
is not. Do not move the root checkout back to an old source tree after handing
off a runnable candidate without explaining where the final source resides.

## 10. Closure gate: when "gameplay ready for durable demos" is justified

All of the following must be true for the named single-player capability profile:

1. No unknown or unimplemented gameplay category remains in the completed
   source/corpus capability audit. Valid unsupported features are listed as
   blockers, not hidden by selecting easier levels.
2. Navigation, combat, authored mechanisms/resources, death, ordinary disk
   continuation and completion/transition have live original-content witnesses.
3. Omega/time control and every other gameplay-significant clock/input follow the
   same ordered contract in recording, rendered replay and true headless replay.
4. Detailed world/object/XL state and SIM RNG agree with live captures, clean
   repeats and required cross-platform paths. No replay-only repairs are used.
5. Missing/disabled optional presentation cannot change that simulation, and
   later presentation improvements are checked by immutable accepted recordings.
6. Existing stock/public/private required tests and current-tip CI are accounted
   for with no concealed failures/skips; Android restoration and source provenance
   are sound; the fork stack and runnable packages are delivered without merging.

If this gate is not met, the handoff names the achieved bounded scope and the
next concrete gap. It must not claim full gameplay support, full campaign
playability, graphical/audio completeness, or whole-stack merge readiness.
