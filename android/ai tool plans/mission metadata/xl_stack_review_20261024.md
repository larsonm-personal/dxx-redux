# XL extension stack review

Review scope: `d2xxl_compat` 87e68dcd5 through water 5477f2d4e,
Observatory inspection 2758f675e, descriptor discovery 127b3a351,
and feature browser 69f025891 (also current `xl_more`)

1. Compare each branch against its actual parent and inspect runtime boundaries
2. Check tests and current PR evidence; separate recorded results from local reruns
3. Record actionable findings and prioritize additional support with explicit gates

Review only: preserve the branch stack and avoid changing gameplay code

## Authorized implementation follow-up

The user subsequently requested implementation of this sequence. The active goal
retains all four stages: shared context/resource admission, detected water with
Android qualification, Counterstrike level-2 progression, and Observatory
flag/resource/translation/no-damage prerequisites. Review conclusions above are
the starting evidence, not a completion checklist already satisfied.

First change: check mission context before both stock-format early returns and
test identical actionable preflight/loader errors for classic and extended files.
The temporary descriptor resource guard remains until safe initialization is
implemented and verified.

## Stack and assessment

The actual order is PR121 water -> PR122 Observatory inspection -> PR123
descriptor discovery -> PR124 browser. All are open; their current remote heads
match the local refs. `xl_more` is already the browser tip, not the old XL base.

| Branch                    | Added capability                                                                             | Actual support boundary                                                                                                       |
| ------------------------- | -------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| xl-level-expansion        | Legacy water motion, contact/turbulence, audio-loop bookkeeping and native save continuation | Explicit original Robotech developer option; no general water admission, multiplayer or rendered/audible parity               |
| xl-observatory-inspection | Bounded 17/36 reader, packed lighting counts, equipment header and neutral sparse references | Inspection only; runtime still requires 15/33; original Observatory retains 26 no-damage segments and 37 unknown flag records |
| xl-descriptor-discovery   | Strict d2x-name/title/level-list discovery and resource isolation                            | Descriptor identity deliberately blocks gameplay, even with classic level bytes                                               |
| xl-feature-browser        | Native counts, null-versus-zero handling, explanatory dialogs, scoped support labels         | Reports requirements; does not admit gameplay; expanded actual-dialog CI still pending when reviewed                          |

The inspection/discovery/UI separation is sound. The original-water physics,
resource and save-continuation tests provide substantially more evidence than a
successful load. They do not establish general water support. Retain their source
pins and limitations when replacing the developer exception.

## Findings to address before broadening support

### P2: Water exception bypasses resource validation at runtime

`runtime/preview.cpp:162` checks the option, mission filename, level filename,
level size and RL2 hash. Both preflight (`:273`) and load (`:324`) then skip
`validate_requirements` entirely. HOG/descriptor hashes and transitive asset
checks exist only in `tests/water_arena.cmake` and `runtime/arena_test.h:184`.

An unchanged RL2 with a missing/replaced HXM, POG or descriptor is therefore not
rejected by the arena admission policy. The test proves the pinned package works;
the interactive developer entry point does not enforce that package's resource
assumptions. Missing optional replacements can fall back to stock content while
the water path activates. This is source-traced, not a new on-device reproduction.

Replace the wholesale bypass with a water capability/profile and a shared resource
validation path. Until that is ready, keep the exception strictly diagnostic and
enforce its complete resource provenance. Add negative tests that leave RL2 bytes
unchanged while removing/replacing companion resources. Fixture hashes should
remain evidence identities, not the eventual gameplay admission policy.

### P2: Descriptor rejection misses preflight for classic level bytes

`runtime/preview.cpp:270` returns success for stock-format data before checking
`MISSION_ENHANCED_XL`. `xl_preview_load_level` delegates the same data directly
to the stock loader. The new guard in `d2/main/gamesave.c:1525` then returns 1.
The new-game preflight in `d2/main/menu.c:962` has already succeeded, so
`d2/main/gameseq.c:987` reaches fatal `Error` with the generic load-failure text;
`xl_level_rejection` is empty on that path.

Reproducer shape: valid d2x-name descriptor listing a mounted classic level,
such as the descriptor test's xlcase fixture, then normal New Game selection.
Existing descriptor tests call load_level directly and require failure, so they
do not establish graceful UI rejection. This finding is source-traced.

Check blocked mission context before the stock-format early return and set the
same specific reason for preflight and loading. Cover selection, progression,
restore and recovery with both classic and extended payloads. Preserve the guard
against loading stock-named HAM data from an unqualified companion HOG.

## Recommended next sequence

1. **Unify mission-context and resource admission.** Fix both findings. Initialize
   trusted base resources separately from mission overrides; validate HXM/POG/HAM
   dependencies before installing them, and restore resources on failed selection
   or mission change. A d2x-name key should ultimately select a descriptor dialect,
   not cause a permanent blanket gameplay ban. Keep it blocked until this pipeline
   is qualified. Test unchanged RL2 with missing/modified resources, malformed
   overrides, stock-name shadowing, and stock -> XL -> stock transitions.
2. **Finish water as a detected feature.** Use Robotech as the original regression
   fixture and add renamed/repacked compatible fixtures. Keep an explicit supported
   fluid profile. Cover the final collision-velocity reconstruction branch, robot
   turbulence and moving weapons before admitting water levels that use them.
   The present implementation handles player turbulence; Robotech has no robots.
   Add Android movement/save checks and reference visual/audio comparisons; keep
   fog/corona/profile differences visible rather than silently calling them parity.
3. **Qualify ordinary multi-level progress with Counterstrike level 2.** This stays
   in the existing 15/33 family and exercises the real next-level path now that
   the automatic level-one stop is gone. Inventory its actual blockers first;
   do not assume admission or completion. Require ordinary controls, authored
   branches, checkpoints, next-level identity and helpful rejection of a blocked
   later level. Follow with matcen/boss/secret-return slices only where required.
4. **Resolve Observatory evidence before implementing no-damage.** Determine the
   meaning/provenance of its 37 raw flag bytes, qualify its POG/resource lifecycle,
   then add bounded 17/36 runtime translation. No-damage needs separate firing,
   ammo/energy consumption, projectile, contact, splash/knockback and boundary
   tests plus native saves/replay. Its lack of an exit makes arena traversal and
   interaction the acceptance criterion. Do not mask unknown flags to unlock it.
5. **Expand formats only for a justified next mission.** Investigate Apocalyptic
   Factor's historical 15/34 light-index layout discrepancy before translating it;
   Neptune 23/40 needs distinct layout/CLR work. Defer cameras, teleports, master
   chains, object triggers and large-capacity stress until each has its own small
   semantic fixture and persistence evidence. Keep LGT emission separate from HAM
   lighting; parsing a brightness table is not lighting fidelity.

The highest-value next implementation is resource/context admission followed by
general water eligibility. A second physics or trigger exception would increase
the number of named pilots without delivering reusable support.

## Validation checked in this review

- Fresh MSVC Debug standalone build at browser tip: 4/4 tests pass, including
  mission labels, lighting, inventory/count contracts and hash-pinned original
  Observatory inventory. Build: `temp/xl-stack-review`
- Feature-browser JVM model tests: 5/5 pass via
  `:mission-metadata-core:test --tests '*XlCompatibilityTest'`; this is not a
  Compose render or on-device gameplay test
- Source comparison included the local pinned XL reference's fluid time scale
  and collision-velocity formula; no physics discrepancy is asserted merely
  because both formulas scale by 75 percent
- Existing private water/descriptor/gameplay results are Dot's recorded evidence;
  this review did not rerun those runtime suites or Android gameplay
- At CI inspection, XL contracts passed at the current heads of PR121-123;
  PR124 contracts and multiple other jobs were pending. Expanded browser run
  40121798177 was preparing its emulator, not yet executing the dialog test
- No branches merged, rebased or published; no gameplay code changed

### Implementation checkpoint: context and companion preflight

- Shared mission-context check now runs before classic-format early returns in
  both preflight and loader. Descriptor integration checks classic and extended
  payloads receive identical actionable reasons before geometry/SIM changes
- New `runtime/resource_validation.cpp` checks mounted HXM/POG companions before
  translation, including the developer water path. HXM uses the native bounded
  validator without mutating its active report; POG reuses the shared bounded
  indexed-bitmap/RLE validator with the loaded bitmap registry limits
- Existing native loaders still install resources. This checkpoint does NOT
  establish transactional override installation, effective override dependency
  closure, or detection of a companion removed from its original package
- Original descriptor and water tests pass; 6/6 standalone/private tests and the
  Counterstrike lifecycle pass with the initial malformed-companion controls
- Added valid/truncated-pixel/out-of-range-index POG controls; final native water
  integration rerun passes with all controls and original save/replay traversal
- Android XL build passes (3m 3s), log temp/xl-resource-preflight-android.log
  This checkpoint has compile coverage, not new on-device gameplay evidence

Next resource step: validate effective HXM overrides against loaded definitions
without changing live tables, then validate/install the resource set as a unit.
Do not remove the context guard or widen water admission merely because structural
validation passes. The resource-name/hash exception remains to be removed after
this pipeline and water semantics are qualified. All later goal stages remain open.

Counterstrike level 2 inventory in the retained original HOG is 15/33 with only
embedded-lighting and generic dependency/runtime warnings. This is prioritization
evidence, not a successful runtime load or completed level. Observatory flag study
confirms the pinned reader retains the byte in ReadExtras and upgrades the old
segment function separately; the located ambient consumers read bits 0/1. A full
historical/reference behavior qualification is still required before waiving the
37 high-bit records.

### Implementation checkpoint: staged HXM dependencies and shared water checks

- HXM records now decode through native readers into private robot, joint,
  model, bitmap and texture-pointer tables. The dependency walker accepts these
  read-only views and checks changed definitions and affected robots before any
  replacement is installed. Replacement model payloads have private ownership
- Water now runs the common requirement checks, including descriptor options,
  palette/PIG availability, object dependencies and player weapon roots. Only
  the qualified water segment-function blocker is waived for the existing
  developer pilot; the previous whole-function bypass is removed
- Known descriptive DLE/Hoggle metadata is accepted. Unknown runtime directives
  still reject. Descriptive custom-resource flags do not yet prove bundle
  completeness or require a companion on every level
- Native controls accept a valid staged bitmap/robot replacement, then reject
  structurally valid replacements referencing absent bitmaps/weapons. Tests
  verify unchanged live definitions and recovery with the original resources
