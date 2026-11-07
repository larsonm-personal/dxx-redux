# Pumo late-campaign combat and authored progression

Status: planning only, November 7, 2026. The initial commit contains this document
and no implementation or asset changes. Intended implementers: Sol agents at
medium reasoning, one bounded ticket and reviewable child branch at a time.

## 1. Decision and intended outcome

Prioritize **ordinary-player combat, object/wall event chains, and earned exits
in Pumo's remaining Mineral Valley route and Coldrom alternate branch**.
Target the main route from the earned level-7 arrival through levels 8, 9 and 10,
and the alternate earned level-14 arrival back to level 5. Fix reusable XL
semantics when a fresh, source-backed failure demonstrates a missing consumer.

This is a progression-driven feature tranche, not a proposal to implement another
large subsystem speculatively. The current evidence establishes a qualification
gap, not a proven missing runtime feature in these five levels. A successful run
with no engine changes is a useful result. Do not manufacture a feature patch to
meet a quota. The first ticket must identify any real blocker before a worker
changes gameplay.

Why this area:

- The existing earned native chain ends at Sophodo's level-7 destination save.
  Coldrom's current test ends after its Fusion pickup, door effects, weapon
  selection and persistence. These are concrete continuation points, not another
  hub/PMC replay. See `android/XL_compat/evidence/original-sophodo-native.md` and
  `original-metillis-native.md` in the same directory.
- The late levels combine existing XL features in gameplay: large trigger sets,
  master/enable/disable chains, authored actor events, spawn selectors, seismic
  actions, boosts, teleports, custom combat definitions and countdowns. Closing
  their interactions is more directly relevant to Pumo than a new unrelated
  format or decorative renderer feature.
- Historical `modern-layouts.json`, `seismic-originals.json` and catalog prose
  contain obsolete unsupported labels. Later original level-9 lightning,
  interlude and PMC evidence already supersedes some of them. Decode counts are
  useful reconnaissance; those old labels are not a current blocker inventory.
- Slow-motion remains valuable general XL work and an identified Warp Shock
  dependency, but no inspected evidence makes it a Pumo late-route prerequisite.
  This tranche must not turn on pickups 48/49, add gameplay slow-motion controls,
  or broaden clock consumers just because it inherits that stack.

Success means reproducible, exact-candidate, ordinary-action route segments with
fresh-process persistence and stock isolation. It does not mean full connected
human traversal, all secrets, every difficulty, phone acceptance, visual/audio
parity or a completed fourteen-level campaign.

## 2. Stack and immutable starting point

Repository: `larsonm-personal/dxx-redux`.

- Parent: `fix/xl-render-clock`, PR #398, commit
  `c0263f8eaead6f76f801f8f35906836acf21d3f7`
- Its open dependency chain is #390 -> #391 -> #392 -> #393 -> #394 -> #395 ->
  #396 -> #397 -> #398. The observed `cmake` head was
  `d5a9e61047e294dff8066393808de698b8742529`
- Plan branch: `plan/xl-pumo-late-progression-20261107`, directly above #398
- PR #405 (`fix/xl-original-fixture-progress-20261107`,
  `cbf7d1ee37ed78d7516d74045ba3839df3e400c1`) is a separate `cmake`-based texture
  fixture/diagnostics repair, not the feature stack tip. Do not silently fold it
  into this branch or retarget this plan to `cmake`
- PR #384's rendered-readiness tool is also separate. Do not assume it exists
  on this stack or silently import it to turn a native probe into a render pass

Recheck the remote parent and task ownership before starting implementation.
If the stack changed, record the new relationship and ask the coordinator which
base to continue; do not force-push, rewrite existing branches, merge PRs, or push
to `cmake`. New child PRs should be drafts. Use a linear integration chain even
when reconnaissance happens in parallel, and keep test repairs separate from
production semantic fixes when practical.

Read `AGENTS.md`, `.github/copilot-instructions.md` and relevant local skills
before edits. No APK building, device installation or user-profile manipulation
is needed for the native implementation tickets. A later device-validation task
needs its own agreed scope. Do not modify trusted CI, assets repositories,
readiness cohorts, original archives or unrelated slow-motion work here.

