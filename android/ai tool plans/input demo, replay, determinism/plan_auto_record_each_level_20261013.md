# Automatically record demos from level start

Status: stopped at the user's request on 2026-10-13; known Guide-Bot replay failure remains

The latest instruction supersedes the broad acceptance backlog below: finish the
features already implemented, pass their targeted tests, perform one adversarial
review, and stop. Do not add another feature or a broad lifecycle test suite.
Earlier unchecked items remain historical planning, not an active work queue.

## Request and recommended UI

Add a persistent, default-off checkbox above "Record per-frame state" in the
existing "Newly-Recorded Demos" section:

- Label: "Record a demo for each level"
- Description: "Automatically record from the start of each new single-player level, without an embedded save game"
- Supporting note: "Loaded saves and restored secret levels are skipped"

Keep the existing recording indicator and "Stop demo" action. Stopping a demo
ends only the current recording; automatic recording remains enabled for the
next newly initialized level. A later manual "Start demo" still uses the
existing checkpoint behavior when needed

Show the staged recording retention policy in this section: "The newest 10
recordings are kept. Save or add recordings to a file set to keep them"
Verify the actual retention scope before wording it as a global total: the
native cap applies to the current PhysFS recording directory

The user described this as Game Preferences. In the current tree the section
is implemented in AdvancedSettingsPage.kt and reached through showAdvancedPage
in SetupActivity.kt. Put the checkbox beside the existing section; moving the
whole demo browser or duplicating it into Game Preferences is separate scope

The preference applies to D1, D2, and D1 missions played in D2. Keep the existing
per-frame state option independent and default-off. Removing the save blob
reduces size, but does not remove RNG traces, classic .dem sidecars, or optional
diagnostics; do not promise a particular total size reduction

## Findings in the current code

- InputDemoPrefs.kt defines the per-frame-state launcher preference
- MainActivity.kt applies it through JNI in android_input.c
- SetupActivity.kt routes this particular key to launcher_prefs for automation
- input_demo_newdemo_shared.c owns quick recording, automatic naming, staging
  into demos/new, sidecar handling, and trimming to INPUT_DEMO_NEW_LIMIT (10)
- Both gameseq.c implementations already have PlayerCfg.AutoDemoSp/AutoDemoMp
  hooks at the end of StartNewLevelSub, before game() is created if necessary
- Those hooks use classic autorecord semantics. They are not equivalent to
  Android quick recording: output location, naming prompts, stop handling, and
  recording ownership differ
- input_demo_prepare_recorder_settings currently decides whether to capture a
  save using ThisLevelTime and the player spawn pose. These are not sufficient
  evidence that a world is newly initialized rather than restored
- input_demo_recorder.cpp emits start_mode=new_level when no checkpoint exists
- input_demo_start_shared.c replays new_level via StartNewGame(recorded_level)
  and applies recorded player preferences, not carried campaign inventory
- Per-frame state is diagnostic comparison data, not a mechanism to restore
  player inventory on the first replay frame
- Existing endlevel.c, switch.c, state.c, game.c, and D1-in-D2 transition paths
  stop recordings at several boundaries; these need an explicit lifecycle audit

## Behavior contract

| Event                                                                | Proposed behavior                                                                                                |
| -------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| New game or direct start at a selected level                         | Start before the first gameplay input/simulation frame                                                           |
| Normal progression into a newly initialized level                    | Finalize the old recording before teardown; start a separate recording with the actual carried player state      |
| Manual Stop demo                                                     | Save the current partial recording; do not restart it in the same level instance                                 |
| Manual Start demo later                                              | Start a normal manual recording, with a checkpoint if required                                                   |
| Death and respawn in the existing world                              | Do not create a fresh-level demo or split merely for respawn; validate existing recording/replay behavior        |
| Full restart that genuinely reconstructs a fresh world               | Start a new recording once; distinguish this from restoring a level-start save                                   |
| Load/resume/rewind or restart through save restoration               | Stop before state replacement; skip automatic start and wait for the next fresh level                            |
| First visit to a fresh secret level                                  | Include only after bootstrap and transition replay tests prove reconstruction works                              |
| Return to or revisit a restored D2 world                             | Skip; do not label restored state as new_level or silently embed a checkpoint                                    |
| Replay, classic demo playback, multiplayer, metadata/simulation jobs | Never start this automatic recorder                                                                              |
| Disable the preference                                               | Apply before subsequent level starts; if live preference updates are supported, let the current recording finish |
| Start/finalization failure                                           | Report a concise failure through the existing UI/log path; keep gameplay usable and do not retry every frame     |