- Verification: 6/6 host tests and Counterstrike lifecycle pass; the final water
  integration rerun also passes after adding the robot control. Scoped formatting
  and git diff --check pass. Android XL assembleDebug succeeds
- On emulator-5554, test_xl_pilot_runtime.jsonc passes all 36 steps, including
  XL startup/save/restore and stock-mission isolation. Retained result:
  temp/xl-hxm-staging-runtime-result.json; runtime log:
  temp/xl-hxm-staging-runtime.log. This is Counterstrike lifecycle regression
  evidence, not Android water, campaign completion or presentation parity

Remaining resource work: the staged tables currently start from live definitions.
Preflight during progression can therefore see the previous level's replacements,
whereas LoadLevel resets those replacements before loading. Establish a clean,
owned mission-base snapshot and validate a complete prepared resource set against
that snapshot. Install that same set transactionally, with rejection/recovery
tests and protection against stock-named HAM shadowing. Native loaders currently
reopen companions later; this checkpoint does not eliminate that gap.

The named/hash-pinned water developer exception, general HXM restriction and
d2x-name resource guard remain. Removing them depends on the resource lifecycle
and water qualification above. Counterstrike level 2 progression, Observatory
flag/no-damage qualification and the subsequent feature slices remain open.

### Implementation checkpoint: owned mission HXM baseline

- Mission HAM initialization now captures private robot/joint/model/texture
  definition tables for HXM staging. Model payloads are deep-copied so freeing
  or replacing a live model cannot invalidate the snapshot. Mission release and
  the start of HAM initialization clear it; capture failure leaves it unavailable
  and HXM admission fails with a resource-specific reason
- Staged HXM validation now starts from this snapshot, rather than previous-level
  replacements. Registry-count changes reject until a fresh baseline is captured
- The water integration includes a valid robot replacement with its required live
  model payload temporarily unavailable. Staging succeeds using the baseline and
  does not repair or alter live state; the control restores live state afterward
- Final host build and all 6 tests pass, along with Counterstrike lifecycle.
  Android XL assembleDebug passes. Scoped formatting and git diff --check pass
  Build logs: temp/xl-mission-baseline-build.log and
  temp/xl-mission-baseline-android.log. No new Android gameplay run for this step

This snapshot isolates HXM staging only. The common level dependency walk still
uses live tables, and native loaders still reopen/install companions afterward.
Next, route both dependency walks through the same prepared resource view and
install that prepared set atomically. The snapshot also reflects whatever HAM
the current mission loader admitted; it does not establish trusted base-HAM
provenance. Keep the d2x-name isolation guard until source selection and rollback
are qualified. All broader gameplay and presentation acceptance gates remain open.

### Implementation checkpoint: shared prepared definitions

- XlPreparedResources now owns baseline copies and decoded HXM overrides for the
  duration of admission. Companion validation and the complete level dependency
  walk use its same graph, including robot model resolution. Prepared model
  payload pointers refer to owned storage rather than live replacement buffers
- The isolation control now makes the live player model payload unavailable and
  requires full level preflight to succeed without changing that live state
- Registry-change rejection identifies robot, joint or model definitions. The
  existing Counterstrike negative test caught a generic error message; improving
  the message allowed the unchanged test to pass
- Host build, all 6 integration/contract tests and Counterstrike lifecycle pass.
  Scoped formatting and git diff --check pass. Host log:
  temp/xl-prepared-view-build.log
- Android compilation and packaging completed, but the first Gradle invocation
  failed moving its optional problems report onto an existing report file.
  Retrying assembleDebug with --no-problems-report succeeded; retained log:
  temp/xl-prepared-view-android-retry.log. No new on-device gameplay run in this step

Remaining installation work is unchanged: native HXM/POG loaders still reopen the
source files. Retain the prepared records/bytes through loading and prepare native
model initialization and bitmap ownership before committing. In particular,
polygon_model_data_read performs alignment/endian conversion and renderer model
initialization; copying uninitialized staged bytes into live models would skip
required work. POG installation currently frees old replacements before reading
new headers/pixels, so it also needs preparation before live resource changes.
The prepared view still uses the loaded sound/texture registries; this does not
prove base-resource provenance or full transactional rollback.

### Implementation checkpoint: installation from prepared companions

- Successful XL loads retain their prepared resources through the native HXM/POG
  installation points. Stock loads and mission release discard retained XL state
- HXM preparation allocates and initializes replacement models with the native
  alignment/endian/renderer sequence. Installation transfers owned model buffers
  and copies only the authored replacement records; it does not reopen the HXM
- POG preparation uses the native bitmap header reader and allocates pixel storage
  before freeing any live replacements. Installation transfers that storage into
  the native bitmap owner, updates texture state and does not reopen the POG
- Both paths retain immutable source definitions/pixels for repeated installation;
  repeated calls allocate replacement buffers before changing live resources
- Controls change the HXM definition or POG pixel after preparation and verify
  installation uses the prepared data. POG is installed twice. Original arena
  startup, water movement, save/replay and stock -> arena recovery still pass
- Host build and all 6 tests pass, along with Counterstrike lifecycle. Scoped
  formatting and git diff --check pass. Logs: temp/xl-resource-install-build.log
  and temp/xl-resource-install-format.log
- Android XL build passes. On emulator-5554, test_xl_pilot_runtime.jsonc passes
  all 36 steps (startup, save/restore and stock-mission isolation), 19.426 seconds.
  Retained evidence: temp/xl-resource-install-android.log,
  temp/xl-resource-install-runtime.log and
  temp/xl-resource-install-runtime-result.json. This does not establish Android
  water movement/presentation parity or campaign completion

This closes companion reopening after preparation. It does not establish a
transaction spanning mission selection, HAM source resolution, palette selection,
geometry translation and all resource state. Missing declared companions,
stock-name HAM shadowing, prior-mission rollback and d2x-name admission still need
qualification. The current native preparation reads each open file for validation
and decoding; an immutable byte source for those two passes remains desirable.
Water naming restrictions and broader gameplay/presentation gates remain open.

### Implementation checkpoint: declared companion completeness

- XL admission now interprets explicit custom_robots/custom_textures declarations
  and requires a corresponding supported HXM/POG somewhere in the mission's
  normal or secret level list. It does not require companions on every level
- Missing declared categories produce specific errors requesting the complete
  package and identifying other custom-resource formats as unqualified. Unknown
  declaration values reject explicitly. Custom music remains descriptive metadata
- The integration fixture preserves the original Robotech RL2 bytes while removing
  its HXM and POG in turn from an isolated private copy. Each case rejects with the
  relevant category, preserves live gameplay/definitions, and recovers after the
  companion is restored. The original archive is never modified
- Host build, all 6 tests, Counterstrike lifecycle, scoped formatting and
  git diff --check pass. Log: temp/xl-package-completeness-build.log
- Android XL assembleDebug passes, log temp/xl-package-completeness-android.log
  No new on-device gameplay run for this declaration-only admission change

Completeness limits: declarations cannot prove an undeclared resource was removed,
nor identify every missing per-level companion when another level still provides
the declared category. That requires a package manifest or stronger authored
dependency evidence. Presence also does not establish source provenance; mounted
resource lookup remains the native lookup used for loading.

The next HAM isolation hook must run before d1_in_d2_load_mission_assets frees
polygon models, not merely inside load_mission_ham afterward. For detected XL
missions, compare the resolved main HAM to a trusted startup baseline before that
destructive reload, and retain a specific preflight rejection when it differs.
The d2x-name guard already avoids this reload and remains in place pending a safe
initializer. Do not remove it solely because companion installation now succeeds.

### Implementation checkpoint: main HAM guard before model disposal

- The structurally validated startup HAM is fingerprinted before any mission
  override is mounted. Detected XL mission reloads compare the currently resolved
  HAM bytes to that baseline before d1_in_d2_load_mission_assets frees models
- Both the direct HAM loader and level preflight also enforce this check. Missing,
  changed or unavailable trusted HAM data gives a specific rejection; custom HAM
  semantics are still unqualified. Identical bytes from a different source pass
- Runtime detection inspects mounted normal and secret level headers when the
  mission browser hint is absent. The browser flag is not required for this guard
- Integration controls accept an identical copied HAM, reject a changed shadow,
  preserve live definitions/model bytes through a real asset reload attempt,
  reject with the browser hint cleared, and recover when the shadow is removed
- The outer reset_level_robots_file wrapper also checks admission before freeing
  extra object bitmaps or clearing Robot_replacements_loaded. The control calls
  that wrapper and verifies replacement state and model/definition bytes survive
- Final host build, all 6 tests, Counterstrike lifecycle, scoped formatting and
  git diff --check pass; host log temp/xl-base-ham-build.log
- On emulator-5554 the HAM guard passes the 36-step startup/save/restore/stock
  isolation regression (19.699 seconds), recorded in temp/xl-base-ham-runtime.log
  and temp/xl-base-ham-runtime-result.json. This device run precedes the final
  reset-wrapper guard; that final negative-path change has native test coverage
- Android assembleDebug also passes with the final reset-wrapper guard, log
  temp/xl-base-ham-android.log

The main-HAM check is not general asset provenance: sound-bank, palette and PIG
overrides still need source isolation. It also does not freeze the source between
checking and native reading, or restore a previous mission descriptor/mount set
after failed selection. Keep d2x-name initialization guarded until those relevant
paths are qualified. No water/Observatory/campaign acceptance gates were waived.

### Implementation checkpoint: fluid final velocity and non-player motion

- Pinned reference commit 2576a98b5de310e3ac7c9777df2677d1627b8bae uses a
  75-percent fluid time factor for all physics objects. Its final wall/bad-start
  velocity reconstruction also applies to all object types, excludes object
  collisions, and multiplies displacement by (1/frame_time)*0.75. Redux's stock
  condition restricts this to player/robot/debris and includes object collisions
- The water profile now uses the reference final-condition scope. Outside authored
  water the existing Redux condition is unchanged. This does not port every XL
  physics option or alter classic mission collision behavior
- Native robot/weapon probes in the original water segment verify exact free-flight
  displacement with unchanged stored velocity. A weapon terminates against the
  authored solid wall and verifies the exact reconstructed velocity from its
  measured displacement. This reaches the final branch that a successful sliding
  retry can bypass by finishing with HIT_NONE
