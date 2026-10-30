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

Native persistence fixtures must also verify running simulation at continuation
boundaries. The low-level save consumes its caller's pause; fixtures must not
resume it a second time. See the [native pause audit](../../XL_compat/evidence/native-persistence-pause.md)
for a reproduced false assumption and the corrected checks. Direct consumer
assertions alone do not establish that pause-aware gameplay frames can advance

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

32. Owned changing-light tables: full source widths and ranges, authored UVL
    updates, exact earned-brightness disk/rewind restoration and Redux isolation
    pass native/source and all 50 Android controls. See
    [changing-light evidence](../../XL_compat/evidence/changing-lights.md) for
    source lookup/overflow/propagation corrections and retained renderer limits.
    Dinter and Sphere sp-03 lose only their light-capacity blocker. Flicker
    capacity/lifecycle is covered below; complete changing-source visual parity remains open.

33. Owned flicker lifecycle: full source tables, fixed-width right rotation,
    source visibility/emission and first-matching authored controls, exact phase
    persistence and Redux isolation pass native/source and all 50 Android steps.
    See [flicker evidence](../../XL_compat/evidence/flickers.md). The fresh survey
    matches 131 original hashes, decodes 103 members and finds 23 flicker-bearing
    levels; no original campaign is promoted. Alternate renderer/settings and
    complete changing-source visual comparisons remain open.

34. Native extended HOG foundation: a shared streaming index supports selected
    long names under DHF/D2X and preserves archive ownership through actual
    lookup, startup and disk-through-stock restoration. See
    [container evidence](../../XL_compat/evidence/extended-hog.md). Launcher
    import/catalog/music integration is covered by the next increment below.
    Playground now decodes and retains independent gameplay blockers.

35. Extended HOG launcher integration: import, metadata, asset/music catalogs and
    preview staging use the shared native format reader. The unchanged Pumo
    distribution has host import and byte-preservation evidence; the normal APK
    exercises a derived extended archive through both preview renderers and game
    activation. See [launcher evidence](../../XL_compat/evidence/extended-hog-launcher.md).
    Pumo's fourteen version-27 levels were still uninspected at this step; the
    next increment establishes their requirements without granting gameplay.

36. Modern 25/27 inspection: signed wide extras, explicit side corners/deleted
    slots and fog records are decoded, with actual source-reader comparisons.
    All fourteen Pumo levels decode and have canonical geometry. The broader
    inventory retains three level failures and one ambiguous archive. The
    [layout evidence](../../XL_compat/evidence/modern-layouts.md) also corrects
    an older light-index source adapter and documents the deliberate unsigned
    encoded-count interpretation. Runtime layout activation/persistence is covered
    below; authored fog, effect/audio ownership, event actions and resources remain.

37. Canonical 25/27 runtime: normal loading preserves compiled geometry, textures,
    objects, events and owned lighting. Native gameplay/disk controls and all 87
    Android transition/checkpoint/save/stock checks pass. The expanded producer
    comparison corrects an earlier initialization omission: geometry-order makers
    replace stale serialized indices before row assignment. Equipment startup,
    boss selection and saved identities use the corrected mapping. See
    [runtime-layout evidence](../../XL_compat/evidence/runtime-layouts.md).
    Noncanonical geometry remains gated; authored fog has its subsequent increment
    below. Pumo's original
    levels retain effect/audio, environment, event, wall and resource requirements.

38. Authored fog: owned water/lava/light/dense settings reach source-derived volume
    boundaries, depth occlusion and paired composition. Admission exercises actual
    float blending and depth transfer, with explicit rejection on incapable drivers.
    Source comparisons, public desktop GPU controls and all 208 Android
    pixel/MSAA/disk/rewind/context/stock checks pass. See
    [fog evidence](../../XL_compat/evidence/fog.md) for reference quirks and scope.
    No original Pumo level is newly qualified. Required regression coverage is
    recorded below; continue effect/audio ownership and dependent event actions.

39. Required XL CI: public Windows/Linux checks now build the pinned source
    adapters and require all 36/37 contracts. The trusted private lane requires
    91 native/source/GPU controls, sixteen original-file outcomes and three
    complete modern source comparisons. A clean local run passes in under eight
    minutes; a focused archive-check optimization subsequently removes repeated
    JSON parsing without reducing its assertions. See
    [CI evidence](../../XL_compat/evidence/private-ci.md) for precise timings,
    stock controls, proposed original assets and the hosted trust boundary.
    Missing tests, skipped cases and omitted source oracles cannot grant a pass.
    These checks preserve feature evidence without qualifying an original campaign.

