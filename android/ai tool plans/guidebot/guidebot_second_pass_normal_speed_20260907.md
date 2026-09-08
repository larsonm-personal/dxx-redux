# Second GuideBot regression pass at normal engine speed

Planning date: 2026-09-07

Status: planning only. This task reads source and writes this plan. No builds,
engine runs, tests, emulator/device access, or regression regeneration were
performed. Another task is modifying routing code and baselines; recheck the
implementation points below when implementation begins. All checkboxes are
future work. Do not edit android/outstanding_bugs.md, which prohibits agent edits.

## Objective and scope

Keep the existing 160 percent route-confirmation pass and add an independently
reported 100 percent pass over the same complete route corpus. Use the second
pass to find failures that affect a player following GuideBot in the game:
door contact, poor turning, oscillation, premature stopping, lost objectives,
waiting/returning to the player, and unsuccessful recovery or replanning.

There are two required implementation milestones:

1. Establish full-corpus 100 percent regression coverage using the existing
   controller, with independent results, appropriate timeouts, and useful failure
   traces. This immediately closes the speed coverage gap
2. Make that second pass exercise ordinary escort AI and realistic player
   interactions. Explicitly report the remaining sandbox differences. A speed
   change alone does not complete the engine-fidelity portion of this plan

"All routes" means every route-bearing mission target and level in the existing
regression inventory, including secret levels, archive/CD variants, and D1
missions executed through D2. Enumerate missing/unsupported/partial inputs too;
they remain visible coverage gaps. This is not an enumeration of every possible
path through a mine. Individual GuideBot commands and alternate player behaviors
receive targeted scenarios in addition to the full level routes.

Standalone D1 GuideBot support, a general combat-playing agent, multiplayer
ownership testing, and the proposed GuideBot mode redesign are separate work.
Reuse existing enhanced GuideBot behavior without waiting for that redesign.

## Findings from the current source

| Location | Current behavior and consequence |
| --- | --- |
| android/helpers/regenerate_all_guidebot_simulations.ps1 | Defaults TestSpeedPercent to 160; accepts 100 only for native Headless/Desktop without WriteRegression; a 100 percent experiment cannot become its own canonical baseline today |
| android/helpers/guidebot_simulation_regression.ps1 | Generation 4, fixed 60 Hz, seed 1; compact records omit speed/controller identity and retain only rounded objective seconds plus total frames |
| shared/route_confirmation.cpp: speed_up_actor | Multiplies commanded velocity after path steering; 160 percent changes motion relative to physics, turning, doors, and other timers, rather than merely making the host finish sooner |
| d2/main/ai.c: do_ai_frame | route_confirmation_drive_companion returns before ordinary AI processing, including normal escort logic and its scheduling/recovery behavior |
| shared/route_confirmation.cpp: route_confirmation_drive_companion | Forces AIM_GOTO_OBJECT, clears SKIP_AI_COUNT, calls ai_follow_path with visibility 2, and has its own endpoint steering/stopping behavior |
| shared/route_confirmation.cpp: start/before_frame | Difficulty 2; actor at authored player start; frozen invulnerable player; ordinary robots removed; retained bosses/key carriers immobilized; actor enlarged to max(player, GuideBot) radius; reactor countdown paused |
| shared/route_confirmation.cpp: apply_objective_action | Some actions call check_trigger, damage, or player pickup handlers directly; reaching an action point is not proof of ordinary player interaction |
| shared/route_confirmation.cpp: recovery helpers | Confirmation has its own flare/door/frontier recovery and replanning; success can depend on behavior absent from the ordinary escort loop |
| headless/route_confirmation_headless_main.cpp and d2/main/game.c | Both advance through calc_game_time and GameProcessFrame; headless already runs fixed ticks without a wall-clock sleep |
| android/tests/test_obsidian_level9_motion_tolerance.ps1 | Already probes 100/120/140/160 percent, but asserts only arrival at the blue key; later route failure is explicitly outside this test |
| android/helpers/guidebot_simulation_regression.ps1 | Engine failure can be replaced by route_mismatch during normalization, losing the primary failure classification |
| android/helpers/regenerate_all_guidebot_simulations.ps1 | Batch process exit currently fails on infrastructure errors; successful process exit alone does not establish successful route coverage |
| android/tests/test_guidebot_simulation_headed_headless_parity.ps1 | Checks semantic parity for Counterstrike level 1; it does not prove ordinary gameplay parity or identical completion frames |