No new naming prompts, dialogs at level boundaries, or automatic file-set
installation. Continue using the existing staged demo browser and naming scheme
Do not silently replace an already active recording. Define precedence with the
existing classic autorecord option: this Android preference owns an eligible
fresh single-player start; otherwise preserve classic behavior without bypassing
the exclusions above

## Main correctness work: carried state without a full save

The checkbox and level hook are small changes. Campaign continuation is the
substantial part: replaying level 2 via StartNewGame(2) does not reproduce the
ship, counters, and initialization context carried from level 1

1. Compare fresh StartNewGame, normal progression, fresh secret entry, and
   restored-world paths in both engines and D1-in-D2
2. Establish the recording/replay boundary and capture inputs to level
   initialization before they are consumed. Audit RNG used during initialization,
   game clocks, object allocation/signatures, and gameplay-affecting persistent
   globals, in addition to player state
3. Add a bounded level-start metadata record if needed, using engine-owned
   capture/apply helpers. Candidate fields include inventory, selected weapons,
   energy/shields, lives/score/campaign counters, timed effects, and D2-specific
   charge/equipment state. Determine the exact set from initialization data flow
   and replay evidence, not by blindly serializing an entire player structure
4. Apply bootstrap fields at the correct phases around normal level initialization
   and before frame zero. Preserve the recorded RNG contract; simply applying the
   first gameplay RNG value after initialization is not proof that the initial
   world matches
5. Keep map objects, doors, AI, and other fresh world state reconstructed by the
   engine. Do not grow a second arbitrary-world save serializer
6. Validate ranges and game-specific fields in the parser. Preserve existing
   checkpoint demos and established new_level semantics for fixtures without the
   new metadata; use an explicit format revision if semantics require it

If an entry path requires restoration of an already modified world, it is not
eligible. Never achieve smaller output by discarding required reconstruction
state, resetting the player's live inventory, or replay-specific compensation

## Implementation sequence

- [ ] Establish the level-start bootstrap and prove a carried-inventory level
      replay in D1, D2, and D1-in-D2 before connecting the user-facing checkbox
- [x] Add PREF_DEMO_RECORD_EACH_LEVEL in InputDemoPrefs.kt, default false; wire
      launcher persistence, MainActivity JNI application, and automation preference
      routing in SetupActivity.kt following the existing demo preference pattern
- [ ] Add the checkbox and concise supporting text to RecordedInputDemosSection,
      including controller focus behavior and an accurate retention note
- [x] Add a shared explicit start operation for automatic fresh-level recording
      that cannot toggle an active recording off and cannot fall back to a checkpoint
      Reuse quick-record naming/staging/finalization and active-state reporting
- [ ] Add narrow hooks in both engines that distinguish fresh initialization
      from restoration, replay, and noninteractive simulation. Track a level instance
      or consume a one-shot start event so manual stop cannot cause an immediate restart
- [ ] Audit and adjust stop boundaries, including terminal result capture before
      next-level initialization, secret transitions, save restore, game over, quit,
      restart, and existing classic autorecord interaction
- [ ] Verify no partial or failed start leaves classic recording active, stale
      temporary files, an incorrect UI state, or misleading staged output
- [x] Document any new bootstrap fields in the existing input-demo schema notes

Keep shared policy and format work under android/app/src/main/cpp/shared with
minimal game-specific hooks. Guard Android UI/preference behavior appropriately;
desktop replay of generated recordings must continue to work

## Validation and acceptance

Add a reusable Android integration runner with owned automation scripts, extending
the existing input-demo test helpers where practical. Run devices serially

- Default-off records nothing automatically; enable survives launcher recreation
- Launch, play, manually stop, and replay a fresh level in all three game modes
- Complete a level with non-default weapons/ammo/shields/score, then record and
  replay the next level independently, without the previous demo or save files
- Compare initial and final state plus RNG behavior; include a real gameplay
  interaction after the transition, not only idle frames or a header assertion
- Assert start_mode=new_level, no checkpoint record/payload, and no demo checkpoint
  capture invocation. Unrelated game autosaves are outside this feature
- Repeat with per-frame diagnostics off (normal use) and on; bootstrap must work
  independently of diagnostic snapshots
- Check manual stop suppression, unique names on repeated level attempts, clean
  finalization on exit, and no naming prompts
- Check save/resume/rewind, respawn, restart, secret first-entry/return/revisit,
  replay with preference enabled, multiplayer, and existing classic autorecord