## 3. Evidence ledger and assumptions to verify

Planning inspected committed code and evidence at the parent SHA and the live PR
stack. It did not obtain original Pumo resources, run gameplay, or requalify any
historical binary. The repository's earlier Windows/Android results remain
historical evidence until reproduced on the implementation candidate.

### Original identity and ownership

The current `runtime/pumo_player_test.h` requires these SHA-256 identities:

- `missions/pmines.hog`:
  `6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043`
- `missions/pmines.mn2`:
  `45b4138e0a0d49ab8ee09334b7cfa97c0ec75ac6c6b6406b21eba28ddaa19cfd`
- `mods/pmines/pmines.vham`:
  `a623c7a909e6a7b546816f5082e57d33a19fc4b59f84bb3f8493df3ac86b6b05`

The same probe checks 78 robot definitions, 65 weapon definitions, robot 68's
model 17, owned model 171, and mission HAM/HXM/VHAM/OOF ownership. Preserve these
checks and verify the full consumed-resource manifest, including models and
skins. Original Pumo owns its definitions; substituting retail Vertigo data
would invalidate the result. Keep resources read-only and raw logs, saves,
extracted levels and images private. Public reports contain bounded metadata,
hashes and behavioral outcomes, never proprietary resources.

### Continuation map

Reconfirm ordinal-to-member mapping from the original MN2, not HOG member order.
The expected targets below follow committed route evidence and historical member
inventories. Counts are static observations, not fresh admission results.

| Target                     | Current useful evidence                                                                                                                               | What is still needed                                                                                                 |
| -------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------- |
| 7, expected `tulev1.rl2`   | Earned arrival from Sophodo; historical 80 wall triggers, 3 object-trigger records, 14 matcens; seismic records 63 and 66                             | Actual required objectives, earned combat/key/event route and normal exit to 8; repeated/high-index trigger evidence |
| 8, expected `tulev2.rl2`   | Historical 42 wall triggers, 3 object-trigger records, 5 matcens; controlled mission-flow coverage is not earned progression                          | Earned entry from 7, actor-dependent gates/combat/objective, exit to 9 and cold replay                               |
| 9, expected `tulev4.rl2`   | Historical 55 wall triggers, 1 object-trigger record; 5 teleport and 26 boost actions; original lightning lifecycle evidence                          | Earned entry, required combat/gates, actual teleport/boost escape and exit to 10                                     |
| 10, `mvend.rl2`            | Historical 19 segments, 5 walls, ambient owner and automatic message; `pumo-interlude.md` records no exit trigger                                     | Earned arrival from 9, original terminal message/ambient state and save restoration; never invent an exit or reactor |
| 14, `mvlev5.rl2` (Coldrom) | Earned entry from Metillis alternate; existing 15-step pickup/selection/persistence control; historical 26 wall triggers and 4 object-trigger records | Required combat/reactor, authored exit back to 5, cold replay and branch rejoin with actually earned inventory       |

Read `original-majha-native.md`, `original-metillis-native.md`,
`original-iquaxel-native.md`, `original-sophodo-native.md`,
`pumo-interlude.md`, `lightning-simulation.md` and `PUMO_PLAYTEST.md` before work.
Preserve their distinction between original resources, derived controls,
retained APKs, controlled exit dispatch and normal earned progression.

### Concrete probe limits at the parent

- `android/XL_compat/CMakeLists.txt` registers Majha -> Metillis -> Iquaxel ->
  Sophodo with CTest fixtures, but no earned successor after Sophodo
- `tests/pumo_sophodo.cmake` preserves `sophodo-main-level7.sav` before its cold
  replay overwrites generic filenames. Use that warm destination as provenance
- `tests/pumo_metillis.cmake` preserves `coldrom-entry.sav` from earned alternate
  arrival, then runs `pumo_coldrom_pickup.json`. It does not prove Coldrom's exit
- `runtime/pumo_player_test.h::run` already supports primary press/release and
  `select_primary` via `do_weapon_select`. Fusion does not require a new firing
  primitive. A held primary button is not proof that a charged shot was released
