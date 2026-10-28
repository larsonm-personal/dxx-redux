# Singleplayer gameplay support roadmap

The target is complete singleplayer fidelity within an explicitly documented XL
version/settings scope. The earlier 10-20-chunk estimate described broadly useful
support, not completion, and no longer determines implementation order. Rejected
valid content remains unfinished work, even when its rejection is helpful

## Dependency order

1. Complete resource ownership, custom gameplay definitions, remaining layouts
   and capacities. Later combat and event work should exercise these foundations
2. Complete actors and lifecycle: custom combat, boss variants, dynamic creation,
   generators, death and respawn. Keep separate XL and Redux implementations
   where semantics differ substantially
3. Close remaining event actions/owners and world interactions, then exercise
   combinations involving delayed actions, generated actors and removed owners
4. Complete campaign transitions, secret levels, revisits and resource changes.
   Persistence is required in every preceding batch, not postponed to this stage
5. Complete presentation fidelity. Gameplay-affecting audio, visibility and
   effects accompany the mechanisms that require them; cosmetic work can follow
6. Finish broad original-mission qualification, long sessions, stress and
   performance. Original missions and cross-feature controls also run throughout

Use mission frequency to choose representative fixtures, not to leave uncommon
requirements permanently unimplemented. Pin reference behavior and document
intentional corrections. Define additional reference versions/settings before
claiming their support; multiplayer remains a separate later scope

## Completion requirements

Keep the existing shared stock/XL binary, capability-based detection and useful
rejections. Every admitted feature needs reference semantics, activation through
the production runtime, persistence/lifecycle checks and stock isolation.
Loading, gameplay qualification and presentation remain separate claims

Defer multiplayer and cosmetic rendering work. Retain gameplay-affecting
visibility, transparency, collision, audio cues and authored resource semantics.
Use controlled poses, physical crossings, authored actions and combat checks;
do not attempt autonomous 3D navigation

The original overlapping work areas remain open until their acceptance evidence
is complete. In particular, completing a parser or a derived fixture does not
finish original-mission qualification

| Work area                                                 | Current evidence and remaining acceptance                                                                                                                                                                                                                                                                                                                                              |
| --------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Teleports, speed boosts, spawn destinations               | Wall teleports and boost direction/constraints/release/orientation, boost-area collision and saved motion have native/source controls. All 49 Android boost and 56 authored-spawn controls pass, including real death/respawn, marker precedence and saved transforms; original mission interactions remain open                                                                       |
| Trigger chains, timing/flags, object events               | Wall scheduling and nonplayer object leaf/immediate-chain activation, lifecycle and disk/rewind persistence verified. Action-specific byte flags have reference/native and Android mixer/persistence controls. Delayed descendants now retain event context/actor identity through disk/rewind; other object actions/owners remain open; boss destinations have separate checked state |
| Segment properties, walls and generators                  | Modern water/no-damage and lava contact/motion, water with stock robots and mixed producers, and bounded robot-matcen assignment, production, contact and persistence verified. Blocked entry/exit and modern repair centers verified; team/ownership, gameplay walls, legacy equipment layouts and expanded generator data remain open; modern equipment controls pass                |
| Layouts, capacities, custom gameplay data and transitions | Audited bounded 15/33, 15/34, 17/36 and 23/40 readers exist. Other observed layouts/payloads, wider tables and dynamic spawn headroom, expanded HAM/HXM robot/weapon data and authored transitions remain open                                                                                                                                                                         |
| Cross-feature regressions and original missions           | Existing Ahayweh Gate qualification and documented Neptune/Observatory evidence remain scoped. Combined movement/events/generators/combat/persistence controls and original missions exercising newly supported features remain open                                                                                                                                                   |

## Chunk ledger

1. Immediate wall teleports: complete within the evidence scope recorded in
   [wall-teleports.md](../../XL_compat/evidence/wall-teleports.md)
2. Immediate wall trigger chains and enable/disable counters: native and Android
   checks complete within the [recorded scope](../../XL_compat/evidence/trigger-chains.md)
   and [chunk plan](xl_trigger_chains_20261026.md)