- Check retention across enough recordings to exceed the cap and grouped sidecar
  deletion without affecting recordings already saved into a file set
- Check a write failure and clean retry on a later eligible level
- Measure representative artifact sizes and finalization/recording overhead;
  do not add a new sidecar toggle or retention redesign without demonstrated need

Build both Android engines and Windows host replay targets; run relevant recorder,
parser, replay tests and the new integration runner to completion. Register its
top-level entry in test_suite_coverage.ps1, give owned JSON scripts _standalone:
false and _owner, set the master timeout if needed, and run both automation catalog
validation tests before committing test additions. Run scoped code quality on
changed implementation paths and fix new compiler warnings

## Scope and current workspace

Existing unrelated modifications include SetupActivity.kt,
game_automate.cpp, test runners, and other plans; preserve them during implementation
Do not edit android/outstanding_bugs.md, which explicitly prohibits tool edits

## Implementation evidence, 2026-10-13

- Added optional typed level_start header metadata: player input to initialization,
  initialization RNG, and persistent firing/cadence/homing runtime values
- Added fresh-entry hooks in StartNewLevel and first-visit StartNewLevelSecret;
  save restore uses StartNewLevelSub and does not trigger the new preference
- Added default-off launcher checkbox, JNI plumbing, and automation preference key
- Windows D1/D2 builds passed; first Android universal build passed
- First D1 emulator recording: exact final state matched desktop replay; RNG
  comparison exposed absolute desktop vs relative Android source paths
- Normalized RNG source paths at emission; first D2 emulator recording then passed
  strict final-state and RNG comparison on desktop
- Added codec round-trip/domain tests; six D1 input-demo unit tests passed
- Integration runner is being extended to actual level progression with carried
  inventory and D1-in-D2. Current tests use an isolated emulator on port 5556,
  because an unrelated overnight followup runner owns emulator-5554
- Existing rewind supports truncating the recorded input/RNG timeline back to the
  restored frame. Preserve that behavior when the target belongs to this recording;
  do not unnecessarily stop a reconstructible demo. Save-load suppression remains
  required, and rewinds outside the recorded timeline still need review

Remaining: run carried-state integration, finish edge-case coverage and format
validation, document the final bootstrap contract, and audit all acceptance gates

## Requested extension: record the all-weapons cheat

Status: cheat event support implemented; cross-platform gameplay validation in progress

The user wants compact recordings made with public shareware game data that can
exercise all weapons supported by that data. Record successful cheat actions as
small typed direct commands in the existing per-frame events array. Do not embed
a save or depend on per-frame diagnostic state. Retain the current cheat UI and
keyboard entry paths; both converge on FinalCheats in each engine

### Implementation

- Extract the weapon-grant effects from each engine's FinalCheats into a callable
  engine operation, used by both live cheat handling and replay. Keep game-format
  and shareware rules in the engine; keep event encoding/decoding and dispatch
  plumbing in shared input-demo code. Do not replay text through the keyboard
- Add typed direct commands for the accepted weapon grant variants: D1 scourge,
  D1 bigred, and D2 honestbob. Preserve variant identity rather than treating the
  distinct D1 grants as equivalent. Use a restricted identifier/enum with strict
  validation, not an arbitrary command string evaluated by the engine
- Record D1 cheat enablement separately: gabbagabbahey can reset score and disable
  scoring on an earlier frame than the weapon grant. The touch UI injects this
  prefix automatically when needed. Playback should not need a user to enable
  cheats or open the cheat menu
- Preserve all effects: cheat-enabled/variant state, score reset, normal scoring
  and bonus suppression, weapon ownership, ammo refill, energy, laser upgrades,
  quad lasers, weapon-info refresh, and existing HUD/audio/classic-demo behavior.
  The weapon cheat does not grant shields or D2 accessories; retain those rules
- Stage events independently of diagnostic recording and replay in the existing
  ordered direct-command phase before weapon use. Audit the actual live event
  timing against recorder frame boundaries, including cheat plus weapon select
  or fire in one frame, pause/menu entry, repeated grants, and end-of-recording
  behavior. Replay must not record the command again
- Add the persistent cheat-enabled state to level-start metadata and audit any
  variant state needed by these actions. A next-level demo must retain cheated
  inventory and scoring behavior without replaying an earlier level's cheat
- Reconcile weapon-flag codec bounds with actual engine values: current D1 bigred
  writes 0xff and D2 honestbob writes masks derived from 0xffff, while new
  level-start validation currently caps weapon flags at 1023. Choose a documented
  engine-consistent representation; do not reject valid cheated inventory or
  silently change live state solely to fit the recorder