Paths abbreviated as shared/ and headless/ are under
android/app/src/main/cpp/. These are source findings, not runtime measurements.

## Define 1.0x precisely

- Use the engine's loaded robot speed, turn rate, acceleration/drag, physics,
  collision, and difficulty settings. Apply no extra harness velocity multiplier
- Retain legitimate engine behavior, including mode-dependent speed changes.
  Do not replace mission-specific robot data with a generic movement constant
- Use one simulation-time clock for AI, weapons, doors, triggers, recovery,
  GameTime64, and progression. Keep the existing remainder-based 60 Hz fixed-point
  accumulator so 60 ticks accumulate exactly one engine second
- A headless run may process those ticks as quickly as the CPU permits. It must
  not enlarge FrameTime, skip physics/AI ticks, shorten waits, or force worker
  completion to accelerate the run
- Separate simulation seconds, host elapsed seconds, and presentation pacing in
  reports. Add paced desktop verification to test timing-sensitive integration;
  do not make every corpus run wait in real time unnecessarily
- Audit asynchronous route work: a fast headless clock can starve wall-time-based
  workers relative to simulated deadlines. Use the production completion/adoption
  path, record pending work and its age, and compare paced runs. Do not make the
  live pass secretly synchronous or exclude waiting from game time

## Profiles, corpus scheduling, and persistence

Introduce explicit profiles, separate from Mode (Headless/Headed/Desktop):

| Profile | Purpose | Stored result |
| --- | --- | --- |
| confirmation_160 | Existing accelerated physical route proof | Existing <mission>.simulation.json |
| normal_100 | Normal-speed coverage, upgraded to live escort behavior | New <mission>.1x.simulation.json |

Use a proposed -Pass Confirmation, Normal, or Both parameter. The complete
regeneration workflow must explicitly request Both. Keep focused legacy tests
explicitly on Confirmation unless they are being extended to both passes.
Single-pass runs remain useful for diagnosis; their summaries must say which
coverage is missing. Keep arbitrary speed sweeps diagnostic-only.

For normal_100, record a fidelity field and independent generation:
confirmation_speed_only for the first milestone, live_escort_navigation after
the second. Increment its generation when controller or sandbox semantics change;
invalidate earlier results rather than treating speed-only evidence as live
coverage. Do not migrate old records into evidence they never established.

- Discover/stage mission inputs once, then expand each selected level into both
  profile jobs. Use identical initial content/configuration within each profile's
  declared fixture, isolated processes, and separate writable pilot/save/cache
  directories. Never continue the second pass from the first pass's modified mine
- Include profile in work-item identity, repeat grouping, paths, progress,
  cancellation, resumption, staleness checks, and retained-result lookups
- Attempt normal_100 even when confirmation_160 fails. The useful comparison is
  both pass, only fast passes, only normal passes, or both fail
- Reuse the existing bounded host process pool. Report completed levels and
  completed profile jobs separately; double-counting jobs as levels is misleading
- Full acceptance uses SampleFraction 1.0. If sampling is requested for development,
  choose levels once and run both profiles for that same sample. Mark it partial
- Retain explicit rows for every expected level/profile, including absent assets,
  no route, unsupported activation, stale result, and interrupted/not-run jobs
- Extend atomic incremental writes to the new sidecar. A filtered run may retain
  other current results only when their profile and input contracts match
- Preserve normalized ordering and UTF-8 without BOM. Existing *.simulation.json
  exclusions cover the proposed filename, but audit all corpus/browser consumers
- Snapshot or hash input content at scheduling time and verify it before writing
  checked-in output. If another task changes metadata, reject stale publication