- The native probe uses real `GameProcessFrame`, enemies, ammo and damage, but
  fixed local poses and optional scripted view aiming. It skips presentation and
  uses a real GL context for admission without normal rendering
- Its `last_activation` changes only when `xl_preview_trigger_mask()` changes.
  `runtime/preview.cpp` exports only indices 0..63 through that mask, while the
  actual activation storage/accessor supports more. Repeated activations also
  leave the mask unchanged. Level 7's recorded 80 triggers make this a concrete
  observation limitation. It is not evidence that gameplay ignores trigger 66
- `xl_wall_trigger_generation()` returns exit generation, not a general trigger
  event sequence. Do not repurpose it and break its existing collision callers
- `runtime/trigger.cpp::operate_wall` uses cached `timer_query()` for the 750 ms
  top-level debounce. Preserve `timer_update()` and real-time pacing where needed;
  the prior Sophodo failure was a probe clock defect, not a trigger rule defect
- PR #398 makes XL Omega depend on actual render boundaries. A GL context alone
  does not advance that clock. If an authored required route uses Omega, qualify
  it through real rendering or report that probe boundary. Never synthesize render
  ticks, switch to a granted substitute weapon, or disable the guard to get a pass

## 4. Invariants for every implementation ticket

1. No level-name or hash-based production gameplay exceptions. Hashes identify
   test originals; generic capabilities and validated resource ownership govern
   runtime behavior. No edited originals, dropped objects, direct trigger/exit
   calls, health/ammo/key grants, invulnerability or suspended enemies in earned
   route acceptance. Explicitly derived fixtures may isolate semantics separately
2. Keep new XL policy/state in `android/XL_compat/`. Use small guarded native hooks
   only after demonstrating the failing production consumer. Audit D1 and D2
   callsites when touching shared code; do not broadly deduplicate the engines
3. Preserve classic and multiplayer behavior, ordinary save layout, stock
   definition tables, RNG consumption and timing. Never replace global FrameTime
   or use slowed time for every subsystem. Inherited slow-motion admission remains
   unchanged. A neutral XL clock is not necessarily a classic-semantic path
4. Respect pre-release XL-save policy: if a demonstrated production change needs
   new persisted state, replace the current schema with validated new state and
   no speculative migration. Do not bump it for diagnostic-only probe changes.
   Do not reuse old earned saves after a schema change; regenerate the chain
5. Use new disposable profiles/output directories, shared read-only resources,
   bounded processes and the existing retention/reserve helper. Never point a
   probe or cleanup at a user's pilot directory. Preserve failed evidence and
   before/after original hashes; do not erase failures by retrying until green
6. A runtime failure must be classified as setup/resource, test expectation,
   input/pose, observation, unsupported behavior, or actual engine defect. Keep
   the failing command, executable SHA, asset hashes and first divergent state
7. Assertions describe authored prerequisites and effects, not a final position
   alone. Check object consumption/ownership for pickups rather than net shields,
   since real combat may immediately reduce the shield total
8. No score inflation: leave sampled readiness, trusted FAST membership, public
   contract requirements and their budgets unchanged. Progression has its own
   ledger and never changes 4/8 or any other readiness score by inference

## 5. Ordered Sol-medium work packets

Each packet ends with a small commit, exact commands/results and a handoff.
A coordinator reviews before advancing dependent work. Estimate work by outputs,
not overnight duration; asset/setup failures can dominate elapsed time.

### P0. Refresh original requirements and reproduce the earned baseline

Dependencies: plan branch only. No production changes.

1. Resolve authorized original Pumo/retail inputs and pinned XL reference source
   revision `2576a98b5de310e3ac7c9777df2677d1627b8bae`. Use existing
   `tests/reference_sources.py` pinning and adapters, not unpinned internet code
2. Build the exact parent candidate with supported host setup. Record compiler,
   configuration, engine path/SHA and resource manifest. Re-run the original
   earned Majha/Metillis/Iquaxel/Sophodo fixture chain, preserving actual level-7
   and level-14 entry files and their producer receipts. If the inherited stack
   fails, compare against the observed `cmake` baseline to localize it; report the
   regression instead of rolling it into an unrelated feature patch