- Unsupported or malformed required commands must fail replay clearly. Continue
  rejecting live cheats in multiplayer. This extension does not claim support
  for unrelated cheats such as invulnerability, rapid fire, warp, or robot changes

### Shareware behavior and validation

Native D1 weapon selection explicitly blocks registered-only weapons when
PCSharePig is true. D2 honestbob explicitly excludes several weapons/ammo when
Piggy_hamfile_version < 3. Preserve these restrictions: all available weapons is
the target, not references to unavailable models, sounds, or weapon definitions.
Test genuine shareware assets, not shareware levels loaded with retail data.
Audit D1-in-D2 separately because its data and weapon semantics differ

- Record from fresh level start, invoke the existing all-weapons UI and keyboard
  paths, select and fire each available weapon, stop, and replay independently
- Cover native D1 standard/all variants and D2 honestbob with actual shareware
  data, plus D1-in-D2 where that data combination is supported. Use retail data
  as separate coverage for the full inventory; do not require it for the public
  shareware regression set
- Assert exact ammo/resources/laser flags after grant, score reset and subsequent
  scoring suppression, and that unavailable weapons stay unusable
- Repeat the grant after spending ammo; test same-frame ordering, first-frame
  and final-frame activation, and diagnostics both off and on
- Continue into another level and replay that level alone; check carried cheated
  inventory, cheat-enabled state, and the weapon-flag codec edge case above
- Compare final gameplay state and RNG traces across Android recording and desktop
  playback; assert no checkpoint payload and measure event-size overhead
- Extend existing codec/recorder/direct-command tests for validation and ordering;
  add owned automation coverage through the existing auto-level integration
  runner, then run required builds, scoped formatting, and catalog checks

### Implementation evidence for the extension

- Shared typed cheat commands preserve activation order independently of optional
  diagnostic snapshots. Native D1/D2 cheat handlers and replay call the same
  engine-owned weapon-grant operation; live keyboard and touch paths converge here
- Added enablement/weapon-cheat state to the level-start header and widened
  ownership-mask validation to the actual engine storage widths
- Moved new-game cheat reset before level initialization, so replay restoration
  is not cleared immediately afterward
- Fresh authored level loads now reset object identity before assigning level
  object signatures. Earlier level-2 RNG traces differed only in signatures
  carried from the previous mine; runtime comparison of the fix is still pending
- Added shareware-aware integration scenarios and cheat event ordering/validation
  unit tests. Six final D1 input-demo unit tests passed, including these cases
- Scoped mixed-language formatting passed. Automation catalog validation passed
  with 95 standalone JSON tests, 392 support scripts, and 242 PowerShell tests
- The separate full-suite catalog test timed out in its child invocation while
  native builds were running; rerun after the builds finish
- First public D1 cheat integration is running on emulator-5556. Its initial
  launcher readiness check timed out; the existing runner is recovering that
  emulator before retrying. This is not yet evidence of gameplay success or failure

### Runtime evidence after recovery

- Both final Windows engines and Android assembleDebug built successfully. All
  six D1 and eight D2 input-demo/RNG unit tests passed
- The full-suite catalog test passed on retry: 341 top-level entries and 392
  support scripts
- Public D1 cheat integration passed both independent levels with exact final
  state and simulation RNG traces. Recorded enable, scourge, bigred, and repeated
  bigred in order. Files were 14,412 and 9,590 bytes, excluding existing sidecars
  Evidence: temp/auto-level-demo/cheats-shareware-d1-run.log
- Public D2 cheat integration reproduced the final player state, but simulation
  RNG diverged at frame 1, before the first cheat at frame 25. Both rendered and
  no-present desktop replay reproduce the same failure: Guide-Bot object 28,
  signature 29, robot id 33 creates an extra randomized path. Investigate engine
  initialization/path scheduling; do not compensate by changing replay RNG
  Evidence: cheats-shareware-d2-run.log and d2-cheat-rendered-replay.log
- A separate D2 shareware inventory run passed level 1 RNG/final state but diverged
  in level 2 at frame 50, again in Guide-Bot path creation. Thus the failure is not
  confined to the cheat command
- Added RecordState integration mode, using typed boolean launcher commands, and
  corrected button release steps to use pressed=0. Weapon exercises now have
  explicit waits while holding fire. These test refinements need final formatting
