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

| Item | Status                                                                           | Branch / commit / PR                                                              | Evidence, limits and revised estimate                                                                                       |
| ---- | -------------------------------------------------------------------------------- | --------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------- |
| G0   | Accepted baseline repair; remaining hosted checks and Android limitation tracked | xl_gameplay_20261111_01_baseline; implementation a5db8d739; draft #465, base #464 | Windows/Linux native each 100 pass / 1 expected skip; Linux public 61/61; canonical private 113/113 and hosted private PASS |
| G1   | In progress: Omega acquisition/combat, time/settings and modal fidelity pass     | xl_gameplay_20261111_02_timeline; parent 96ce2f8be; not published                 | Latest: Omega242 frames/246 world records/1616 SIM/488 FX match three clean plays; production admission remains closed      |
| G2   | Not started                                                                      | 03_continuation                                                                   | Complete XL observer/cold-continuation audit required                                                                       |
| G3   | Not started                                                                      | 04_runtime                                                                        | True-headless XL and optional presentation independence unqualified                                                         |
| G4   | Not started                                                                      | 04_runtime                                                                        | Source no-op smoke, makers, subzero and wider gameplay predicates require decisions                                         |
| G5   | Not started                                                                      | 05_progression                                                                    | Existing assisted views do not prove connected original play                                                                |
| G6   | Not started                                                                      | 06_qualification                                                                  | No newly frozen forward-compatible XL corpus yet                                                                            |
| G7   | Not started                                                                      | 06_qualification                                                                  | Layer 01 draft published; final packages and remaining layers not created                                                   |

G0 publication checkpoint, November 11 at 08:37 PST (16:37 UTC):