40. Owned level messages: shared MSG/SND text decoding, actual wall/object message
    actions, visible timed text and modal pause pass source/native and all 80
    Android controls. Resource ownership, exact same-frame saves, disk/rewind,
    rejection recovery and Redux transitions are covered. All 21 Pumo message
    actions resolve to source-matched owned MSG rows across fourteen original
    levels. See [message evidence](../../XL_compat/evidence/level-messages.md).
    Sound playback and effect objects retain separate gates. Required public
    coverage is now 38/39 cases and the private selection is 94; no original
    Pumo level receives gameplay qualification from this increment.

41. Channel-preserving WAV foundation: a shared bounded RIFF reader now feeds
    both unchanged classic mono conversion and authored mono/stereo PCM.
    Independent Windows/Linux comparisons cover all 25 Pumo WAVs and retain
    original mono golden samples. See
    [stereo evidence](../../XL_compat/evidence/stereo-pcm.md). Required selections
    add one fast comparison, becoming 39/40 public and 95 private cases. Named
    playback, loops, voice ownership and persistence remain separate requirements;
    sound action 30 is still gated.

42. Named-audio source contracts: actual sound requests, resource precedence,
    repetition, completion flags and restore helpers now execute in required
    fast CI. All 60 Pumo sound actions resolve to owned SND/WAV resources on
    both hosts. See [source evidence](../../XL_compat/evidence/named-audio-source.md)
    for the recording-adapter boundary and reference restore defects. Required
    public/private membership becomes 40/41 and 96. This is a runtime prerequisite;
    action 30, ambient objects and original mission qualification remain gated.

43. Full wall-key bytes: format, translation and extracted-source comparisons
    cover all 256 values without masking or normalization. Actual player hits,
    brain door decisions, physical crossings, disk through stock and all 90
    Android controls retain the different consumer semantics. See
    [wall-key evidence](../../XL_compat/evidence/wall-key-bytes.md). Pumo loses
    exactly 73 key-value rejections across nine levels, with all other inventory
    results unchanged. Wider wall flags and every level's independent gameplay
    blockers remain; required public/private membership stays 40/41 and 96.

44. Initial package-resource audit: valid true-color POGs passed indexed bounds
    validation and were misinterpreted by the installer. Pumo has 26 replacements
    in five POGs; original Robotech Arena has nineteen. The native reproduction
    and trial-guard limitation are recorded in
    [the resource audit](../../XL_compat/evidence/truecolor-pog-admission.md).
    The subsequent implementation is recorded below. HXM and INI application
    remain package-level audit obligations.

45. Owned true-color POGs: single-frame RGBA resources now retain dimensions,
    pixels, transparency and averages through installation, rendering, collision
    and destructible textures. All 26 Pumo replacements and nineteen Arena
    replacements have source and desktop shader comparisons. All 58 normal
    Android GPU-readback, disk/rewind, context-loss and stock-transition steps
    pass. See [POG runtime evidence](../../XL_compat/evidence/truecolor-pog-runtime.md).
    Animation strips, cockpit replacements and remaining rendering profiles are
    separate requirements; this does not qualify either campaign.

46. Owned named-sound playback: separate XL wall/object voices preserve stereo,
    repetitions, cooldowns and exact saved playheads. Actual SDL output and all
    73 Android controls cover mute/volume, pause, disk/rewind, level transitions
    and stock cleanup. All 60 Pumo actions pass production resource preparation
    at 44.1/48 kHz on Windows and Linux. See
    [audio runtime evidence](../../XL_compat/evidence/named-audio-runtime.md).
    Required selections become 43/46 public and 103 private cases. Mission/level
    sound-folder ownership, ambient spatialization and effect-object lifecycle
    remain subsequent consumers. Every Pumo level still has independent blockers.

47. Owned mission and level sound folders: source priority, signed incoming
    ordinals, repeated normal/secret filenames and selected-context ownership
    now have native and 65-step Android output, save/rewind and stock evidence.
    All 644 Pumo ambient objects resolve to 41 owned external WAVs with independent
    PCM checks. See [sound-folder evidence](../../XL_compat/evidence/audio-scopes.md).
    Ambient spatialization, global addon installations and the remaining external
    sound replacement consumers remain separate; Pumo is still unqualified.