- Correction to the earlier robot-turbulence plan: reference CheckSegmentPhysics
  contains robot branches, but its only caller CheckWallPhysics returns immediately
  for non-player objects (objects/updateobject.cpp:188). The call at line 659 thus
  does not apply robot turbulence. Do not port unreachable helper branches as new
  gameplay. Native probes verify no non-player turbulence RNG consumption
- Host build, all 6 tests and Counterstrike lifecycle pass. Existing original arena
  ordinary-control traversal and save/replay still pass. Log:
  temp/xl-fluid-physics-build.log
- Scoped formatting and git diff --check pass. Android assembleDebug passes;
  log temp/xl-fluid-physics-android.log. No new Android water gameplay evidence

These probes qualify isolated native fluid motion, not AI combat in water, Android
water controls, visual/audio parity, or the general water admission policy. Those
gates and the remaining mission/resource work stay open.

### Implementation checkpoint: detected experimental fluid admission

- Removed the runtime rtarena mission/name/size/SHA exception. The explicit
  -xl-water-physics profile detects water segments from bounded inventory and uses
  the shared feature/resource gates. Without that option, water is rejected with
  an explanation of its experimental status
- Multiplayer and water with robots, robot contents or matcens reject with specific
  unqualified-capability messages. Isolated native robot motion evidence does not
  qualify AI combat or generation in fluids
- Developer capture accepts an explicit -mission name for this profile, rather
  than silently selecting Robotech. Original Robotech remains hash-pinned only as
  a private regression fixture, not as runtime admission policy
- The integration test repacks all original archive members under new mission and
  level names, changes the level hash, and starts that mission normally. Water
  state and the custom HXM model load correctly. A separate mutation introduces
  a robot requirement and is rejected with the water/robot reason; original loading
  then recovers. All 6 host tests and Counterstrike lifecycle pass
- README.md and EXTENSIONS.md now describe the detected experimental profile and
  its remaining restrictions. Runtime log: temp/xl-fluid-detection-build.log
- Scoped formatting and git diff --check pass. Android assembleDebug passes,
  including generic developer startup; log temp/xl-fluid-detection-android.log

Correction from the first Android probe: init_args_common deliberately excludes
AppendIniArgs in Android mode. A staged d2x.ini therefore did not enable the
profile, and normal menu admission rejected water with the expected experimental
profile explanation. The runner restored the original device data; player/save
SHA256 lists before and after were identical. Android qualification instead uses
an explicit debug-automation profile toggle, accepted only outside gameplay in
the opt-in XL build. This leaves normal admission unchanged. Successful native
loading is not evidence of Android movement, audio or rendering parity.

### Implementation checkpoint: controlled Android water lifecycle

- Added a reusable private-fixture runner, test catalog registration and timeout.
  It hash-pins original Robotech inputs, preserves the existing pilot/save/config
  and matching mission files, and restores them in finally. Interrupted restore
  backups are retained and a later run refuses to overwrite them
- The opt-in XL debug build exposes an explicit automation-only water profile
  toggle. It refuses changes during gameplay and retains all shared admission
  checks. Normal interactive admission still rejects water
- New introspection reports player speed and active-player water state. The test
  caught and fixed stale water reporting after abort: retained geometry alone
  must not imply that the player is currently in water
- The script starts via menus, explicitly repositions to authored segment 187,
  applies joystick thrust toward the -X wall and checks displacement/speed without
  shield loss. It accounts for stock idle wiggle instead of treating nonzero speed
  alone as evidence of input-driven movement
- Save/restore returns from a dry pose and changed difficulty to the wet checkpoint.
  Stock loading clears active fluid state; restoring the saved arena restores it.
  Disabling the experimental profile then produces the specific helpful normal
  admission rejection. No mission name or source hash is used for runtime policy
- Android assembleDebug passes (1m 9s). On emulator-5554 all 45 steps pass in
  25.875 seconds. Evidence: temp/xl-water-android/result.json, steps.jsonl,
  state.json, temp/xl-water-android-runtime.log and xl-water-android-build.log
- Before/after SHA256 lists of all pilot/save files match exactly. Temporary ini
  and backup state are absent after cleanup. Native build, all 6 XL tests and
  Counterstrike lifecycle pass. Both automation catalog validators, scoped
  formatting and git diff --check pass

This qualifies a controlled emulator movement and persistence slice, not Android
authored traversal, measured reference fluid timing, physical-device controls,
audio or rendering parity. The original native arithmetic/traversal evidence
remains separate. Next: close those fluid presentation/traversal gates, then
continue Counterstrike level-2 progression and Observatory prerequisites without
waiving the remaining resource provenance and mission-transaction work.

### Implementation checkpoint: fluid sound recovery and Android portal crossing

- Reproduced a real sound-lifecycle bug with the native registry: removing the
  linked water sound left the caller's playing marker set, so subsequent fluid
  frames did not recreate it. The new regression failed before the fix with
  `water loop did not recover after native sound reset`
- Fluid loops now verify native linked-sound ownership before trusting that marker.
  Failed allocation does not mark the loop active. Tests cover native removal and
  recovery, repeated calls without duplicate sounds, idle cleanup, registry
  exhaustion and retry after capacity returns. Classic loop policy is unchanged
- Sound registry inspection lives in XL_compat/runtime/audio_state.h, included
  through a guarded native hook. Ownership checks include object signature and
  translated sample. The Android playback diagnostic queries Mix_Playing directly:
  the legacy SDL digi_is_channel_playing wrapper is a constant-zero stub. This
  diagnostic does not change that unrelated backend behavior
- Android diagnostics showed the staged ship transitions to the waterfall sample
  when its radius overlaps the authored portal. Requiring the interior-water
  sample at that pose was an invalid test assumption. The corrected test verifies
  the actual portal-contact sample and its live SDL channel, including idle
  contact; it then verifies sound removal after leaving the portal into dry space
- The Android test now crosses 146 -> 187 and 187 -> 146 using normal thrust
  through the original illusion portal, from explicit staging poses. It checks
  movement without shield loss, native save restoration with live waterfall
  playback, stock-to-water save restoration and normal-profile rejection
- All 57 steps pass on emulator-5554 in 26.768 seconds. Pilot/save hashes match
  exactly after the runner restores them; its temporary backup is removed.
  Evidence: temp/xl-water-android/{result.json,steps.jsonl,state.json},
  temp/xl-fluid-audio-android-runtime.log and xl-fluid-audio-android-build.log
- Android assembleDebug passes (1m 19s), native build and all 6 XL tests pass,
  Counterstrike lifecycle passes, scoped formatting and git diff --check pass.
  The failing-before native evidence is temp/xl-fluid-audio-before.log

This adds real portal traversal and mixer-channel lifecycle evidence, not full
spawn-to-water Android traversal, recorded output/reference audio equivalence,
water rendering parity, robot combat or broader campaign support. Those original
gates remain open. Temporary diagnostic logging used to identify the sample
transition has been removed from the final implementation.

### Implementation checkpoint: Observatory reserved flag audit

- Audited the pinned XL segment reader, all segment `m_flags` consumers and the
  ambient recomputation call path. Only bits 0/1 have ambient-water/lava meanings;
  high bits are preserved and not consumed as properties. Old function 13 remains
  the distinct no-damage property. Full source pins, raw-value histogram and scope
  are in android/XL_compat/evidence/segment-flags-17-36.md
- The 17/36 inventory now classifies the 37 original high-bit records as preserved
  reserved observations, retaining every raw byte. Other version profiles keep
  their previous unknown-flag behavior. No runtime version gate was widened
- Public synthetic tests exercise all 256 values and require runtime rejection.
  Original Observatory retains its section endpoints, 26 unsupported no-damage
  functions and descriptor/translation rejection. No mission identity exception
- An optional hash-pinned oracle compiles the reference AmbientMarkBfs and ambient
  flag-update functions. All 256 input bytes, four water/lava texture masks and
  open/closed propagation preserve high bits and recompute only ambient bits
- Built an isolated inspector from the actual branch-base sources, after finding
  the old cached inspector predated Robotech parser support. Its original Robotech
  JSON is byte-identical to the updated inspector output. Evidence:
  temp/xl-segment-flags-water-before.json and xl-segment-flags-water-after.json
- All 7 XL tests pass against the rebuilt native runtime; Counterstrike lifecycle
  passes. Android assembleDebug, scoped formatting and git diff --check pass.
  Logs: temp/xl-segment-flags-build.log, xl-segment-flags-runtime-build.log,
  xl-segment-flags-android-build.log and xl-segment-flags-format.log. The original
  Observatory inspection report is temp/xl-segment-flags-observatory.json

The reserved-bit interpretation prerequisite is closed for the audited profile.
This is not an Observatory gameplay qualification. Safe descriptor/resource
initialization, 17/36 translation, no-damage semantics, native saves/replay and
presentation remain required, alongside the previously open fluid and campaign
work. No original resource bytes or runtime/save state changed in this step.

### Implementation checkpoint: changing-light count admission

- Found a representability gap in the existing 15/33 translation: inventory reads
  an unsigned light-index count, but stock game 32 reads a signed byte and
  normalizes negative counts to zero. Counts 128 through 255 could therefore
  pass admission and silently lose their lighting targets
- Shared runtime admission now rejects counts above 127 with the source record,
  observed count and supported limit. Inventory preserves the source value;
  browser eligibility and gameplay translation use the same rejection
- Serialized fixtures cover counts 0, 1, 127, 128 and 255, including browser
  rejection and translator refusal at the signed boundary. All 7 XL tests pass
- A possible follow-up is splitting large counts across repeated indices for the
  same source face, with an expanded-table capacity check. Do not enable that
  conversion until native light-off/on behavior, ordering and clamping are tested.
  Keep this work separate from 17/36 packed-record and header translation

This closes an admission hole; it adds no new gameplay capability. Logs:
temp/xl-light-count-build.log, xl-light-count-runtime-build.log and
xl-light-count-format.log. The native runtime rebuild passes. Android behavior
has not been rerun for this boundary-only change.

### Implementation checkpoint: lossless changing-light count translation

- Replaced the temporary per-record rejection above with bounded lossless
  translation for 15/33. Stock change_light visits all matching source-face
  indices in order, so chunks of at most 127 preserve delta order and clamping.
  The source inventory remains unchanged; the translated table is appended and
  its explicit section offset/count updated. Ordinary tables remain byte-identical
- Admission now checks the expanded index count against the stock limit of 500.
  Serialized tests verify 0/1/127/128/255 targets, exact emitted source/target
  order, a 500-entry translated table and helpful rejection above that limit