3. Use current `xl_inspect`, `admission.cpp::runtime_blockers` and real loader
   resource checks to produce a fresh 14-level requirement ledger. Include
   per-target actors/objectives, trigger links/flags/times, normal activation,
   required weapon/pickup definitions and authored destination. Do not execute
   artificial exits for earned-route credit. Preserve historical JSON unchanged
4. For 7/8/9/14, derive a local sequence of required gates and safe approach poses
   from original geometry and source semantics. Distinguish required progression
   from optional branches. Record an unresolved field rather than guessing a
   key color, reactor count, actor identity, countdown or exit number

Deliverables: concise fresh evidence file and a per-route contract; verified
entry save hashes/private receipt locations; explicit next executable probe.
Go/no-go: missing originals or failed baseline blocks original qualification,
not permission to downgrade it to synthetic coverage. Asset-free probe work can
continue, but label originals unrun. If a genuinely missing feature emerges,
write a narrow source/consumer/test ticket under P5 before changing it.

### P1. Make late-route observations complete without changing gameplay

Dependencies: P0's code/record findings; private baseline can be unavailable for
public negatives. Main files: `runtime/pumo_player_test.h`, existing native
probe tests and receipt checks in `tests/pumo_player_phase.cmake`.

1. Remove the 64-bit activation mask as the sole reason to refresh old-level
   trigger evidence. Prefer full-range read-only state already exposed by
   `xl_preview_trigger_activated` and `xl_wall_trigger_state`. Include repeated
   state changes relevant to the contract, not just first-ever hits. These APIs
   expose latched/scheduled state, not a successful-dispatch counter. A repeated
   permanent action can leave both unchanged; elapsed age is not event evidence
2. Specify the receipt boundary explicitly: pre-frame, post-frame, or pre-exit.
   A pre-frame snapshot cannot prove an action later in the same frame. Keep
   normal destination arrival independent of the old-level snapshot. If a required
   final event is lost during teardown, or a required successful repeat has no
   distinguishable authored effect/state, add the smallest developer-only
   successful-operation observer at the correct existing boundary. Count one
   operation once: both native switch dispatch and XL dispatch can report the
   same action through `xl_preview_trigger_result`. Do not build a general event
   bus, infer success from an attempted call, or change gameplay dispatch order.
   Keep unavailable final fields unknown
3. Record all local repositioning and view aiming as assists, and preserve the
   earned entry's hash. If multiple checkpoints in one level are needed, copy
   them to distinct phase-owned files with receipts before the generic slot is
   reused; avoid a production save-format change for test naming
4. Keep receipt validation strict: exact action count, source/destination, normal
   transition count, executable/plan/input identities, fresh-process evidence,
   failure/timeout distinction, and required state. An old file in a reused output
   directory must not satisfy a missing newly generated result

Acceptance: asset-free negatives cover trigger indices 63/64/66/79, repeat
activation, no activation, state reset/load, a transition in the same frame,
missing/old/truncated receipt and wrong save hash. Exercise the actual observation
producer with a public or explicitly derived fixture, not just a fabricated JSON
parser test. Distinguish one successful dispatch, two successful repeats, a
debounce-rejected attempt and an idle frame, including duplicate-report negatives.
Existing Majha..Sophodo assertions remain intact. No production
trigger, clock, countdown or actor behavior changes. Do not widen timeouts.

### P2. Finish Coldrom's authored alternate route and rejoin

Dependencies: P0, P1 if its observations are needed. Separate proposed owner
`xl_pumo_coldrom_player`; extend the Metillis fixture relationship without
changing its historical acceptance criteria.

1. Resume Metillis's exact earned `coldrom-entry.sav` into a new profile. Verify
   its health, energy, inventory, objectives, original definitions and position
2. Retain the existing Fusion pickup, three door effects, auto-selection,
   unowned-weapon rejection and save/load controls. Use ordinary press, bounded
   charge, release and subsequent simulation when testing Fusion. Observe actual
   shot creation, energy use and reactor/actor damage; selection alone is not fire