48. Ordinary D2 sound resources: pinned bank/WAV selection and an owned stereo
    preparation path now identify 1,988 original Pumo selections using 196 WAVs.
    Production rejects unapplied replacement WAVs and unsupported custom banks
    with the selected path; it does not silently fall back to stock playback.
    See [bank evidence](../../XL_compat/evidence/classic-audio-bank.md). Actual
    replacement output, spatial consumers, priorities and persistence remain
    subsequent work; no campaign is qualified by resource decoding.

49. Owned named-channel allocation: source circular selection, class threshold,
    persistence and recorded volume now determine replacement in the 128-voice
    pool. Saves retain the next allocation position and captured priorities.
    See [channel evidence](../../XL_compat/evidence/audio-channels.md) for full-pool
    output/continuation controls and the explicit global-slider output policy.
    Ordinary replacements and ambient owners remain separate consumers; this
    does not qualify their shared exhaustion behavior or the Pumo campaign.

50. Owned texture-change actions: packed values, orientation retention,
    addressed-side behavior and future resource dependencies now have executable
    source and native consumers. Actual rendering, collision and earned delayed
    continuation through disk/rewind have controlled integration coverage. See
    [texture evidence](../../XL_compat/evidence/texture-actions.md). Exactly fifteen
    Pumo action rejections in three levels are removed; all fourteen original
    levels remain blocked. Required private membership becomes 104; public
    membership stays 43/46.

51. Owned countdown actions: wall/object seconds, source frame accumulation,
    warnings, rotation and terminal death now use an XL consumer. Persistence
    retains displayed seconds, earned reactor extension and partial cadence.
    See [countdown evidence](../../XL_compat/evidence/countdowns.md) for the
    explicit simulation-clock boundary and controlled output/restore checks.
    Exactly nine Pumo action rejections disappear with every other requirement
    unchanged. Required private membership becomes 105; public membership
    remains 43/46. All fourteen original levels still have independent blockers.

52. Owned robot disarming: wall/object type-34 actions bind neutral state to
    current robot lifetimes, including generated slots. Actual ranged, melee and
    thief controls cover attack suppression and source collision-time quirks;
    disk/rewind, malformed state and stock transitions protect persistence.
    See [disarming evidence](../../XL_compat/evidence/disarming.md). One Pumo
    action rejection disappears with every other requirement unchanged; all
    fourteen original levels remain blocked. Required private membership becomes
    106; public membership remains 43/46. Reprogramming stays separate.

53. Per-channel gain and pan foundation: executable source controls preserve
    startup/update quantization and priority changes; actual SDL PCM comparisons
    exercise independent simultaneous channel settings. Existing named output
    and persistence retain their prior behavior. See
    [channel-control evidence](../../XL_compat/evidence/audio-controls.md).
    Ordinary/ambient playback, shared owner cleanup and saved nondefault channel
    settings remain separate work. No admission gate or CI membership changes.

54. Sound-owner lifecycle foundation: actual source callback/container controls
    and mixed PCM tests verify the 500-owner/128-channel limits, completion,
    replacement, dense compaction and native lifetime reuse. Ordered deletion
    intentionally repairs a proven source back-reference bug. See
    [ownership evidence](../../XL_compat/evidence/audio-ownership.md).
    Ordinary/ambient engine consumers, spatial updates and saved owners remain
    unfinished. No admission rule or CI membership changes.

55. Shared ordinary audio and spatial ownership: the complete stock bank now
    shares the XL channel pool with named WAVs. Logical translation, linked
    object/segment lifetime, spatial routing and saved ownership have source
    comparisons and native/Android output controls. See
    [linked audio](../../XL_compat/evidence/linked-audio.md) and
    [engine dispatch](../../XL_compat/evidence/gameplay-dispatch.md).

56. Gameplay sound callers: separate XL weapon/explosion, robot/boss and
    afterburner policies now have actual event, PCM and stock-isolation checks.
    Source corrections and output-profile limits are explicit in
    [combat audio](../../XL_compat/evidence/combat-audio.md) and
    [actor/afterburner audio](../../XL_compat/evidence/actor-afterburner-audio.md).