3. Delayed wall actions and permanent/alternate/autoplay flags: native and Android
   disk/memory controls complete within the
   [scheduler evidence](../../XL_compat/evidence/trigger-scheduler.md) and
   [chunk plan](xl_trigger_scheduler_20261026.md)
4. Nonplayer object events and immediate masters: native/reference and Android
   disk/rewind checks complete within the
   [recorded scope](../../XL_compat/evidence/object-triggers.md) and
   [chunk plan](xl_object_triggers_20261026.md). Delayed descendants, other
   actions/owners remain explicitly gated; boss bookkeeping has separate evidence below
5. Robot matcens: complete within the bounded chunk scope. Native/reference and Android disk/rewind, activation,
   selection, generated health, contact impulse/damage/RNG, exhaustion and stock controls pass within the
   [chunk scope](xl_generators_20261026.md) and
   [evidence](../../XL_compat/evidence/robot-generators.md). Legacy equipment, expanded
   data/capacities remain open; object spawn actions have separate evidence below

6. Object-triggered boss spawning and separate per-owner cloak/teleport/death
   lifecycle: native/reference and all 66 Android lifecycle/combat checks pass.
   Final generated-robot combat and formatted-build verification are recorded in
   [the boss evidence](../../XL_compat/evidence/boss-spawning.md) and
   [chunk plan](xl_boss_spawning_20261027.md). This does not qualify an original
   mission; dynamic boss creation remains gated; object-teleport destinations are covered below

7. Modern equipment makers: native/source comparisons and 61 Android steps pass
   within the [bounded equipment scope](../../XL_compat/evidence/equipment-generators.md)
   and [chunk plan](xl_equipment_generators_20261027.md). Initial target discovery,
   shared producer ordering, authored toggles, actual pickups/replenishment and
   disk/rewind state are covered. Legacy encodings, addon/multiplayer items and
   all eleven original candidates remain independently gated

8. Effect payload inspection: versioned particle/lightning/sound/thruster records,
   waypoint controllers and known static movement now have bounded decoding and
   separately reported reference defaults. Native/source and Android metadata
   controls pass; all new behavior remains gated. The
   [evidence](../../XL_compat/evidence/effect-layouts.md) reconciles original
   candidates without gameplay promotion. Unknown payloads, capacities and
   independent event/property/resource requirements remain open

9. Boss object teleports: owner-only destination setup, preserved clocks and
   checked earned-route disk/rewind state pass native/source and 48 Android
   controls within the [recorded scope](../../XL_compat/evidence/boss-transport.md).
   This supersedes the earlier boss-teleport gate; original missions remain
   separately qualified

10. Action-specific byte flags: 6,400 source comparisons, native scheduling,
    one-shot/alternate/combat/teleport controls and all 66 Android mixer/lifecycle
    steps pass within the [recorded scope](../../XL_compat/evidence/trigger-flags.md).
    Bits 32/64 are admitted for implemented actions; boost orientation is covered
    by the movement tranche below. XL's bounded illusion handler intentionally corrects the reference's
    back-side lookup while preserving its counterintuitive cue condition

11. Authored boss destination groups: all byte labels are retained independently
    of team ownership, with 768 format contracts, 512 new source comparisons,
    actual native selection and 48 Android lifecycle checks. Open passage/size
    negative controls and atomic cross-group route rejection establish the
    [recorded scope](../../XL_compat/evidence/boss-groups.md). Team ownership and
    original missions retain separate gates

12. Player-resource wall actions: type-17 shield units and type-18 energy
    percentages have separate XL semantics, with 840 source comparisons, real
    native wall/master/scheduler/death controls and 62 Android lifecycle steps.
    See [resource evidence](../../XL_compat/evidence/player-resources.md). Default
    ship behavior is qualified within that scope; direct object variants,
    optional recharge/scaling and original missions retain separate requirements

