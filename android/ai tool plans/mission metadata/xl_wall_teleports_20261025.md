# Wall-trigger teleports and singleplayer roadmap

## Scope and estimate

The 2026-10-25 planning estimate is 10-20 further chunks comparable to the modern
properties tranche for broadly useful singleplayer gameplay fidelity, with a
longer tail for unusual missions. This is a provisional estimate, not a measured
completion percentage or a promise that all XL missions will work

| Remaining area | Estimated chunks |
| --- | --- |
| Teleports, speed boosts, spawn destinations | 2-3 |
| Trigger chains, timing/flags, object-attached triggers | 3-5 |
| Remaining segment properties, walls, generators | 2-4 |
| Additional layouts, capacity, custom gameplay data, mission transitions | 2-4 |
| Cross-feature regressions and original-mission qualification | 2-4 |

Ranges overlap and must not be added as independent estimates. At one completed
chunk per working day this suggests 3-5 weeks; at two per week, 2-3 months.
Confidence is greater in chunk counts than calendar time. Trigger scheduling and
custom robot/weapon data are the largest uncertainties

Track original missions whose gameplay requirements are covered, rather than raw
feature counts. Defer multiplayer and cosmetic rendering differences, but retain
gameplay-relevant visibility, transparency, collision and cues. Qualification
uses controlled placement, authored activation, combat and source save/restore;
do not attempt autonomous 3D navigation or claim an uninterrupted playthrough

## Implementation and acceptance plan

1. Compare original missions containing wall-trigger type 14 with a fresh native
   inventory and complete remaining blocker lists, including capacity and assets
2. Audit the pinned reference's dispatch, destination selection, orientation,
   velocity, actor restrictions, flags, repeat behavior, sound and appearance
3. Implement detected wall teleports through minimal core hooks and XL-owned code
   in the shared binary, without mission-name or hash production exceptions
4. Validate malformed destinations before commit and preserve useful rejection
   messages for object triggers, speed boosts, spawn actions and unsupported flags
5. Exercise authored triggers and derived negative controls in native integration
   tests, including multiple destinations, nonplayer actors, repeat/one-shot,
   collision after arrival, source save/restore and stock isolation
6. Run a registered Android integration test through actual activation and
   persistence, then update the catalog and durable acceptance evidence

## Initial source findings

Reference: `temp/d2x-xl-reference/main/trigger.cpp`, pinned study source described
in the existing evidence records. `CTrigger::DoTeleport` selects a random link,
stops speed boost, calls `TriggerSetObjOrient`, places the object at the target
segment, and requests appearance effects in singleplayer. Dispatch plays the
secret-exit sound, rejects a dead local player, and distinguishes object-trigger
robot teleports from wall-trigger teleports. Wall teleports are excluded from the
ordinary 750 ms trigger debounce. Nonplayer dispatch exists and must be audited;
implementing player relocation alone would not establish full wall semantics

The inventory currently rejects every type above 13 and validates geometric
target indices only for types <=13. Teleport admission therefore needs explicit
target validation as well as runtime implementation. No support gate has been
relaxed yet

Current status: immediate singleplayer wall teleports are implemented and admitted
by detected requirements. Native source loading, save restoration, arrival
collision and 252 extracted-reference transforms pass. Both the initial and final
Android runs passed 36/36. The final native regression selection passed 10/10;
both automation catalog checks and scoped formatting/lint passed.
See the [maintained evidence](../../XL_compat/evidence/wall-teleports.md)

Completion audit for this immediate wall-teleport tranche:

1. Planning estimates and uncertainty are recorded above; the durable candidate
   inventory includes remaining decoded blockers, unknown parse tails, capacities
   and declared assets. No original mission is promoted by this work
2. Reference dispatch and transform rules are traced; 252 runtime samples match
   independently compiled, hash-pinned XL functions exactly
3. The shared runtime uses detected source requirements, minimal D2 hooks and no
   mission identity exceptions. Default singleplayer admission accepts immediate
   teleports, while unsupported scheduler/flag/action requirements remain blockers
4. Destination bounds and delay thresholds have native positive/negative controls;
   the browser distinguishes supported immediate actions from blocked variants
5. Native controls cover real wall entry on a derived fixture, eligible actor
   types, a physical nonplayer crossing, repeat/one-shot, arrival collision,
   source save restoration, pose and stock timer/isolation behavior
6. The registered Android runner passes all 36 steps on the final APK. The native
   selection passes 10/10, both catalog checks pass, and scoped formatting/lint
   and git diff whitespace checks pass

The chunk is complete within the documented immediate singleplayer profile.
Delayed scheduling, extended flags, boosts, object triggers, multiplayer, original
campaign qualification and teleport-aware guidebot routing remain separate work