- Native fixtures change a real pilot index to 128 and 255 targets. Normal
  preflight/loading accepts them; every native index is checked, then all 70
  source faces undergo light-off/on and repeated-toggle verification against
  ordered, clamped reference UVLs. Original pilot loading recovers afterward
- All 7 XL tests and the native lifecycle pass. The lifecycle CMake wrapper
  requires both large-count verification markers. Evidence:
  temp/xl-compat-ninja/preview-tests/6d35d3e069ae/preview.log and
  temp/xl-light-split-{build,runtime-build,runtime}.log
- Opt-in Android x86_64 assembleDebug passes, as do scoped formatting and
  git diff --check. Build evidence: temp/xl-light-split-android-build.log.
  This turn did not install the APK or claim Android execution of the new fixtures

This is a verified translation capability, not new campaign, rendering or 17/36
qualification. The remaining resource, campaign and Observatory work is unchanged.

### Implementation checkpoint: base sound-bank provenance

- Traced piggy_load_mission_data: safe HAM identity alone does not prevent a
  mounted sound bank from being decoded and installed. Both playback fallback
  and canonical authored-duration reads can consume the S11/S22 banks
- Startup identity capture now records both banks, including absence. The shared
  base-resource guard compares their contents before model disposal, mission HAM
  reload, robot reset and XL preflight. Renamed the guard from HAM rejection to
  base-resource rejection to reflect its scope; ordinary mission policy is unchanged
- Native arena controls mount identical copies, then replace each bank with a
  malformed sentinel. Identical copies pass; changed banks produce a named error
  even without the browser XL hint. Initialization/reset leave definitions, model
  payloads, sound registry, arena state and replacement flags unchanged. Unmounting
  restores admission. The CMake runner requires the verification marker
- Native build, all 7 XL tests and Counterstrike lifecycle pass. Logs:
  temp/xl-sound-source-build.log and temp/xl-stack-review/water-arena/water.log
- Opt-in Android x86_64 assembleDebug passes (51 seconds); scoped formatting and
  git diff --check pass. Android compile evidence is in
  temp/xl-sound-source-android-build.log; no on-device run is claimed for this step

This closes sound-bank identity coverage, not custom-bank support or immutable
check-to-read handling. Palette/PIG provenance and full selection rollback remain
open. The d2x-name resource-initialization guard remains in place until those
requirements and initialization behavior are verified.
Next resource boundary: gamepal.c load_palette derives the PIG filename from the
palette name and can replace the PIG before applying the palette. Validate both
resolved inputs before this mutation, including same-name mounted overrides and
failure recovery; merely retaining the HAM and sound checks is insufficient.

### Implementation checkpoint: selected palette/PIG provenance

- Startup capture inventories mounted root .256/.pig files and retains bounded
  content hashes. XL level admission checks its selected palette and derived PIG
  against those definitions, rather than merely checking existence. Flat .256
  names must fit the native FILENAME_LEN; unsupported names and unknown or changed
  graphics resources receive specific errors. No mission-name rule is involved
- Native tests mount byte-identical copies of the original arena's palette/PIG,
  then change one byte while retaining file length. Preflight and real load_level
  reject both overrides with the resource name. Palette bytes, fade table, bitmap
  registry, palette tracking and gameplay state remain unchanged. Unmounting and
  native StartNewGame restore the original water profile. These are state and
  provenance tests, not rendered-image qualification
- All 7 XL tests pass (the graphics fixture's case-sensitive filename assertion
  was corrected), and the expanded Counterstrike lifecycle passes. Test-local
  poisoned files left by the failed assertion were removed before rerunning.
  Logs: temp/xl-graphics-source-build.log and
  temp/xl-stack-review/water-arena/water.log

This closes selected-level graphics identity checking for trusted startup assets.
It does not validate arbitrary custom PIG/palette formats or freeze files between
check and read. Descriptor initialization still needs its default/transition
resource path and full failure recovery verified before its guard is removed.

- Android x86_64 assembleDebug passes (1m 33s), scoped formatting and diff checks
  pass. Installed the APK on emulator-5554 and reran the original water runner:
  all 57 steps pass in 67.532 seconds, including startup, portal crossing, mixer
  playback, native save restoration and stock isolation. Pilot/save SHA256 lists
  match exactly after cleanup, and the temporary backup directory is absent
- Evidence: temp/xl-graphics-source-{android-build,android-runtime,format}.log,
  temp/xl-water-android/{result.json,steps.jsonl,state.json}, and
  temp/xl-graphics-state-{before,after}.txt. The prior Android run took 26.768s;
  repeated resource hashing adds substantial load/restore cost in this debug
  build. Consolidate validated owned resource reads within a load transaction
  rather than adding unchecked filename/time-based caching. No renderer-parity
  or arbitrary custom-resource qualification is implied by this passing run

### Implementation checkpoint: descriptor base initialization

- Opt-in d2x-name selection now follows native D2 base initialization after base
  resource checks. The descriptor dialect never enters the legacy extra-HAM
  branch. Ordinary builds preserve discovery-only behavior
- Descriptor base checks include the default palette/PIG before the D1 transition
  can dispose of active definitions or reset graphics. Native controls reject
  companion HOGs containing shadow HAM or default PIG data, preserving prior
  definitions/model payloads and recovering after selecting stock again
- Tests deliberately change an old robot definition before selecting original
  Observatory, then require exact stock definitions and an owned resource
  baseline. This distinguishes initialization from leaving stale state untouched
- With private D1 assets available, native StartNewGame installs actual D1 assets;
  selecting Observatory restores D2 definitions and default graphics, and stock
  recovery succeeds. This optional path reports explicitly when D1 assets are
  unavailable; it ran and passed locally
- Removed a redundant context/hash check in validate_requirements: both callers
  already validate context before reading or translating a level. No cross-call
  cache or file-identity shortcut was introduced
- All 7 XL tests and Counterstrike lifecycle pass. Evidence:
  temp/xl-descriptor-init-build.log and
  temp/xl-stack-review/observatory-descriptor/descriptor.log

This enables verified base initialization, not descriptor gameplay. The runtime
error and browser description now distinguish that progress from the remaining
gameplay/failed-selection rollback gate. No 17/36 or no-damage gate was widened.

Opt-in Android x86_64 assembleDebug, scoped formatting and git diff --check pass;
see temp/xl-descriptor-init-{android-build,format}.log. This checkpoint did not
install the APK or claim an Android descriptor-transition run. The preceding
graphics-resource checkpoint's 57-step Android run remains separate evidence.

### Implementation checkpoint: preserve active state on candidate rejection

- Found that xl_preview_load_level discarded prepared resources, source inventory,
  water, lighting, material and diagnostic state before candidate validation.
  A rejected level could therefore leave native geometry active but remove its
  extension behavior. The developer translated-fixture path also loaded native
  bytes before comparing them against the expected translation
- Moved active-state clearing to the native commit boundary, after read-only
  admission, companion validation and exact translated-byte comparison. The
  developer preview uses that same ordering. Successful stock loading still
  clears extensions, and changing to multiplayer explicitly clears singleplayer
  state even when the candidate is rejected
- Native pilot tests reject an unsupported source version and a same-sized
  coordinate mutation, require exact native geometry/object/light-mask/identity
  preservation, and exercise the existing shading/material assertions afterward.
  Arena tests retain the active water contract after palette/PIG rejection and
  after an unsupported robot-capability mutation. CMake requires these markers
- All 7 XL tests and Counterstrike lifecycle pass. Evidence:
  temp/xl-rejection-state-build.log,
  temp/xl-compat-ninja/preview-tests/28d9818d2936/preview.log and
  temp/xl-stack-review/water-arena/water.log

This is pre-commit level rejection preservation, not full mission-selection
rollback. Descriptor/mount ownership and native-loader failures after commit
remain unresolved. No descriptor, 17/36 or no-damage admission gate was widened.

Opt-in Android x86_64 assembleDebug, scoped formatting and git diff --check pass;
see temp/xl-rejection-state-{android-build,format}.log. No new on-device run is
claimed. Next selection boundary: load_mission currently calls free_mission,
including archive removal and XL baseline disposal, before parsing the new
descriptor; Android additionally prepares/activates a staged asset context.
Retaining and restoring that context requires ownership/staging changes, not
another boolean guard in the level loader.

### Implementation checkpoint: shared descriptor preflight before teardown

- Extracted the existing D2 mission descriptor parser into a helper operating on
  caller-owned Mission fields and an explicit warning counter. Allocation errors
  return to the owner instead of calling free_mission or closing its input. The
  normal loader uses the same helper; no second descriptor grammar was added
- Opt-in builds preflight non-builtin D2 candidates before free_mission and, on
  Android, before android_mission_assets_prepare. Android's publication already
  copies complete descriptors to discovery aliases, so the same virtual-file
  reader can check them without activating a mission's asset context
- Native tests retain the original Observatory archive while rejecting a valid
  title followed by a malformed second level entry. Existing negative descriptor
  controls also require the previous Mission pointer/path, resource definitions,
  archive origin and owned baseline to survive. This includes failures discovered
  by mission discovery as well as deeper parser failures
- Native build and descriptor regression pass. Evidence:
  temp/xl-descriptor-preflight-build.log and
  temp/xl-stack-review/observatory-descriptor/descriptor.log

This prevents malformed-descriptor teardown; it is not full rollback. Preflight
currently discards its parsed candidate and normal loading reparses after mounting.
Immutable candidate publication, activation/resource failure recovery and prior
Android context restoration remain required. Android execution of the malformed
selection case has not yet been verified; compilation is tracked separately.

All 7 XL tests and Counterstrike lifecycle pass after the final shared-parser
changes. Android x86_64 assembleDebug passes (1m 33s), as do scoped formatting and
git diff --check. Logs: temp/xl-descriptor-preflight-{android-build,format}.log.

### Implementation checkpoint: owned descriptor commit

- Preflight now retains the parsed level/secret lists, dialect, alternate-HAM
  field and ignored-origin counter. The normal loader transfers those owned
  fields instead of reopening/reparsing the descriptor after activation
- Briefing/ending directives are normalized and retained in order, then resolved
  after the mission archive mounts. This preserves repeated-directive fallback
  without looking up mission resources in the previous context. Staging is bounded
  to a 1 MiB descriptor and uses linear-time directive collection