3. Determine required combat and reactor behavior from P0, then earn them with
   available weapons and original pickups. Do not assume stock Gauss cadence or
   substitute Vertigo actors. Do not require Fusion to kill the reactor unless
   original behavior makes that a requirement; qualify its authored pickup/door
   and firing behavior separately from the valid earned combat route
4. Follow the original master/countdown/boost/exit path with ordinary inputs to
   level 5. Keep the escape flight continuous after its declared starting pose
5. Save before the critical earned event and at the destination. In a separate
   process restore the exact checkpoint, repeat the exit, and verify destination
   state/mission ownership. Continue a bounded existing Iquaxel gate from the
   branch-derived save to prove rejoin, rather than substituting the direct
   Metillis->5 save. Adapt expected inherited inventory only from earned evidence

Acceptance: warm route and cold replay, actor/trigger prerequisites, authored
countdown if present, normal 14->5 transition, destination save/load and branch
provenance. Keep input/position assumptions and any actual engine defect separate.

### P3. Extend the main chain through levels 7 and 8

Dependencies: P0/P1. P2 is independent until integration. Proposed owners
`xl_pumo_tulev1_player` and `xl_pumo_tulev2_player`, named only after the MN2
mapping is confirmed. Mirror existing JSON/CMake owners rather than adding a
second route framework.

- Add a fixture successor consuming `sophodo-main-level7.sav`, not a fresh level-7
  pilot with granted inventory. Require source save/receipt identities
- Implement one small gate group at a time: locked/absent-key negative, normal
  pickup or combat, ordinary shot/contact activation, changed gate, physical
  crossing, then checkpoint. Compose the passing groups into a bounded combined
  route with no inventory reset between them
- For level 7, explicitly cover required high-index events, master ordering,
  enable/disable state, original source-defined seismic behavior and relevant
  matcen/respawn interactions. Type 28 defines respawn points, not an immediate
  teleport. Require only authored critical-path operations, not all 80 triggers
- For level 8, prioritize the original object-trigger owner and its damage/death
  threshold, pending effects and target state. Verify exactly-once/repeat behavior
  where required; a wall mask cannot prove an object event. Trace its current
  consumer in `runtime/object_trigger.cpp` before diagnosing it
- Qualify the normal 7->8 and 8->9 exits, saved destination ownership and cold
  replay from each earned critical checkpoint. Preserve original difficulty
  and resource economics; use bounded normal aiming/flight instead of grants

Acceptance per level: exact original contract, earned entry->exit, relevant
negative controls, warm/cold persistence and explicit fixed-approach limits.
If one critical operation fails, stop that route, retain evidence and use P5;
independent Coldrom work can continue. Do not relax the operation to reach level9.

### P4. Reach the level-10 interlude by normal level-9 progression

Dependencies: P3's earned level-9 save.

1. Reuse original level-9 lightning/resource evidence without reimplementing it.
   Derive required gates and target order from the fresh level inventory. Test
   actual physical teleport/boost crossings and ordinary actor restrictions
2. Preserve energy/shields/keys, authored countdown and normal 9->10 exit27 if
   P0 confirms that index. Verify any chained side effects before transition
3. At level10, require the original automatic message 1 decoded from `mvend.msg`,
   ambient owner identity/state, preserved earned arrival and disk restoration.
   `pumo-interlude.md` establishes a terminal interlude with no exit trigger; do
   not invent a level11 transition or count the PMC chapter as earned by this path
4. Keep rendering/audio assertions in their actual owners. The native `-nosound`
   route can prove message/state selection, not audible playback or visible text

Acceptance: earned 9->10 and cold replay, terminal authored state and save/load,
no forced ending. Summarize what the main route now proves and what remains
unqualified instead of reporting a blanket fourteen-level pass.

### P5. Fix one demonstrated XL semantic defect at a time

Conditional ticket; instantiate only from a P0/P2/P3/P4 failure.

The coordinator supplies a short issue packet containing original operation,
minimal repro, pre/post state, expected pinned-source behavior, actual divergent
consumer, relevant lifecycle state and classic invariant. Likely entrypoints:

- Event order/actor semantics: `runtime/trigger.cpp::operate_wall`,
  `runtime/object_trigger.cpp`, `runtime/trigger_save.cpp`,
  `d2/main/switch.c`, `d2/main/collide.c`
- Combat/pickup: `d2/main/laser.c`, `d2/main/weapon.c`, `d2/main/powerup.c`,
  mission definition/model preparation and existing HXM/camera weapon controls
- Movement/countdown: existing `runtime/teleport.cpp`, speed-boost consumer,
  `runtime/reactor_countdown.cpp`, `d2/main/physics.c` and reactor hooks

For each patch: reproduce failure first; compare actual pinned source behavior
with matching defined inputs; implement the smallest generic guarded consumer;
add fail-before/pass-after integration coverage; test malformed/rejected and
stock/network paths; test save/reload and stock->Pumo->stock transitions; rerun
both the failing original route and affected prior owners. Source arithmetic
adapters alone are not production-consumer qualification. Do not port unrelated
classic tuning, fix undefined reference behavior silently, or describe an
observation/test-clock repair as a gameplay feature.

A change requiring a broad clock/model/AI rewrite, new private assets, altered
admission scope or an unverified reference decision goes back to the coordinator
for a revised plan. The medium worker should not expand the tranche on its own.

### P6. Integrate, qualify boundaries and hand off

Dependencies: completed route packets and any P5 fixes. One integration owner.

- Rebuild the integrated candidate and regenerate schema-incompatible saves.
  Run the composed earned native chain and alternate/rejoin, not just separate
  leaf tests. Keep both warm and cold destination artifacts distinct
- Exercise Pumo->Counterstrike->Pumo and ordinary stock save/load on the exact
  final code. Validate definition/model/resource identity restoration and failed
  malformed-resource restore without replacing the running world
- If production consumers changed, run the relevant actual rendered native
  owner. If the route needs Omega, obtain real-frame evidence; a nonrendered route
  is not an acceptable stand-in. Device/phone and full audiovisual qualification
  remain separate work unless explicitly included later
- Update one concise evidence ledger with source SHA, executable SHA, full input
  identities, original/derived status, commands, expected/actual outcomes, assists,
  render/audio boundaries, preserved-profile hashes where relevant, and remaining
  failures. Leave historical files as historical; link superseding evidence
- Publish draft child PRs and verify exact remote heads. Report hosted CI as
  passed/failed/pending separately from local tests. Nothing here authorizes a
  merge, history rewrite or APK delivery

## 6. Validation and reproduction contract

Use an existing supported native D2 build setup with `BUILD_TESTING=ON`; select
its actual lifecycle executable rather than assuming a path from another machine.
Configure the standalone XL test tree with at least:

```sh
cmake -S android/XL_compat -B "$XL_BUILD" -DBUILD_TESTING=ON \
  -DXL_PREVIEW_LIFECYCLE="$ENGINE" -DXL_RETAIL_DATA="$RETAIL" \
  -DXL_PUMO_PLAYER_ROOT="$PUMO" -DXL_REFERENCE_GEOMETRY_ROOT="$XL_SOURCE"
cmake --build "$XL_BUILD" --parallel
ctest --test-dir "$XL_BUILD" -N -R '^xl_pumo_'
ctest --test-dir "$XL_BUILD" -R '^xl_pumo_sophodo_player$' --output-on-failure
```

PUMO contains the hash-matched `missions/`, `mods/` and `data/` layout expected
by the existing runner. It is not an arbitrary archive pathname. The last command
uses the existing fixture prerequisites to produce earned saves. Run serially
with PowerShell Core and a real desktop GL3 display. On a configured Linux host,
wrap the invocation with `xvfb-run -a -s '-screen 0 1024x768x24 -noreset'` when
appropriate; a dummy video driver does not satisfy the required GL admission.
Do not mistake test enumeration with zero matching tests for a pass.

For a verified existing earned entry, the current owner pattern is:

```sh
cmake -DENGINE="$ENGINE" -DDATA="$RETAIL" -DRESOURCES="$PUMO" \
  -DENTRY="$EARNED_LEVEL6_SAVE" -DOUTPUT="$NEW_OWNED_OUTPUT" \
  -P android/XL_compat/tests/pumo_sophodo.cmake
```