57. Ambient surfaces: the real initializer now preserves XL per-segment gain,
    overlay precedence, portal identity and deferred permanent owners. Source
    receipts and native stock PCM checks qualify this consumer; platform status
    is maintained in [surface audio](../../XL_compat/evidence/surface-audio.md).
    Qualify wall/texture-change lifecycle and ambient sound objects, then
    install classic replacements only after their remaining consumers are
    covered. No admission expansion follows from initializer tests alone.

58. Stationary-listener audio routing: explicit source update points now
    invalidate cached paths after texture actions and door/cloak/exploding-wall
    lifecycle events. The regression demonstrates the prior stale route, then
    exercises real consumers with retained ownership, playback and deferred
    state. See [route evidence](../../XL_compat/evidence/audio-routes.md) for
    platform results and limits. Next implement external WAV linked-owner
    creation and restore validation before sound-effect object activation and
    lifetime handling. Resource resolution alone cannot qualify those objects.

59. External linked audio: prepared mission WAV identity now survives object
    motion, stopped/deferred playback and save restoration in the shared mixer.
    Actual source creation comparisons and reproduced lookup defects distinguish
    fidelity from deliberate repairs. See
    [external audio evidence](../../XL_compat/evidence/external-audio.md) for
    current platform results. Sound-effect object loading, enabled-state
    consumers and saved object lifetimes are covered by the following increment.

60. Ambient sound objects: source-normalized stationary sound effects now own
    enabled state, permanent spatial loops and lifetime-qualified restoration.
    Wall/attached activation, actual PCM, removal/reuse and normal/legacy layouts
    have source, native and Android controls. See
    [ambient-object evidence](../../XL_compat/evidence/ambient-objects.md).
    Particles, lightning and other independent effects remain blocked.

61. Ordinary WAV replacements: the owned complete bank now installs selected
    mission/normal/secret WAVs for gameplay and linked consumers, with actual
    output and saved voice isolation through Redux. Source startup defects and
    the deliberate fixes are documented in
    [classic playback evidence](../../XL_compat/evidence/classic-playback.md).
    The partial-bank cache experiment did not show enough improvement to retain;
    production caching remains unchanged. Custom S11/S22 banks were still gated
    at this increment; the following mission-bank work replaces that gate.

62. Static explosions: type-19 stationary vclip objects use separate XL
    animation, sprite/light output, noncollision and permanent sound ownership.
    Loaded 23/40, 27/40, 17/36 and secret controls compare source startup/animation,
    PCM and persistence; Android checks actual pixels and restore through Redux.
    See [static-explosion evidence](../../XL_compat/evidence/static-explosions.md)
    for current acceptance results and remaining graphics/settings limits.
    Pumo contains no type-19 records. Optional mission/settings-dependent secret
    manual saving remains unqualified; low-level secret serialization is not
    proof of that UI behavior.

63. Owned mission sound banks: bounded S11/S22 decoding now installs selected
    samples with source-matched name and folder/HOG precedence. Partial banks
    preserve omitted stock samples instead of reproducing a source offset bug.
    Actual Windows, Linux and Android output, signed scopes, mutation rejection and
    restoration through Redux pass. See
    [mission-bank evidence](../../XL_compat/evidence/mission-sound-banks.md) for
    the 106-test Linux acceptance and exact platform receipts. DTX2 files, imported-D1
    banks, global addons and original custom-bank campaigns remain unqualified.

64. Mine disturbances and Earthshaker rocking: XL-owned type-36 actions retain
    authored seconds, one-shot pending intervals, player/companion physics,
    distance-scaled detonation slots and saved cadence. Actual source comparisons,
    both native layouts and 64 Android disk/rewind/PCM controls pass. See
    [seismic evidence](../../XL_compat/evidence/seismic-gameplay.md) for final
    regression status, clock/RNG policy and remaining profiles. Two Pumo action
    rejections are removed; independent wall/effect blockers remain.

After the wall, cloak, trigger, object and changing-light batches, continue the capacity and resource foundations, then the
remaining actor and event variants in dependency order. Use original counts to
choose effect/audio fixtures, with activation, gameplay cues and cleanup treated
as required acceptance rather than cosmetic deferral. Do not mark
the overall goal complete from a count of completed chunks or green fixture
tests. Audit each area against live behavior and original mission requirements