- Native test-only instrumentation replaces the descriptor with invalid contents
  immediately after preflight. Selection still commits the validated one-level
  snapshot; the changed on-disk contents are asserted separately. Repeated
  briefing directives select the later existing TXB, and a subsequent missing
  directive preserves it. Ending selection is checked independently
- Staged allocations are released on preflight, Android preparation/activation
  and Mission allocation failures, or transferred once at commit. Built-in/D1
  and ordinary non-opt-in descriptor parsing continue through the same parser

This closes the descriptor reread gap. It does not yet retain the prior mission
through later mount/resource failure, nor freeze every companion resource. No
descriptor gameplay or new serialization profile is admitted by this change.

Validation: all 7 XL tests, Counterstrike lifecycle, scoped formatting and diff
checks pass. Android x86_64 assembleDebug passes (2m 41s); no new on-device run
is claimed. Evidence: temp/xl-descriptor-stage-{build,android-build,format}.log
and temp/xl-stack-review/observatory-descriptor/descriptor.log. The test-only
source mutation hook is compiled only under DXX_XL_DESCRIPTOR_TEST in the native
lifecycle target.

### Implementation checkpoint: Android mount preflight before reset

- Android preparation previously checked path existence but deferred actual mount
  attempts until after teardown. It now probes unmounted candidate resources
  through PhysFS under a private virtual prefix, then removes the probes before
  resetting anything. It rejects duplicate paths and mounts owned outside the
  active catalog context. Existing owned mounts remain eligible for reselection
- Added a native integration fixture that compiles the actual Android catalog
  implementation and uses real PhysFS mounts. Engine reset functions are spies:
  corrupt HOG, duplicate-path and foreign-owner failures must leave reset counts,
  owner/context JSON, generation, search-path order and root resource lookup
  unchanged. The candidate's colliding root filename must not become visible
- Replacing the rejected archive with a valid DHF at the same path permits
  activation and reading its payload. Returning to the original context and
  reselecting it both succeed. Fixture cleanup detaches owned mounts and removes
  its private catalog. The descriptor CMake runner requires the success marker

This tests the shared catalog/mount code, not Android engine reset/rendering
behavior. Files changing after the probe, actual activation failures after
teardown and later resource installation still need transactional recovery.

Validation: all 7 XL tests and Counterstrike lifecycle pass. Scoped formatting
and diff checks pass. Android x86_64 assembleDebug passes (1m 34s); no new
on-device run is claimed. Evidence: temp/xl-mount-probe-{build,format,android-build}.log
and temp/xl-stack-review/observatory-descriptor/descriptor.log. The native
fixture's private catalog is absent after completion.

### Implementation checkpoint: activation transition and late archive loss

- Activation now requires a successful preparation and consumes the transition
  once. Rejected preparation, repeated activation and nested preparation cannot
  initiate or replace a destructive context switch
- A late activation failure clears pending ownership and advances generation
  after detaching partial candidate mounts. Previously this fallback to base
  left the generation unchanged despite the active context having changed
- The native catalog fixture removes a valid archive between preparation and
  activation. A trailing directory mounts before the failure; assertions verify
  that it and the prior context are detached, base ownership/context is exposed,
  and generation advances. Recreating the archive and preparing again succeeds

Validation: all 7 XL tests, Counterstrike lifecycle, scoped formatting and Android
x86_64 assembleDebug pass. Evidence: temp/xl-activation-state-{build,format,android-build}.log
and the descriptor integration log. No new on-device run is claimed. Reset hooks
in this fixture remain spies: these results verify catalog state and PhysFS
cleanup/retry, not restoration of the previous playable mission or engine assets.

### Implementation checkpoint: Counterstrike level-two prerequisites

- Reinspected all 30 original HOG levels. Level two, Turnabout Bore, is 15/33
  with 457 segments, 1,604 vertices, 148 objects (69 robots), 213 walls,
  39 triggers, three robot generators, 56 light indices and 4,069 delta records
- Trigger inventory: 13 open-door, 11 open-wall, one close-wall, three unlock-door,
  ten generator activations and one exit. These are explicit future authored
  behavior gates, not inferred successful interactions from decoded records
- Added campaign_test.h to the existing original-source native lifecycle test.
  It starts level one, invokes AdvanceLevel directly, verifies original level-two
  identity/native counts/robots and admission, runs 120 ordinary thrust/fire
  frames without cheats, then saves and restores after a level-one restart
- Restore assertions cover identity, XL activation, exact position, energy,
  shields, robot count and light UVLs. They do not prove whole-state replay,
  generator persistence, branch completion, earned exit or Android presentation
- Run this prerequisite after existing scenarios. The initial placement before
  the old level-one combat probe altered simulation history and its damage
  assertion failed; no engine or combat assertion was weakened. The final
  ordering passes both. Navigation fixtures retain their deliberate level-one
  stopping policy and do not run this separate next-level prerequisite

Validation: native build, all seven XL tests (17.21s), combined Counterstrike
lifecycle (16.84s), scoped formatting and diff checks pass. Evidence:
temp/xl-level2-inventory.json, temp/xl-level2-{build,format}.log and
temp/xl-compat-ninja/preview-tests/0a4b43d12c75/preview.log. This change is
test coverage only; no runtime eligibility policy or supported-mission label
was expanded. Next: earned level-one exit into level two, then its actual
generator/door branches and normal-input completion with saved checkpoints.

### Implementation checkpoint: earned exit exposed a frame-boundary crash

- Added a separate earned campaign-transition case. It runs the existing ordinary
  key/reactor/escape controller and earned checkpoint restores, then permits the
  native exit to continue into original level two instead of the test-only stop
- The initial run crashed with integer divide-by-zero after level two loaded.
  A diagnostic rerun confirmed object_move_all continued through level-two
  object slots with FrameTime=0 after slot zero changed levels. Direct invocation
  of AdvanceLevel outside simulation in the earlier prerequisite could not
  expose this failure. Failure evidence: campaign-tests/92374cc52220 and
  campaign-tests/9191967aef17 under temp/xl-compat-ninja
- D2 now stops object iteration and the remainder of the old simulation frame
  when an exit changes levels. Its existing inner trigger-loop guard now also
  applies to non-network builds. Applied the corresponding guards to D1's
  equivalent loops. Temporary diagnostic logging was removed
- The new test checks survival without lost lives, positive countdown before
  the crossing frame, original level-two identity/XL activation and native
  arrival checkpoint restoration. Existing level-one escape/recording fixtures
  retain their explicit stop behavior. Campaign mode does not claim an exported
  cross-level replay or interactive presentation verification

Validation: earned transition and existing timed escape each pass twice. Both
campaign runs cross in 407 frames with prior countdown 2,505,660 fixed units
(38.2 seconds), arrive alive and restore the arrival checkpoint. Passing logs:
temp/xl-compat-ninja/campaign-tests/{6a0887bfb44c,a67f9047fedb}/preview.log.
All seven XL tests pass (18.41s), scoped formatting/diff checks pass, and the
Android x86_64 build passes (1m 11s), compiling both engines. No on-device or
D1 gameplay transition is claimed. Level-two generator/branch combat and
completion, plus Android transition presentation, remain open.

### Implementation checkpoint: level-two ordinary key routes

- Extended the test navigator to continue from earned level-two arrival instead
  of restarting or resetting player inventory/RNG. Added xl_level_two_keys to
  the private original-source CTest suite, retaining mandatory earned level-one
  key/reactor/exit checks and separate second-level checkpoint assertions
- Blue-key route passed in 7,589 frames, with 15 robot kills and the player alive.
  The earned blue key and remaining energy/shields survive native save/restore.
  Evidence: temp/xl-compat-ninja/level-two-route-tests/a8a08ace0ef7/preview.log
- Initial red-key continuation died. Extending the existing low-health supply
  search moved the failure later, into a thief/defender encounter in segment 226.
  Damage diagnostics preserve aim, firing inputs, range, health and final state;
  see level-two-route-tests/{bd342644eb53,7940329d29d6}. This is incomplete
  controller/gameplay evidence, not proof of an unsupported engine feature
- The controller now prefers visible non-thief defenders to a closer thief in
  second-level combat. No enemy, weapon, health, physics or resource changes
  were made. Generator links are excluded from wall-passability observations
  because they identify generator segments; generator semantics require their
  own observation/assertions and are not counted as verified doors

Latest outcome: the non-thief priority gets past the initial encounter, but a
concussion blast kills the player at 22 kills (dea4a2f38724). A normal launch
clearance check then reaches 27 kills before death at red-route frame 10,786
(995e0a7aef3f). These directories are under the same level-two-route-tests root.
The new xl_level_two_keys acceptance test deliberately still reports failure;
it is neither disabled nor marked expected-failure. Blue-key evidence remains
valid, while the combined route, generators and completion remain unfinished.
Native build, scoped formatting and diff checks pass. No runtime gameplay policy
changed and no new Android validation is claimed for these test-controller edits.
The existing earned campaign-transition test also passes after the controller
changes (23.37s); the new combined key-route failure is not a regression in that
previous acceptance boundary.

### Implementation checkpoint: active generator observation and persistence

- Added read-only GeneratorRouteObservation to the ordinary second-level route.
  It records enable/life changes and unique spawned signatures, validates each
  spawned robot against its source center's authored mask, and keeps these
  observations separate from door-passability coverage
- Blue-key route has zero generator activations/spawns. Red-key route activates
  center 1, segment 233, consuming one life and spawning robot IDs
  25,39,25,25,25 before capacity reaches zero and the center disables
- Added a native checkpoint immediately after the first observed spawn. Exact
  station/mask/capacity/lives/timer fields, robot IDs/signatures/creator/position/
  shields, and earned player energy/shields survive restore. The test does not
  equate this field snapshot with complete morph/rendering/replay qualification
- Observation-only run 4f157f330301 and checkpoint run 0bf4668c7f31 have the same
  five spawn signatures and terminal red-route death at frame 10,786. Both are
  under temp/xl-compat-ninja/level-two-route-tests. The combined acceptance test
  still fails because the player does not reach the red key; no expected-failure
  or bypass was added

Native build and scoped formatting/diff checks pass. Evidence:
temp/xl-generator-route-{build,format}.log and the two route logs above. This
adds a verified active-generator persistence slice, not all-generator behavior
or completion. Remaining generator gates include the other centers, all ten
activation triggers, exhausted/repeated activation and reference comparison.
The existing earned campaign-transition regression passes (23.28s). A direct
comparison confirms all nine logged generator-state/spawn and route-result
records agree between the observation-only and active-checkpoint runs.