- Diagnostic capture d2_d2demo_level1_20261013_091449 first differed only in the
  AI-local hash at frame 30, while simulation RNG matched. Replacing object 29's
  cosmetic next_misc_sound_time in an offline hash calculation with the recorded
  FX-derived value reproduced the expected hash exactly (2979289480). Removed
  cosmetic sound deadlines from the simulation hash; expanded object traces
  retain them. This last source change needs a fresh build and recording

Still required: account for the planned Guide-Bot exclusion, validate D1-in-D2 and secret
entry/return paths, complete remaining lifecycle/retention/error cases and cheat
weapon coverage, rerun checks after final changes, and complete the acceptance
audit. Do not infer completion from D1 success

### Diagnostic verification and route-cache investigation

- Rebuilt both Windows engines and Android after excluding cosmetic AI sound
  deadlines from the simulation hash. Six D1 and eight D2 focused native tests
  passed. The comparison-policy test passed (64 embedded Python tests), as did
  both catalog validators and scoped formatting before the new cache fixture
- D2 public shareware cheat recording now passes both independent levels with
  diagnostics enabled: 340 and 82 frame states, plus 319 and 523 comparable
  simulation RNG calls. Evidence: diagnostic3-shareware-d2-run.log under
  temp/auto-level-demo. This validates the hash correction and cheat effects,
  but does not resolve the earlier Guide-Bot failure
- ReplayDebugLog now adds -safelog so the wrapper's normal post-result process
  termination does not discard buffered diagnostics. The existing failing
  recording still fails RNG as expected, but its retained gamelog is now
  835,356 bytes instead of empty. It shows frame-1 Guide-Bot goal selection
  followed by extra path creation. Evidence: d2-safelog-wrapper-check.log
- Original recording began at 09:07:50.082; its asynchronous route metadata
  reported completion at 09:07:51.835. Android LoadLevel prepares cached route
  metadata without expensive planning, while desktop LoadLevel computes it
  synchronously. Existing record/replay guards prevent later cache adoption,
  but do not preserve this initial readiness difference
- Temporarily moved only emulator-5556's D2 route cache aside and restored it
  in finally after a fresh public inventory recording. Level 1 passed all
  55 frame states because launcher analysis completed before play. Level 2
  diverged first at frame 52, Guide-Bot object 55/signature 56. Evidence:
  cold-cache-shareware-d2-run.log. The original warm cache is restored; the
  newly generated test cache is retained as route-cache-auto-level-cold
- Added ColdRouteCache to the integration runner and owned scripts. Its
  automation fixture disables persistent route-cache use only between stopped
  recordings, then asserts calculating readiness after fresh level-2 entry and
  re-enables cache use before leaving. This avoids relying on worker timing to
  reproduce the missing initial state. Its 54 Android steps pass, including an
  explicit calculating-readiness assertion in level 2. Evidence:
  forced-cold-shareware-d2-run.log. Independent playback of its second level
  first differs at frame 51 in Guide-Bot object 55's AI/pose hashes, before the
  frame's starting RNG state differs. This reproduces the same failure class
  with readiness asserted rather than inferred from asynchronous completion logs
- The fixture's first Android build found a missing recorder header; added the
  include; Android assembleDebug then passed. The first runtime attempt exposed
  a test-step placement error (cache reset before the launcher entry); moved that
  reset to the intended post-recording position before the successful 54-step run
- Final scoped mixed-language formatting and diff whitespace checks pass. The
  updated catalog checks pass (95 JSON, 392 support, 242 PowerShell; full-suite
  integration 341 top-level entries). No route-state production fix has been
  applied yet; the optional ColdRouteCache replay case is expected to fail until
  the initial route state is represented

Superseded implementation direction (see scope update below): capture initial canonical route availability and
the available canonical route plus plan summary in level-start metadata, restoring
them at the normal route initialization point before live Guide-Bot decisions.
Preserve pending, failed, partial, and complete states; do not assume every route
is complete or force synchronous expensive planning while recording. Use explicit
portable field encoding (the existing guidebot save-field serializers provide a
model), bounded decompression and validation, not native cache struct bytes or a
full world/save checkpoint. The public D2 level-1 native cache record is 78,232
bytes, but compresses to 455 bytes / 608 base64 bytes, so preserving this input
can remain compact. That size is evidence for feasibility, not a portable format.
Also audit live unexplored-route budgeting separately: it has Android-only
wall-clock budgets, but the default compiled route selector ignores that budget,
so it is not evidence for changing scheduling to fix the present failure

### Guide-Bot scope update