13. Delayed nonplayer object chains: 140 source timing cases, actual native
    removal/slot-reuse/nested deadlines and 45 Android disk/rewind steps pass
    within the [recorded scope](../../XL_compat/evidence/object-chain-delays.md).
    Event geometry survives removal; motion requires the original live actor.
    Context retention and reusable-slot rejection intentionally correct the
    reference lifecycle. No original mission is newly qualified

14. Speed boosts: per-owner movement constraints, orientation, area boundaries,
    release and saved motion pass 4,740 source comparisons and actual native
    crossings/collisions/teleport controls. All 49 Android checkpoint and
    stock-transition steps pass; see [boost evidence](../../XL_compat/evidence/speed-boosts.md).
    This covers the default ship/boost-strength profile, not original mission
    qualification or optional physics settings

15. Authored spawn points: 1,536 source transforms, 16 source cue comparisons,
    native actual death/marker/delayed-event controls and all 56 Android
    persistence steps pass; see [spawn-point evidence](../../XL_compat/evidence/spawn-points.md).
    Six surveyed originals retain independent capacity, effect and gameplay gates

16. Modern water with stock robots and generators: 72 source motion comparisons,
    actual native AI/physics/combat and all 86 Android producer/combat/persistence
    steps pass. See [combined water evidence](../../XL_compat/evidence/water-actors.md).
    Legacy water with robots, custom definitions and original-mission qualification
    retain independent gates; all 15 surveyed modern-water originals were freshly
    decoded without promoting their runtime status

17. Modern lava volumes: separate XL wall/volume/scrape behavior, saved interval,
    10,240 source contact and 672 motion samples, and native death/restore/stock
    controls pass. All 58 Android audio/gameplay/persistence checks pass; see [lava evidence](../../XL_compat/evidence/lava-volumes.md).
    Thirteen original candidates were audited without gameplay promotion

18. Modern blocked segments: 2,304 source comparisons and actual actor-specific
    collision, exit, projectile, recovery and stock controls pass. All 66 Android
    movement, automap, generation and persistence checks pass within the
    [recorded scope](../../XL_compat/evidence/blocked-segments.md). Twenty-nine
    originals retain separate container, decoding and gameplay requirements

19. Modern repair centers: 5,760 source repair samples, 42 actual player frames
    matched to the extracted caller, physical boundaries and stock-rate controls
    pass. All 70 Android healing/cap/audio/automap/persistence checks pass; see
    [repair evidence](../../XL_compat/evidence/repair-centers.md). Twenty-eight
    originals retain independent requirements and qualification

20. Actor-specific closed walls: 55,296 extracted-source comparisons, 120 actual
    native crossings and all 90 Android key, collision and persistence checks
    pass within the [recorded scope](../../XL_compat/evidence/keyed-closed-walls.md).
    Blue/red key ownership affects the player alone; gold, blocked destinations
    and boost constraints retain source behavior. The audit of 121 originals
    retains their independent decoding, gameplay and qualification requirements

21. Separate XL doorway decisions: source comparisons across admitted types,
    states, flags and signed cloak bytes, plus actual animation, physical and
    visibility rays, reversal and disk-through-stock controls pass. See the
    [doorway evidence](../../XL_compat/evidence/doorways.md) for Android acceptance
    and the signed-byte correction to the preceding keyed-wall adapter. This
    does not qualify every wall animation or an original mission

22. Authored close-door actions: the XL path overrides automatic waiting while
    preserving active entries/timers and obstruction on both sides. Source/native
    controls and Android acceptance are recorded in
    [close-door evidence](../../XL_compat/evidence/close-doors.md). Seven original
    candidates retain independent requirements and qualification; dynamic
    capacity, other wall behaviors and optional settings remain open

23. Legacy 17/36 repair centers: extracted Upgrade mapping, 1,440 source samples,
    12 actual player frames and physical boundary/disk-through-stock controls
    extend the separate XL repair path. See [legacy repair evidence](../../XL_compat/evidence/legacy-repair.md)
    for Android acceptance and retained 15/33, optional-settings and original
    mission requirements. The pinned survey contains no legacy repair candidates