- Continue collecting failures across the corpus, then make a dedicated regression
  gate fail for any required missing/non-ok result. Infrastructure success,
  route success, determinism, and complete coverage must be separate report fields

Baseline identity must include profile, generation, fidelity/sandbox policy,
route-input hash, relevant asset hashes and mount order, engine mode, seed,
difficulty, fixed-Hz policy, speed, and radius/actor policy. Preserve engine
binary/source identity in run provenance, including dirty-source identity where
available, so results remain attributable while routing fixes are in progress.
Changing executable identity requires new validation, not reuse as a fresh run.

## Make the normal pass exercise ordinary gameplay

The existing confirmer can remain the controller for confirmation_160. Reuse
its lifecycle, reporting, and read-only observation where appropriate, but give
normal_100 a small scenario driver that does not steer GuideBot itself.

1. Start a clean local game with pinned settings and loaded mission assets. Use
   the game's deploy/release and end-of-level/NEXT command entry points. Record
   the initial player/GuideBot state and deployment policy
2. Let GuideBot run through do_ai_frame, do_escort_frame, guidebot_route.c,
   ordinary path creation/following, visibility, retry handling, and recovery.
   The confirmation-controller early return must be inactive in this profile
3. Add a deterministic simulated player that follows GuideBot using ordinary
   controls and player physics. A bounded breadcrumb follower can use observed
   GuideBot positions and collision queries; it must not teleport behind him or
   independently solve the entire metadata route, concealing failed guidance
4. Allow the player to approach an objective GuideBot has actually presented,
   aim/fire through the normal weapon path, pick up keys through player collision,
   and physically cross trigger/exit sides. This is how the player assists a guide
   in real play. Do not give GuideBot new powers to finish the fixture
5. Observe resulting world state and normal goal transitions to decide completion.
   Do not rewrite walls, keys, objective indices, or GuideBot position/velocity
   to make a failed step complete
6. Instrument forbidden confirmation interventions with counters. A live-fidelity
   run cannot pass if forced visibility/mode, custom steering, direct objective
   activation, fallback teleport, or confirmation-only recovery was used

The simulated player is a substantial part of this milestone. Give it separate
diagnostics and failure reasons: player follower blocked, unable to aim/fire,
waiting for guidance, dead, or unable to complete an interaction. A broken
follower must neither be blamed on GuideBot without evidence nor converted to
success. Validate it on simple corridors and interaction fixtures before corpus
use. Preserve its inputs so a failed case can be replayed in a rendered game.

Use the actual loaded sizes for player and GuideBot physics in the live pass;
continue using max(player radius, GuideBot radius) for route-clearance decisions.
The current enlarged verification actor remains useful in confirmation_160.
Record the two physical radii and planning radius separately so a radius-policy
change is not mistaken for a speed fix.

Audit every confirmation recovery helper against the production equivalent.
When a helper embodies a needed gameplay fix, implement/share the fix in the
production navigation path and cover it in both passes. Do not copy the whole
confirmation controller into a second implementation or patch individual missions
only inside the test harness. Place reusable new harness code under android/;
keep D2 hooks small and gated, preserving other platform builds.

### Declare the sandbox boundary

The first live-escort corpus pass is a navigation integration test. A deterministic
combat fixture may remove ordinary hostiles and neutralize required combat targets,
with player invulnerability/ammunition setup, to avoid requiring a general combat
agent. Specify each modification in the profile manifest. Keep normal doors,
trigger/restorer behavior, key-drop physics, firing/collision semantics, and object
lifecycles. Player attacks must still reach required targets and their normal
death/drop handlers must run; direct damage/trigger/pickup shortcuts are forbidden
in the live-fidelity result.

Pausing the reactor timer permits long navigation coverage but leaves escape-time
feasibility untested. Record that policy and add targeted unpaused escape tests;
do not silently increase the authored timer or describe a paused run as a valid
timed escape. Terminal hooks may prevent the next level loading only after the
normal engine exit event has been observed for the correct actor.