The user plans to exclude Guide-Bot from demos entirely while it remains under
active development. Initial route serialization is therefore out of scope.
Removed the experimental route payload, its codec/test target, and the optional
cold-route automation fixture. The earlier investigation remains historical
evidence of why simply omitting Guide-Bot diagnostic hashes is insufficient:
its live behavior also consumes simulation RNG and can affect other objects.

User clarification: leave Guide-Bot behavior separate; the user will keep it
disabled while recording. Do not add demo-specific removal, disabling, or
routing-state preambles. Guide-Bot-dependent D2 recordings can still diverge;
do not classify them as successful by filtering hashes or RNG traces.
Keep compact level-start inventory, supported weapon-cheat events, and the
independent correction to cosmetic AI sound deadlines in simulation hashes.

Scope-cleanup validation: both Windows engines and Android assembleDebug pass.
Six focused input-demo tests pass per engine (12 total). Scoped mixed-language
formatting and whitespace checks pass. The automation catalog validates 95
standalone JSON tests, 392 support scripts, and 242 PowerShell tests. The separate
full-suite catalog timed out during concurrent builds, then passed on retry
with 341 top-level entries and 392 support scripts. Evidence is under
temp/auto-level-demo/guidebot-scope-* and *-build-guidebot-scope.log. These checks
validate removal of the experiment, not production Guide-Bot exclusion or the
remaining broader feature acceptance cases listed above.

### D1-in-D2 replay and expanded weapon coverage

- Fixed the owned scenarios to launch the d1-in-d2 target and resolve the public
  D1 mission title. Previously they selected the D2 campaign in shareware mode.
  Export checks now assert the engine and built-in mission identity.
- Added -D1Mission to the desktop replay runner for D2-native recordings using
  D1 assets, without cross-engine translation. The built-in descent mission is
  inferred automatically. Added a launch-policy assertion and README guidance.
- Public D1-in-D2 inventory recording passes both levels independently with
  diagnostics off: 7,273 and 8,834 bytes excluding sidecars. With diagnostics on,
  all 52 and 70 frame-state records match; simulation RNG comparisons match
  98 and 298 calls. Logs: shareware-d1-in-d2-run.log and
  diagnostic-d1-in-d2-run.log under temp/auto-level-demo.
- The comparison-policy test passes, including the native D2/D1-data launch case
  and its existing 64 Python checks.
- Expanded the cheat scenario to select and fire secondary weapons and assert
  ammo consumption. Fixed-delay firing was insufficient for the shared rearm
  timer, so it now waits for observed consumption with a five-second bound.
  Native D1 shareware rejects smart/mega selection despite cheated inventory
  flags; preserve that existing restriction and exercise the three supported
  secondary weapons. D2 shareware exercises its seven supported secondaries.
- Public D1 expanded cheat recording passes both levels with exact final state
  and RNG. Files: 33,322 and 8,567 bytes excluding sidecars. Evidence:
  secondary3-cheats-d1-run.log.
- D2 passes all 153 Android scenario steps but its expanded level-1 replay
  diverges at frame 218: actual adds an awareness RNG call, while expected's
  next RNG use is Guide-Bot at frame 221. Final shields and position also differ.
  This is not enough to attribute the cause to Guide-Bot; a diagnostic repeat
  is running to locate the first state change. Evidence: secondary-cheats-d2-run.log.
- Final scoped formatting passes. Automation catalog validates 95 standalone
  JSON tests, 392 support scripts, and 242 PowerShell tests. Full-suite catalog
  checks hit the 45-second child timeout; the direct normal catalog command
  subsequently succeeds in 21.3 seconds. A final serial retry is pending.

Remaining acceptance work includes the new D2 secondary divergence, fresh secret
entry and restored return/revisit, lifecycle/default-off/error/retention cases,
retail full-inventory coverage, UI/controller verification, and the final audit.
Guide-Bot behavior remains separate; the user will keep it disabled. Do not add
special handling, weaken trace comparisons, or restore route serialization.

### Secondary-input scheduling investigation

- The original failing D2 recording reproduces the extra awareness RNG call
  in ReadControlsReplayPostFrame at game time 831389, immediately before the
  next frame releases secondary fire. Live weapon processing runs on input
  events, whereas replay guesses both pre-frame and post-frame calls. This is
  evidence of a scheduling mismatch independent of the next Guide-Bot RNG call.