### Investigation checkpoint: red-route controller choices

Tested nearby unowned-weapon/energy pickup detours, selected-projectile lead and
movement while engaging distant defenders. These did not qualify the route:

- c41e72b7d25c and 281f31bd7360 changed the blue-key encounter and died at frame
  3,161 in segment 110 instead of reaching its previously earned checkpoint
- Restricting new choices to the red continuation (711ae7a48011) retained blue
  completion but died at red frame 10,442 in segment 167. Weapon diagnostics
  showed primary=0, owned=1: the detours had not actually acquired another gun
- Enabling movement during distant combat (6e7e112c75de) died at red frame 177
  in segment 97, much earlier. Reverted these unsuccessful tactical changes;
  retained only selected/owned-weapon diagnostics. All run directories are under
  temp/xl-compat-ninja/level-two-route-tests

These tests change the next investigation: audit the red key's authored
door/switch dependencies and route choices before more combat tuning. A
successful run remains mandatory, and neither combat death nor a bot's poor
route choice establishes an engine incompatibility. The existing generator
checkpoint and earned blue-key observations remain scoped evidence.

Follow-up dependency inspection found three unlock-door triggers (17,19,25)
which the test navigator had excluded from its prerequisite candidates. Added
TT_UNLOCK_DOOR alongside open-door/open-wall candidates. This alone retains the
same failing red route (aa9be235ed9c). A direct-objective comparison that skipped
optional pickups also died (750b0bb59d4b, red frame 2,547); removed that shortcut.
These remain controller limitations, not runtime unlock support findings.

The key planning issue is now source-traced: when an objective is unreachable,
the navigator chooses the nearest reachable host that opens any frontier. It
does not score whether that frontier leads toward the objective. The direct
comparison selected wall 28 near segment 223 and activated trigger 0 before
death, still without reaching the red key. Next implement objective-directed
prerequisite selection, retaining the earned checkpoints and normal inputs.
Only unlock-trigger eligibility and selected/owned-weapon diagnostics remain
from these experiments. No gameplay/resource rules changed.
Final native build, scoped formatting/diff checks and earned campaign-transition
regression pass (23.42s). The combined level-two key acceptance remains failing.

### Investigation checkpoint: destination scoring and 17/36 translation boundary

Tested prerequisite scoring using travel-to-host plus optimistic topological
distance from the newly opened frontier to the objective. With normal pickups,
2e9bbb7cfd7a retained the same frame-10,786 death. With red-route detours disabled,
3acc2e16efdd retained the direct-route frame-2,547 death. Both are under
temp/xl-compat-ninja/level-two-route-tests. The earlier hypothesis that this
scoring change would fix the route is not supported. Removed the experiment
and restored the prior controller; the red-key acceptance remains open.

Continue the independent Observatory prerequisites while retaining that route
failure. Source inspection identifies concrete 17/36 translator work:

- Its game header has ten section descriptors versus stock's nine. Retagging
  the version alone leaves the equipment descriptor where stock expects the
  level-name line. Remove the extra 12 bytes and rebase every retained section
  and legacy player-block offset; require an empty equipment section
- Packed light indices must always be rebuilt for game 36, even when counts
  are below 128. The existing converter only rebuilds large-count tables; side
  bits and counts would otherwise be misread. Retain split-count capacity checks
- Integer texture colors are already decoded by inventory; preserve authored
  source data for runtime adapters. Preserve audited reserved segment bits
- Empty sparse object-reference tails are already inspected; do not infer support
  for actual object triggers. No-damage remains a distinct semantic blocker

These are source findings, not a new admission grant. Implement bounded 17/36
translation with byte/layout controls and real native-loader checks before
changing its eligibility. Original Observatory remains blocked.

### Implementation checkpoint: gated 17/36 layout translation

- Added explicit prepare_serialization_probe_17_36 for developer qualification;
  prepare_gameplay_probe still rejects 17/36. Only the serialization blocker is
  relaxed in the qualification entry; no-damage, object triggers, equipment and
  other unknown/unsupported requirements still reject conversion
- Shared conversion removes the 12-byte equipment descriptor, sets the stock
  header size, rebases retained section and legacy-player offsets, and always
  rebuilds packed game-36 indices. Counts split into ordered chunks of at most
  127 using the existing expanded-capacity checks; raw source inventory remains
  available to future runtime adapters
- Public binary contracts cover counts 0,1,31,32,127,128,255,300,8191, level-name
  placement, reserved bits, legacy/section offsets and exact delta payloads.
  Wrong profiles, equipment generators, no-damage and normal runtime admission
  are negative controls
- Native integration retains the original Observatory bytes, verifies that the
  original rejects, then creates an explicitly altered private control replacing
  exactly 26 function-13 records with zero. Stock loading checks name, complete
  vertex coordinates, geometry/wall/trigger counts, reserved metadata bits,
  split light indices and every delta contribution, followed by stock recovery

The neutralized fixture is not an Observatory port and is not offered through
mission admission. No no-damage semantics, gameplay, source lighting/rendering
parity or custom-resource lifecycle is claimed by the native layout control.

Validation: all seven XL tests pass (21.64s), Counterstrike lifecycle passes
(19.37s), native builds and scoped formatting/diff checks pass, and Android
x86_64 assembleDebug passes. Evidence: temp/xl-format17-{build,native-build,
format,android-build}.log and temp/xl-stack-review/observatory-descriptor/descriptor.log.
No new on-device gameplay test was run. The level-two red-key acceptance
failure remains open independently of this layout work.

### Investigation checkpoint: no-damage semantics

Audited every HasNoDamageProp consumer in pinned reference commit
2576a98b5de310e3ac7c9777df2677d1627b8bae. The detailed source matrix and required
controls are in the
[no-damage audit](../../XL_compat/evidence/no-damage-reference.md).

The property cannot be implemented as shield immunity alone: it separately
suppresses local weapon creation, primary input and queued primary firing,
bomb input and several collision effects. Flare input still consumes energy;
an already queued missile can consume ammo before creation is refused. Robot
firing and robot damage are not blanket-disabled. Wall-hit decisions use the
hit segment, while weapon creation uses the parent's segment, making boundary
controls necessary. Fusion and guided state need temporal tests on entry/exit.

Next implementation slice: a source-bound property adapter plus paired native
controls for firing/resources and collision effects, followed by matched
reference boundary checks. Keep function 13 and normal 17/36 admission blocked
until semantics, descriptor/resources and original-level qualification pass.
No runtime behavior changed in this audit; no new gameplay pass is claimed.
The level-two red-key route and remaining water/transaction work stay open.

### Implementation checkpoint: no-damage player and direct-impact controls

Added the source-function query and two guarded D2 hooks: final player damage
and direct weapon/player collision. The latter returns before impact effects,
projectile disposal and bump response, rather than only preserving shields.
Normal admission still rejects function 13 and 17/36; no original Observatory
gameplay is enabled. D1 is unchanged because XL runtime support is D2-only.

Headless descriptor qualification binds an owned original-property inventory
to the neutralized layout after checking profile, vertex coordinates and segment
vertex indices. This binding is unavailable in Android/ordinary executables.
Native paired controls verify unchanged player/projectile bytes, shields,
palette and effect count for protected impacts; ordinary damage flashes and
reduces shields, while the same projectile hitting a player in an ordinary
segment is disposed and creates an effect. Invalid geometry preserves the prior
binding, multiplayer disables queries and stock loading clears the binding.

All seven XL tests pass (17.85s), including original admission rejection and
the new required collision marker. Counterstrike lifecycle passes (18.78s).
Native build passes without new warnings. Remaining work includes firing and
resource accounting, wall/environment and splash effects, other collisions,
temporal boundary behavior, save/restore and matched reference execution.
These controlled calls are not earned traversal or complete no-damage support.

### Implementation checkpoint: no-damage firing and resource accounting

Added guarded D2 hooks for primary input and queued primary fire, fresh missile
input, bomb input and local console weapon creation. Creation checks the parent's
segment, retaining the robot path. The existing flare path still spends energy
and advances cooldown before creation refuses. Queued missiles retain ammo and
cooldown accounting, then stop after refused creation before recoil, bomb-drop
bookkeeping or autoselection, matching the reference ordering for this case.
Stock behavior outside the property is unchanged.

Native controls use real weapon/item dispatch and direct creation calls to
compare protected/ordinary behavior, including fresh versus queued missile
input, bomb counts, flare costs, opposite parent/muzzle segment combinations,
robot weapon creation and ordinary firing after leaving the region. Collision
controls now create their incoming projectile while the player is outside the
protected segment, before moving the controlled player into it.

These are controlled source-derived expectations, not matched reference runs.
Charged fusion, guided state, wall/environment and splash effects, remaining
collisions and persistence still require qualification. No admission grant or
original Observatory traversal is claimed.

### Implementation checkpoint: no-damage wall/environment contacts

Added guarded wall-hit suppression after ordinary wall processing/force-field
response but before bump damage, ordinary impact sound and awareness. Volatile
contact keeps its original lava/water type while suppressing damage/red flash;
protected contact applies random rotation only when the ship has thrust.
Unprotected and non-XL behavior remains unchanged.

Synthetic native contacts use an ordinary-segment player with protected and
ordinary hit segments, demonstrating that the hit segment governs these hooks.
Paired controls check bump damage, awareness suppression, blue force-field flash,
identical seeded force-field rotation, lava damage/red-flash suppression,
classification, and idle/thrusting rotation including idle RNG preservation.
The initial test incorrectly expected an unclamped blue flash of 60; source
inspection showed PALETTE_FLASH_ADD clamps to MAX_PALETTE_ADD (30) without a
flash effect. Corrected that expectation, with the diagnostic retained.

Contact surfaces/textures are private synthetic controls and are restored after
testing. Original geometry traversal, authored wall triggers, scrape motion,
audible effects and reference presentation parity remain unqualified. Normal
Observatory/function-13 admission remains blocked.

### Qualification checkpoint: no-damage radius explosions

Added paired native explosion controls at identical coordinates with identical
initial physics and seeded RNG, changing only the headless test property map.
Both protected and ordinary cases retain identical nonzero linear/rotational
response. Only the ordinary case loses shields and gets damage flash. A second
pair using the actual flash-weapon definition verifies that protection preserves
blinding palette state and exactly the ordinary Flash_effect duration.