New owners should retain this contract with their actual required entry. Current
phase timeout is 150 seconds; outer owners are 240/300 seconds. Keep finite
budgets. If a genuinely longer composed scenario is required, split independently
bounded earned phases and document the reason rather than removing watchdogs or
stretching them to hide a deadlock. Register top-level automation changes, declare
support scripts with their existing owner convention, and run both required
catalog checks before committing test additions.

Minimum validation by changed scope:

| Scope                                   | Required evidence                                                                                                                                                                                     |
| --------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Plan only                               | Referenced paths/symbols checked, stack parent verified, no non-document diff, whitespace/Markdown checks; no gameplay claim                                                                          |
| Probe/receipt only                      | Public failure controls, actual producer integration, existing earned fixtures, no live gameplay-state mutation from observation                                                                      |
| Trigger/object/countdown                | `xl_wall_trigger_chains`, `xl_wall_trigger_scheduler`, `xl_object_events`, `xl_object_chain_delays`, `xl_trigger_flags`, relevant native save owners, original affected route, classic controls       |
| Weapon/model/combat                     | Relevant HXM/camera/projectile/water/boss owners and source comparisons, stock weapon controls, owned-source rollback, affected earned routes; active timing consumers retain their original tests    |
| Any `d1/`/`d2/` or shared hook          | D2 `test_upstream_compat` and `test_classic_trigger_demo`; D1 build/tests and stock startup when shared behavior is touched; multiplayer bypass and classic save/RNG checks for the affected consumer |
| Source/admission/public contract change | Existing public source/GPU/audio owner and policy negatives; no omitted tests silently counted as passed                                                                                              |
| Final integration                       | Composed exact-head routes, fresh-process saves, stock isolation, relevant actual-render qualification, exact-head CI status                                                                          |

Useful existing commands after supported dependency setup:

```sh
python android/XL_compat/tests/public_contracts.py --work "$NEW_PUBLIC_WORK" --gpu --audio
python -m unittest discover -s android/tests -p 'test_xl_private_assets.py'
ctest --test-dir "$D2_BUILD" -R '^(test_upstream_compat|test_classic_trigger_demo)$' --output-on-failure
pwsh -NoProfile -File android/tests/test_validate_automation_catalog.ps1
pwsh -NoProfile -File android/tests/test_run_all_tests_catalog.ps1
```

Run the repository's single scoped mixed-language formatter on actual changed
paths (`android/run-code-quality.ps1 -Fix -Paths ...`) and wait for it to exit
before editing. If a host lacks the `/proc/self/task/.../children` interface,
record the catalog supervisor failure and its exact command as blocked; do not
weaken its process supervision. Do not inherit an old host failure without trying
the required check on the selected implementation environment. Full source-oracle
sanitizers and full-engine sanitizers are different claims.

## 7. Worker coordination and final acceptance

Suggested order: P0 -> P1 -> P3 -> P4, with P2 allowed to research in parallel
once its earned baseline exists. Serialize shared probe/CMake edits and all live
engine/device use of the same profile. Insert a narrowly scoped P5 only where
reproduction justifies it; finish with P6. Prefer one medium worker per route or
semantic fix, and keep the coordinator responsible for scope and evidence review.

Every handoff must include:

1. Exact base/head, changed files and what is implemented versus planned
2. Original authored requirement and why the change is needed
3. Input/executable/save hashes and private artifact location, without publishing
   proprietary assets or raw logs
4. Commands and passed/failed/blocked/unrun results, including first failed attempt
5. Source-derived versus native versus rendered versus Android coverage
6. Stock/RNG/save isolation results and any inherited-stack regression
7. Next bounded command/ticket, unresolved questions and PR URL/status

Done for this tranche: independently reproduced remaining route segments and
alternate rejoin, or precise retained blockers for the uncompleted segments;
all implemented fixes qualified through their actual consumers; no stock
regressions in the required matrix; original bytes and user data preserved;
reviewable draft stack with accurate evidence. A plan, successful decode,
controlled exit, eight-case readiness score or source-adapter pass alone is not
completion of the remaining Pumo campaign.