Add representative scenarios with live moving key carriers, robot obstruction,
normal player damage, and unmodified countdowns. These quantify the gap between
the navigation fixture and a full game. Make pending limitations visible in the
report; full-corpus normal-speed navigation success is not a claim that every
combat, difficulty, or multiplayer situation has been verified.

## Budgets and detection of getting stuck

The existing simulation budget is min(300, 30 + ceil(distance / 30)) seconds;
independent no-progress detection is 60 simulation seconds. Do not reuse the
accelerated travel allowance unchanged or multiply every game timer by 1.6.

For the initial speed-only milestone, use
min(480, 30 + ceil(distance / 18.75)) as a provisional normal-speed travel budget:
scale the empirical travel allowance by 160/100 and retain fixed startup slack.
This is a starting policy, not measured proof or an engine speed constant. Gather
full-corpus timings and calibrate the live-escort budget for player-following and
interaction time before calling it canonical. Record distance validity, estimated
travel, interaction allowance, cap, and effective budget in results.

Missing/partial distance must not imply a tiny valid route. Use a documented
conservative bounded fallback, identify the missing estimate, and report cap
hits. Keep process/startup watchdogs separate from engine budgets; paced runs need
the full simulation allowance plus load/worker/shutdown slack. Audit the existing
engine's 3600-second limit and all native/automation parsers together if changing it.

Use actual displacement, waypoint/portal advancement, state changes, objective
progress, and repeated route-state cycles to distinguish:

- Stationary contact or movement oscillation without traversal
- Repeated frontier/replan loops that reset local progress counters
- Legitimate door opening, pickup settling, player interaction, or return-to-player
- Unfinished asynchronous planning versus a navigation failure
- Overall route budget exhaustion versus infrastructure stall

Keep a bounded semantic-progress watchdog that replanning cannot reset forever.
Do not use straight-line distance alone: valid detours initially move away from
the goal. Never fix a repeatable stall solely by raising the deadline or adding a
harness-only nudge. Treat permitted production recovery as an observable event;
success that relies on repeated expensive recovery should be flagged for review.

## Evidence and comparisons

Preserve raw engine status/problem separately from expected-route comparison and
repeat determinism. A timeout with a different observed objective sequence must
remain visible as both facts. Compare 160/100 outcomes and failure locations, not
identical frames, RNG consumption, or scaled objective timestamps between speeds.

Keep stable objective identities and exact completion frames/ticks in detailed
results. Retain a bounded pre-failure trace of player/GuideBot pose, velocity,
segment, AI mode/visibility, path index/length, semantic target versus frontier,
door/wall/trigger state, contact normals, retry/recovery events, pending planner
generation/age, and player input/action. Diagnostic observers must not mutate
engine state, consume RNG, or trigger replanning.

For speed-only runs, preserve existing expected-step checks. For live escort runs,
record actual action/world-state evidence and prerequisite ordering as well as
route-step correspondence. A legitimate alternate ordering may be diagnosed as
route drift; it cannot be silently accepted based only on reaching the exit, nor
can baseline differences erase completed prerequisite evidence. Every required
objective needs a verified outcome or an explicit missing/unsupported result.

Repeat identical profile/input/seed runs in fresh processes. Compare exact stable
simulation evidence within the same deterministic configuration. Compare desktop
and Android runs semantically first, while retaining exact frame differences for
investigation. Do not compensate for engine nondeterminism in the replay layer.

## Implementation sequence and acceptance

- [ ] Phase 0: Re-audit the current routing revision and enumerate all simulation
  interventions, ordinary equivalents, and retained sandbox assumptions. Specify
  profile schema, coverage manifest, and deterministic initialization contract
- [ ] Phase 1: Add profile-aware scheduling, native speed/config propagation,
  independent persistence, budgets, reporting, and strict regression gating.
  Make complete regeneration run both profiles. Run every route at 100 percent
  and store fidelity=confirmation_speed_only; repeat the initial full baseline
- [ ] Phase 2: Add passive diagnostics and investigate normal-speed-only failures.
  Fix movement/recovery in production code and preserve each minimal failing
  scenario. Re-run both profiles over affected routes, then the full corpus
