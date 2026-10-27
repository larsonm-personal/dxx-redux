# Singleplayer gameplay support roadmap

Continue the original 10-20 comparable chunks estimate from
[the teleport plan](xl_wall_teleports_20261025.md). The estimate describes broadly
useful gameplay fidelity with a longer tail of unusual missions; it is not a
fixed feature count, percentage, or promise of universal mission support

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

| Work area                                                 | Current evidence and remaining acceptance                                                                                                                                                                                                                                                                                         |
| --------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Teleports, speed boosts, spawn destinations               | Immediate wall teleports verified. Boost direction/constraints/release/orientation, boost-area collision, spawn state and interactions remain open                                                                                                                                                                                |
| Trigger chains, timing/flags, object events               | Wall scheduling and nonplayer object leaf/immediate-chain activation, lifecycle and disk/rewind persistence verified. Action-specific byte flags have reference/native and Android mixer/persistence controls. Delayed object descendants, other object actions/owners remain open; boss destinations have separate checked state |
| Segment properties, walls and generators                  | Modern robot-free water/no-damage and bounded robot-matcen assignment, production, contact and persistence verified. Repair/team/ownership/lava/blocked/speed-area behavior, gameplay walls, legacy equipment layouts and expanded generator data remain open; modern equipment controls pass                                     |
| Layouts, capacities, custom gameplay data and transitions | Audited bounded 15/33, 17/36 and 23/40 readers exist. Other observed layouts/payloads, wider tables and dynamic spawn headroom, expanded HAM/HXM robot/weapon data and authored transitions remain open                                                                                                                           |
| Cross-feature regressions and original missions           | Existing Ahayweh Gate qualification and documented Neptune/Observatory evidence remain scoped. Combined movement/events/generators/combat/persistence controls and original missions exercising newly supported features remain open                                                                                              |

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
    Bits 32/64 are admitted for implemented actions; boost orientation remains
    gated. XL's bounded illusion handler intentionally corrects the reference's
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

Return to delayed object-chain lifecycle and other owner variants as part of the
unfinished event area above. Use the refreshed original counts to prioritize
effect/audio behavior, with activation, gameplay cues and cleanup treated as
required acceptance rather than cosmetic deferral

After that, continue the unfinished areas above one chunk at a time. Do not mark
the overall goal complete from a count of completed chunks or green fixture
tests. Audit each area against live behavior and original mission requirements