This matches the ordering found in pinned reference effects/fireball.cpp:
blinding and physical response precede ApplyDamageToPlayer. No explosion-code
change was needed; the existing central damage hook handles the tested paths.
The source hash is recorded in the no-damage audit. The native descriptor test
passes with a required splash marker, including all earlier layout, firing,
wall and admission-rejection controls. Native build passes.

This slice changes headless tests and evidence only. Every missile type, smart
children, cross-segment occlusion, audible/rendered effects and matched reference
execution remain unqualified. Original Observatory remains rejected.

### Implementation checkpoint: no-damage object bumps

Added guarded player/clutter and player/player returns before the collision
sound gate and bump response. Native dispatch controls compare protected and
ordinary property maps on identical geometry, asserting unchanged object bytes,
shields, RNG and sound timestamp for protection, and changed velocities for both
objects plus sound-gate entry without protection. Direct boundary controls also
distinguish the player's segment in clutter dispatch from the first receiver's
segment in player-pair dispatch. All cases run in singleplayer test context;
multiplayer admission remains rejected.

Reference CollideActorAndClutter additionally receives robot/debris and
player/debris pairs; Redux dispatch disables both. This is an outstanding broader
behavior difference, not evidence that those pairs have been ported. No debris
collision behavior was enabled here. Audible playback, earned traversal and
matched reference execution remain separate qualifications.

### Qualification checkpoint: no-damage guided control transitions

Added a native sequence using normal weapon/item dispatch to launch a guided
missile, place the player in protection, attempt fresh fire, leave protection
and release, then relaunch and exercise an already queued event in protection.
Fresh protected fire preserves missile bytes, guidance, ammo and cooldown;
ordinary exit permits release. Queued protected fire releases and updates the
cooldown before ammo processing, without another projectile or ammo expenditure.
The first fixture omitted guided ownership despite supplying ammo; diagnostics
showed normal autoselection to concussion. Corrected the fixture's ownership
flag and reran successfully, without changing autoselection or engine behavior.

Native build and the descriptor integration test pass (4.58s), including the
required guided marker and prior no-damage/layout/resource rejection controls.
This slice adds tests/evidence only. Boundary placement and queue injection are
explicit controlled setup, not ordinary flight or recorded replay. Guided
rendering, physics traversal, save/restore and matched reference execution remain
open, as do charged-fusion timing and the other mission admission prerequisites.

### Implementation checkpoint: charged fusion across protection

Full GameProcessFrame controls reproduced lost fusion fire: the timer expired
inside protection, retaining charge/queue, but Redux FireLaser replaced the
queue with zero on exit when input was released. Reference FireGun accumulates
input without clearing the existing queue, so ChargeFusion processes it on exit.
Added a narrow XL adapter retaining charged pending fusion for detected profiles
containing no-damage segments. Multiplayer and profiles without that property
retain their existing behavior; no extra persistent state or named exception
was introduced.

The passing frame sequence retains charge and energy inside protection, charges
one additional frame on exit, then emits two bolts with the retained charge
multiplier. It checks both that frame's charge energy and the normal authored
emission cost. Initial assumptions of no exit/emission cost were corrected from
the reference code and native diagnostics. A neutral-property negative control
verifies classic queue replacement. The fixture initializes loaded multiplayer
start markers for runtime and suspends robot AI; this is controlled timing and
placement, not an earned flight or original Observatory gameplay qualification.

Initial charging, overcharge cadence, switching weapons while protected,
save/restore and matched reference execution remain open. Function 13, normal
17/36 admission and descriptor gameplay remain blocked independently.

### Qualification checkpoint: fresh fusion input and cancelled queues

Extended native full-frame controls with three held-input frames inside a
protected segment. Charge, firing queue, timer and energy remain unchanged with
no new fusion projectiles. A manual primary switch with charge/queue pending
then verifies that native selection cancels the charge/queue, the next protected
frame cancels the timer, and leaving protection does not resurrect a projectile
or spend energy. Native build and descriptor integration pass (5.31s).

The source audit found an additional behavior difference: XL DoSelectWeapon
refunds current fusion charge before any manual primary selection, even before
same-weapon/availability checks. Redux selection clears charge on switching
without refund. The reference code does not gate this policy on an XL level or
no-damage property. Recorded it as a separate unported selection policy, not as
no-damage parity or a silent engine change. This test-only slice preserves the
native policy while proving the recent pending-queue adapter cannot resurrect
a cancelled shot. General selection-policy support, overcharge timing,
persistence and matched reference runs remain open.

### Implementation checkpoint: mission identity allocation before teardown

Resource-path review identified two post-teardown allocations in load_mission:
the Mission object and its duplicated path. Opt-in builds now reserve a zeroed
Mission and PATH_MAX path storage after descriptor preflight but before Android
asset preparation or free_mission. Android resolution fits the reserved storage;
preparation/activation failure releases the unused identity. Ordinary builds
retain their existing loader path; D1's separate engine is unchanged.

Headless fault injection independently refuses the object allocation and path
allocation for a valid descriptor. Both reject while preserving the exact prior
Mission pointer/path, archive owner, gameplay definitions and usable owned
resource baseline. The descriptor integration test passes with a required
identity-allocation marker. Existing accepted selection and D1-in-D2 recovery
controls pass as part of that test.

This closes only the identity-allocation failure window. Android activation can
still fail after teardown, and full rollback for later graphics/resources/native
initialization remains open. Descriptor gameplay remains guarded; no new mission
or feature is admitted by this change.

### Implementation checkpoint: native companion archive preflight

Opt-in native descriptor preflight now probes a visible companion HOG at a
private PhysFS mount point without exposing its members at the game root.
Unreadable archives and archives mounted by a foreign owner reject before
identity allocation/teardown. The current mission's own archive is accepted for
reselection; absent optional companions still permit loose-level missions.
The staged descriptor records whether a HOG was present. Commit now rejects a
lost/unmountable companion rather than silently proceeding with missing assets.

Native controls exercise corruption, repair, owned reselection, foreign mount
ownership and removal between preflight and commit. Preflight failures preserve
the prior Mission, archive, definitions, owned baseline and complete search-path
contents/order. Repaired/reselected archives become the actual owned resource
source. Late removal explicitly fails, clears the partial mission identity and
allows loading the original mission again. That last case is recovery, not
rollback to the previous playable mission.

This covers companions visible in native descriptor lookup. Android catalog
mount preflight, alias resolution and later resource initialization retain their
separate boundaries; no blanket transaction or gameplay admission is claimed.

Validation completed: native build and scoped formatting pass; all seven XL
tests pass (19.95s), Counterstrike lifecycle passes (16.13s), and Android XL
x86_64 assembleDebug succeeds (1m 32s). These are host/runtime and compile
checks, not a new on-device traversal or reference presentation comparison.

### Remaining release gates after the companion archive checkpoint

1. Complete mission/resource transaction ownership and rollback across late
   activation and native initialization failures. Do not remove the descriptor
   gameplay guard based on successful preflight alone.
2. Qualify detected water through original-mission Android traversal, saves,
   and visual/audio reference comparisons. Robot water remains rejected.
3. Finish Counterstrike level-two ordinary progression. Earned level-one exit
   and the level-two blue key have evidence; the red-key route still fails.
   Keep that acceptance test failing until gameplay or controller evidence
   resolves it; a successful lifecycle test does not supersede it.
4. Qualify original Observatory only after resource transactions, 17/36
   translation, no-damage persistence and reference behavior are established.
   Controlled neutralized-layout fixtures do not qualify the original mission.
5. Expand additional formats and feature families only after these gates have
   an explicit supported profile and mission-level evidence.

### Implementation checkpoint: native archive ownership handoff

Native opt-in descriptor selection now acquires its final root mount before
free_mission rather than reopening the archive after teardown. The resolved
archive path (or absence) belongs to the staged descriptor. Reselection transfers
the existing owned mount, including open members. When switching archives, an
unmount failure on the previous archive removes the candidate mount and rejects
before clearing mission identity, definitions or the owned resource baseline.

Native integration controls now require late corruption and deletion to preserve
the prior mission and complete search-path order. Holding an original archive
member open forces failed switches to both a replacement archive and a loose
mission; both retain the old state. Closing the handle permits the replacement,
and reselection with an open member preserves readable source bytes. The native
build and descriptor integration pass (6.53s).

This supersedes the earlier native late-loss cleanup limitation. Android catalog
activation and later native resource/graphics initialization are still separate
transaction boundaries; descriptor gameplay remains guarded.

Final validation for this handoff: all seven XL tests pass (21.13s), Counterstrike
lifecycle passes (18.76s), scoped formatting passes, and Android XL x86_64 builds
successfully (1m 47s). No on-device traversal was added by this change.

### Android transaction boundary audit

The Android path cannot inherit the native HOG handoff alone. Successful
android_mission_assets_prepare calls android_mission_asset_reset_before before
returning to load_mission. That hook stops audio, frees sound/bitmap replacements,
menu backgrounds and GL/model caches, closes gamedata, and resets PIG tables.
load_mission then frees mission identity before android_mission_assets_activate
detaches the old catalog mounts and acquires the new ones. Existing native
catalog controls use reset spies; preserving their mounts would not prove that
the real prior mission remains playable after these resource resets.

The next transaction implementation must acquire candidate resources before
this reset boundary or retain a restorable engine resource bank. It must handle
overlapping catalog mounts and their precedence, not just independent HOGs.
Acceptance needs the real Android reset hooks and a resumed prior mission after
failure, including bitmap/model and sound access. Keep the descriptor guard until
that evidence exists; do not count mount-only restoration as gameplay rollback.

### Current Android water regression checkpoint

Installed current XL x86_64 APK SHA256
55f06251bfb34b1970aadc3313962d311245190e6517fcc4af16ff26d7008fab
on emulator-5554 only. The original Robotech integration passed all 57 steps
in 66.039s, run ba23912f052f445e8c48cbb232e3bd57. Evidence is under
temp/xl-water-android, with runner output temp/xl-water-current.log. The owner
runner exited zero after restoring its backed-up state; its backup directory
was absent after completion.

This refreshes Android evidence after the resource/descriptor changes: controlled
portal entry/exit with thrust, water motion, live mixer-loop state, wet checkpoint
restore, stock mission transition, cross-mission restore, and helpful rejection
when the experimental profile is disabled. Starting poses are injected. It does
not qualify spawn-to-water traversal, heard audio or rendered reference parity,
robot fluids, or failed-selection rollback through real resource reset hooks.