- Move weapon/item consumption to one simulation-frame boundary shared by live
  play and replay, and remove the replay-only post-frame and release-marker
  compensation. Record pending gameplay pulses before consuming them; stage
  only rear-view and automap pulses consumed by the event handler. Validate
  with fresh recordings, including press/release and rearm boundaries.
- Full-suite catalog final retry passed: 341 top-level entries and 392 support
  scripts. The diagnostic repeat passed its first level; its second level
  failed and still needs examination. A diagnostic repeat passing does not fix
  the original timing-dependent failure.

The scheduling change now calls do_weapon_n_item_stuff immediately after frame
capture in both GameProcessFrame implementations. Removed the replay-only
post-frame call and release-marker heuristic, along with event-driven weapon
consumption. UI pulse staging now includes only rear-view/automap; gameplay
pulses remain pending for frame capture. Scoped formatting passed; builds and
fresh recording/replay validation are in progress.

The diagnostic repeat's level-2 failure is isolated: all 74 frame-state records
match except one diag.fireball_changed_prev_hash at frame 30. The reset function
invalidates hash history but leaves the backing arrays populated; slots without
a fireball at the new level's first frame retain previous-level data after the
history becomes valid. Correct the diagnostic history reset and verify with a
fresh multi-level diagnostic recording; do not filter this comparison away.

Scheduling validation so far: both Windows engines and Android assembleDebug
pass, along with six focused input-demo tests per engine. The new public D2
capture passes all 153 Android steps; its level-1 replay passes exact final state
and RNG after exercising all seven supported secondary weapons. Its independent
level-2 replay fails at frame 50: actual RNG next enters Guide-Bot object 55's
create_random_xlate, while the recorded next call is an awareness event. Preserve
this failure evidence and the Guide-Bot scope boundary. Files are 23,658 and
8,427 bytes excluding sidecars; log: weapon-frame-d2-run.log. D1 validation is
running. Do not mark the complete feature validated from the level-1 pass.

D1 scheduling validation passed all 120 Android steps and both independent
desktop replays with exact final state and RNG. Evidence:
temp/auto-level-demo/weapon-frame-d1-run.log. All formatter, build, unit-test, and
runtime processes from this scheduling validation have exited. The diagnostic
history correction is identified but not yet applied. Fresh D1-in-D2 validation
after the scheduling change and the broader acceptance cases remain pending.

### Diagnostic history and secret transitions

Previous goal turn made progress: unified live/replay weapon scheduling and
verified fresh D1 and D2 level-1 recordings. The active goal remains incomplete.

- Cleared backing diagnostic hash arrays when resetting recording history, so
  object slots absent from frame zero cannot inherit previous-level hashes.
- Added the owned test_auto_level_secret.jsonc scenario and -Secret runner mode.
  It uses a small automation fixture to prepare the first authored secret
  entrance, then calls the normal entry/exit functions with their modal UI.
  It records the fresh secret level, exits while recording, and checks that
  restoring the base and revisiting the saved secret do not start a recording.
  Replay checks require independent new_level files with no checkpoint payload.
- Applied the existing regression_demos/README.md recording policy: general D2
  scenarios choose Original routing through normal launcher preferences and
  assert the exported mode is zero. Historical Enhanced failures remain evidence;
  no playback override, Guide-Bot removal, or route payload was added.
- Scoped mixed-language formatting passed. Both Windows engines built. Android
  packaging and focused native tests are in progress. The automation catalog
  validates 95 standalone JSON tests, 393 support scripts, and 242 PowerShell tests.
  Runtime checks for the new changes and the full-suite catalog remain pending.

Both builds and all 12 focused native tests passed after the history reset and
secret fixture changes. Both catalog validators passed (341 top-level entries,
393 support scripts). The secret test reached fresh level -1 but recording
cancelled with an invalid frame-time error; the D1-in-D2 diagnostic attempt then
reproduced the same failure immediately after level 1 loaded. These are runtime
failures, not passing validation: secret-first-run.log and
history-d1-in-d2-run.log under temp/auto-level-demo.

Added precise failure logging and rebuilt. The repeat identifies frame 1 with
ft=94110 (about 1.44 seconds); the existing codec capped frame time at 65536 even
though the engine had simulated the longer frame. Extend the codec to the full
positive signed 32-bit engine fixed-point domain, retaining zero/negative and
integer-overflow rejection. The codec test now round-trips the observed value,
the former boundary and its successor, and the storage upper bound. No live or
replay clock is clamped. Also stop the paired quick/classic recording after an
input-recorder error instead of leaving an orphan classic recorder running.
Scoped formatting passed; fresh builds and runtime verification are in progress.