24. 15/34 packed light layout: source-reader comparisons and derived geometry,
    all-source light updates and saved-state controls are recorded in
    [format evidence](../../XL_compat/evidence/format-15-34.md). Fourteen surveyed
    originals retain invalid packed-range rejections. The remaining level's HXM
    now has separate gameplay controls below. The format tranche does not qualify
    Apocalyptic Factor XL

25. Classic HXM gameplay definitions: bounded model geometry, retained base and
    computed custom radii, robot/gun/joint dependencies and actual AI firing now
    have source/native controls. Original Alarlof startup, combat, model/texture
    restoration and stock isolation pass; the registered Android integration
    covers 42 steps. See [HXM evidence](../../XL_compat/evidence/hxm-gameplay.md).
    Expanded registries, signed boss profiles and alternate gameplay data remain
    unfinished; this does not certify the original campaign

26. Wide wall identities: source/native boundaries and 80 Android steps cover
    2047-wall loading, authored doors, cloaking, persistence and stock isolation.
    Classic/wide demo events preserve IDs across recording, playback and rewrite.
    All 21 original survey members match their hashes; 17 decode with other
    blockers and four retain reader failures. See
    [wide-wall evidence](../../XL_compat/evidence/wide-walls.md). Wall-trigger capacity now has separate controls below; object allocation
    capacity has separate controls below; concurrent cloaks are covered below

27. Separate XL cloak lifecycle: 3,200 source comparisons, 100 concurrent entries,
    active disk through stock, eight malformed/truncated save controls and all
    81 Android steps pass within the [recorded method-0 scope](../../XL_compat/evidence/cloak-lifecycle.md).
    Actual flat-draw alpha, completed audio, physical passage/blocking and memory
    checkpoints are covered. Redux retains ten entries. Other lighting/settings,
    colored/team walls and complete demo fidelity remain open

28. Trigger capacity and byte identities: 254 wall rows, complete signed-byte
    consumer normalization, checked saved counts/reads and metadata capacity have
    source/native controls. All 77 Android steps cover authored high-ID actions,
    a physical crossing, disk through stock and memory checkpoints. See
    [trigger-capacity evidence](../../XL_compat/evidence/trigger-capacity.md).
    Stock/imported D1 retain 100; object-action bounds match the source's 254.
    The two original high-trigger members remain independently blocked

29. Projectile ownership and impact lifetime: defined source history comparisons,
    1920 native impact samples, physical piercing, checked disk/memory state and
    all 53 Android steps pass. See
    [projectile evidence](../../XL_compat/evidence/projectile-history.md) for the
    intentional FIFO/full-save corrections. Wider object storage has separate controls below; remaining
    combat/AI requirements remain unfinished

30. Expanded object pool: source/native controls cover source-sized pools up to 32768 runtime
    slots and wide lifetime signatures, separate XL reclamation, high-ID AI/combat/events and exact allocator
    restoration. All 53 Android disk/checkpoint/stock checks pass. See
    [object-capacity evidence](../../XL_compat/evidence/object-capacity.md).
    Dinter and Anthology lose only the object-count blocker. Wider object slot
    references, other capacities and broad performance remain unfinished.

31. Separate XL active-door lifecycle: full 90-entry allocation, correct live
    compaction and waiting timers, per-face animation and checked saved ownership
    have source/native controls. Paired-face and reference defect corrections,
    Android acceptance and remaining profiles are recorded in
    [door lifecycle evidence](../../XL_compat/evidence/door-lifecycle.md).
    Redux retains its existing alternate; original missions remain separately
    qualified.

After the wall, cloak, trigger and object-capacity batches, continue the capacity and resource foundations, then the
remaining actor and event variants in dependency order. Use original counts to
choose effect/audio fixtures, with activation, gameplay cues and cleanup treated
as required acceptance rather than cosmetic deferral. Do not mark
the overall goal complete from a count of completed chunks or green fixture
tests. Audit each area against live behavior and original mission requirements