- [ ] Phase 3: Implement and verify the simulated player and ordinary escort path.
  Remove confirmation interventions from normal_100, enable action evidence and
  intervention assertions, and bump its generation to live_escort_navigation.
  Re-run the complete corpus; this phase is required for the fidelity objective
- [ ] Phase 4: Verify representative fixed-step desktop/Android parity and paced
  gameplay, then targeted 30/60/120 Hz and bounded frame-jitter scenarios. Keep
  each timestep schedule a separately identified test configuration. Validate
  live-object, player pause/return, and unpaused-countdown scenarios
- [ ] Phase 5: Integrate both statuses into the simulation browser and regeneration
  summaries, document limits and reproduction commands, and regenerate/review
  both complete baselines. Preserve unresolved failures rather than weakening
  checks until the report is green

Implementation validation, to run only when build/device work is available:

- Extend schema, runner, timeout, and top-level regeneration contract tests for
  profile isolation, matching selections, speed propagation, invalid profile/speed
  rejection, filtered writes, staleness, interruption, and missing-result gating
- Extend Obsidian 9 to require complete normal-speed route coverage separately
  from its existing blue-key checkpoint. Include narrow portals and wall bounce
- Exercise Counterstrike 6/12/20 doors, 10/17 key carriers and pickups, 23 route
  loops, 24 final boss/key/exit behavior; Obsidian 3/6/12/13; Castaway restored
  triggers; long First Strike routes under D2; and Vertigo/Enemy Within cases
- Discover the exact mission targets from metadata at implementation time. The
  current RoutingDevelopmentSet helper contains only four missions, so it does
  not substitute for the requested Vertigo/Enemy Within or full-corpus coverage
- Include fail-detection fixtures where the live guide remains stuck despite
  repeated replans, cannot traverse a grate, stops above an uncollected key, or
  relies on a forbidden harness intervention. The gate must reject each case
- Validate baseline normal follow, deliberate player pause/loss of sight/return,
  and repeated NEXT/exit command sequences without forcing AI visibility
- Run relevant Windows/D2 CMake tests and Android tests serially once resources
  are available; check D1 build compatibility if shared interfaces change. Run
  one scoped code-quality invocation over the changed implementation files

The final full-corpus report must list every expected profile/level identity,
whether it ran, primary outcome, route comparison, fidelity, repeat result, and
artifact location. Acceptance requires real 100 percent configuration, ordinary
escort execution for the live milestone, no forbidden interventions, and no
missing coverage disguised as success. Existing mission failures may remain as
explicit findings while the harness is introduced; reliability completion still
requires fixing and rerunning them, not changing their labels.

## Main files to change later

| Area | Files |
| --- | --- |
| Profile, results, budget, corpus scheduling | android/helpers/guidebot_simulation_regression.ps1; android/helpers/regenerate_all_guidebot_simulations.ps1 |
| Complete workflow and browser | android/regenerate_all_regression_data.ps1; android/helpers/watch_guidebot_simulation.ps1 |
| Engine session, observation, profile dispatch | android/app/src/main/cpp/shared/route_confirmation.h and .cpp; route_confirmation_result.cpp; new small scenario/player harness files beside them |
| Configuration in all execution modes | android/app/src/main/cpp/headless/route_confirmation_headless_main.cpp; shared/route_confirmation_desktop.cpp; shared/game_automate.cpp; introspection fields as needed |
| Thin production hooks and real fixes | d2/main/ai.c; escort.c; guidebot_route.c; aipath.c; game.c; normal interaction handlers only where evidence requires it |
| Reusable integration coverage | android/tests/test_guidebot_simulation_*.ps1 and relevant mission regressions; android/game_scripts/*.jsonc for ordinary in-game scenarios |

Coordinate with guidebot_optimization_and_modes_20260907.md for ownership and
mode changes, and with ../2026-09-07-guidebot-route-videos.md for trace reuse.
Neither larger project is a prerequisite for adding normal-speed coverage.