The native dispatch fixture now passes actual wall entry, center placement,
stationary velocity, one-shot/disabled state, dead-player rejection, delayed and
malformed action rejection, 64 immediate repeats covering both destinations,
actor-type filtering and stock recovery. A powerup moved through a real shared
portal via `object_move_one` and teleported from segment 0 through 15 to 92

That physics test first failed with an empty crossed-segment list. Redux only
recorded that list for players. XL-eligible nonplayer actors now record it too;
the same test then passed. The pinned reference continues processing the frame's
original crossing list after relocation, so the port preserves that behavior
instead of adding an unverified early break

The first placement implementation also exposed a stock/XL numerical mismatch.
XL computes segment centers with float accumulation, uses default-format
normalization and trigonometry, truncates dot products toward zero, and retains
the positive asin endpoint for either sign at unit magnitude. The teleport path
now follows those source rules. Independent extracted-reference transform tests
are still required; the dispatch checks alone do not establish rotation parity

Native test entry: `tests/teleport.cmake`, registered as `xl_wall_teleports`, with
logs in `temp/xl-wall-teleports-20261025/dispatch`. These controls mutate a loaded
fixture; they do not yet prove source-based admission, save restoration, arrival
collision or Android behavior. Those remain required before support is enabled

## Earlier implementation checkpoint (superseded by current evidence)

The reader now validates type-14 segment/side destinations and reports delayed
teleports separately. Native inventory controls pass for malformed negative and
upper-bound targets across 15/33, 17/36 and 23/40, plus positive/negative delay
thresholds and INT_MIN. The standalone `xl_inventory_test.exe` passed; CTest in
the initial CTest filter did not match the registered test name, so that invocation
is not counted as a test pass

An initial `runtime/teleport.cpp` implementation and `check_trigger` hook compile
in the native headless engine. It implements actor filtering, random destination,
orientation/velocity rotation, segment relinking, one-shot state and sound/message
cues. This is a draft, not qualified support. Type-14 admission remains blocked

Still required before admission: independent reference math comparisons, native
authored/derived trigger integration tests, physics dispatch for nonplayer
crossings, stale crossed-segment handling after relocation, actual destination
collision, direct activation call-site audit, save/restore, Android integration,
and support catalog/evidence updates. Source geometry math and appearance behavior
must be verified rather than inferred from the successful build. No speed-boost
state exists in the admitted profile; boost combinations remain blocked

Scoped mixed-language formatting/lint passed after allowing the installed
clang-format executable to run outside the workspace. Native compilation emitted
only the existing weapon.c POrderList/SOrderList return-path warnings

## Candidate comparison, first pass

The supplied survey contains 108 wall teleports across 19 payloads in 10 HOGs.
Fresh native inspection of those original HOGs is saved under
`temp/xl-wall-teleports-20261025`. This is inspection, not admission; capacity,
resource closure and runtime checks are still required

| Candidate | Native findings beyond teleport actions |
| --- | --- |
| Entombed (`etombz`) | Decoded level; four red-team, four blue-team and six repair-center functions, 110 non-neutral owner/group records, and level-specific HXM remain blockers |
| Glass House (`g-house`) | Decoded records; blocked/outdoor property bits, function 2, 126 type-8 walls, 47 boosts, camera action, three object triggers, unparsed object-trigger tail, three type-16 objects and level-specific HXM |
| Speed! | Reader stops at unsupported render layout, object 163; supplied survey also identifies boosts and camera actions |
| PyroTechnics (`ptech-d3`) | Reader stops at unsupported render layout, object 163; supplied teleport flags include bit 8 |
| Styx, Anthology, Sphere, Boiling Point, Dinter, Tankboy | Native reader stops at unsupported object movement/render layouts; remaining requirements cannot yet be claimed completely inventoried |

The [durable candidate inventory](../../XL_compat/evidence/wall-teleport-candidates.json)
includes decoded blocker groups, capacity checks, assets, source hashes and parse
failures. Asset closure remains unqualified where earlier requirements block
loading; no unknown requirement is treated as absent

No original mission has yet been established as unlocked by teleports alone.
Entombed is the strongest initial fully decoded candidate, but its modern segment
functions must be understood before choosing it as a production acceptance pilot.
Do not discard failed parses from comparison output or treat their absent trigger
records as evidence that no teleports exist. Lunar Outpost Reloaded is not among
the supplied wall-teleport payloads and is not a confirmed candidate

Reference orientation is specifically the negated destination side normal, with
angle-based velocity rotation; it does not simply zero velocity. Delay detection
also depends on serialized time (absolute values 100 through 900000), independent
of trigger flag support. A teleport gate must reject unimplemented delayed actions
even if its flags are otherwise accepted

The reference `include/segment.h` confirms modern functions 7/8 are team rooms,
not legacy water/lava. Entombed's remaining blockers therefore include team-room
semantics and repair centers; they cannot be waived as cosmetic data for a
singleplayer teleport fixture. A derived fixture can isolate teleport behavior
without qualifying the unchanged original mission