- Draft [#465](https://github.com/larsonm-personal/dxx-redux/pull/465) targets
  `d2xxl_nov10`, parent `e87f4bfaeec60fd7a9697488413e02cc88230ade`.
  Published commit `a5db8d739dbf9ce7e526e474f0f28bd60e79123f`, tree
  `cfe0a0d495a83eb9b089fecc775e2f94bcab8a62`. The local post-commit hook
  adjusts commit timestamps; the wall-clock checkpoint here is the actual time.
  No child or native stack exists yet. Prerequisite branch, stashes and independent
  review remain unchanged.
- Repairs and precise validation scope are in `pr464_ci_fixes_20261110.md`.
  The 19-category capability audit and fixed witnesses are complete for G0;
  unresolved valid/unknown gameplay predicates remain explicit G1-G5 work.
- Linux public GPU/audio contracts pass 61/61 with zero skips; the linked boss
  consumer passes 1/1. Fresh root-bound GCC developer desktop and metadata link.
  Their SHA256 values are respectively
  `5c764bccdc92ef784488dca670c8cad0f6796b9e7ca7c3cf39a5a2e34f846e16` and
  `2ad4fe57553064f5484c8f9b20aa2a8b61275631a4e9b848a636e81308fd3454`.
  Cache source is `/mnt/c/local/dxx-redux/d2`; implementation matches the
  published commit, with compilation begun before publication.
- Normal full-target Linux build uses the same root source and separate stock
  cache. Its initial two-job build was deliberately interrupted and resumed with
  four jobs after checking 14 GiB available RAM; interrupted logs are retained.
  This changes no test timeout or assertion. The canonical private owner awaits
  that cache; its 113-test cohort remains unchanged.
- Hosted engine `43059713354`, generic sample `43059713325`, XL contracts
  `43059713245` and private controller `43059766693` are pending. The generic
  sample now executes test selection. The private controller's own SHA is the
  trusted trunk, not the candidate; its title pins `a5db8d739`.
- Android full GPU owner still fails the unchanged timeout, with exact protected
  restoration documented in the fix plan. No new connected original gameplay or
  durable XL input demo is claimed by this layer.
- Elapsed wall time since the nominal November 10 21:55 start is 10h42m,
  including WSL unavailability and overnight idle intervals, not 10h42m of
  productive implementation. G0 needs roughly another 30-90 minutes of build/CI
  validation if no new failure appears. G1-G7 retain approximately 22.5 hours of
  work with substantial uncertainty; the original Wednesday-evening estimate
  is no longer credible. There is no cutoff or scope reduction.
- Next: finish stock build/CTest, run the unchanged canonical private fast owner,
  inspect exact-head hosted failures, fix any deterministic regression, then
  complete G0 and create layer 02 for the ordered clock/input contract.

G0 acceptance checkpoint, November 11 at 09:23 PST (17:23 UTC):

- The actual GCC normal desktop, replay, metadata and route targets and the
  developer desktop/metadata targets compile and link. Full remaining native
  build passed; CTest has 100 passes, zero failures and one expected NDEBUG
  connectivity-warning skip, 51.32 seconds. Windows has the same pass/skip count.
  All Linux caches bind to `/mnt/c/local/dxx-redux/d2`, not an older scratch tree.
- Canonical unmodified private fast CLI completed: 113/113 required tests,
  zero skips, 16 format witnesses, three modern source levels and four navigation
  anchors. Camera-addon subchecks are explicitly not run and level-two completion
  remains unqualified. Optional rendered Warp is excluded from this owner; the
  separate Windows warm/cold actor witness is assisted mechanics, not traversal.
- Nine immutable D2 shareware demos passed headless and nine passed rendered
  under Xvfb/software GL. These are terminal-summary and clean-exit controls,
  not G6 detailed XL SIM/RNG qualification. Full command/log/JUnit receipts are
  under `temp/pr464-ci-fixes-20261110/`.
- Published implementation remains `a5db8d739`, tree `cfe0a0d495a83eb9b089fecc775e2f94bcab8a62`.
  The documentation follow-up changes only this ledger and the fix evidence.
  Normal desktop SHA256 is `d78bfb153aedddec5dac79771cd78e697f67e655ffa5c704fbe9dad2648bf419`;
  headless replay is `070c25535993d5831ad6281e82820a484d79f0e13688c0582fecdf8326a30c1f`.
  Remaining binary hashes are in `linux-required-binary-hashes.json`.
- Hosted XL compatibility, scripts/CI, LAN and generic Linux/JVM/Android sample
  pass. Trusted private controller `43059766693` completed its host owner,
  finalizer and exact-head publication for `a5db8d739`. Its frozen readiness
  report remains the historical 4/8 measurement. Public save integrity passed;
  downstream replay and ASan workloads remain active. Engine D1/D2 failures are
  inspected apt DNS failures before compilation; retry after that run terminates.
  Exact job IDs and skip predicates are in `pr464_ci_fixes_20261110.md`.
- Android full GPU timeout remains unresolved with exact protected restoration;
  sparse timings are diagnosis only. No gameplay guard is relaxed by G0. No new
  connected original route, forward-compatible XL demo or full gameplay claim.
- Actual elapsed wall time since the nominal start is 11h28m, including overnight
  idle/WSL unavailability. G0 is accepted for its stated repair/audit scope.
  G1 now needs roughly 5-8 hours because the checkpoint clock-origin boundary
  must be solved alongside ordered clocks; G2-G7 retain about 17.5 hours, with
  substantial uncertainty. There is no cutoff or reduced scope.
- Next: commit the two evidence updates, create layer 02 from this layer, define
  and implement the bounded version-6 clock/input extension in existing owners,
  preserve format-5 stock recordings, and keep Omega/time guards until faithful
  live/immediate/repeated replay passes. Continue monitoring outstanding G0 CI
  without replacing pending results with passes.

G1 implementation checkpoint, November 11, after G0:

- Layer 01 evidence follow-up is committed and pushed as
  `96ce2f8be5f917178024298cf51272781abc0caa`, tree
  `efa492e4869ae526b69c9a0f25fc0ab27e58d411`. It changes only the two evidence
  documents relative to tested implementation `a5db8d739`. New exact-head CI
  started normally; results from the prior head remain identified as such.
- Local layer 02 starts at that commit. The existing codec now models an explicit
  version-6 environment origin/capability and per-consumer input/SIM samples.
  It validates sequence, phase/operation, closed consumer names, integer bounds,
  monotonic clocks, modulo render-counter progression, required input/tail
  boundaries and complete sample counts. Failed parse preserves the prior demo.
  Existing format-5 stock behavior remains supported.
- This is an unqualified contract under implementation. Existing readers reject
  unknown fields, so a capability extension could also reject safely; version 6
  makes the semantic boundary explicit. Playback currently rejects version 6
  before replacing its loaded session. No recorder emits it yet, and Omega/time
  guards are unchanged. Initial checkpoint clock origin, runtime consumption,
  ordinary controls and live/replay evidence remain required before acceptance.
- Root-bound Linux fixture/recorder/replay targets build; their three existing
  CTests pass in 2.76 seconds, including new valid wrap/interleaving and malformed
  timeline cases in the existing fixture owner. Logs are `g1-codec-build.log`
  and `g1-codec-ctest.{log,xml}` under the baseline artifact directory.
- Next: wire the environment stream through existing recorder/replay owners and
  ordinary simulation consumers, bind the saved clock origin before restore,
  then qualify actual control capture before relaxing any gameplay guard.

G1 stream/controls checkpoint, November 11 at 10:30 PST (18:30 UTC):

- The preceding codec checkpoint is historical. Local codec commit is
  `98c5f6bfe6b50aff03c11c4541ad3972b1c986eb`; this follow-up remains on layer 02,
  with no child branch, second PR or native stack yet. The independent review,
  prerequisite branch, three stashes and authentic assets remain preserved.
- Shared recorder/replay now carry the ordered per-consumer environment stream,
  including interleaved input and SIM observations, mandatory boundaries, wrap,
  exact consumer identity and unconsumed-tail rejection. Playback must be armed
  explicitly after world initialization. Stream validation fails without advancing
  its cursor; invalid capture remains unpublishable. Recorded output enforces the
  existing record/file limits before opening the destination.
- Runtime consumers use the same transition, pickup and Omega observations.
  `GameProcessFrame` already wraps its body in the shared phase owner on D1/D2;
  its pre-SIM capture hook is explicitly an input boundary inside that wrapper.
  The terminal-exit path verifies consumption before advancing to the input tail.
  Ordinary Alt+S/Alt+B requests use the existing typed command owner; repeated
  requests remain ordered while runtime pending booleans retain source coalescing.
  Android Extra routes through that existing key handler; no new Android proof.
- Native world admission still rejects format 6 before restoration. Production
  settings do not enable environment capture yet. The library can load/consume
  v6 for contract tests; this is not accepted gameplay playback. Saved-origin
  handoff, new-level inherited Omega state, complete required settings and real
  live/immediate/two-clean-process SIM/RNG fidelity remain unfinished G1 work.
- Native XL schema 35 preserves pending slow/bullet requests. One clock sample
  supplies save transition/pickup ages; the typed captured origin is available to
  recorder setup. The existing rendered clock probe now checks ordinary pending
  requests across a real disk save and rejects malformed pending booleans without
  changing runtime state. Its unknown-schema negative advances to 36.
- Final Windows input-demo CTests pass 6/6 in 8.55 seconds; Linux pass 6/6 in
  3.11 seconds, including repeated/tail commands and oversized-output rejection.
  Final root-bound Windows desktop and Linux desktop/headless/metadata builds
  pass without new warnings. The Linux developer desktop also links. No gameplay qualification
  is inferred from compilation. Logs are `g1-*.log` in the protected baseline
  artifact directory.
- Windows actual-GL Warp 5 rendered probe passes all 10 events and clean exit in
  `g1-pending-control-save-windows-final/result.json` on final source. Its EXE SHA256
  is `0103457716a9940952c79eb7f59ed08cf978150f6293ed4e908cc488b328ca76`.
  The initial attempt exposed an
  inherited `loadgl.h` true/false macro affecting JSON boolean comparisons and
  receipt types; fixed with explicit bool values. A stock Linux binary without
  developer probes produced no receipt and was correctly rejected. The proper
  developer Linux/llvmpipe attempt times out at the unchanged 45-second limit;
  its instrumented repeat reaches 8 events through `main_again`, then times out
  during disk restoration. This is an unresolved Linux result, not a skip/pass.
  Receipts retain both failures. These are assisted clock/save mechanics, not
  connected travel, live input-demo fidelity or original completion.
- Authentic Warp HOG remains
  `2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c`;
  pinned source remains `2576a98b5de310e3ac7c9777df2677d1627b8bae`. No fixture
  expectations, original bytes, required test membership or timeouts changed.
- All remaining hosted baseline checks now pass at `96ce2f8be`: D1/D2/ASan
  `43067970513`, public save/headless/rendered replay `43067970602`, generic
  Linux/JVM/Android `43067970565`, LAN `43067970833`, plus XL/scripts and trusted
  private controller `43068003067`. The latter pins candidate 96ce while running
  trusted trunk b3bb. PR #465's body now reflects these exact-head results. The
  local Android full-GPU timeout and historical 4/8 readiness remain unchanged.
- Official temporary-cleanup preview, with active assets/caches/reports and
  emulator workspaces protected, finds only 11 MiB reclaimable at about 12 GiB
  free. After owned jobs finished, apply removed those 276 artifacts and reported
  failure to reach the 50 GiB target; 11.96 GiB remained. No hand-deleted caches
  or WSL restart. Scoped mixed-language quality passes; final validation ended
  November 11 at 10:33 PST (18:33 UTC).
- Revised estimate: G1 still needs roughly 5-8 hours; G2-G7 remain roughly 17.5
  hours with substantial uncertainty. This checkpoint completes a contract/runtime
  plumbing increment, not G1 acceptance. Next: commit the guarded increment,
  bind successful checkpoint origin into ordinary recorder startup, implement the
  new-level initial clock state, and extend existing native capture/replay owners
  for live versus repeated detailed SIM/RNG comparison before opening admission.

### G1 checkpoint: ordinary capture and first live comparison, November 11 11:55 PST

- Previous goal turn classification: progress. Layer 02 remains local on
  `xl_gameplay_20261111_02_timeline`, parent/last committed head
  `5b56b2ffe1d880b36a2c609bb09461c8dafae57e`. This increment is not G1 acceptance
  and is not published. PR #465 remains the accepted G0 baseline; no other PR,
  stack link, upstream write or merge was performed.
- Developer builds now start ordinary XL format-6 recordings through the existing
  checkpoint/new-level owners, capture the save's actual clock origin, and retain
  the initial Omega marker and carried time flags for natural level entry.
  Replay checks its required startup profile before restoration and arms the
  timeline afterward. Production guards remain closed. Full asset/profile and
  pre-mutation world admission still require qualification before opening them.
- Extended the existing original time-equipment probe to record its physical
  slow-motion pickup, Alt+S activation/reversal and intervening live AI/combat.
  Its approach pose is disclosed and occurs before recording. The later assisted
  second pickup/save controls are outside the captured interval. The probe still
  runs its complete existing seven-event scenario with the unchanged 45-second
  timeout. Both captures completed; neither is a frozen gameplay oracle.
- First capture `g1-live-time-capture-1` wrote 198 frames. The existing replay
  runner exposed unsupported D2X asset preflight and unverified supplemental HAM
  ownership. Its bounded HOG checker now accepts the shared DHF/D2X directory
  dialect, including long names, while rejecting malformed headers, aliases and
  duplicate/truncated members. The same runner accepts an explicit owned
  `ExtraDir`. Supplemental HAM identity is still unverified; diagnostic playback
  used the existing explicit `AllowUnverifiedAssets` switch and cannot be counted
  as accepted corpus evidence.
- The first native attempt crashed in checkpoint SHA hashing with an uncaught
  `std::bad_alloc`; its failure receipt and dump remain under
  `g1-time-replay-1` and the wrapper sandbox. The SHA helper now streams 64 KiB
  chunks and returns allocation errors. A maximum-size checkpoint digest test
  retains the independently computed SHA-256. Playback then completed, but its
  frame-1 object allocation and SIM caller context differed despite matching
  terminal state and random values.
- Root cause: the ordinary save lost `Do_appearance_effect` before the initial
  simulation frame. XL save schema 36 retains this pending effect, with strict
  boolean validation; restore uses the ordinary save owner. No expected values,
  RNG forcing or replay-specific world patch was introduced. The unknown-schema
  negative now uses 37. Generic recording diagnostics can emit existing detailed
  object/world observations; live metadata may declare an unknown horizon of zero
  while the comparator still requires every actual frame at EOF.
- New capture `g1-live-time-capture-2`, demo
  `d2_wshock20_level6_20261111_105632.dximdemo`, and clean replay
  `g1-time-replay-3` match all 198 frame state/RNG/diagnostic hashes and all 811 SIM
  RNG events, including caller context. Both replay terminal checks passed.
  Detailed raw observations remain different: player `polyobj.alt_textures` is
  0 live versus -1 restored, and an effect's `sidenum` is 0 versus -1. The existing
  strict world validator also assumes a 1000-slot pool and rejects this XL pool.
  These are unresolved audit findings, not excluded fields. Full XL state, an
  immediate in-process replay, two clean repeats, Omega live capture and platform
  qualification are still outstanding.
- Retained Omega policy controls explicitly construct legacy format-5 inputs
  and still exercise rendered/headless/paused, acquisition and terminal-tail
  rejection. The developer ordinary recorder separately proves a timeline is
  selected. The first damage probe failed because its synthetic target was less
  than one second old, inside the implemented source birth-protection gate;
  the setup now advances its explicit test clock past that grace period. JSON
  booleans use explicit bool types because engine headers define true/false as
  integer macros. `g1-retained-omega-2` passes all eight existing rate/mode cases.
- Windows native build and six focused input-demo CTests pass (8.60 seconds).
  Asset checker tests: 19 pass, one expected Windows case-alias skip. Comparator:
  66 pass. XL asset/receipt tests: 45 pass with the MSVC/CMake environment loaded;
  the earlier bare-shell run lacked cmake/ctest and its ten errors are retained.
  Linux developer desktop and stock headless/test builds pass; all six focused
  Linux CTests pass in 2.93 seconds. Retained Windows boss spawning, transport
  and groups pass 3/3 in 81.13 seconds on the rebuilt metadata executable.
  Scoped mixed-language formatting/lint and `git diff --check` pass. The metadata
  build retains the existing int/bool comparison warning in
  `changing_light_test_records.h:18`; no new warning was found. No required
  cohort or timeout was changed.
- Authentic Warp HOG remains
  `2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c` and the pinned
  XL source remains `2576a98b5de310e3ac7c9777df2677d1627b8bae`. Reports and private
  checkpoint bytes remain in `temp/pr464-ci-fixes-20261110`. C: has fallen to
  about 3.94 GiB, below the 4 GiB producer reserve. Official cleanup preview
  found only 193 MiB in the replay wrapper, including the retained crash dump;
  that evidence is now explicitly excluded. No protected evidence was deleted.
  Large captures must wait for sufficient actual free space; source work continues.
- Revised estimate: G1 still roughly 5-8 hours; G2-G7 about 17.5 hours with large
  uncertainty. No time cutoff. Next: finish scoped checks/formatting and commit
  this guarded increment; resolve supplemental HAM identity and the detailed
  restore/observer differences through existing owners, then perform immediate
  and repeated faithful Omega/time recordings before publishing layer 02.

### G1 detailed-observer checkpoint, November 11 at 11:59 PST

- Previous turn/increment classification: progress. The ordinary-startup/save
  increment is committed locally as `0890fce0f9057f9ac8ca8a369cc6c4a8ee8f6f05`,
  tree `a08472b8ac0f511e6aef3dfe09f607abca75e49d`. No layer-02 publication,
  downstream branch or merge. G1 remains unaccepted.
- Extended the existing comparator with explicit native D2 left/right engine
  identity. D1 mappings remain on the D1 path. XL format-6 capability permits
  1000-32768 object/AI slots and at most 254 triggers; complete allocator arrays,
  stable capacities and matching AI/object capacities are still required.
  Projectile history uses the existing source's ordered twenty-victim list,
  enforcing unique bounded slots and the newest-hit mirror. Classic storage
  and unknown/mixed union data are not silently accepted as XL history.
- Audited `object.c` draw consumers and `endlevel_multi_impl.h`: alternate
  texture values 0 and -1 both mean no override. Audited `effects.c`,
  `effect_runtime_shared.c`, `collide.c` and `camera_glass.cpp`: inactive effect
  side 0/-1 is unused when segment is -1 and EF_ONE_SHOT is clear; activation
  assigns it before use. Native comparison normalizes only these two sentinel
  pairs and preserves the raw traces. Positive overrides, active effect sides,
  all RNG streams, endlevel storage and unknown fields remain compared.
- This audit exposed omitted static AI on XL camera objects. The collision
  admission predicate in `XlPreparedResources::validate_camera_query` reads
  `ctype.ai_info.behavior`; the existing object observer now includes that AI
  storage for control 18/type 16. The comparator requires it. The earlier live
  capture and replay lack it and now fail closed as incomplete at slot 331,
  recorded in `g1-time-replay-3/native-detailed-comparison.json`. Matching coarse
  hashes did not establish full state coverage. No original bytes or expected
  results were changed. Full typed XL time/owner state remains unfinished.
- Five new focused comparator controls pass, including malformed capacity,
  missing camera AI, hit ordering and active-side negative cases. Windows and
  Linux root-bound developer desktop builds pass. Scoped mixed-language quality
  and diff checks pass. Final comparator suite: 70/71 pass, with frozen-runner
  integration failing before its expected receipt at the 4 GiB reserve. After
  cleanup restored space, that sole failed test passed on retry in 2.03 seconds.
  The failed runs remain archived; no test was skipped or reserve lowered.
- Official protected cleanup preview and apply found zero eligible artifacts
  and failed to reach 50 GiB. The initially identified 193 MiB replay wrapper
  contains the retained crash dump and is now explicitly excluded. About
  3.8 GiB remained. The documented builds-only mode subsequently removed one
  obsolete 928.6 MiB Android native configuration from the old registered
  `temp/xl-stack-review-20261109/source` worktree, after a scoped preview with
  the current cleaner and resolved absolute build root. Source changes, APKs,
  reports and current dependency caches remain preserved. Free space reached
  5.27 GiB; the 50 GiB target still was not reached. Other workers/emulators
  were left running. A fresh capture/replay is now possible at the normal reserve.
- Fresh seven-event capture `g1-live-time-capture-3` and clean accelerated
  playback `g1-time-replay-4` pass. Demo
  `d2_wshock20_level6_20261111_120259.dximdemo` has SHA256
  `c3172bcdb284b5563d3142cb1a5eb3a466e92a521493866fff8c38b441a0dfd4`;
  Windows executable SHA256 is
  `36bf654976d16d603e514332b38f698a1867d63e8832afa0ce0bd7eafa1470bc`.
  Detailed comparison passes all 198 object frames and 202 world/boundary
  records and all 198 frame hashes, with all 811 strict SIM RNG events matching.
  Final verification ended November 11 at 12:05 PST (20:05 UTC). This covers emitted
  generic state plus the camera AI addition, not all XL owner state. Supplemental
  HAM remains unverified, so playback still uses explicit `AllowUnverifiedAssets`
  and this is not a frozen corpus. The pre-recording approach assist is unchanged.
- Further source audit found `camera_controller_begin_frame` reading SDL ticks
  directly for camera rotation. Its controller angle/delta/t0 and physics-union
  brakes also need typed observation/persistence review. This remains a G1
  external-clock gap even though the current witnessed cameras did not diverge.
  Include it in the ordered environment stream rather than declaring cameras
  cosmetic: orientation participates in installed geometry/visibility behavior.
- Next: finish this comparator/observer increment's scoped checks, then add a
  nonmutating typed XL time observation, fingerprint supplemental HAM sources
  through existing asset ownership, and qualify fresh immediate/two-clean-run
  Omega/time recordings when sufficient disk space is available. G1 estimate
  remains roughly 5-8 hours and G2-G7 roughly 17.5 hours, with high uncertainty
  and no cutoff. This checkpoint is not gameplay or corpus acceptance.

### G1 camera/time observation checkpoint, November 11 at 12:22 PST

- Increment classification: progress; G1 remains unaccepted. Previous increment
  is local commit `aaeb3a3a23048e61f503a350d1a1a40cebeadca4`, tree
  `a51634dfc3922923bb4d1ea8c9f441e533a68e7e`, on layer 02. This increment is based
  on that commit. Layer 02 is not published; no downstream branch or merge.
- Camera simulation now consumes an ordered `camera_frame` environment sample
  at its existing begin-frame boundary. Closed consumer/phase validation and
  recorder/replay tests cover it. Typed XL time and camera observations extend
  the existing world trace and strict comparator. Their clock basis is the last
  consumed sample; querying them does not consume samples, refresh the save
  origin or retire stale camera identities. The native live capture checks that
  purity. Complete owner coverage and active-camera cold continuation remain open.
- Ordinary recorder finalization emits the terminal observation after finishing
  the input tail, while recorder state is still available. Previously the probe
  emitted it before the tail while replay emitted it afterward. The shared owner
  now controls that boundary; the probe's duplicate emission was removed.
- Windows D2 desktop and Linux developer desktop builds pass. Six focused
  input-demo CTests pass on Windows in 9.21 seconds and Linux in 2.95 seconds.
  All 72 comparator tests pass in 36.656 seconds, zero skips. The root-bound
  Windows D1 desktop builds successfully too; existing scanline function-pointer
  warnings remain. Scoped mixed-language formatting/lint passes. No fixture,
  required test membership, timeout or producer reserve was weakened.
- Fresh seven-event native capture `g1-live-time-capture-4` passes. Demo
  `d2_wshock20_level6_20261111_121905.dximdemo` has SHA256
  `b312b1a25338f6fff5e612e17af07795f9ee14dba2410c425dfe7ac1f3ff4eb8`;
  tested executable SHA256 is
  `db6f4f58621a020814884adc345a2d34847ce5ebe9e35167fc5e66cb0777990f`.
  Clean accelerated replay `g1-time-replay-5` exits naturally and matches all
  198 detailed object frames, 202 world/boundary records including XL time and
  cameras, 811 strict SIM RNG events and all 198 frame hashes. The original
  pre-recording approach assist remains disclosed; no pose changes occur during
  the recorded interval. This is not connected original progression, complete
  XL coverage, immediate replay or two clean repeats. Supplemental HAM identity
  remains unknown, with explicit `AllowUnverifiedAssets`; no corpus freeze.
- Pinned reference remains `2576a98b5de310e3ac7c9777df2677d1627b8bae`; authentic
  Warp bytes are unchanged. The camera owner preserves the existing pinned
  rotation predicates. Further audit confirms trigger.cpp's cooldown reads
  timer_query directly, while the reference uses cached SDL ticks and a 750 ms
  threshold. Audit cached-clock ordering before integrating that consumer;
  restore currently also clears cooldown activation, requiring persistence work.
- After active jobs completed, the documented scoped builds-only cleanup preview
  and apply removed obsolete `.cxx/Debug/3j749a1m` under the registered old
  `temp/xl-implementation-20261109/source` worktree, reclaiming 2.66 GiB. Current
  dependency caches, modified worktree sources, authentic assets, reports and
  running foreign workers were preserved. The 50 GiB target was not reached;
  free space was about 6.3 GiB at 12:20 PST. Cleanup logs and all validation
  receipts remain under `temp/pr464-ci-fixes-20261110` with protected exclusions.
- Next: commit this validated observer increment, then integrate source-qualified
  trigger cooldown timing/persistence through existing owners and resolve
  supplemental HAM identity. Complete ordinary Omega/time recording qualification
  before publishing layer 02. G1 estimate remains 5-8 hours, G2-G7 about 17.5 hours,
  highly uncertain; no cutoff and no gameplay-readiness claim.

### G1 trigger-clock checkpoint, November 11 at 12:37 PST

- Previous increment was progress: `bab60e4697c5d2215c6717381c662a92d2051862`,
  tree `fad3c5c58ac282810afd2138a80b15c41638fa8b`, records camera timing and typed
  XL observations. This increment is based on that local layer-02 commit.
  G1 remains unaccepted; no layer-02 publication or downstream branch.
- Audited the clock owner: timer_query returns cached fixed-point elapsed time,
  not a fresh timer read. The pinned reference uses cached integer SDL ticks in
  CTrigger::Operate, with a strict elapsed <750 ms rejection. Its default
  PHYSICS_FPS=-1/PHYSICS_IN_BACKGROUND=0 path samples GetSlowTicks from
  CalcFrameTime before controls and simulation. Redux keeps its existing event
  and frame timer-update boundaries, exposing the exact last SDL sample through
  timer_query_milliseconds without another read or fixed-point conversion.
- The guarded format-6 environment now carries bounded, nondecreasing cached_ms.
  Wall activation consumes a wall_trigger sample with its trigger index in input
  or SIM order; nested, attached, teleport and speedboost exemptions remain.
  Camera rotation now uses this cached value too, as pinned cameras.cpp does.
  Old unqualified format-6 drafts missing this field reject; stock format 5 and
  immutable fixtures are unchanged. The C/C++ ABI assertion is updated to 20
  bytes and validated by existing packed-header compilation controls.
- Save schema 37 retains cooldown activation and age clamped at the source's
  750 ms threshold, rebasing onto current cached time or admitted replay origin.
  All rows validate before applying. Existing detailed world traces now contain
  scheduled trigger/actor/destination and cooldown state, with strict typed
  comparison. Reads do not advance the clock or consume environment samples.
- Extended the existing native chain owner and hash-pinned trigger adapter:
  eight active/inactive 0/1/749/750 ms runtime samples match the extracted source
  cooldown block; source eligibility controls cover nesting/attachment and exempt
  types. Native checks prove invalid restore is atomic and an active cooldown
  survives real disk save/load across a stock transition. Delayed object-chain
  trailer negatives include malformed cooldown fields. This is not yet a fresh
  process cold-continuation or live original-content wall-trigger recording.
- Windows D2 desktop/metadata and Linux developer desktop builds pass. Six
  focused input-demo CTests pass on Windows in 8.71 seconds and Linux in 2.94
  seconds. All 73 comparator tests pass in 38.886 seconds, zero skips. Windows
  D1 desktop also builds. Native chains, delayed object chains, boss spawning
  and trigger flags pass 4/4 in 33.28 seconds. The existing metadata int/bool
  warning in changing_light_test_records.h remains; no new warning was found.
  Initial builds caught the old ABI-size assertion and a missing native-test
  include; both were fixed before these successful runs. Scoped mixed-language
  quality passes. No membership, expected fixture, timeout or reserve changed.
- Fresh seven-event capture g1-live-time-capture-5 and clean accelerated replay
  g1-time-replay-6 pass. Demo d2_wshock20_level6_20261111_123330.dximdemo SHA256:
  `f1ae6cb566255bc2c96ce657876a929c042c54cf237e1f857f18af97e9c7f8b3`.
  Tested Windows executable SHA256:
  `d99f7268beadd540c716ebd519cac7cfdb6d68e5a3f20812c48b5baf879d9d98`.
  All 198 object frames, 202 world/boundary records including time/camera/trigger
  state, 811 strict SIM RNG events and 198 frame hashes match. This sequence
  contains no wall_trigger consumption; activation behavior is covered by the
  derived native and codec controls above. The pre-recording approach assist is
  unchanged. Supplemental HAM still requires explicit AllowUnverifiedAssets;
  these are diagnostic receipts, not frozen or connected-progression evidence.
- Reference remains `2576a98b5de310e3ac7c9777df2677d1627b8bae`; no source or
  authentic mission bytes were edited. Reports remain protected under
  temp/pr464-ci-fixes-20261110. Free space is about 5.8 GiB. Current elapsed
  implementation/check interval was roughly 12:25-12:37 PST.
- Next: commit this timing/persistence increment, then fingerprint supplemental
  HAM sources through the existing asset owner, including stacked loads and
  temporary container mounts. G1 still needs the complete settings/admission
  audit, ordinary Omega capture, immediate/two-clean-run and altered-pacing
  evidence. Revised estimate is 6-10 hours for G1 and roughly 17.5 hours for
  G2-G7, with substantial uncertainty and no cutoff. Full owner completeness,
  true-headless/platform proof and gameplay closure remain open.

### G1 supplemental asset checkpoint, November 11 at 12:54 PST

- G1 remains active/unaccepted on xl_gameplay_20261111_02_timeline. Source parent
  is f50da0615045fb0fbcd9e800f7b982335b90b587 (tree
  4f986da5fea541c3ffe3c4c7c253bb6285068329). This increment is local, with no
  layer-02 PR or hosted CI claim. G0 publication and prerequisite remain unchanged.
- The existing asset owner now records up to 16 ordered supplemental HAM/VHAM
  loads, including main mod HAM replacement and Vertigo's temporary HOG mount.
  Same-handle hashing preserves loader cursors; physical container provenance
  survives unmounting and is rechecked before capture. Numbered roles carry
  load order; gaps, orphan containers and mismatched container indices reject.
  Base HAM reload resets the sequence. Unresolved/overflow sources stay unknown;
  loading additional definitions during recording still invalidates the claim.
- Extended the native codec, existing native asset fixture, and shared Python/
  PowerShell preflight. ExtraDir supplies supplemental sources. Temporary mounts
  do not incorrectly join the final base-file precedence check. Native post-load
  comparison checks the actual sequence. Updated INPUT_DEMO_ASSETS.md. This is
  HOG/PIG/HAM provenance only, not proof of all HXM/POG/settings dependencies.
- Windows D2 desktop/asset fixture and Linux developer desktop/stock fixture
  builds pass. The actual manual asset fixture passes on Windows and Linux,
  including loose and temporary archive sources, cursor preservation, changed
  bytes after unmount, restoration, and ordinary manual recorder capture.
  Windows fixture ran in the private evidence parent (native.log); Linux ran in
  g1-supplemental-assets-native/linux. Neither used an authentic asset directory
  as its write root. An initial retention command used the wrong relative cwd;
  the root helper was subsequently invoked successfully for planned outputs.
- Windows D1 desktop builds; fixture/recorder/replay CTests pass 3/3 on D1
  (7.96 s) and D2 (7.61 s). Linux codec fixture passes 1/1 (0.17 s). Python asset
  tests pass 21 with one expected Windows case-alias skip (22 total, 0.601 s),
  including renamed VHAM identity and malformed/container mismatch controls.
  Scoped mixed-language quality and git diff --check pass. No test membership,
  timeout, authentic input, expected fixture, or free-space reserve changed.
- Fresh seven-event original Warp 6 time capture g1-live-time-capture-6 and clean
  accelerated g1-time-replay-7 pass WITHOUT AllowUnverifiedAssets. Header includes
  complete d2_extra_0_hog (authentic d2x.hog) and d2_extra_0_ham (d2x.ham), in
  addition to base and mission identities. Demo
  d2_wshock20_level6_20261111_125122.dximdemo SHA256:
  `4ee0d3ebce503ef769c41d455bc4a030402128784b83abad010e00408f3d054e`.
  Tested executable SHA256:
  `2cd2eaf86e064859e6de39849f46acad769b5af162426fdbe233443dca9c5bed`.
  Detailed live/replay comparison passes all 198 object frames, 202 world records,
  811 SIM RNG events and 198 frame hashes. Results are in protected
  temp/pr464-ci-fixes-20261110/g1-supplemental-assets-live-comparison.json.
  The previously disclosed pre-recording approach pose remains; no new assist
  occurs during recording. This is not connected progression or frozen corpus.
- Authentic Warp HOG remains
  2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c;
  Vertigo HAM is faa67189ccf5c7d6cdc8e37fa009fb21a1d57ee2c6c212ce091642e4366e3c0f.
  Reference source remains 2576a98b5de310e3ac7c9777df2677d1627b8bae. Free space
  is about 5.7 GiB. Work/check interval was approximately 12:41-12:54 PST.
- Next: commit this coherent asset increment, then inventory simulation-affecting
  settings and pre-mutation admission in shared recorder/start/codec owners.
  Ordinary Omega firing/damage/recharge capture, two clean repeat playbacks and
  altered pacing remain required. G1 estimate remains 6-10 hours; G2-G7 roughly
  17.5 hours with substantial uncertainty and no cutoff. Production guards stay
  closed until qualification; no Android or whole-stack closure is claimed.

### G1 startup-profile checkpoint, November 11 at 13:03 PST

- Previous goal turn made progress: supplemental identity increment committed
  as f0095687a4c0447c0ad79a688dc0a51e6545f2c4, tree
  9e8012c8c3a3626d66ef1984180885171276aa95. Current work stays on layer 02;
  G1 remains active/unaccepted, no layer-02 push/PR/CI claim.
- Found and repaired a real admission gap: format-6 headers could omit startup
  settings and receive legacy defaults, with only some omissions rejected later
  by the developer runtime. Codec admission now requires explicit player config,
  projection, rear-view baseline, lcg_state, and XL level-start clock state when
  starting a new level. Every ordinary emitted D2 config field is required;
  boolean integers must be 0/1. Recorder startup uses the same codec validator
  before resetting/activating the capture. Existing format-5 stock behavior is
  preserved. Production XL guards remain closed.
- Extended existing codec/recorder tests with missing-field, noncanonical flag,
  wrong RNG and startup-baseline controls. File/text parse rejection preserves
  the prior parsed recording; invalid recorder settings leave it inactive.
  Windows D2 desktop and tests build, focused CTests pass 3/3 in 8.82 s;
  Linux developer desktop/tests build, CTests pass 3/3 in 2.92 s; Windows D1
  desktop/tests build, CTests pass 3/3 in 8.12 s. Zero skips in these runs.
  Scoped quality and diff checks pass. Automation catalog passes (99 JSON,
  315 PowerShell standalone, 547 support); run-all catalog passes (428 top-level,
  547 support). No test membership or timeouts changed. One Linux test invocation
  hit shell regex quoting; corrected with WSL --exec and ran to completion.
- Retained complete capture g1-live-time-capture-6 replays cleanly with verified
  assets on the new executable: g1-time-replay-8 accelerated and
  g1-time-replay-9 realtime. Both detailed comparisons pass all 198 object frames,
  202 world/boundary records, 811 SIM RNG events and 198 frame hashes. Together
  with immediate replay-7, this capture has two clean repeats and altered pacing.
  EXE SHA256 0f3267b03c1c32c463b8fa7d3fd8ded8919bf489a22dd8ab4be80b5f9c4df3ca.
  Demo identity is unchanged from the preceding entry. Authentic Warp HOG was
  rehashed unchanged (2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c).
  No new capture assists; existing pre-record approach pose remains disclosed.
  This does not prove connected gameplay, ordinary Omega, or cross-platform replay.
- Added a settings ownership table to regression_demos/README.md: captured
  difficulty/config/input scaling/view/clock/RNG versus still-open configuration
  changes during capture, strict-FP build admission, complete cheat/global cold
  state, and audio/effect simulation independence. Strict FP is ON in the tested
  Windows cache. Do not interpret this partial audit as settings closure.
- Evidence uses protected temp/pr464-ci-fixes-20261110/g1-startup-profile-*.
  Work interval approximately 12:55-13:03 PST; free space about 5.3 GiB with the
  4 GiB reserve unchanged. Pinned reference/assets remain unchanged. No Android
  worker or WSL process was interrupted.
- Next: commit this admission increment, then extend the existing ordinary
  gameplay probe to capture Omega firing, actual damage and subsequent recharge
  through ordinary input/recording. Keep setup assists explicit and compare live
  detail against immediate/repeated playback. G1 still needs the open settings,
  remaining external-clock/state audit and qualification before publication.
  Estimate remains 6-10 hours for G1 and roughly 17.5 hours G2-G7, with high
  uncertainty and no cutoff. No closure/package/whole-stack readiness claim.

### G1 Omega live-recording checkpoint, November 11 at 13:21 PST

- Previous increment committed as 64b09d9fc5de9b6edff208730edb709ef214232b,
  tree 83a4ecebb32bae72610571b8591279da47f85071. This turn made progress;
  layer02 remains local and G1 unaccepted. No new PR or pushed SHA.
- Extended the existing Omega damage owner with --recording, retaining its eight
  default rate/component controls. Native dispatch remains developer-only and
  requires the existing isolated output/user directory. After 66 ordinary warmup
  frames, the probe chooses an original uncloaked robot, assists only the approach
  pose and initial inventory, invalidates the old pose's gameplay-view cache,
  then records 90 held-fire plus 90 released-fire frames. It requires damage
  while Omega is still selected, consumed charge, recharge and zero capture-time
  assists. Python checks the receipt and the single format-6 checkpoint demo;
  it explicitly reports live_replay_qualified=false. README documents scope.
- Retained capture attempts 1/2 failed: first selected a cloaked robot; second
  fired during source one-second robot spawn protection (collide.c). A JSON bool
  macro mistake in the first failure receipt was corrected to bool{true/false}.
  Warmup now expires protection through actual simulation rather than injecting
  time. Capture3 passed live, but replay1 failed first-shot RNG: pre-record pose
  relocation retained the old gameplay-view list. Existing relocation owners
  reset that cache; applying the same reset resolves this probe defect.
- Capture4 passes on original Warp 5, robot117/signature118/id7. Shields fall
  16384000 -> 6844288 while Omega remains selected, then 3419776 after held fire;
  charge 65536 -> 17488 -> 42058. No robot kill or connected progression claim.
  Demo d2_wshock20_level5_20261111_131446.dximdemo SHA256
  16f8a232552ed07ced84abca75f6a2d69431f5130a57a055ea182313305c7366;
  EXE SHA256 5bc66695b999d696129f8c867d59ada04f175abecf9fdd7b4bf152d09e9e6c6b.
- Clean accelerated replay2 with verified assets and required clean exit passes
  terminal comparison, all 180 frame/diagnostic comparisons and 1059 SIM events.
  Detailed object/world comparison FAILS across 180 frames/184 records. Preserve
  g1-omega-replay-2-comparison.json, not a frozen regression recording. Differences:
  checkpoint FrameTime1092 vs0, SIM calls453 vs0, camera ticks12941 vs0; scrolling
  UV offsets lost at restore and persisting thereafter; FX state/count and robot
  next_misc_sound_time at frames98/138. AI2 consumers of that timer schedule
  chatter via FX RNG, but audio independence is not yet qualified. No comparison
  fields were removed. UVs are material: fvi.c find_hitpoint_uv computes them for
  check_trans_wall, which calls xl_side_pixel_transparency for XL collision.
  These require owner repairs/audits, not repeated playback
  of the same known mismatch. Cache fix does not establish checkpoint completeness.
- Windows verified D2 desktop build passes. Latest native source also builds in
  Linux /var/tmp/dxx-pr464-20261111/private/preview (2/2). Existing Python private
  owner plus new receipt negative controls: 46/46 pass, zero skips in 4.426 s.
  First Python invocation lacked CMake on PATH (10 subtest errors, one skip);
  sourcing the established MSVC/CMake environment fixed the invocation. No
  timeout or required membership changes. Scoped quality passes; automation
  catalog validates 99 JSON, 315 PowerShell and 547 support scripts. Run-all
  catalog passes including its scheduling/guard controls. Logs are under
  protected temp/pr464-ci-fixes-20261110.
- Work approximately 13:03-13:21 PST. Authentic Warp HOG rehashed unchanged:
  2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c.
  Reference remains 2576a98b5de310e3ac7c9777df2677d1627b8bae. Free space about
  4.3 GiB; preserve the 4 GiB reserve and use official cleanup before further
  large captures if needed. No Android worker or WSL process interrupted.
- Next: commit the coherent probe/evidence increment, then restore scrolling UV
  state through the existing XL save owner with atomic validation and real cold
  save tests. Audit camera controller angle/delta/t0 plus clock epoch restoration
  and detailed boundary FrameTime/SIM counters. These G2 prerequisites are needed
  for G1 live-vs-replay acceptance. Then qualify FX/audio independence and make a
  fresh Omega capture for immediate/repeated/altered-pacing comparisons. G1 still
  estimated 6-10 hours; G2-G7 roughly 17.5 hours, overlapping prerequisite work
  to be subtracted once accepted. No cutoff or gameplay-closure claim.

### G1 checkpoint UV repair, November 11 at 13:48 PST

- Previous goal turn made progress: ordinary Omega witness committed as
  64dd2731aef14092d6f09ba962a275ec0197f955, tree
  6f1c8b5715733702119245d4deac9af2d83049f8. This increment fixes the concrete
  save-state loss discovered by that witness. Layer02 remains local/unaccepted.
- Existing trigger_save.cpp advances development schema 37 to 38 and captures all
  48 signed UV values per segment, independently of current texture scrolling.
  Entire geometry-sized arrays validate before publication; L remains owned by
  the existing lighting state. Maximum trailer bytes add the worst-case encoded
  UV array to the previous 8 MiB allowance. No stock save-format change or legacy
  development-save migration. Actual consumers are game.c slide_textures and
  fvi.c find_hitpoint_uv/check_trans_wall -> xl_side_pixel_transparency.
- Existing changing-light native owner now earns both UV coordinates using actual
  texture scrolling, saves to disk, transitions through stock and restores every
  coordinate plus the collision UV interpolation. Existing CMake owner adds a
  fresh-process resume of the same save against the captured UV receipt. Both
  dispatch/invalid-row variants cover 93 segments; nineteen malformed light/UV
  payloads reject without partial UV publication. Boss malformed-schema control
  now uses 39. No test membership/timeouts or immutable fixtures changed.
- Windows developer desktop/metadata builds pass. Both changing-light owners
  pass (2/2,22.72 s), including cold resume; boss spawning passes (1/1,23.62 s).
  Rebuild emits the pre-existing changing_light_test_records.h:18 int/bool C4805
  warning; no new warning in the modified save owner. Linux desktop/metadata
  build passes (91 steps); its existing test-header write-strings warnings remain
  outside the edited code. Linux owners pass 3/3 in 18.74 s, including both fresh
  process UV restores. Zero skips. Both automation catalogs pass: 99 JSON,
  315 PowerShell, 428 top-level entries and 547 support scripts.
- Original Warp 5 capture5 reaches all 180 frames, complete gameplay proof,
  exported input/classic demos and terminal trace, but the 45s process timeout
  fails it (exit_code=null). It overlapped Linux compilation. Preserve this
  failed outcome; a quiet rerun after compilation tests the load hypothesis
  without increasing the timeout. Do not qualify/freeze this recording.
- Quiet capture6 passes with the unchanged 45 s limit, same original robot117,
  damage and recharge, and the same two pre-recording setup assists (zero during
  capture). Clean accelerated replay3 passes terminal comparison and required
  clean exit with verified assets. Detailed comparison confirms ZERO UV
  differences across 184 world/boundary records; all 180 frame comparisons and
  1059 SIM RNG events pass. Object/world scope still fails on the retained
  FrameTime1092/0, SIM-call453/0 and camera-tick14077/0 checkpoint differences,
  plus FX RNG and chatter timers (objects200/117, frames82/129). Nothing was
  filtered from comparison. G1 remains unaccepted, no corpus frozen.
  Demo d2_wshock20_level5_20261111_134420.dximdemo SHA256
  0a16b5970c8d7976ca9eae231e0d3c90c9577a24992137002086111844f440c2;
  tested Windows EXE SHA256
  fb540f070971ea7431f1f6832ea51e8009b0460f4307254f4838e7940298ed74.
  Detailed evidence: g1-omega-replay-3-comparison.json, native cold/controls logs
  and platform CTest logs under protected temp/pr464-ci-fixes-20261110.
- Scoped mixed-language quality and git diff checks pass. Authentic Warp HOG
  rehashed unchanged (2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c),
  as did original Neptune HOG (4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303).
  Reference remains 2576a98b5de310e3ac7c9777df2677d1627b8bae. Actual work
  approximately 13:23-13:48 PST, including cleanup and Linux rebuild waits.
- Space maintenance: official temporary cleanup with exclusions/emulator
  preservation found 0 eligible, so target 50 GiB was unmet. Existing replay
  compress_trace then losslessly compressed 25 completed RNG traces and 2 crash
  dumps, verifying decoded SHA256 before replacement (385998285 bytes saved).
  g1-uv-evidence-compression.json records original/encoded hashes and paths;
  older direct raw-RNG paths now have .gz siblings. A refined official preview
  and cleanup removed 181 byte-identical replay runtime copies (60347392 bytes),
  retaining at least one copy of all 28 distinct binary hashes and every result,
  save, trace and crash artifact. Manifest g1-uv-duplicate-binaries.json and
  cleanup-exclusions-uv.json preserve the exact choices. Target 50 GiB remained
  unmet then. Free space subsequently rose to about 48 GiB; the logged cleanup
  does not account for that larger recovery. The 4 GiB reserve is unchanged.
- Next: commit this validated UV owner repair, then continue camera controller
  rotation/timing and checkpoint clock-boundary restoration in their real save
  owners. Preserve cold epoch semantics, reject malformed state atomically and
  qualify actual resumed camera movement. Audit FrameTime/SIM diagnostic origins
  and audio/FX independence before changing semantic comparisons. G1
  estimate remains 6-10 h and G2-G7 roughly 17.5 h, with shared prerequisite work
  credited only once accepted. No Android interruption, external blocker or
  gameplay-closure claim.

### 2026-11-11 14:21 PST - G1 camera motion and cached-clock continuation

- G1 remains active and unaccepted. Layer02 remains local on
  xl_gameplay_20261111_02_timeline, parent e0da21fac888ed1fbc76e88ddcf56d0a59b34892
  (tree46e84f2b62d7ff02a5ebe16ba43d7c203a22dd91). This tested change is being
  committed as "Preserve XL camera motion and cached clocks across saves";
  resulting commit identity is reported in the handoff. No new PR, push or CI
  result is claimed for layer02. G0 PR465 acceptance remains the earlier entry.
- Save schema39 adds the logical cached-clock origin and every camera monitor's
  scan angle/direction, pinned-zero t0 and shared-owner brake time. Validation
  rejects malformed rows, owner/monitor mismatches and inconsistent shared-owner
  brake times before publication. Restore applies motion after authored payload
  and attachment restoration, which previously reset the scan. New-level v6
  metadata carries xl_camera_ticks, bounded by its cached environment origin.
- The existing environment owner adds a constant offset to the stock cached
  timer after disk restoration; wall-trigger cooldowns and camera frames use
  that domain. The offset survives level transitions. Accepted replay samples
  update the domain for subsequent ordinary play. The stock timer is unchanged.
  Hit-repair validation uses the saved frame clock, avoiding cold-process epoch
  errors. A real cold-process test initially found an extra 12 ms from the final
  reset_time during load. The ordinary state restore now anchors the saved clock
  after that reset, excluding load finalization from elapsed gameplay time.
- Existing projectile/camera owners now compare 5400 resumed native object
  frames across nine orientation/behavior profiles and reject 153 malformed
  motion snapshots atomically. Existing source receipts still cover 10800
  controller frames, plus the retained contact/rotation/motion/hitbox references.
  Trailer controls expanded from34 to44; new cached-clock/motion corruption must
  leave live clocks, hit histories, flash and projectile state unchanged.
  Manual native frame drivers now call timer_update before camera_begin_frame,
  matching the ordinary producer of the cached clock.
- The same existing CMake projectile owner runs a second process against its
  physics-earned disk save, comparing cached/frame clocks, all hit repair ages,
  flash and six smart-child states, then advances the next cached frame. This
  passes on Windows and Linux with the original addon and Vertigo present:
  camera subchecks explicitly executed, no optional-asset omission. Motion
  continuation uses the existing constructed native bindings/two monitors per
  owner; full cold continuation of an authored moving camera in an original
  mission still needs its own G2 witness. These are assisted mechanism controls,
  not connected campaign progress.
- Windows developer desktop/metadata builds pass. D1 and D2 fixture/recorder/
  replay CTests each pass3/3, zero skips. Windows changing-light, invalid-row and
  boss save owners pass3/3, including retained UV cold restores. Linux developer
  desktop/metadata builds and final incremental state rebuild pass; codec3/3
  and those save owners3/3 pass, zero skips. Both full native camera CMake runs
  pass their pinned source comparisons and cold process. Prior unchanged MSVC
  int/bool and GCC string-literal warnings remain; no new warning was found.
  Scoped quality, git diff check, automation catalog (99 JSON/315 PS/547 support)
  and runner catalog (428 top-level/547 support) pass.
- Windows EXE SHA256
  24a0e8c683575528cffb1c5e69becdb2c9a54f6b659d6219354f4a4b745be24a;
  metadata88c259a4242b197df18da45b7eccdcb619d62cab553810ca429fa9cd1d33cab7.
  Linux desktop e12bcc4114e53fa914c421e131e72fe0350651cb3f468e182a494096f1227c67;
  metadata32f0411bc85243a0574142aef9697097a18bc83b3e8480fca47a5b1b4a60c2a8.
  Root-bound caches remain the prior Windows build-d2-reference/build-d1/build-xl
  and Linux /var/tmp/dxx-pr464-20261111/private preview/contracts caches.
- Native assets rehashed unchanged: Neptune4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303,
  addon df69c62aad949f2d92bc1bbea4b597623cd29d0a0f594cdcc3d66568d6ce482a,
  Vertigo faa67189ccf5c7d6cdc8e37fa009fb21a1d57ee2c6c212ce091642e4366e3c0f.
  The initial test command chose the different e7351d66 addon variant and failed
  the unchanged hash gate; rerun used the preserved pinned original. Reference
  remains2576a98b5de310e3ac7c9777df2677d1627b8bae, especially cameras.cpp Rotate
  and the existing extracted source references. No authentic bytes changed.
- Fresh capture7 FAILED its unchanged45-second clean-exit deadline, despite all
  180 frames, complete proof and demo/terminal traces being written. No build
  overlapped this capture, so prior build contention does not explain this run.
  It is preserved as failed evidence, not retried unchanged or qualified.
  Original Warp5 robot117/signature118/id7: shields16384000 ->6844288 while
  Omega selected ->3419776; charge65536 ->17488 ->42058. Warmup66 normal frames,
  one pre-record pose assist and one inventory assist, zero during recording,
  no kill. Demo d2_wshock20_level5_20261111_141718.dximdemo SHA256
  49794e328bc56cdfe6955956cfe156263c1fa622c882439617278b7051e32956.
- Diagnostic replay4 from that failed capture has verified assets, strict
  terminal PASS and clean exit. Detailed comparison:180 frame checks PASS,
  1059 SIM events PASS; zero camera differences across184 world/boundary records
  (the previous two camera clock paths are fixed), UVs remain equal. Objects
  still fail3 paths: FX calls/state and robot chatter. World still fails11 paths:
  restored FrameTime1092/0 and SIM calls453/0, chatter timers for objects200/117
  at frames163/177 and terminal, and terminal FX state/count. No field was
  filtered. G1 and the recording remain unqualified.
- Evidence under protected temp/pr464-ci-fixes-20261110: g1-camera-* build,
  CTest, quality/catalog logs and Windows/Linux native continuation directories;
  g1-live-omega-capture-7, g1-omega-replay-4 and its full comparison JSON. WSL
  responded normally on entry; no restart was needed or performed. Android
  devices were not interrupted. Free space about47 GiB; no cleanup beyond the
  required retention previews (zero eligible) occurred in this item.
- Actual work approximately13:56-14:21 PST. Revised G1 estimate remains6-10 h;
  G2-G7 roughly17.5 h, crediting overlapping prerequisites only once accepted.
  Next: commit this bounded repair, then audit/preserve ordinary checkpoint
  FrameTime and SIM counter origins. d2/main/state.c resets SIM calls after
  restoring the seed; world trace retains FrameTime and object trace retains
  frame_time. Audit FX/chatter separately: ai2.c lines1553-1596 reads/writes
  next_misc_sound_time for sound gates, but complete simulation independence
  still requires all consumers/FX stream users and counterfactual tests.
  Also investigate capture deadline cost before attempting qualification; do
  not extend deadlines, hide state differences or bless the failed capture.

### 2026-11-11 14:43 PST - G1 checkpoint frame/RNG origins and capture timing

- Previous goal turn made progress: camera continuation committed as
  4d4237147700304bd3a702949fa30698dadfbc01, tree304940e47e027d3f41eb9ea2dc3ff64b67ed07bf.
  This turn continues local layer02, G1 active/unaccepted. No new PR/push or
  exact-tip CI claim. Intended commit: "Preserve XL frame time and SIM counter on
  restore"; resulting identity is reported in the handoff. G0 PR465 remains the
  accepted baseline; all G2-G7 closure requirements remain in scope.
- Ordinary XL save schema40 adds FrameTime and the SIM call counter. The stock
  runtime already saves the SIM seed but resets its counter during restore;
  FrameTime was omitted. XL now validates both fields before publishing any
  trailer state and restores them after the other owners. FrameTime must be a
  nonnegative signed32 value, the counter an unsigned32 value; missing, boolean,
  float, negative and overflow values reject. This preserves normal disk saves,
  with no replay-only branch and no changes to stock save layouts. The counter
  affects diagnostic/ordered-input validation, not the chosen random values;
  FrameTime is also the fallback in the native time calculation.
- Existing native projectile/camera corruption controls now cover56 cases,
  including12 new field corruptions, and assert no mutation of frame/count state.
  Expanded warm/cold receipts compare frame time, SIM seed/count and all prior
  camera fields. The first stronger test correctly failed because its fixture
  had reset FrameTime to zero after its physics controls. The fixture now uses
  reset_time/calc_frame_time to earn a real positive duration before saving;
  the failed run log is retained as g1-checkpoint-native-windows-fixture-failure.log.
- Windows and Linux developer desktop/metadata builds pass after correcting
  diagnostic compile mistakes (iostream include and RNG output-parameter API).
  Only the previously recorded MSVC int/bool and GCC string-literal warnings
  remain. Full original-addon/Vertigo native camera runs pass on both hosts,
  including all pinned references, 5400 resumed frames, 153 malformed-motion
  controls and cold saves. Both cold receipts match exactly: SIM seed159418783,
  count40808; earned FrameTime2622 Windows and2621 Linux, each restored exactly
  within its own process pair. This is native mechanics evidence, not original
  connected progression or an authored moving-camera cold witness.
- Changing-light, invalid-row and boss save CTests pass3/3 on each host, zero
  skips. Private Python checks46/46 pass. Automation catalog99 JSON/315 PS/547
  support and runner catalog428 top-level/547 support pass. Scoped mixed quality
  and git diff checks pass. No shared codec or D1 code changed in this item;
  prior D1/D2 codec evidence remains applicable to those unchanged files.
- Tested Windows EXE f469c5d8e75f913d85f05e97c99ba67e76e44e958fe7412a7d68f420616da5c9;
  metadataa8f44d490852e6f32e79e0c6ece6ed0ca5c56f5ef8650fbf39d91332db036f02.
  Linux desktop047bc82a4355af22a45c2e2ad49387a577667fa911bd6a1c3aaf7c8951887795;
  metadatac9f2ae7b0ae3d320bce4f543d7c5de70839ad9b0007b6588b1d01eb055e9c357.
  Same verified root-bound Windows/Linux caches as the preceding item.
- Existing Omega probe/runner now expose observational phase milliseconds and
  total monotonic process duration. No deadline, frame count, prerequisite,
  comparison field or gameplay path was relaxed. Fresh capture8 passes with
  exit0 in41.406 seconds against the unchanged45-second deadline, without build
  or native-test overlap. Probe phase endpoints: warmup/setup1901ms,
  recording-start2180ms, frames-finished27450ms, finalization28966ms. Recorded
  frames occupy25.270 seconds; finalization1.516 seconds; startup plus remaining
  shutdown outside the probe approximately12.440 seconds. This distinguishes
  the measured phases; it does not yet isolate rendering from trace work inside
  those180 frames. Trace writes currently gzflush each record. Timing margin is
  still only3.594 seconds, and earlier failed captures remain failed evidence.
- Original Warp5 target117/signature118/id7 again receives real Omega damage:
  shields16384000 ->6844288 while selected ->3419776, no kill; charge65536
  ->17488 ->42058. Warmup66 normal frames, one pose and one inventory assist
  before recording, zero during180 frames. Demo
  d2_wshock20_level5_20261111_143755.dximdemo SHA256
  5c258634bb75b3969e30ff4cdc41b97279df3091f7e8970191d72490463ab1c0.
  Warp HOG rehashed unchanged2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c.
  Native addon/Neptune/Vertigo gates and source references retain their recorded
  exact identities. Reference2576a98b5de310e3ac7c9777df2677d1627b8bae unchanged.
- Replay5 verifies assets, strict terminal state and clean exit. Detailed
  comparisons:180 frame checks and1059 SIM events PASS; restored boundary now
  fully matches, including FrameTime1092 and SIM count453. UV/camera differences
  remain zero. Object scope still fails3 paths (FX count/state and chatter),
  world fails8 paths (two chatter timers at frames122/149 and terminal,
  terminal FX count/state). No fields were filtered. Thus G1 and capture8 remain
  unqualified; no corpus was frozen. Evidence: protected g1-checkpoint-* logs
  and native directories, g1-live-omega-capture-8, g1-omega-replay-5 and full
  g1-omega-replay-5-comparison.json under temp/pr464-ci-fixes-20261110.
- Actual work approximately14:23-14:43 PST. Free space about46 GiB; retention
  previews found zero eligible and no other cleanup occurred. No WSL restart or
  Android interference. Revised G1 estimate remains6-10 h, G2-G7 roughly17.5 h;
  shared prerequisites count once accepted, not twice. No external blocker.
- Next: commit this validated repair, then trace FX RNG producers/consumers and
  robot chatter ownership through simulation versus presentation. Current raw
  mismatches cannot simply be filtered as cosmetic: prove independence with
  counterfactual runs and retain complete diagnostics. Separately measure the
  recorded-frame trace/render costs and improve margin without losing fields or
  raising deadlines. Keep production guards until full live/repeat/cross-platform
  and headless acceptance; continue the remaining original plan afterward.

### 2026-11-11 15:22 PST - G1 presentation RNG isolation and faithful Omega replay

- Previous increment is local commit 4bea763a5705d2108dc4fe528211d9d3a54a392b,
  tree 9ddc89f710591a06bf65fc2e823156ec40367481. This increment made progress on
  layer 02; G1 remains active/unaccepted. Intended commit: "Isolate XL presentation
  randomness from simulation effects". No layer 02 push, PR or current-head CI claim.
- The retained failed capture 8 consumed 233682 particle FX draws versus 126056 in
  replay 5. Other FX consumers had identical call counts: ambient 181, palette 180,
  chatter 4. Source audit found transient particle storage and lightning bolt/draw
  geometry outside the game object allocator. Lightning/flare illumination feeds
  preview.cpp's private dynamic vertex buffer and shading; no AI/collision reader
  of that buffer was found. Lightning linked-sound allocation remains a separate
  G3 dependency, not blanket proof that missing audio/rendering is safe.
- Shared RNG now has stream 2, D_RNG_PRESENTATION, for XL particles, lightning and
  XL flare illumination. It uses the existing LCG and remains independent even
  with NO_WATCOM_RAND; stock SIM/FX libc behavior and stock flare draws are retained.
  AI chatter, palette, ambient/seismic sound timing, endlevel object effects and
  other FX consumers stay on stream 1. No gameplay/world field or FX history was
  removed from native D2 comparisons. Transient presentation state is rebuilt,
  not saved. Complete draw/reseed diagnostics remain in the raw RNG trace and a
  separate presentation comparison; missing events/fields still reject.
- Existing RNG tests now cover interleaved presentation reseeds/draws, independent
  counters/restoration and all three trace streams, including libc builds. Native
  particle/lightning lifecycle and flare tests additionally require unchanged FX
  state/count, preserving SIM isolation and exact draw/lighting assertions. Camera,
  deferred bubble and GPU controls follow the actual presentation stream; no
  source oracle, test membership, deadline or authentic input was weakened.
- Windows D1/D2 and Linux developer desktop/metadata builds pass. RNG CTests pass
  3/3 on each of Windows D1, Windows D2 and Linux D2, zero skips. Comparator tests
  pass 74/74. Native effect-inspection and Neptune lighting/features pass 2/2 on each
  host: final Windows 36.80 s, Linux 16.08 s. Final Windows metadata rebuild includes
  the removal of an unnecessary test-route presentation seed; its two native
  controls were rerun afterward. Existing compiler warnings outside the edited
  owners remain; no new warning was introduced by this change.
- Linux GPU test first timed out at 60.07 s while compilation was active and software
  GL was not explicitly selected. Retained g1-presentation-rng-linux-gpu.log is a
  failure. A quiet run with LIBGL_ALWAYS_SOFTWARE=1 and GALLIUM_DRIVER=llvmpipe
  passes1/1 in 26.83 s against the unchanged 60 s limit. Because both load and driver
  selection changed, this does not isolate which condition caused the timeout.
  It is rendered EGL evidence, not true-headless gameplay. Quality/diff checks
  and both catalogs pass: 99 JSON/315 PS/547 support and 428 top-level/547 support.
- Fresh original Warp5 capture 9 passes cleanly in 28.344 s against 45 s. Phase endpoints:
  warmup/setup 1814 ms, recording-start 2092 ms, frames-finished 22608 ms, finalized 24017 ms.
  Recorded frames took 20.516 s and finalization 1.409 s. No other build/native job ran
  during capture. Earlier failed captures remain failed; timing improvement is an
  observation, not proof of a particular performance cause. The prior capture 8
  trace contains about 322 MB decoded JSON, mostly world records; stage-level trace
  profiling remains the next step if performance margin needs further work.
- Same original robot 117/signature 118/id 7: shields 16384000 ->6844288 while Omega
  selected ->3419776, no kill; charge 65536 ->17488 ->42058. Warmup 66 normal frames,
  one approach-pose and one inventory assist before recording, zero during 180
  captured frames. No connected progression claim. Demo
  d2_wshock20_level5_20261111_151308.dximdemo SHA256
  67ac36c3c9d598b012429ae8b18f9d012736bce41cfec0ded21f83a52fc62836.
- Immediate clean accelerated replay 6, independent accelerated replay 7 and real-time
  replay 8 (maxfps 60) all verify assets, strict terminal state and natural clean exit.
  EACH matches 180 object frames, 184 world/boundary records, 180 frame checks,
  1059 SIM events and364 FX events, with zero field differences. This includes
  the formerly divergent chatter timers. Presentation draws differ: live 199287;
  replays 173430/165743/182226. They remain complete diagnostic differences. Thus
  this assisted Windows Omega witness now satisfies live/immediate/two-repeat
  fidelity under altered pacing; it is not a frozen or cross-platform corpus.
- Windows D2 EXE 1879ba7d15ed9a51c9e9a86c93913b0a494f727d24ea4344d50f307ef43b3fd4;
  final metadata 611180190d934c65c5ba1f230e4d60f2ba9319fd9a8d27c7f030e48df8e7b515;
  D1 EXE 7bd5f1c365520893dbbcdbc00907c71c80ab83c47f6695ee914bcb90313d9ecb.
  Linux desktop 56fd402d5028663a1e40b05d9880b8f756afdd53f1e2af3e60ee55e67f9720cc;
  metadata 63ebc72a741fc5166795abd84fa59712a5f55ba98a66708c59265a31a63e84f4.
  Same verified root-bound caches. Warp HOG rehashed unchanged 2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c;
  pinned reference 2576a98b5de310e3ac7c9777df2677d1627b8bae and native input gates unchanged.
- Evidence under protected temp/pr464-ci-fixes-20261110: g1-presentation-rng-* logs,
  g1-fx-consumer-counts.json, capture 9, replays 6/7/8 and their full comparison JSON.
  Actual work approximately 14:47-15:22 PST, with documentation/commit following.
  WSL was stopped on entry and started successfully; no forced restart was needed.
  No Android interruption or cleanup beyond zero-eligible retention previews.
  Free space about 45.8 GiB; producer reserve 4 GiB unchanged. No external blocker.
- Next: commit this coherent fix, then refresh Warp6's ordinary time-equipment
  witness against schema 40/current runtime using xl_render_clock.py --time-equipment
  --mission wshock20 --level 6 --xl, new g1-live-time-capture-7 output, followed by
  immediate and repeated detailed playback. Prior Warp6 evidence used older save
  schemas and is not current-tip qualification. Continue the remaining settings/
  configuration-change, strict-FP, cheat/global and external-clock/state audit.
  Keep production guards closed until G1 acceptance; G2 complete owner/cold-state,
  G3 true-headless/audio independence, G4-G7 gameplay/platform/CI closure remain.
  Revised estimate: G1 roughly 5-9 h, G2-G7 roughly 17.5 h with overlapping prerequisites
  credited once accepted. No cutoff, scope reduction or full-gameplay claim.

### 2026-11-11 15:43 PST - G1 lossless world tracing and refreshed time witness

- Previous increment is a8ea3bf30068a4029ed34cfb399e556f7f53ac3f, tree
  cce5a014197711c9aa9e82413fff4da064af68be. Layer 02 remains active/unaccepted,
  with no push/PR or exact-head hosted CI claim. This is progress, not a blocked
  turn or full gameplay completion. Independent untracked review is untouched.
- Current-runtime time capture 7 failed its unchanged 45-second clean-exit bound.
  Diagnostic capture 8 also failed at 45.219 seconds, despite completing all seven
  events and its final save. Both remain failed evidence. Added observational
  SDL tick timings to the existing time probe, plus monotonic process duration in
  xl_render_clock.py; no frame count, pacing, assertion or deadline changed.
- Capture 8 entered the probe at 9811 ms, started recording at 10049, finished its
  198 frames at 31862 and finalized at 32259. Recorded frame totals: delay6771 ms,
  simulation/tracing14558, rendering256, flip101. The 248 unrecorded frames spent
  only74 ms in simulation, with delay7307/render634/flip115. Startup and trace
  production are material costs; rendering alone does not explain the timeout.
- input_demo_world_trace.cpp now moves changed JSON sections into the record for
  serialization, then returns them to the previous-state cache. This removes a
  deep clone of large AI/segment subtrees while preserving complete values, key
  order, delta/reset rules and per-record flushes. No diagnostic field is filtered.
  A fixed failed-capture-8 recording was replayed before (replay10) and after
  (replay11): both strict terminal/asset/clean-exit checks pass, and all724 decoded
  trace records are byte-identical, including198 object/frame records each and
  200 world records. World-byte SHA256
  bd5eb7a1e2e4529053b5f4640ab6c8ae60347da209b61878c589bf1b8630de19.
  Replay elapsed27.762 ->22.735 seconds is one measured pair, not a latency SLA.
- Fresh original Warp6 capture9 passes in43.469 seconds against45. Probe entry9439,
  recording start9678, initial boundary9743, frames finished29073, finalized29468,
  save entered38280/finished38300 ms. Recorded simulation/tracing12266 ms versus
  diagnostic capture8's14558; delay6640/render263/flip114. The deadline margin
  remains narrow; startup/shutdown costs are not fully resolved by this change.
- New demo d2_wshock20_level6_20261111_153632.dximdemo SHA256
  43f764f2821b2feee4c84a908343a311ea035e638954d087b92575665e4adba3.
  One pre-record approach-pose assist; no pose changes during198 captured frames.
  The ordinary original slow-motion pickup plus activation/reversal is recorded.
  Original bullet-time pickup/control and movement checks occur afterward, outside
  the captured portion. Five total pose assists, zero level-load assists; ordinary
  24-frame thrust distances170752 slow and432955 bullet. This is not connected
  progression or original-level completion. Production guards remain closed.
- Immediate accelerated replay12, independent accelerated replay13 and real-time
  replay14 all pass verified assets, strict terminal result and natural clean exit.
  EACH detailed comparison passes198 object frames,202 world/boundary records,
  198 frame checks,811 SIM events and201 FX events, with zero differences.
  This level emits zero presentation RNG events in both capture and these replays.
  No corpus was frozen. Evidence is g1-live-time-capture-{7,8,9},
  g1-time-replay-{10,11,12,13,14}, full replay12/13/14 comparison JSON and
  g1-trace-copy-comparison.json under protected temp/pr464-ci-fixes-20261110.
- Windows D1/D2 and Linux D2 desktop builds pass with the changed shared observer.
  D2 Windows EXE f9b0bec6f14399a3f0ac0e9df55d98e66ea19cd379575d60bf34d4745d48b50b;
  D1 99fa548f2bfc55534b3422f97feac62d86e1feaf90921360f99e5f647965525e;
  Linux D2 c59bb24f3d7190bf647ed8164a42188cb3cbf392ba6c76948db12670e8993d22.
  Root-bound RelWithDebInfo caches and strict-FP options remain unchanged. Metadata
  executables were not rebuilt for this observation-only change and retain their
  prior identities. Two render-clock runner/pin tests pass, zero skips. Scoped
  code quality and diff checks pass. Both catalogs pass:99 standalone JSON,
  315 PowerShell,547 support scripts;428 top-level integration entries.
- Authentic Warp HOG remains2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c;
  reference2576a98b5de310e3ac7c9777df2677d1627b8bae and all source pins unchanged.
  WSL responds normally; the user's restart authorization did not need to be used.
  No Android interruption or cleanup beyond zero-eligible retention calls.
  Free space about45 GiB; producer reserve4 GiB retained. No external blocker.
- Actual work approximately15:26-15:43 PST, with catalog/commit finalization
  following. Revised remaining G1 estimate5-9 h; G2-G7 roughly17.5 h, with shared
  prerequisites credited once accepted. No cutoff or reduced scope.
- Next: finish this coherent commit, then inventory/capture the ordinary player
  configuration-change policy and reject unsupported floating-point build modes
  explicitly. Existing startup player_cfg capture alone cannot certify changes
  during recording. Reuse shared dispatch/start/newdemo/recorder owners; preserve
  stock format5 and existing ordered commands. Continue the remaining clock,
  cheat/global, full-state, headless and original gameplay queue afterward.

### 2026-11-11 16:23 PST - G1 ordered player configuration

- Parent870309152e91975ccd35724ef02a84f00eaf4052, tree2729baf5dbbaa010841272896c0c362a89194502;
  layer02 remains local/unpublished and G1 active. The previous turn made progress
  by committing lossless tracing and a faithful time-equipment witness. This
  increment closes the missing configuration-change timeline without forbidding
  ordinary preference changes. No production guard was relaxed.
- Reused startup capture/application owners and the existing ordered dispatch.
  Format6 now carries complete typed player_cfg operations, with the same fields
  as startup metadata. Settings are observed before input consumers and simulation,
  deduplicated, and included in the no-SIM stop tail. Header settings stay immutable;
  playback retains its current profile when pilot reload reapplies preferences.
  A preference first discovered changing inside simulation cancels capture instead
  of publishing a late operation. Stock format5 behavior is retained; it rejects
  the new operation. No second dispatcher or replay-only world repair was added.
- Guide-Bot routing required a real owner hook: guidebot_routing_set_mode resets
  live AI routing, whereas restore_mode intentionally preserves saved AI. The
  ordinary setter records its resolved mode before its routing reset. Playback
  calls that setter after applying the other preferences. Two changes between
  frames therefore remain ordered even if their final mode equals the original.
- Moved the shared player_cfg value type into the dispatch header's explicit
  pack(4) ABI. Compile-time checks require64-byte settings and188-byte operations,
  including existing enclosing-pack1/pack16 controls. JSON remains the wire format.
  Full settings/key/range/order validation precedes encoding and file execution,
  including before array traversal. Missing/unknown fields, noninteger or invalid
  boolean values, duplicate/invalid orders and use in D1/format5 reject.
- XL world diagnostic schema11 adds required full xl_player_cfg observations at
  restored, per-frame and terminal boundaries; stock world schema10 is unchanged.
  The comparator requires and compares every settings field, preserving unknown
  fields. Older XL schema10 receipts retain historical scope and do not qualify
  this larger observation. No prior immutable stock fixture was changed.
- Expanded the existing Omega recording probe, preserving its66-frame warmup,
  180-frame combat/recharge workload and45-second limit. It changes auto-leveling,
  original homing and primary weapon order, calls the ordinary routing setter
  twice at frame40, restores preferences at135, and changes routing in the stop
  tail. Runner checks all four operation sites in the actual demo. These are
  scripted menu-equivalent preferences/setter actions, not UI-menu interaction.
- Fresh Warp5 capture10 passes39.250 s. Probe phase endpoints2427 ms at recording
  start,23054 frames finished,24784 finalized. The original robot117/signature118/
  id7 goes16384000 ->6844288 while Omega selected ->3419776 after firing, no kill.
  Charge65536 ->17488 ->42058. Same one pre-record pose and one inventory assist,
  zero during recording. No connected progression or level-completion claim.
  Demo d2_wshock20_level5_20261111_155624.dximdemo SHA256
  65d7946bb4e5562fcb44b999c2a6f478be40b8d7836589da1e8f694c039778e0.
- Immediate accelerated replay9, clean accelerated replay10 and real-time replay11
  each pass asset verification, strict terminal result and natural clean exit.
  EACH full comparison passes180 object frames,184 world/boundary records,
  180 frame checks,1059 SIM and364 FX events. This includes current preferences
  and AI routing changes. Presentation RNG differs and remains a complete separate
  diagnostic failure; no SIM/FX difference is excluded. No corpus was frozen.
- Windows D1/D2 and Linux D2 desktop builds pass. Fixture/recorder/replay CTests
  pass3/3 on each, zero skips. Recorder cases cover same-batch order, deduplication,
  unchanged startup header, tail changes, current replay profile, invalid complete
  snapshots, oversized counts, wrong format and sticky in-simulation rejection.
  Comparator tests75/75 and Omega receipt test1/1 pass. First Windows build failed
  because the new test supplied an extra argument to input_demo_replay_load; fixed
  the test call and reran affected targets/tests. No new compiler warning remains.
- D2 Windows EXE2a168e2a74f7471589bb9b800aa4163046f375bf9b6e144978dcfa4dea42d3c5;
  D1a0a71b367077d3e07600532e476f66234aa03869aae8ffeddda892c38ca213f2;
  LinuxD217d7e80dca2a1c3a35ec558dbc75c6754501b8819c14d6023942f9ad13aeccc3.
  Same root-bound caches and strict-FP flags. These are desktop builds, not refreshed
  metadata/headless executables or release packages. Source is parent plus this
  working patch; cached build Git labels are provenance limits, not admission keys.
- Scoped mixed-language quality passes; both catalogs pass99 standalone JSON,
  315 PowerShell,547 support and428 top-level integration entries. Full unchanged
  public graphics suite exits0: all34/34 native runs pass, with pinned assets,
  unchanged live oracles, strict SIM/diagnostic/terminal checks and natural exit.
  Existing imported-D1 flight gameplay-state/SIM-RNG scope passes. The four other
  imported probes and full diagnostic/object/world qualification remain incomplete;
  raw paired exit1/differences and three declared imported exclusions are retained.
  No membership or deadline changed. Report:
  temp/pr464-ci-fixes-20261110/g1-settings-public-graphics/report.json.
  Evidence logs use g1-settings-*;
  original capture/replays and full comparison JSON are protected in that parent.
- Actual start15:44, validation ended16:23 PST. Warp HOG fingerprint remains
  2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c;
  reference2576a98b5de310e3ac7c9777df2677d1627b8bae verified by the original owner.
  No WSL restart, Android operation or cleanup beyond zero-eligible retention.
  No external blocker. Independent untracked review remains untouched.
- Next: commit this coherent change, then enforce the supported floating-point build profile at format6
  admission using existing FP/build owners. Continue remaining external clocks,
  cheat/global and complete state audit; G2-G7 and final platform/CI/stack gates
  remain open. Estimated G1 remainder4-8 h; G2-G7 roughly17.5 h, shared prerequisites
  credited once accepted. No cutoff, no reduced scope and no full-gameplay claim.

### 2026-11-11 16:47 PST - G1 floating-point admission

- Ordered-settings increment committed as661b6ace2bf444956e51a4dc4809d5c7b89e3521,
  treef978e65f35b1a57d0e0c74f69359fac69a59e031. Layer02 remains local/unaccepted;
  no pushed SHA, PR or current-tip hosted CI claim. G0/PR465 remains the published
  baseline. Independent untracked CI review is untouched.
- Existing input_demo_fp_env and build-metadata owners now declare/check the
  supported strict C/C++ profile. Recorder startup and replay load reject an
  unsupported translation-unit profile before publishing a new session. Native
  capture checks before checkpoint creation; playback checks before and after
  world restore. Live input/frame boundaries and replay steps reject changed FP
  controls. Format6 stops using the Windows replay-only reset; stock5 retains it.
  No replay-only state correction or new timeline field was added.
- Runtime checks require nearest rounding, masked traps and gradual underflow;
  MSVC x86 also requires53-bit x87 precision. GCC/Clang SSE paths check x87 trap
  masks separately. ARM checks FPCR/FPSCR control bits, leaving accrued status
  alone. Unknown build/hardware profiles reject. This is admission, not proof
  of cross-ABI numerical equivalence. Compiler basis: existing /fp:strict and
  -ffp-contract=off/-fno-fast-math; official documentation reviewed at
  https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior
  and https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html. ARM register fields
  checked against Arm's FPSCR/FPCR manuals. Production gameplay guards stay closed.
- Windows D1 and D2 desktop builds pass. Each selected fixture/recorder/replay/FP
  suite passes6/6, zero skips. Added x87 precision controls then reran each FP
  group3/3. Linux D2 build and selected tests pass6/6; after adding the independent
  x87-mask check, affected tests pass5/5. Default/unqualified/actual-fast-math C
  and C++ builds, three non-nearest modes, denormal controls, masked exceptions,
  accrued inexact status and no-mutation rejection are covered. Recorder/replay
  negative tests verify refusal without replacing the active session. Existing
  weapon return and packed-member warnings recur on the broader rebuild; no new
  warning was reported for changed production/test code.
- Windows evidence EXE9b972d0c8f1484978f87eea8be12a27b0d6481ec091187c01b41c2ea480b6e7b;
  D189a2d1ef49919c5cbe0ff108c8f717d7e8d184781bcdf8ca85662fbb2fe63a8d;
  Linux after its x87 check8688aa9ae5f2a3d9c742d2f01e1f361f7102e6495c56f51eaa0ff5b7da21f622.
  Root-bound caches, Windows x86 MSVC14.44 and Linux x86-64 GCC. Windows binaries
  precede only the subsequent non-MSVC x87 branch/test addition, which is excluded
  from their compiled runtime. Metadata executables and packages not refreshed.
- NDKr30 actual/pinned30.0.16248370/API24 C and C++ compile checks pass for ARM32,
  ARM64, x86 and x86-64; strict and fast-math preprocessor admission checks pass
  in each language. Refreshed all four after the final non-MSVC x87 assertion;
  g1-fp-android-compile/result.json records final source hashes and each result.
  These are compile-only probes, not an APK/device pass.
- Fresh Warp5 Omega capture11 passes in23.953 seconds against unchanged45. Demo
  d2_wshock20_level5_20261111_163413.dximdemo SHA256
  f27c146bfb14afb34b025ff0bc646fd2d782262099cec4cee4f6e5d29b8e7ca2.
  Same180 frames/66 warmup, original robot117/signature118, one pre-pose and one
  pre-inventory assist, zero in-record assists. Damage/recharge and all four
  configuration sites pass. Process timing is one measurement, not a new SLA.
  Immediate/repeated accelerated and realtime playbacks12/13/14 all pass strict
  terminal/verified assets/natural exit. EACH detailed comparison passes180 objects,
  184 world/boundary records,180 frame checks,1059 SIM and364 FX events, zero
  differences. Presentation RNG remains diagnostic and raw: each comparison
  reports174025 events compared and differences, without excluding SIM/FX fields.
- Fresh Warp6 time capture10 passes in27.781 seconds against unchanged45. Demo
  d2_wshock20_level6_20261111_164118.dximdemo SHA256
  dae7e520880f654116f9d7ca24aeb8125f8d69363678e47f307cc2b2eb35e0d1.
  Same198 captured frames, original slow pickup/activation/reversal, one pre-record
  approach pose and five total pose assists across the entire probe; no in-record
  pose changes or level-load assists. Bullet pickup/control/thrust remains outside
  the recording. Probe enter3402 ms, record start3608, frames finished19419,
  finalized19698, ordinary save27540-27556. The earlier45-second failures remain
  failed evidence; this faster run does not establish a general timing guarantee.
  Immediate/repeated accelerated and realtime playbacks15/16/17 all verify assets,
  strict terminal and natural exit. EACH matches198 object/frame records,
  202 world/boundary records,811 SIM and201 FX events, zero differences, including
  schema11 settings observation. Presentation events0 on both sides. These are
  Windows bounded mechanics witnesses, not connected progression or a frozen corpus.
- Fresh immutable stock D1/D2 flight controls each pass360 live-oracle frames,
  terminal/diagnostic fidelity and ordinary action checks; SIM counts96/778.
  These two controls supplement, not replace, the full34-run public pass on the
  preceding settings increment. Both catalogs pass99 JSON/315 PowerShell/547
  support and428 integration entries. Scoped mixed-language quality passes.
- Evidence uses g1-fp-* logs/probes and capture11/replays12-14 under protected
  temp/pr464-ci-fixes-20261110; time capture10/replays15-17 and g1-fp-stock-d1/d2
  retain full new receipts. Actual work approximately16:25-16:47 PST. No WSL restart, Android device operation, timeout
  or membership reduction. Retention previews had zero eligible artifacts;
  free bytes46014091264 at16:38, unchanged4 GiB producer reserve.
- Next: commit this coherent admission change. Continue remaining clock/global
  audit and explicitly close the time-equipment combat witness gap before G1
  acceptance; the existing time capture proves pickup/controls/movement, not
  deliberate recorded time-equipment combat. Review existing render_clock_probe
  and original-level inputs before extending that owner, without extra in-record
  pose/inventory assists or relaxed deadlines. Remaining external clocks include
  message/camera/effect/model consumers;
  message rendering mutates saved message age and remains an explicit G3 owner
  review, not presumed simulation-independent. No external blocker. G1 remainder
  estimated3-6 h after this increment; G2-G7 roughly17.5 h with shared prerequisites
  credited once accepted. No cutoff or reduced gameplay/validation scope.

### 2026-11-11 17:20 PST - G1 recorded slow-motion combat

- G1 remains active and unaccepted; G0 remains accepted, G2-G7 remain open.
  This coherent increment closes the missing deliberate combat inside the time
  pickup recording. Production format-6/Omega/time guards remain closed.
  Branch xl_gameplay_20261111_02_timeline; parent commit
  0997c253c889c75b5cb773877456fe6f45a4d2f8, tree
  1657d2dbd3b054ede2ae8158a8fa74caf0b5e957. No layer-02 push or PR yet.
- Existing render_clock_probe.h, xl_render_clock.py and test_xl_private_assets.py
  own the change; README documents the stronger witness. The probe advances 66
  ordinary warmup frames, searches geometry-clear approaches outside original
  robots, records the actual original pickup, steers through normal Controls,
  then holds the starting laser for 84 slowed frames and reverses time for 84.
  Original robot signatures, player projectile attribution and both actual slow
  factors are required. The existing robot_damage trace supplies collision
  evidence; the next pre-frame world row supplies the completed frame's factors.
  No damage injection, changed mission data, new framework or reduced deadline.
- Failed attempts are retained under temp/pr464-ci-fixes-20261110. Captures11/12
  rejected every original approach before recording: only one was wall-clear,
  and that pose overlapped original robot189. Expanded generic approach search
  found a clear diagonal. Capture13 recorded real slowed damage, then failed the
  later unrecorded bullet pickup. That approach also overlapped a robot; the same
  occupancy check now applies to both pickups, and outside-recording poses reset
  residual angular velocity as well as linear velocity. Capture14 native passed
  in31.828 seconds, but the new Python verifier failed because it compared an
  in-frame collision with that frame's pre-simulation state. The correction uses
  the next boundary; the failed receipt is not rewritten or claimed as passing.
- Fresh capture15 passes native and independent collision/world verification in
  32.703 seconds against unchanged45. Demo
  d2_wshock20_level6_20261111_171519.dximdemo SHA256
  0d58407d888253359724313cc042314e84a88e9bf3ba3232206826dedeb776b8.
  It records231 frames including47 ordinary aiming frames. Original robot189,
  signature190, id27 falls from2949120 to2359296 shields during firing. First
  attributed player-laser damage is frame105, world/player factors both
  2.049999237060547. Both subsequent factors and exact projectile parent/signature
  are read from native trace records, not inferred from a terminal score.
  Timing: entry3405ms, recording entry5374/start5561, initial boundary5621,
  frames finished24615/finalized24906, ordinary save32416-32433.
- One pose assist precedes capture; five total outside-recording pose assists
  include later bullet/movement checks. Zero inventory, in-record pose or
  level-load assists. Original Warp HOG remains
  2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c;
  existing render/automap/Omega/loadgame reference pins and revision
  2576a98b5de310e3ac7c9777df2677d1627b8bae are verified by the runner.
  This is assisted mechanics/combat evidence, not a connected route, completed
  level, cold continuation, frozen corpus or cross-platform gameplay claim.
- Windows desktop and Linux desktop builds pass. Windows EXE SHA256
  252f2ae32c4a053a257f9bfad6768e703ed522d27eb5360b45478296ed6e35bd;
  Linux /var/tmp/dxx-pr464-20261111/private/preview/main/d2x-redux SHA256
  1c6cada3e364def02e7ec715895f4af3d686993819a1e3c97b8523a19b9587bb.
  Root-bound cache/source associations are unchanged. Linux retains known packed
  member warnings outside this probe; no new probe warning. No new metadata,
  Android package or device proof is claimed.
- Focused Python tests pass3/3, including wrong parent/signature/frame, ordinary
  clock factors, missing fields and invalid type/aiming bounds. Scoped mixed
  quality and diff checks pass. Automation catalog passes99 JSON/315 PowerShell/
  547 support; master integration passes428 entries. Full public34-run evidence
  remains the preceding settings increment; this probe-only increment does not
  claim a fresh complete public suite.
- Immediate accelerated replay18 and repeated accelerated/realtime replays19/20
  verify authentic assets, strict terminal expectations and natural exit. Detailed
  comparison18/19/20 each pass231 object/frame records,235 world/boundary records,
  1193 SIM and236 FX events, zero differences; presentation events0 on both sides.
  Full receipts remain in g1-time-replay-*; all three comparisons finished before
  committing this increment.
- WSL responds normally; the user's restart permission was not needed. No device
  operations, timeout/membership changes or cleanup deletions. Free bytes
  53167173632 at17:14 PST. Actual work approximately17:03-17:21 PST.
- Next: commit this coherent witness, then
  continue the external-clock/input audit. Concrete remaining clock owner:
  level_messages.cpp samples steady_clock in xl_message_frame and again during
  draw; its age() mutates saved message identity. Pinned main/textdata.cpp samples
  cached nSDLTicks[0], and negative duration pauses through a modal. This requires
  a deliberate timeline/presentation boundary decision and tests, not an assumed
  cosmetic exclusion. Camera/effect/model consumers and remaining global-state
  admission remain open. G1 remainder estimated3-6h; G2-G7 roughly17.5h with shared
  prerequisites credited once accepted. No cutoff or reduced scope.

### 2026-11-11 17:40 PST - G1 level-message clock owner

- G1 remains active/unaccepted; G0 accepted, G2-G7 open. Branch
  xl_gameplay_20261111_02_timeline now has the preceding combat witness committed
  as b625aa268accfbdac718f8d6b1f5f3c5128c86ef, tree
  76594f6e57134aaf3661dc3674e04c3dbe0159fa. The commit hook printed another hash;
  these identities were reread from HEAD. No layer-02 publication or guard removal.
- level_messages.cpp used steady_clock for source cached SDL time. It now reads
  the existing qualified cached clock, which returns recorded cached_ms while
  replay is loaded. No new time format, wall-clock repair or extra sampling.
  A second cached message clock was also removed: save/trace readers derive
  effective age/expiry directly, without requiring a draw/update or mutating the
  message owner. Normal frame/action/draw paths still retire expired messages.
  Ordinary restore rebases elapsed age on the saved cached domain already restored
  by trigger_save.cpp; the serialized message layout and strict validation remain.
- Exact source anchor: pinned main/textdata.cpp ShowGameMessage consumes
  gameStates.app.nSDLTicks[0] for start/end/fade/expiry; SHA256
  d6172ffd9c3e94162358228ce168dd005d03feba231f45165b97f9b4ff1c0c5c,
  reference revision2576a98b5de310e3ac7c9777df2677d1627b8bae. Its negative-duration
  modal pause/resume branch remains unchanged. Modal input/clock ordering and
  no-device operation are not newly qualified; they remain explicit G1/G3 audit
  work, along with optional message output policy.
- Existing level_message_test.h now checks that a host delay without timer_update
  cannot advance the cached message clock. For each derived version23/27 fixture,
  it records two ordinary frames, reloads that actual format-6 stream, establishes
  a labeled near-expiry message control, waits/updates the host timer and verifies
  its age remains950ms. Consuming the original recorded samples then advances and
  expires the message, observed directly without drawing. This is a clock-consumer
  control, not a full world replay, original route or new frozen corpus.
  The fixtures still derive from unchanged audited Neptune geometry
  4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303.
- Retained failed attempts: first Windows native run failed a pre-existing Boolean
  negative because loadgl.h's true macro serialized integer1, not a Boolean. The
  test now uses bool{true}, preserving the intended malformed-type rejection.
  A later build failed linking the newly used C++ input_demo_file_read API because
  lifecycle_test.cpp first included its header through packed extern-C engine
  headers. The fixture codec is now explicitly included before those headers,
  preserving C++ linkage and native struct layout. Finally the stronger direct
  save/trace observation control failed on the duplicate frame_clock before its
  removal (g1-message-clock-boundary-before.log). All failures remain evidence.
- Final Windows desktop/metadata and Linux desktop/metadata builds pass. Native
  message and pinned text reference tests pass2/2 on each host, zero skips:
  Windows11.99s; Linux5.59s. Each derived version retains14 malformed-trailer
  atomic rejections, wall/master/object/autoplay dispatch, ownership rejection,
  duration/expiry and disk-through-stock restore. Extracted reference controls
  cover52 expiry/fade cases, three modal cases,2061 tables and6159 exact row/lookup
  comparisons. Raw logs are g1-message-clock-* under the protected evidence root.
  The existing Windows C4805 in changing_light_test_records.h and Linux packed
  member warnings remain outside this change; no new edited-owner warning.
- Windows desktop SHA256
  6667a4cde2e7e6547d78bd00fe050ebd62c39a06861fe855c52beeb2a12d8af8;
  metadata SHA256
  1fee2869ba4400e13cadd8f931ff25ce860cde7be1c7ef397206ef2057775cea.
  Linux desktop SHA256
  aa91e4f8be8fafbb1d6fa847e586bd8cbe7a07534f5a963d051ac89dbfc2a44f;
  metadata SHA256
  cbe37ebc18a22e323924692dc4f3aac2f04aa8dd89acd84b525840f429b4a703.
  Existing root-bound cache paths are unchanged. No Android operation/package,
  production whole-level message replay or final-source hosted CI is claimed.
- A current-engine replay21 of the unchanged accepted time-combat capture15 is
  running as the scoped original regression check; finish its terminal and full
  semantic comparison before committing. Source quality and final doc/diff checks
  must pass. Catalog inputs are unchanged from the preceding passed99/315/547 and
  428-entry checks; no test registration or deadline is changed.
- Actual work approximately17:22-17:42 PST, sequential in the root checkout.
  No WSL restart, independent worker, device operation or asset mutation. Free
  bytes52380295168 at17:41. Next: commit this coherent increment, then continue the
  G1 clock/input/global audit, including modal/nested message clock ordering.
  Remaining renderer owners use SDL ticks for camera scheduling, OOF spin and
  particle/lightning output; their full simulation independence is still G3 work.
  Main-engine timer audit confirms network spawn/drop reseeds are multiplayer-only,
  Guide-Bot asynchronous cache polling already returns during recording/replay,
  and Omega debug timer only throttles diagnostics. Muzzle-light/shield-warning
  output and remaining persistent globals still require their owning G2/G3 audit.
  G1 remainder estimated3-5h, G2-G7 roughly17.5h with shared prerequisites credited
  once accepted. No cutoff or reduced gameplay/platform scope.

### 2026-11-11 17:56 PST - G1 paused menu frame duration

- G1 remains active/unaccepted; G0 accepted, G2-G7 open. Sequential root checkout
  on xl_gameplay_20261111_02_timeline, parent
  6e77e6a2ea4908f8a8bd46c1c4ddc3ead62fcce3, tree
  d6da16981436b17a212e1cf2975ca8f98b845214. No layer-02 push, PR or guard removal.
  The preceding message-clock replay21 finished successfully before that commit:
  strict assets, terminal state and natural exit passed; all231 object/frame,
  235 world,1193 SIM and236 FX comparisons had zero differences, presentation0.
- Audit corrected an initial suspicion: input_demo_step_replay_frame_shared
  already rejects stepping while game_is_time_paused, before preparing/reapplying
  an input batch. The actual live defect was calc_frame_time running during
  background menu draws with an ordinary nonzero duration. The pinned source's
  menus/menu.cpp RenderMenuGameFrame calls CalcFrameTime while paused; its
  main/gametime.cpp paused branch sets frame and real-frame time to zero and
  resets the host timer origin. Source revision remains2576a98; SHA256 values
  7d622b4dbcbe6bf0d725c89e6df5265d64227b14a439cac93d98e303fd2244e1 and
  758b4a92b3800c0407c659f2829792e64cdabba7a818b27a64e6e267446d269b respectively.
  Both exact CRLF/LF forms are now pinned by the existing render-clock owner.
- d2/main/game.c now gives paused XL draws FrameTime0 and resets their timer
  origin. Stock D2 and D1 behavior is unchanged; no replay-specific world repair.
  Existing render_clock_probe.h opens a real newmenu window and processes three
  full event loops while paused. It checks FrameTime, GameTime64, SIM RNG count
  and actual render-counter contributions before continuing the existing
  automap/endlevel/save/restore sequence. xl_render_clock.py and its existing
  negative controls require these receipts; no new runner, test membership or
  timeout. README explicitly preserves the unfinished modal continuation gate.
- Before-fix Windows Warp6 run failed with FrameTime1442, despite unchanged game
  time and SIM RNG. After-fix Windows Warp6 passed all11 clock events in6.625s,
  with FrameTime0 and three real render increments. Windows retail D2 level1
  stock control passed all11 in3.844s, retaining nonzero paused duration1507.
  An initial stock invocation used mission d2 against shareware data and failed
  mission lookup before any probe event; retained, corrected with retail data.
- Linux desktop/metadata built without new warnings. Linux Warp6 reached all
  nine events through main_again, including the new paused-menu assertions, then
  hit the existing45s deadline during save/restore. This is a retained failure,
  not Linux original-level acceptance or an excuse to increase the timeout.
  The same unchanged11-event probe passed on the existing derived xmessage
  fixture in13.999s and on stock retail D2 level1 in4.376s. Linux uses Xvfb and
  llvmpipe, explicitly rendered rather than headless. Derived message geometry
  remains the audited Neptune input4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303;
  original Warp HOG remains2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c.
- Native message plus pinned text-reference tests passed2/2 with zero skips on
  Windows13.27s and Linux5.47s. Two Python render-clock acceptance/pin tests
  passed. Raw receipts and failed attempts are g1-paused-menu-* under
  temp/pr464-ci-fixes-20261110. The native probe is deliberately assisted
  (Omega inventory/projectile, controlled clocks, views and endlevel state);
  it proves no connected route, completion, cold-process resume or new demo.
- Current Windows desktop SHA256
  a06e661ac79c19a1f2219cc2443dd9a3b545e89d40615ec5d30306c26fae52a0;
  metadata c490ef7b35beb0eb8b7df4398082496168a44120f816dd1972e2ea4b881f63ce.
  Linux desktop64710604ac20f9caea2c7f00b9780fd339c004c0dbcb862871a6c371598f0f10;
  metadata2bb849427f081b3c195cba4c49d6835c1f0fe69f5b8c8ca0956da8f29ff22b88.
  Root-bound caches unchanged. Final mixed-language quality and diff checks passed.
  Catalogs passed99 JSON/315 PowerShell/547 support scripts and428 master entries.
  Free space52121845760 bytes at17:55 PST. No Android, WSL restart,
  asset edits, external publication or new hosted-CI claim.
- Next coherent item remains G1 modal trigger continuation. Pinned PauseGame and
  ResumeGame flush controls; current newmenu activation also flushes them. Modal
  source return has no choice-dependent gameplay result. Audit and prove the
  cached-clock/render-counter/FrameTime boundary when a modal interrupts SIM,
  including replay acknowledgement and subsequent actions in that same frame.
  Do not infer output independence from the menu test or silently bypass the UI.
  Reuse the message fixture, ordered dispatcher and existing rendered owner;
  production format6 guards remain until full live/replay acceptance.
  Work17:44-17:56 PST; G1 remaining3-5h and G2-G7 roughly17.5h, with shared
  prerequisites credited once. The Linux Warp save/restore deadline remains an
  explicit performance gate. No cutoff or reduction of required scope.

### November 11, 18:33 PST: modal continuation qualified on rendered hosts

- G1 remains active. This coherent increment extends the existing message,
  clock, codec and rendered-probe owners; no new runner or production guard
  relaxation. Branch xl_gameplay_20261111_02_timeline, parent
  04b1ee17f4df7503c05ba30eb8174bd7a4d0ae1c, parent tree
  ae5788e443b9cf86c84d00188524122dbaa6e320. No layer02 PR/push yet.
- Pinned source2576a98b5de310e3ac7c9777df2677d1627b8bae shows that paused
  CalcFrameTime returns before GetSlowTicks. The modal holds that cached tick
  through the interrupted frame, releases at the next input/frame boundary,
  and clears on restore/reset. Its single acknowledgement has no action-result
  branch. Live/replay use the real pause/window/input-clear lifecycle, zero
  paused FrameTime, and an ordered message_resume sample after closing. Replay
  automatically acknowledges after displaying; elapsed real clocks/render count
  come from the recorded timeline. True-headless output remains G3 work.
- The existing reference adapter now executes extracted CalcFrameTime during
  the three negative-duration message cases, then checks the chained timed
  message's cached start/expiry. Existing fade/table controls remain. Codec
  validation requires message IDs1..32767 and preserves phase/order bounds.
- Existing derived Neptune fixture now supplies modal23/modal27: an autoplay
  master invokes a modal and then a timed child in the same SIM frame. Source
  geometry hash4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303
  unchanged. Ordinary quick recording, fixed flight input and real SDL Return
  acknowledgement produce60 frames, cleared heading/zero resumed duration and
  timed-child expiry. No in-record pose, inventory, trigger or clock-state writes.
  Derived setup and input pacing are explicit; this proves no original route,
  combat, completion or true-headless behavior.
- Windows captures7/8 passed both23/27 layouts. Capture7 demo SHA256
  04d3a9d89faf8c46253017e8c46e4128d73e6c4cc6cd554f9a64719cf7d0adb1;
  capture8 8243a19527ca80e3591ff025153094218e9a533dc734def94ccf2beda370331a
  (19.578s). Linux live capture9 passed11.992s, demo
  52226838c75fe28e8832a57e35543f724c50156d061b5dde26f58868778ddf5e.
  Windows replays3/4/5 of capture7 (accelerated/accelerated/realtime), replay6
  of capture8, Linux replay7 of Windows capture7 and replay8 of Linux capture9
  all passed strict asset/terminal/natural-exit checks. Each detailed comparison
  passed60 objects/frames,64 world records,60 FX draws,zero SIM/presentation
  draws with zero differences. Zero SIM draws reflect this robot-free fixture;
  they are not combat coverage. Linux uses rendered Xvfb/llvmpipe.
- Evidence is g1-modal-* under temp/pr464-ci-fixes-20261110. Failed attempts
  retained: timer subsystem initialization; derived master incorrectly inherited
  positive semaphore value and was source-correctly disabled; initial receipt
  asserted zero age before the source cached-hold fix. Replay1 rejected strict
  mission asset lookup before launch. Later runs used isolated g1-modal-data
  with unchanged authentic data and generated mission files; no asset bypass.
- Windows D2 desktop/metadata and D1 desktop built; codecs passed3/3 on each
  (8.66s/11.79s). Linux desktop/metadata/codecs built and codecs passed3/3 in2.88s.
  Native/source message tests passed2/2 on Windows17.03s and Linux6.00s; Python
  clock controls passed3/3. Catalogs passed99 JSON/315 PowerShell/547 support
  scripts and428 master entries. No test membership or timeout changes.
- Windows desktop SHA256
  cf3990b0a93ffde66a626ec265311192b2e6c617b79dfc29b05b9fa944c4d4ab;
  metadata0a60ebee4ceaef62f132feadf23a49c7e0f9ad39c42fa4980de088c4a08d87ec;
  D1 desktop55d035e22b599d064bcce12361bda09ec75916a9af0d48da7e9785541a1da237.
  Linux desktop30ccc1c77abcd38dc183a6e81b39f177fd1cbb1813e4849a1572590a8b35a51c;
  metadata5567c332f9a5a2148b47491994902656a9ac11d02e425d2aaa977fd9352a966c.
  Root-bound caches unchanged. First Linux launch failed Wsl/Service/E_UNEXPECTED
  before building. One user-authorized WSL shutdown/restart recovered it; both
  failed and recovered build logs retained. No active work was preserved by
  request. No Android, new hosted CI, authentic asset edits or cleanup deletions.
- Work17:56-18:33 PST. G1 remaining estimate3-5h, G2-G7 approximately17.5h;
  estimates remain uncertain and are not a cutoff. Next: finish the remaining
  input/clock audit, run the final G1 public/source gates, then qualify supported
  production admission and publish layer02. Linux original Warp45s save/restore
  deadline remains unresolved; this derived pass does not replace it.

### November 11, 19:18 PST: startup profile audit and public replay refresh

- G1 remains active/unaccepted on xl_gameplay_20261111_02_timeline. Modal work
  committed as c7c6ebc3b98b2e355b37bc58b756b312dcfadee4, tree
  561208526e457e377a7439112bcdc01f46cf4c09; this is the current increment's parent.
  No layer02 push/PR/hosted-CI claim. Independent PR464 review remains untracked.
- Found a real startup gap: unsupported native cheat commands already cancelled
  active capture, but startup accepted their existing uncaptured flags. The XL
  profile now rejects20 flags outside the ordered enabled/all-weapons contract
  and unsupported Game_suspended bits. Ordinary Endlevel_sequence/SUSP_ROBOTS
  remains allowed. Source consumers include physics.c/fvi.c ghost passage,
  laser.c bounce/rapid fire/robot targeting and ai2.c firing suspension.
- Low-memory mode also lacked a startup contract. piggy.c:1481/1598 substitutes
  GameBitmaps through GameBitmapXlat; fvi.c:1317 consumes those pixels for
  transparency collision. Capture now rejects -lowmem before checkpoint creation;
  format6 playback rejects before world restoration. Stock format5 stays unchanged.
  Developer admission no longer wipes these profile errors. Runtime guard calls
  retain the same profile checks; production timeline guards are still closed.
- Existing modal owner exercises22 rejected startup profiles, verifies unchanged
  player bytes/controls/SIM count and inactive recorder/classic-demo state, checks
  normal endlevel admission, restores settings and records60 ordinary frames.
  Windows capture14 passed19.734s and Linux capture13 passed14.152s. Their demo
  hashes are2d10ec27736047d37bed1d90e369e863350630a310de65b358bc792ad14e7934
  and77171d9920d62820c5083c4a0562c382d85d02d4f90e45cd2f9cd98c3ff1ea51.
  Linux replay9 of capture13 passed strict assets/terminal/natural exit and all
  detailed comparisons:60 objects/frames,64 world records,60 FX draws,zero SIM
  and presentation draws,zero differences. This remains a derived robot-free
  modal control, not original progression/combat or true-headless acceptance.
- Negative lowmem replay1 on Linux and replay3 on Windows rejected the valid
  capture7 before world restoration, with only a meta trace and no actual result.
  Failed Windows attempt2 is retained: engine exited1 before world restore but
  its printf reopened CON, leaving captured stdout empty. The new diagnostic now
  uses con_printf and persists in gamelog.txt; replay3 verified that exact reason.
  Captures10-12 retain the earlier21-profile controls. No failed receipt replaced.
- Full unchanged public graphics owner exited0. Native17 scenes twice passed34/34
  with pinned assets, live-capture frame/diagnostic/terminal/SIM fidelity and natural
  exit. Required imported D1 flight gameplay/SIM scope passed; four other imported
  probes remain unqualified, raw paired exit1 and three existing exclusions retained.
  Report: temp/pr464-ci-fixes-20261110/g1-profile-public-graphics/report.json.
  This run used D2 SHA8f43d8d1f064e971d8d1ef874c5b0fe4166e16dc931c0706b8279965e0193822
  and D1c90c5ba7e57281f890717d1450e497d87e61bdcc14298a7624344973ae88a020,
  before the lowmem rejection/logging additions. Imported tests used frozen binary
  copies; originals rebuilt only after native runs and copy ownership were verified.
  This is explicit scoped evidence, not final published-head CI.
- Windows D1 desktop/D2 desktop+metadata and Linux D2 desktop+metadata build.
  Source slow-motion/contact/text controls pass3/3 on each host (0.24s/0.03s).
  Python clock controls pass3/3; catalogs pass99 JSON/315 PowerShell/547 support
  scripts and428 master entries. No membership, timeouts, stock fixtures or
  proprietary inputs changed. Existing unrelated metadata compiler warnings remain.
- Final Windows desktop887f36520ae417cde11995063f9a759783bb9de9ded7a6a9dc4d94ce154cb0cc;
  metadataff8f9d2f1ee728105dcf563c5bd66a853e8ff5f8eaac8fa55653bf830e994840;
  D1 desktop51f0b08647f024a30256651cf3ec4c4fa4e9f0553482374c07d5f4934c6b7211.
  Linux desktop203492509c65034d80af710213a47340b2b46051aa73a212f782d7fb94a04e1e;
  metadata88b430f7c3e270ba547ac53f18844d5a94892a9af0e2e128f64eaafb992c96b1.
  Capture13/14 precede only the con_printf diagnostic change; final Windows
  negative3 exercises the final desktop. Root-bound cache locations unchanged.
- Clock audit: rear-view/automap decisions are ordered operations; device scaling
  precedes captured controls; Guide-Bot pending-cache polling returns during
  capture/replay. Shield warning timers only select HUD/sound output. Muzzle light,
  OOF spin, camera capture/visibility, particles/lightning and backend-driven audio
  state still require the G3 independence controls; no omission from comparison
  or presentation-only acceptance follows from this static inventory.
- Work18:33-19:18 PST, free51325734912 bytes at19:18. No additional WSL restart,
  Android run, cleanup deletion or external publication. Next coherent G1 action:
  extend existing omega_damage_probe.h/xl_omega_damage.py so its assisted combat
  recording acquires Omega through ordinary pickup during capture (current witness
  grants ownership before capture). Preserve its original target, label any derived
  pickup setup, and compare live/immediate/repeated/pacing-varied playback. Then
  finish supported production admission, final G1 gates and layer02 publication.
  G1 estimate2-4h; G2-G7 approximately17.5h, no cutoff. Linux Warp45s save/restore
  performance gate and Android full-GPU timeout remain open; no new external blocker.

G1 Omega acquisition checkpoint, November 11 at 19:48 PST (November 12 03:48 UTC):

- Continued the existing omega_damage_probe.h/xl_omega_damage.py owner from
  parent8076db4b3823f130633ddab3d5b27ef982ceac22, tree
  631cef8f2894a747de77091d9d3130efa194958a, on layer02. No production admission
  changes in this increment. Existing component controls remain intact.
- The assisted checkpoint now starts without Omega ownership or charge. It
  relocates original pickup73/signature74 from segment763 to the existing
  approach pose, then acquires, selects and fully charges it through ordinary
  collision during recorded frame0. The original target remains robot117,
  signature118, ID7. Two position assists and one inventory setup assist precede
  capture; zero state assists occur during recording. No fabricated pickup,
  mission-byte edit, connected route or completion claim.
- Ordinary selection sets a one-second rearm deadline: d2/main/weapon.c:386 and
  weapon.h REARM_TIME; pinned XL weapons/weapon.cpp:407 and include/weapon.h:159
  agree for this normal-speed profile. Let 61 ordinary released-fire frames pass
  before the full90 firing/90 recharge frames. Total242 frames, settings
  operations at102,102,197 and terminal tail242. No deadline reset or replay repair.
- Failed capture12 is retained: acquisition succeeded but the old39-frame combat
  assertion ran before rearm expired. Capture13 completed gameplay and finalized
  at31.412s after probe entry, but failed the45-second process timeout (47.000s
  receipt); it is not accepted. Intermediate capture14 passed in44.594s and its
  three replays15-17 matched241 frames. Final diff review restored the one firing
  frame initially replaced by acquisition, preserving all90 original firing
  frames. Fresh capture15 passed in40.250s, with27.217s from probe entry through
  finalization. This does not establish a latency improvement. The owner remains
  45s and preserves the full combat/recharge workload; failed receipts remain.
- Capture15: temp/pr464-ci-fixes-20261110/g1-live-omega-capture-15/result.json;
  demo d2_wshock20_level5_20261111_194308.dximdemo SHA256
  61bf922ac076cc213d6f1b71b79d750db721c0b18bfcbb09895733d86998b25b.
  Original target shields16384000 ->6844288 while Omega selected ->3664384;
  charge65536 ->17488 ->42058. Capture artifact remains private/local.
- Windows clean-process replays18/19/20 (accelerated/accelerated/realtime) all
  pass strict assets, terminal state and natural exit. Each detailed comparison
  passes242 objects/frames,246 world/boundary records,1616 SIM RNG events and488
  FX RNG events, with zero differences. Presentation RNG remains separately
  reported as different (285555 events; initial call count11697 vs1), retaining
  the prior qualified SIM versus unqualified presentation distinction. Comparison
  receipts are g1-omega-replay-{18,19,20}-comparison.json in the evidence root.
- Windows and Linux root-bound D2 desktop builds pass without new warnings.
  Windows executable SHA3a2ed709c06f8d6cb84b10bde5afd2d69d53651b5b0189160f334e7b942825e3;
  Linuxae3397a67a6f0ae9449846c4141ecbdc1bd58337959fe2336e7e687de4efaf0d.
  WSL responded normally; no additional restart. Linux build is compilation
  evidence, not Linux playback of this new capture. Python Omega controls2/2
  pass; catalogs99 JSON/315 PowerShell/547 support and428 master entries pass.
  Scoped mixed-language quality and diff checks pass. No stock fixture, required
  membership, deadline, external publication, Android profile or asset changed.
- Original Warp HOG rehashed unchanged:
  2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c.
  Reference remains2576a98b5de310e3ac7c9777df2677d1627b8bae. Free space
  49544671232 bytes at19:47; no cleanup deletion. Work19:21-19:48 PST.
- Next G1 action: finish supported production admission in existing recorder,
  level-start, startup and runtime guard owners, keeping legacy/malformed,
  unsupported profile and unqualified backend rejection. Compile and exercise
  the non-developer path, finish final G1 gates and publish layer02 with exact-head
  CI. G1 estimate2-3h; G2-G7 about17.5h, no cutoff. G2 completeness, G3 true
  headless/presentation independence, G5 connected play, G6 Android/platform proof
  and G7 delivery remain open. Linux Warp45s save/restore and prior Android GPU
  timeout remain failures, not reclassified by this assisted witness.

G1 normal desktop admission checkpoint, November 11 at 21:06 PST
(November 12 05:06 UTC), continued from ab67cd3008dac6d97106ec0ebd6bff5ba5a3f425:

- Normal Windows/Linux OpenGL builds now admit the validated format6 clock/input
  profile through the existing recorder, level-start and replay startup owners.
  Complete player/settings/view/FP checks remain; legacy, malformed, low-memory,
  unsupported cheats and unqualified backends still reject. True headless and
  Android remain closed outside developer qualification. This is bounded G1
  admission, not G2 state completeness or G3 presentation independence.
- Ordinary UI exposed a real shared D1/D2 lifecycle bug: restoring the menu before
  unlinking a startup-resumed game window deactivated that window twice. The next
  game retained pause depth1, froze physics and eventually overflowed the ordered
  input batch. Both game handlers now restore menus in EVENT_WINDOW_CLOSED,
  following the existing Android ordering. No pause reset or batch-limit increase.
  Existing campaign owner now runs two real game/menu lifecycle cycles. Negative
  ui3 and positive ui4 logs retain the observed depth transitions.
- Replay completion now finalizes RNG and state traces before publishing the
  terminal result. Previously a large RNG export exceeded the wrapper's unchanged
  two-second shutdown grace after a matching result, leaving incomplete evidence.
  Replay RNG events now spool beside the owned output instead of growing a
  contiguous 32-bit allocation. Same JSON schema, ordering and full SIM/FX/
  presentation streams; truncated exports fail. Existing codec owner checks
  failed-finalization admission, active exports, tail rollback, replacement draws,
  exact memory/spool byte equality and cleanup. Recording still buffers in memory;
  parsed replay frames are not constant-memory. No unlimited-recording claim.
- Real normal UI ui2 recorded1614 frames on original Warp5. Replay1 failed clean
  exit with partial RNG/gzip; replay4 passes natural exit and1614 frame states,
  18836 SIM/1618 FX events after finalization fix. ui4 starts from an unchanged
  assisted checkpoint (SHA cac10e1ef70f15556990e0eb0c3490d36d2e4fa8dd4e660a405d78c6dec2ff15),
  returns to menu, loads again, acquires Omega through collision and records normal
  primary input with F5. No state assists during capture; high-resource checkpoint
  origin remains assisted. No connected route, kill or level-completion claim.
- ui4 recording SHA ccae0e4e263a719f299c4ba6fb8ab20cd536dac691ff1a72177f7ec08e87db5b,
  2549 frames. Capture executable d2f54addcb3d1d34d095bb2b71314631017c09474b02ebb6c6abcb81020919ac
  included four temporary pause diagnostics, removed from final source. Windows
  replay5 matched state but truncated at4194304 RNG events; not accepted for RNG.
  Linux replay6 and final Windows replay7 pass strict assets, terminal state,
  natural exit and full traces within the unchanged300-second owner deadline.
  Both match12067 SIM/5103 FX events and2549 live frame states. Complete world/
  object trace payloads are byte-identical across the two replay platforms; gzip
  OS header differs. This is not a live-capture full-world claim. Presentation RNG
  differs and remains unqualified. Linux4,890,287 and Windows4,502,949 trace events
  are complete, truncated=false; temporary spools removed. Final Windows memory
  stayed approximately473MB. Comparisons/identity receipts use prefix
  temp/pr464-ci-fixes-20261110/g1-admission-ui-replay-.
- Compact assisted Omega capture15 additionally passes normal Linux replay21:
  242 frames/objects,246 world boundaries,1616 SIM/488 FX events. Normal Windows
  time replay22 passes terminal/clean-exit,231 frames/objects,235 world records,
  1193 SIM/236 FX events with zero differences; no presentation draws in this case.
  No authentic mission bytes changed: Warp HOG SHA
  2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c.
  Pinned reference remains2576a98b5de310e3ac7c9777df2677d1627b8bae.
- Windows D1/D2 and Linux D2 normal desktop builds pass. Final Windows D2 SHA
  5d35bb2240fddaf4b84bb0509fb355cafb7149a43d52a89e8428b2b4a6e803c1;
  metadata5eb9b3fa2c6547d6b77de0e870affd76272b42f5db4ee66d08e2edce2a152000;
  D1 desktop2a8fba60ea3214e8dcc7b6c743a08648c60f778981d0654f4489ae3b0bf2c300.
  Linux D2 desktop88661d0785b65a45d749752531617bfd5345113fd94d0f437041e3e756ec8099;
  metadata2487d0ae997197c749d9d22dc1bb98aba8c45acfe1eaf404c216271f03d6f4f8.
  Root-bound caches unchanged; normal D2 developer probe OFF, strict FP ON.
- Final Windows D2 CTest103 pass/1 expected skip of104: connectivity diagnostics
  are absent in this release profile. Codecs3/3 pass on Windows D1, Windows D2
  and Linux D2. Campaign integration passes21 native/imported D1 observations,
  three ordinary D2 cadence and12 death cases; Linux campaign JSON byte-identical
  (SHA3c3dedfb6292c4cd40b0076e80f58a7e7d070912b3919bbc144083fa09ad4607).
  Two stale campaign assertions corrected against prior implementations: Fusion
  cadence already draws SIM, and saves already preserve GameTime64. No immutable
  fixture changed. Intermediate failed attempts retained, final campaign-6 passes.
- Full public graphics owner passes34/34 native replays and required imported
  flight scope. Four other imported cases remain unqualified; three existing
  exclusions and raw paired failure remain visible. Native runs overlapped the
  trace-storage rebuild, so this is explicitly not final-binary qualification.
  Final-source codecs/CTest/large replays pass; exact published-head public CI
  remains required. Reports/logs use g1-admission- under the evidence root.
  Catalogs pass99 JSON/315 PowerShell/547 support and428 master entries. Scoped
  quality passes; no required membership, deadline or private asset was changed.
- Work19:50-21:06 PST, continued publication/CI pending. PR464 remains open at
  e87f4bfaeec60fd7a9697488413e02cc88230ade; PR465 draft G0 head
  96ce2f8be5f917178024298cf51272781abc0caa. Layer02 still local at this checkpoint.
  No Android run/APK or further WSL restart. Free space about37GiB; official
  cleanup preview in progress with evidence/assets/build dependencies protected.
  Next: finish time comparison, quality/commit, push layer02, create immediate-
  parent draft PR and native stack link, inspect exact-head CI. G1 estimate1-2h;
  G2-G7 approximately17.5h, no cutoff. Linux Warp45s save/restore and prior Android
  GPU deadline remain failures; G2 completeness, G3 headless/presentation, G5
  connected play, G6 Android/corpus and G7 delivery remain open.

G1 publication checkpoint, November 11 at 21:09 PST:

- Published implementation119fa3afef61f887a646ca2b0c8935a5ff70d860, tree
  879ac6a41e83261102f1c130c87d8787c1c67c38, as draft PR467 based on PR465.
  Commit hook displays an intermediate hash/future timestamp; identities here
  are reread from Git and time is actual UTC/PST. Independent review stays untracked.
- Initial native link of465/467 correctly refused to remove464: server metadata
  now has existing stack466 containing464/465, based on cmake b3bb12cb. Preserve
  that user-established topology and append467 to stack466 instead of removing
  or reparenting464. This is the explicit reason for retaining the prerequisite
  in the existing stack, superseding the plan's proposed new tranche-only stack.
  All PR branch bases remain their immediate parents; no branch rewrite or merge.
  Append succeeded: PR467 is position3/size3 in server stack466. Local gh stack
  view has no tracking record; server REST metadata verifies the result. Avoid
  importing the stack locally because its prerequisite branch has preserved local
  ancestry. Connector metadata and explicit fork REST/CLI remain authoritative.
- Head119fa3 CI started: public save43159195448, engine43159195485,
  XL43159195590, scripts43159195514, sample43159195615, LAN43159195421.
  Private request43159195454 succeeded; actual qualification still pending.
  Runs are pending evidence, not accepted coverage. Expected manual/historical
  jobs and no-Rust-change omission remain distinct from executed checks.
  Checkout audit identifies merge9518a725 in default PR jobs, distinct from
  head119fa3. Do not label those jobs literal head execution. Native developer,
  trusted private and selected sample owners bind the candidate explicitly;
  dispatch branch-specific engine/public-save/XL workflows for literal-head
  execution after this publication-only ledger update. Their exact run/head
  results will be reported in PR metadata and the next acceptance checkpoint.
- Official temporary cleanup preview/apply found zero eligible artifacts with
  active evidence, authentic assets, reference and dependencies protected.
  Apply correctly failed its50GiB target;36.90GiB remains. No files deleted.
  Continue within reserve; do not discard required evidence to satisfy the target.

G1 CI repair checkpoint, November 11 at 21:26 PST:

- Direct-head ef0e4e100daaf897c6f4c735768ebca4453cbc69 exposed three test
  integration gaps. XL run43159537011 passed61 Linux/56 Windows public contracts
  and both developer builds, then the Linux standalone boss consumer failed to
  link the new timeline endpoints. Its bounded harness now supplies the live
  trigger clock endpoints while preserving the real trigger/cooldown code and
  the existing timer-event assertion. Linux consumer build/CTest passes1/1.
- Tooling run43159557730 failed its public-effect synthetic trace tests because
  the fixture omitted AI/object capacities now checked by the detailed reader;
  corrected fixture fields/order without weakening validation. Its replay-menu
  test also failed because extracted wrapper functions had no ExtraDir parameter
  initialization. Initialize it and verify both omitted and explicit paths with
  spaces. No production guard, required test membership or timeout changed.
- Full public D1 fault-tools owner passes on Windows/Linux:110 tests across six
  modules,109 passed/one expected missing-retained-capture skip per platform.
  Menu owner passes Windows/Linux. Initial WSL menu invocation reached the final
  suite-cancellation check then failed reading this shared checkout's Windows
  dependency_base.txt; rerun with process-local c: PSDrive mapped to /mnt/c passes.
  No tracked dependency configuration changed. Scoped mixed-language quality
  and git diff --check pass. Evidence prefix temp/pr464-ci-fixes-20261110/g1-ci-.
- Both Linux engine D1/D2 jobs in43159529751 pass. ASan, public-save43159533419,
  trusted private and final frozen-binary local graphics owner remain pending;
  pending checks are not accepted coverage. This test-only repair requires new
  published-head CI. G1 remains open until those gates and any failures resolve.
  Actual time21:26 PST; revise G1 remaining to1-2h, G2-G7 approximately17.5h.
  Next publish this coherent harness repair, inspect exact-head CI, and finish
  the unchanged-binary graphics run before accepting G1 or creating layer03.

G1 private-owner repair checkpoint, November 11 at 21:53 PST:

- Trusted private run43161281763 tested head9c81a6b67cb4f9e2096ac59632f9b4eeb9bc13af
  and failed exactly xl_wall_trigger_scheduler, xl_doorways and xl_water_arena.
  Its sanitized receipt reports710.917s elapsed, native-tests entered458.291s,
  attempted_tests=true and coverage_complete=false. Earlier119fa3 private run
  43159215256 reported the same three failures. These are actual native failures,
  distinct from its stale-head readiness publication rejection. Current readiness
  measurement/reporting completed the unchanged frozen4/8 result.
- Reproduced all three on Linux before editing. Scheduler expected restore to
  reset the SIM diagnostic count; it now checks the saved count and seed. Arena
  likewise verifies preserved count and the subsequent draw delta, and requires
  the new XL save_checkpoint origin instead of the old new_level assumption.
  Doorway's animation-only loop does not advance the host clock; wait800ms and
  update the clock before retrying the restored trigger. Keep the existing
  closing-door reversal assertion and all later physical/visibility checks.
  No production code, immutable fixture, required membership or deadline changed.
- Rebuilt developer metadata from9c81 plus these three test-owner changes;
  focused CTest passes3/3 in25.12s (scheduler6.98,doorway2.71,arena15.43).
  Arena ordinary-control dry-spawn traversal/replay matches1235frames; saved
  water-exit continuation matches90frames and position/orientation/velocity,
  player/time/SIM state. Scoped quality and diff whitespace checks pass.
  Evidence: temp/pr464-ci-fixes-20261110/g1-private-three-{repro,fixed}.log,
  g1-private-owner-{build,quality}.log and g1-private-9c81-ci/results.json.
- Full canonical run_xl_originals.py --fast is now running locally against the
  root-bound /var/tmp/dxx-pr464-20261111/private caches and pinned reference.
  Developer rebuild passed; normal stock targets are rebuilding. Its final
  result remains pending. Log g1-private-canonical-fixed.log; raw native logs
  remain in that private work directory. Publish the focused repair now so its
  fresh hosted validation can run while this complete local owner finishes.
- Frozen-binary public graphics owner has now completed:34/34 native pass and
  required imported-D1 flight gameplay/SIM comparison pass. Four other imported
  cases remain unqualified; three retained exclusions and raw paired exit1 remain
  visible. Sourceef0 normal binaries stayed unchanged through end checkout9c81;
  only harness/ledger changed. g1-published-public-graphics-final.json verifies
  both D1/D2 hashes. This supersedes the earlier mixed-build graphics limitation.
- Literal9c81 CI: XL43161285272 passes61 Linux/56 Windows public contracts, linked
  boss consumer and both developer builds. Tooling43161288560 passes both hosts.
  Engine43161277834 D1/D2 pass; ASan still running. Public-save43161281866 passed
  ordered-input phases and reached ordinary-save persistence; still pending.
  New repair requires new exact-head runs. Expected historical/manual/platform
  omissions remain skips, not runtime coverage. No Android/APK change here.
- G1 remains open. Work21:36-21:53 PST; allow another1-2h for complete private and
  current-head CI, G2-G7 approximately17.5h with no cutoff. Read-only G2 audit
  identifies lost runtime object birth time/order and pure-observer requirements;
  no G2 branch or implementation yet. Next: publish this coherent test repair,
  complete canonical/private and exact-head public gates, then accept G1 and
  continue layer03. Independent untracked review and existing stashes preserved.

G1 public door-observer repair checkpoint, November 11 at 22:00 PST:

- Published private-owner repairf93ad24ac513f726caae7f0831a51c5d46d1fa70, tree
  7ef63189a9ca8daa45e457fe2c6e91e6fcb41cf4, to the same layer02/PR467. Native
  stack466 and immediate parent465 unchanged. Canonical local private run still
  active: preview and stock metadata built, stock route target now compiling.
  No native cohort conclusion yet. Preserve that process/session and its logs.
- Prior-head9c81 engine43161277834 completed successfully including ASan.
  Both direct public-save43161281866 and PR-merge43161265041 then failed the
  compact door screenshot check. The immutable D1 door_observations differed,
  while frame count/hashes, SIM count/hash and terminal hash matched. Dependent
  replay jobs were skipped because of this failure; they supplied no coverage.
- Root cause is duplicate diagnostic emission introduced by the new ordinary
  recording observer. public_level_replay_fixture.hpp emitted its own pre-frame
  door probe, then shared input_demo_record_game_frame emitted it again through
  input_demo_world_trace_write. Local pre-fix D1 capture has437 records, every
  duplicated record identical. One copy of each reproduces the immutable219-
  record SHA b0d4d60ab37ff1ecad2a89ab7b1c777253fefe9642c9dd3d1fc7efbbafc35d07.
- Remove the fixture's redundant per-frame call; retain explicit restored and
  terminal observations and the shared ordinary recorder. No filtering or
  deduplication was added to validation; no oracle or gameplay code changed.
  Rebuilt Windows D1/D2 test_upstream_compat targets and ran fresh screenshot
  captures. Both match all six immutable oracle fields and screenshot checks:
  D1 244frames/219door records/19images, D2 362frames/315door records/26images.
  D2 door SHA80a12f84351320d7c910ab1173d9964c7f81b58c890189439e7b57896767e23a.
- Evidence prefix temp/pr464-ci-fixes-20261110/g1-door-; comparison JSON records
  current sourcef93 plus this one-line fixture change and executable hashes.
  Cached build stamps are older (D1 04b1ee17f,D2 ab67cd300); these are explicit
  incremental local comparisons, not new fixture admission/provenance claims.
  Hosted Linux exact-head capture remains required. Downloaded prior CI reports
  are under g1-public-save-9c81-ci; no large raw artifact download was needed.
- Currentf93 direct runs engine43164486453,public-save43164489124,
  XL43164491695,tooling43164495098 and trusted private43164491258 started before
  this newly diagnosed fixture repair. Publish this coherent correction and run
  exact-head validation again. G1 remains open; G2 audit only, no child branch.
  Work21:54-22:00 PST; remaining G1 approximately1-2h, G2-G7 approximately17.5h.

G1 validation checkpoint, November 11 at 22:41 PST:

- Current published head0a2df4aebc4e87be3d1ae802f05c2fce64b55072 remains on
  layer02/PR467, immediate parent465, server stack466. No source changes in this
  checkpoint. Independent review remains untracked; existing stashes preserved.
- Literal-head engine43165269464 passes D1/D2 and ASan. XL43165276558 passes
  public contracts and both developer builds; tooling43165280117 passes both
  hosts. Public-save43165272925 build/save job131005721865 now passes, including
  the immutable door screenshots. Native131013301146 and headed131013301158
  downstream replay jobs are executing; no aggregate replay conclusion yet.
- Trusted private43165315754 passes the complete canonical owner at current0a2:
  113 required tests,16 format witnesses,3 modern source levels,4 fixed navigation
  anchors in4858ms; owner927.189s. Check-results and exact-head publisher both
  pass. Sanitized receipt: temp/pr464-ci-fixes-20261110/g1-private-0a2-ci/results.json.
  Earlierf93 run43164491258 also passed its complete owner; its publisher rejected
  the superseded head. Keep that provenance rejection separate from test results.
- Local canonical native cohort passes113/113, zero CTest skips,279.07s. Its
  invocation then failed on a previously existing navigation output directory.
  Preserving that evidence, resumed unchanged canonical functions in a fresh
  directory:16 formats,3 source levels,4 anchors22.927s, asset re-verification and
  strict summary validator pass. Composite receipt/provenance are under
  g1-private-postchecks-0a2-final; this is not an exit-zero claim for the original
  invocation. Canonical freshly rebuilt developer executable SHA256
  8dae8f10efdd67ef555a290d9c945d9fba65c45f9ebb6416b866ec908b0ebadf;
  normal headless metadata3fce8feef542f0f2970a1cd6d8e678ec78733a2d152a6475abd442040e231a4d,
  route8a28c2cc1164220cc0e913ced3d54b5e4638afd887373c70d054d87f8f46029c.
  Build source is9c81 plus the private fixes committed asf93; later0a2 changes
  only the public capture fixture/ledger. Full receipts retain this distinction.
- Standard private camera_subchecks explicitly not_run_missing_addon; rendered
  full-resource Warp actor checks are outside this owner. Supplemental current
  developer projectile CMake owner with pinned addon df69c62a and Vertigo
  faa67189 passes all camera subchecks, fresh-process continuation and source
  comparisons under unchanged150s outer limit. Source comparisons:history1650,
  controller10800,motion2160,rotation32448,each model hitbox7560. Evidence prefix
  g1-camera-supplement-0a2; genuine inputs remain byte-identical.
- Broader sample43165276576 host/JVM pass. JVM1322 executed,12 skips:8 optional
  private-content inputs and4 owned live-service/auth prerequisites. Host extractor
  retains sow_real_media_tests skip and hfs/sti2/stuffit real-media exclusions.
  Android reviewed test selection, final job and aggregate result check pass
  (confirmed22:44 PST). This sample does not certify authentic XL
  Android gameplay. Receipts/plan/reports use g1-sample-0a2- prefixes.
  Final summary confirms11 selected owners PASS and none selected without a
  pass; only2 owners provide complete leaf coverage. Its outcome remains
  incomplete with3128 catalog owners pending, not a full-catalog qualification.
  Compact final receipts are in g1-sample-0a2-summary.
- Frozen readiness remains4/8 with its expected red status, distinct from actual
  rendered XL. Format6 Android/true-headless guards remain closed. G2 state/cold
  continuation, G3 presentation independence, G5 connected play and G6 platform
  qualification remain open. G2 read-only notes identify birth time/order loss,
  missing native/XL semantic fields and whole-save atomicity to reproduce; no
  G2 source implementation or child branch yet.
- Work22:00-22:41 PST. G1 remaining estimate30-60min for replay/Android closure
  and any failures; G2-G7 at least17.5h, with increased G2 uncertainty from the
  full-state/atomicity audit. No cutoff. Free space35.21GiB; no cleanup deletion.
  Next inspect the live replay and Android jobs, fix deterministic failures, then
  accept G1, create layer03 from its unchanged published parent and reproduce the
  object-birth cold-continuation gap in the existing projectile owner.

G2 preparatory reproduction, November 11 at 22:55 PST:

- G1 hosted native replay job131013301146 now passes; headed job131013301158
  remains live in43165272925. Published parent0a2 is unchanged; no layer03 branch
  or production persistence change yet. This is preparatory regression evidence
  while the final G1 job runs, not G2 acceptance or whole-stack readiness.
- Extended existing projectile_test.h with unpublished opt-in birth/birth-resume
  modes. Derived setup creates one native robot/weapon, saves at age0.5s, then
  compares the same real collide_robot_and_weapon call at age0.75s uninterrupted,
  after same-process disk restore, and after a fresh process loads that save.
  Default canonical owner behavior and membership are unchanged by these modes.
- Both disk paths reproduce the material defect: uninterrupted victim100 shields
  and projectile15; restored victim60 and projectile25. Hit history remains true;
  SIM state3554416254 and call count2 agree. Thus this is not merely a missing
  diagnostic counter. Save SHA256
  4fb3448e28ec7e8c77f28913e90260381a8eb5935c4acbc2efccad52fd8dc644.
  Evidence: temp/pr464-ci-fixes-20261110/g2-birth-repro/{birth-future,birth-restored,
  birth-cold}.json and g2-birth-{repro,cold-repro}.log. Both commands exit1 as
  expected for the unfixed defect; failed evidence is retained.
- Root-bound Linux developer target rebuilt successfully with the opt-in test
  patch over0a2; normal games and published CI binaries unchanged. Existing GCC
  string-literal warnings remain. No authentic input or expected fixture edited.
  Developer EXE SHA256b8a9e4e25ca0c76c87926561970582077b245e861da7e8d21083031eb4110d49.
  Scoped mixed-language quality and diff whitespace checks pass. Formatter-only
  line wrapping followed execution; retained test patch SHA256
  4e84c15f8247df8beacfa0aac92ec099bfae29395006d4e066abf3580559dd7c,
  g2-birth-repro-working.patch. No current canonical executable identity claim.
  Next: finish G1 hosted headed result, create layer03 once G1 closes, preserve
  born time/order through the shared save owner, extend invalid/slot reuse/order
  controls and integrate these cold-process checks into the existing owner.

G1 native replay receipt, November 11 at 22:58 PST:

- Literal0a2 native replay job131013301146 passes34/34 runs across17 scenarios;
  every run has exit0, status pass and fidelity pass. Wrong-oracle/bad-input
  controls reject with exit1, and no-collect control passes its authored-loot
  scope. Report qualification remains incomplete and paired imported-D1 parity
  explicitly not_run; that requires the separate headed job131013301158, still
  live. No native/XL headless equivalence claim follows from stock-only results.
- Compact reports: temp/pr464-ci-fixes-20261110/g1-public-replay-0a2-headless.
  Large725MB raw artifact was not downloaded. Published parent remains0a2;
  current unpublished diff is the preparatory projectile regression and ledger.

G1 acceptance and G2 entry, November 11 at 23:04 PST:

- G1 ACCEPTED for the bounded normal Windows/Linux OpenGL Omega/time timeline
  profile at published0a2df4aebc4e87be3d1ae802f05c2fce64b55072, tree
  16f41c900c030b49c62a3dd410e9dc9df3410ea9, PR467. All required current-commit
  dispatched series now pass:engine/ASan43165269464,XL43165276558,
  tooling43165280117,public save/replay43165272925 and trusted private43165315754.
  Broader sample43165276576 and public LAN also pass. No merge performed.
- Final hosted headed131013301158 passes34/34 native runs and required imported
  D1 flight gameplay/SIM scope. Four imported scenarios remain unqualified,
  three exclusions and raw paired exit1 remain visible, detailed diagnostic/
  object/world parity incomplete. All three real-engine negative controls pass.
  Thus existing retained scope passes; no whole-simulation/imported parity claim.
  Compact reports:g1-public-replay-0a2-headed;853MB raw artifact not downloaded.
- Fork refresh confirms origin/02 equals0a2 and cmake remainsb3bb12cb. PR464
  remains open at e87f4bfa; server stack466 still464/465/467, tip position3/3.
  Three original stashes and independent review preserved. G1 final acceptance
  ledger is carried into the next coherent layer without changing the already
  tested parent commit or rerunning CI for a documentation-only parent push.
- Created xl_gameplay_20261111_03_continuation from refreshed origin/02 at0a2,
  carrying the unpublished preparatory test and ledger. No layer03 commit, push
  or PR yet. G2 remains OPEN; no production state fix has been applied.
- Second G2 regression now reproduced in an isolated fresh process: corrupt
  only the checksum of the derived newborn save. Native restore returns false
  with Corrupt XL trigger state payload, but player energy changes91 to saved100
  before rejection. Receipt g2-birth-repro/birth-invalid-restore.json and log
  g2-atomic-repro.log. This confirms whole-save atomicity is weaker than existing
  trailer-only tests. Opt-in birth-atomic mode retains that failed evidence;
  default canonical owner/membership still unchanged. No authentic input edited.
- G1 work exceeded its estimate because real live/cold/CI integration failures
  required repair. Actual checkpoint23:04 PST; G2 estimate revised to4-6h for
  complete observation, forward persistence and atomic cold continuation, with
  G3-G7 approximately14.5h afterward. No cutoff or scope reduction. G0/G1
  accepted; G2-G7 open. Normal Android/true-headless format6 guards stay closed;
  frozen readiness remains4/8. Next finish the shared birth snapshot/validation/
  restore contract and its cold/order/reuse controls, then the remaining state
  map and whole-save rejection behavior through existing save/rewind owners.

G2 birth-state implementation checkpoint, November 11 at 23:24 PST:

- Added shared read-only birth snapshot and validated restore in object_birth.*;
  schema41 in trigger_save.cpp preserves every live slot/signature, creation time,
  append order and next counter. Bound rows/fields, exact live membership, unique
  nonzero orders, nonnegative times no later than GameTime64, and counter overflow
  reject before birth-table publication. Expanded storage stays on the heap.
  Authored objects retain zero births; deleted rows have no future effect and the
  counter still preserves their consumed append positions. No source-order/RNG
  compensation or old-development-save compatibility reader was introduced.
- The original derived damage regression now passes both same-process disk and
  fresh-process load. Extended it to compare new allocations, deleted/reused slots
  and monotonic next orders. Integrated both modes into the existing projectile
  CMake owner. Added19 malformed birth cases, preserving all56 prior cases, with
  explicit unchanged birth-table checks. Camera cold snapshots now include births.
  Historical source hit-state/reset controls remain; forward birth semantics are
  documented explicitly in regression_demos/README.md as a deliberate extension.
- Full Linux projectile owner with authentic pinned addon/Vertigo and all existing
  source references passes under unchanged150s outer deadline. Camera subchecks
  executed;75 malformed cases, valid recovery, native/stock collision controls,
  full camera save-through-stock/cold process, and newborn warm/cold continuation
  all pass. Evidence:g2-projectile-contract-fixed and its log. Linux developer EXE
  SHA256e6d25309bd6b486e4b2ab6e2c3830a3a44a36a115aa2120033bc6bc42ec305e7.
- Initial expanded owner rejected the camera fixture because its smart-child
  helper retained actors/effects created at later times, then rewound GameTime64.
  Removed that fixture clock rewind so retained runtime actors stay in their
  creation domain; validation was not weakened. Failed evidence remains in
  g2-projectile-contract. A temporary all-row diagnostic exceeded con_printf's
  buffer and aborted the diagnostic attempt; it identified the future rows and
  was removed. That failed attempt remains g2-projectile-diagnostic, never a pass.
- Windows developer cache verified CMAKE_HOME_DIRECTORY=C:/local/dxx-redux/d2,
  x86 MSVC14.44, developer ON, strict FP ON. First invocation incorrectly used
  output name xl-preview-lifecycle; current CMake requires target
  dxx-redux-d2-headless-metadata. Corrected build is live, log
  g2-birth-windows-metadata-build.log. Never ran its stale November4 executable.
- Source remains uncommitted on layer03; no push/PR yet. Earlier scoped quality
  passed; final quality after the fixture/validation edits still required.
  Whole-save malformed-trailer atomicity remains reproduced and unfixed, as do
  the remaining complete-state observation/cold-continuation gates. Next finish
  Windows build and existing-owner runtime validation, then continue G2 state
  completeness and whole-save rejection using existing owners. No G2 acceptance.

G2 birth-state validation checkpoint, November 11 at 23:38 PST:

- Final shared projectile owner passes on Windows x86 and Linux with authentic
  addon/Vertigo, all camera/source comparisons,75 malformed cases, valid recovery,
  save-through-stock/cold camera state, and newborn uninterrupted/warm/cold future
  collisions including new allocation and deleted/reused slots. Windows19.647s;
  Linux completed under enforced150s outer limit. No required test or deadline
  removed. Logs/directories:g2-projectile-final-{windows,linux}.
- Newborn observations match across hosts:birth_time131072,birth_order1,
  GameTime180224,victim100 shields,projectile15,hit historytrue,transient order4,
  replacement order5,SIM3554416254,calls2. This compares actual native collision
  results, not a replacement expected fixture or replay-only repair.
- Windows first failed new case67 because legacy loadgl.h defines false as0:
  the fixture emitted valid integer0 instead of a JSON boolean. A subsequent
  test assertion likewise compared JSON true with macro-expanded integer1.
  Corrected the new cases to explicit json::boolean_t and get<bool>, preserving
  validation. Both failures retained under g2-projectile-windows[-fixed]; final
  runs above pass. Error reporting now distinguishes opened/accepted/closed/
  unchanged and always closes an unexpectedly accepted malformed file.
- Final developer EXE hashes:Windows
  c3aba7447a1e4c254dcd074f2ab329c4ea8406cf61ff1a32447e1a4f01913909;
  Linux3b8d7abc4204bb33af00f57af0da068d1c889ad1e28e41fa2e96a96257c2ad68.
  Both built0a2 plus this working source patch, with strict FP and developer
  probes. These are developer validation binaries, not new normal-game packages.
- Scoped mixed-language final quality and whitespace pass. Automation catalog
  passes99 JSON/315 PowerShell/547 support; runner catalog passes428 top-level/
  547 support. Logs:g2-birth-final-quality,g2-birth-catalog,g2-birth-master-catalog.
  Existing compiler warnings unchanged; no new warning identified.
- Repeated whole-save atomicity probe on fresh schema41 save still fails as
  expected:restorefalse,live energy91 replaced by saved100. Evidence
  g2-atomic-after-birth.log and g2-projectile-final-linux/birth-invalid-restore.json.
  This is an explicit remaining G2 defect, not a passing rejection control.
- Commit this coherent birth preservation change locally on layer03; do not
  publish/accept G2 yet. Whole-save atomic restore and complete semantic/native/XL
  observation/continuation remain open. Continue through the existing save/rewind
  owners; Android's state_save_to_memory/state_restore_from_memory and file
  adapter are existing reuse anchors, not permission to enable Android-specific
  policy on desktop. G3-G7 remain open. No merge or private asset publication.

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