Slow-frame builds passed on both Windows engines and Android, and all 12 focused
native tests passed. The next run retained recording through slow frames but
hit a test wait bound (25 of 45 frames in 15 seconds). Extended frame-count waits
to 60 seconds without reducing their required counts or changing game clocks.

The retry completed all native secret lifecycle checks: fresh entry records,
exit finalizes, restored base and secret revisit remain unrecorded. It produced
a 6,950-byte level-1 demo (50 frames, maximum ft=227344) and a 7,420-byte secret
demo (62 frames, maximum ft=165216), both without checkpoints and with Original
routing. The secret demo independently passes exact final-state and RNG replay:
temp/auto-level-demo/secret-independent-replay.log. The top-level test still
failed its final launcher wait because that phase requires field/value syntax,
not the native expectation-object syntax. Corrected the owned script and will
rerun the complete runner. Evidence: secret-slow-frame-retry-run.log.

A rendered replay probe then timed out before loading the secret mine. Unlike
normal-level startup, StartNewLevelSecret did not consume the replay intro-skip
flag, leaving a modal Secret Exit message. Added that existing flag to the
secret startup presentation guard; normal secret entry still shows its dialog.
Scoped formatting passed; D2 host/Android rebuilds and rendered verification are
pending. Probe evidence: secret-visual-probe.log and its retained failure folder.

### Bounded wrap-up and adversarial review

The user requested completion of the implemented features and one adversarial
review, then a stop. The broader lifecycle, retention stress, controller UI,
retail weapon matrix, and performance measurement backlog is deferred. No new
Guide-Bot behavior or routing-state payload is part of this work.

Reviewed the fresh-entry/restore distinction, manual-stop suppression, classic
recording ownership and error cleanup, replay bootstrap ordering, metadata bounds,
cheat parsing/order/refill/scoring, shared live/replay weapon timing, object identity
reset, and the independence of diagnostic data. No remaining blocking finding was
identified in that review. Restored displaced upstream comments. This is a source
review plus targeted integration evidence, not a claim that the deferred matrix
was exercised.

Final validation:

- Android assembleDebug passes (secret-intro-android-build.log); subsequent engine
  edits restore comments/whitespace only
- Both Windows engines build (wrap-windows-build.log)
- All six focused native input-demo tests pass in each engine, 12 total
- Replay comparison policy passes all 64 Python checks (wrap-comparison-policy.log)
- Both catalogs pass: 95 standalone JSON/242 PowerShell tests and 341 top-level
  entries, with 393 support scripts (wrap-automation-catalog.log,
  wrap-full-catalog.log)
- The complete secret runner passes all 52 Android steps and both independent
  final-state/RNG replays (secret-final-run.log)
- Visible secret playback passes final-state/RNG comparison after honoring the
  existing intro-skip flag (secret-visual-fixed.log)
- Scoped formatting and whitespace checks pass; unrelated dirty workspace files
  are preserved

All named logs are under temp/auto-level-demo. The final D1-in-D2 diagnostic run
passes both independent replays: all 50/66 frame-state records and 140/510
simulation RNG calls compare successfully (wrap-d1-in-d2.log).

The public D2 weapon-cheat run passes level 1 but fails level 2 at frame 26:
expected awareness RNG versus actual Guide-Bot object 55 create_random_xlate
(wrap-d2-cheats.log). Original routing does not disable Guide-Bot. Keep this
capture and failure evidence; do not add route state, filter the comparison, or
claim that active Guide-Bot replay is fixed. The bounded test correction keeps
the weapon exercise on level 1 and rotates on level 2 without firing into its
cage, explicitly asserting Guide-Bot remains unreleased before and after that
carry-over check. That rerun passes all 155 Android steps, both unreleased
assertions, and level-1 replay, but level-2 RNG still diverges: actual Guide-Bot
object 55 creates a path at frame 32 while expected next consumes RNG for robot
53 at frame 48 (wrap-d2-caged-cheats.log). Caging does not disable its simulation.
No further attempts, engine changes, or comparison relaxations are made. The
suite is not fully green; this remaining failure is explicitly outside the
requested Guide-Bot scope and is preserved for that separate workstream.

The final fixture passes scoped formatting and the automation catalog validator
(wrap-caged-format.log, wrap-caged-catalog.log). All owned build, formatter, and
test processes have finished. Stop here as requested; do not restart the deferred
backlog or claim the full original acceptance plan is complete.

Export all captures before starting any independent replay so a replay failure
cannot discard the other level's evidence.