### Water spawn input replay

The native original-arena test now records the existing ordinary-control flight
from the authored dry spawn to water segment 187. It exports a private
preview-user/xl-water-spawn.dximdemo and player profile. The recorder recognizes
the untouched start as new_level, so no injected pose or mid-level checkpoint is
needed. Export checks the mission, level, start mode and complete frame count,
and ignores recordings left by previous runs.

The same CTest now launches a fresh native process to consume those 1,235 ordered
input frames through the normal replay dispatcher. It requires a terminal result,
arrival alive in water, and a full embedded-trailer match with no reported
mismatch. The integration passes (8.99s). Replay setup initially used the wrong
success convention for input_demo_start_loaded_replay (zero means success);
corrected the test caller without changing replay behavior.

This creates a portable traversal fixture for the next Android check. Native
replay is verified; Android replay, visual/audio comparisons, and traversal
beyond the existing water destination remain unverified. Private outputs remain
under temp/xl-stack-review/water-arena; no mission bytes are checked in.

### Android spawn replay integration

Extended test_xl_water_runtime.ps1 with optional Replay and ReplayOnly modes.
Replay preserves the existing game-state backup, launches through SetupActivity,
and requires structural equality of the complete terminal JSON against the
native recording trailer. It saves expected/actual results and logcat, rejects
missing terminal output, and notices early game-process exit. The standard
57-step controlled-pose check remains available separately.

Initial integration exposed two startup constraints rather than gameplay
divergence. Native replay validates the full recorded projection; the original
640x480 capture rejected on the 1920x1080 emulator. The water fixture now records
and independently replays at 1920x1080 with matching pixel aspect, without
editing recorded metadata or bypassing projection validation. A targeted Android
breadcrumb identifies projection rejection before engine exit.

Android intentionally ignores d2x.ini. Its experimental water opt-in must be
explicit at startup, not placed in that file. SetupActivity now forwards an
xl_water_physics boolean only with a replay request; MainActivity consumes it
once, and only DXX_XL_PILOT native builds translate it to the existing flag.
No mission identity exception or broader admission rule is introduced.

Final combined Android run passed on emulator-5554: all 57 controlled-pose/save/
rejection steps (61.086s, run 7daeb92a706b4f2c9ea6097ab85ac97a), followed by the
1,235-frame original-spawn replay. Full terminal JSON equality covers mission,
level, difficulty, game time, player inventory/status, position/orientation and
level summary. Arrival is segment 187 at fixed-point position
(-655809, -3376281, 18379252), game_time64 1348949. No pose action runs during
that replay. This is rendered Android simulation evidence, not a comparison of
rendered pixels or heard audio with XL.

The first successful replay left the engine's ai_schedule_probe.log beside its
result. The owner runner now copies that diagnostic and deletes the exact owned
file during cleanup. The final combined runner exited zero, restored its backup,
and the backup directory was confirmed absent. Expected/actual results, replay
logcat, AI diagnostic and baseline step evidence are in temp/xl-water-android;
the runner log is temp/xl-water-spawn-android.log.

APK SHA256: 5e2a860011420bbfdad4789e494cda7b19ddfd0277bbb1434195ad36742ab9dd
Recording SHA256: 3fb3250ea19ec658f499e272f1a36276b7204d553a3e9ea32892e930819f8431
Native 7/7 tests passed (34.70s), Android build passed (2m 15s), scoped formatting
passed, and both automation catalog checks passed. Android transaction rollback,
broader traversal/presentation evidence, Counterstrike red-key progression and
original Observatory admission remain open.

### Owned descriptor requirements

Found a remaining split in descriptor ownership: native selection used staged
fields, but validate_requirements reopened the descriptor to read resource
declarations and unknown options. A disk edit could therefore change admission
under an already-selected mission. Opt-in selection now captures the bounded
descriptor once, normalizes DOS/Mac line endings and rejects embedded NUL bytes.
Both native parsing and later XL requirement checks consume that owned snapshot.
Built-in D2 descriptors use the same snapshot parser when loaded; ordinary builds
and D1-in-D2 descriptors retain the existing file parser. No Mission struct
layout change is required.

The original-water integration edits its private descriptor after selection:
the active mission remains admissible until reselection acquires the unsupported
option. Restoring disk bytes cannot bypass that selected requirement; a fresh
selection restores admission. Failed-selection tests retain the previous
snapshot, and mutation between preflight and commit cannot replace it. Added
DOS/Mac and embedded-NUL controls. The DOS fixture initially wrote CR-CR-LF
through Windows text translation; switched fixture output to binary so the
intended line-ending bytes are tested.

Native build, all seven XL tests (20.12s after the final D1 parser guard), and
Counterstrike lifecycle (15.34s before that D1-only guard) pass. Android build
passed (41s). This closes descriptor requirement mutability, not later resource/graphics
rollback, missing companion provenance or descriptor gameplay qualification.

The Android rerun exposed a controlled-pose test assumption: after releasing
thrust, coasting and ship wiggle can leave waterfall contact. The immediate
assertion saw zero loops; the following state capture saw one linked and playing.
The fixture now reacquires its documented portal pose and waits for linked and
playing audio before saving. It still requires sound during real thrust and
cleanup in dry space. This does not change production audio or the separate
ordinary-input spawn replay, and does not establish uninterrupted contact sound.

Final Android rerun exited zero: 57/57 steps in 49.593s (run
816d2f8631a44d5e909ff19979585d87), followed by all 1,235 spawn-replay frames
matching the complete native result JSON. Backup restoration completed and the
test backup directory is absent. APK SHA256:
c0137c14073659f6ed5f1376813d0861ea243764726585e54e4393a32bedd3ac.
Recording hash is unchanged from the preceding checkpoint. Evidence is in
temp/xl-descriptor-snapshot-device.log and temp/xl-water-android. Scoped fixture
formatting and both automation catalog checks passed. This is still rendered
simulation and mixer-state evidence, not an audible or visual reference comparison.

### Acquire Android catalog mounts before resetting engine data

The publisher gives each mission a separate context directory and archives under
that directory. Preparation now acquires the candidate's complete mount set at
the game root before calling the destructive graphics/audio reset. Allocation
of ownership bookkeeping precedes acquisition. If a mount fails, only the newly
acquired mounts are removed, preserving the previous search order and context.
Activation transfers these acquired mounts rather than opening archives after
free_mission. Reselection retains its existing mounts, including open handles.
Cross-context overlapping mounts are rejected as ownership conflicts; the actual
publisher produces disjoint contexts. Base transitions still release the old set.

Native integration uses the real Android catalog/PhysFS implementation with reset
spies. A hook removes the archive between probe and acquisition, after a preceding
candidate directory can mount. Rejection preserves the original owner, generation,
search order and both reset counts, and leaves no candidate mounts. Repair/retry,
nested-preparation rejection, single-use activation, cross-context overlap rejection
and reselection with an open resource handle pass. All seven XL tests passed in
22.72s; native build and scoped formatting passed.

This closes the candidate catalog mount-acquisition window, not the whole Android
transaction. Old-mount release failure remains fatal; later resource initialization
and loose companion-HOG acquisition still need transactional handling. Directory
mounts do not freeze their contents. Real-device resumed graphics/audio after an
injected failure is not yet verified by these reset spies. The existing campaign
isolation runner mutates imported mods and publications without full restoration;
use a preserved device fixture before running it for this change.

Android x86_64 XL-enabled APK build also completed successfully. No new device
runtime claim is made for this mount change; the preceding water replay tested
the descriptor-snapshot APK, not this one.

### Live Android acquisition-failure probe

Extended the preserved water runner with two private directories and a descriptor
inside its exclusive backup directory. A debug + Android + XL-only automation
probe appends a temporary in-memory catalog context, injects failure on its second
mount after the first succeeds, and uses the production preparation/cleanup path.
It verifies unchanged owner index, mount list, search order and generation, no
retained candidate mounts, and no pending transition; the temporary catalog entry
is then removed. Production warning output remains enabled. This is controlled
fault injection, not a claim that the filesystem spontaneously failed on Android.

The device sequence now has 59 steps. After rejection it checks the loaded water
profile, segment, shields, running simulation and linked/playing waterfall audio,
then continues authored portal movement, wet checkpoint restore, stock transition
and cross-mission restore. The separate original-spawn replay remains unchanged.
The owner runner restores existing player/config/save state and removes only its
owned probe files/directories. Android build passed (1m 16s). All 59 Android steps
passed in 66.873s, run dcc5887d818b47cebf4b5d309717a46c. The acquisition marker
confirms the injected branch ran; step 26 verifies live water/audio state afterward.
The old mission in this device test is a loose water mission with base catalog
ownership. Retention of an already-mounted imported context is covered by the
native test, not claimed as a new device result. The separate 1,235-frame spawn
replay matched the full native result JSON. The combined runner exited zero and
its backup directory was confirmed absent after restoration. Scoped formatting,
both automation catalog checks and git diff --check passed. Evidence:
temp/xl-acquisition-device.log and temp/xl-water-android. APK SHA256:
bb7d97c5208dd516acea6bee40ea275ef09215a092ff298c28b3b8a3021f62be.
No screenshot or audible reference comparison is implied by these results.

### Red-route evasion measurements

Added read-only damage-time measurements of actual lateral speed and four
ship-radius clearance rays (right/left/up/down). The observation-only run
88a0815319b5 preserves blue completion at 7,589 frames and the same red death at
10,786 frames. Several red damage events occur with nearly zero clearance in the
requested dodge direction and collapsed lateral speed. This supports investigating
the controller's fixed left/right oscillation; it does not establish missing XL
physics or prove that wall avoidance will fix the route.

An experimental normal-input controller retained a clear lateral/vertical dodge
direction until clearance dropped below four units, then selected the clearest
direction. Run 6be920d0629d died earlier, at red frame 3,380 in segment 52 with
17 kills and no generator activation. Reverted this unsuccessful controller
change; retained only read-only diagnostics. Evidence is under
temp/xl-compat-ninja/level-two-route-tests. Aim alignment changes sharply during
that encounter, but target identity was not recorded, so target switching remains
an untested hypothesis. No damage, difficulty, inventory or admission rules changed.
The red-key acceptance remains failing and is not marked expected-failure.
After reverting the experiment, native build, scoped formatting and diff checks
pass; the earned campaign-transition regression passes in 18.38s. This change is
test instrumentation only, so no new Android gameplay result is claimed.
