# D2X-XL extension support catalog

Production XL admission remains unsupported. The native 15/33 inventory,
translation, static-light preview and assisted route are separate experiments.
None grants mission compatibility. The executable catalog is `compatibility.cpp`;
the Android browser displays its per-source evidence. Keep descriptions and
support text synchronized with this catalog

This revision incorporates Dot's survey, checked on 2026-10-23. See
[evidence and qualifications](evidence/README.md) and the preserved
[original bundle](evidence/dot-survey.zip). Counts below are recomputed from its
decoded records, not a fresh scan of all mission bytes. The 131 distinct XL
payloads span 56 HOGs, level versions 11/15/17/18/19/21/23 and game-data versions
31/33/34/35/36/40. Our native reader covers only 15/33

## Maintained categories

The 19 browser rows retain the original IDs and expose previously bundled work.
Dot's ten areas map to several rows where decoding, behavior and presentation
need separate evidence. A category can contain supported classic features and
unsupported XL extensions

| Browser ID / category                                 | Inventory and corpus evidence                                                                                                                     | Current production support / investigation                                                          | Acceptance slice                                                                                                           |
| ----------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| `container` Archive/descriptor dialect                | DHF versus D2X, signed member sizes, long names, `d2x-name`; survey reports one D2X HOG and 48 XL descriptors                                     | DHF exists; extended archive/descriptor admission unsupported                                       | Bounds, truncation, name collisions, descriptor precedence and dependency closure                                          |
| `serialization` Versioned layouts                     | Separate level/game versions; v13 wall-width, v21 segment-layout and color/object transitions                                                     | Production levels 1-8; native inspection only 15/33                                                 | Exact section ends, malformed references and version fixtures; never just relax version checks                             |
| `capacity` Tables and reference widths                | 21 levels exceed 255 walls; two exceed 1000 objects; two exceed 100 triggers                                                                      | Stock: 9000 segments, 36000 vertices, 255 walls, 1000 objects, 100 triggers; no general XL widening | Indices 254/255/256, sentinels, allocation headroom, saves/demos/network and metadata consumers                            |
| `segments` Physics, properties, ownership             | Old combined types versus v21+ split properties; blocked, water/lava, outdoor, no-damage, shield/energy damage, teams/groups                      | Classic repair centers exist; XL semantics unverified                                               | Equivalent old/new encodings, damage timing, traversal and ownership                                                       |
| `walls` Wall types and flags                          | Extended types/flags; paired sides and door/open/close/cloak transitions                                                                          | Classic subset; XL parity unverified                                                                | Collision, projectile passage, activation and persistence                                                                  |
| `triggers` Stateful events and wall triggers          | 99 masters, 14 enable and 19 disable wall actions; delayed/permanent/alternating/autoplay flags; v39 master-value interpretation is a source lead | Classic actions exist; XL scheduler/event semantics unsupported                                     | Finite event interpreter, dependency chain, delay/repeat, cancellation, save/reload; no general scripting language assumed |
| `object_triggers` Object activation                   | 212 triggers in 22 levels; damage/destruction activation and stable references                                                                    | Unsupported                                                                                         | Threshold/destruction, ordering, once/repeat flags, restore and network identity                                           |
| `objects` Objects and AI                              | Type/control/movement/render variants, identities and robot behavior                                                                              | Classic behavior exists; field agreement is not AI parity                                           | Ordinary combat, collisions, contained items and object lifecycle                                                          |
| `gameplay_assets` Custom game data                    | Robot/weapon/model dependencies, HAM/HXM/PMH and expanded tables                                                                                  | Substantial classic HXM/extra-HAM exists; XL alternate formats/counts unverified                    | Reference closure, failure rollback and custom weapons/robots; never alias PO2 PMH into stock HAM                          |
| `baked_lighting` Embedded and sidecar colors          | Non-gray vertex/side/texture tables in 124/58/129 levels; CLR is a separate resource source                                                       | Inspected 15/33; isolated static preview only; production unsupported                               | Float/integer transitions, resolved color precedence and matched reference frames                                          |
| `dynamic_lighting` Changing lights and modes          | Switchable/flickering lights, dynamic sources, reactor/seismic flash, per-pixel/lightmap/shadow settings                                          | Arithmetic comparison exists; full integration/modes unverified                                     | Light on/off, shot sources, flash, save/reload; pin profile, never extrapolate across modes                                |
| `visual_assets` Texture/model formats                 | TGA/OOF, layered/animated materials; broad-corpus counts are not exclusively XL                                                                   | Individual formats/paths need verification                                                          | Orientation, filtering/alpha, model rendering and disclosed fallback                                                       |
| `mission_flow` Campaign behavior                      | Secret origins/returns, directed exits, dependencies and objectives                                                                               | XL campaign behavior unverified; no modern hub-persistence claim                                    | All branches, transitions/returns and saves; one level never certifies 30                                                  |
| `runtime` Qualification                               | Combat, progression, presentation, persistence, isolation, replay and multiplayer                                                                 | No verified production XL mission                                                                   | Scope and evidence for each claim; all required categories must qualify                                                    |
| `navigation` Teleports, boosts and spawn destinations | 108 wall teleports, 281 boosts, 20 spawn-setting actions; object-trigger transport also occurs                                                    | XL movement actions and route-analysis edges unsupported                                            | Disconnected regions, position/orientation/velocity, respawn, collision and guidebot routing                               |
| `generators` Equipment and spawning                   | 53 equipment generators in 11 levels; distinct from robot matcens; spawn/countdown actions                                                        | Classic robot matcens exist; XL behavior unverified                                                 | Activation, replenishment/exhaustion, ordering, capacity and restore                                                       |
| `wall_materials` Authored opacity/materials           | Type-8 colored walls (Glass House: 126), opacity on ordinary walls; pilot has 14 nonzero cloak/opacity records                                    | Classic cloaking/bitmap transparency exists; XL semantics unverified                                | Visibility, collision, targeting and projectiles independently; fixed opacity and layers                                   |
| `effect_objects` Particles, lightning, ambient sound  | In 129 decoded sections: 330 particle, 151 lightning, 1335 sound records                                                                          | XL payloads and behavior unverified                                                                 | Versioned payload ends, preserved IDs/references, activation, looping-audio cleanup; cues may affect gameplay              |
| `resource_scope` Precedence and isolation             | Mission/level textures, models, music, sounds, CLR and custom-data overrides                                                                      | Standard resource paths and HMQ dispatch exist; XL precedence/cleanup unverified                    | Mission A -> stock -> mission B, failed overrides, saves and audio/model/texture cleanup                                   |

No surveyed level exceeds current segment/vertex capacities (maximum 6744/19257).
This does not prove all XL geometry fits or that bigger arrays fix narrower
references. D.I: Monolevel's 1888 walls, 1857 objects and 246 triggers make it a
later stress fixture. Dynamic spawning needs headroom beyond serialized counts

## Qualification and browser policy

- Loading, completable gameplay and presentation fidelity are separate claims,
  each bound to source hash, engine build, runtime profile and test scope
- Green requires verified full production support for the selected scope
- Orange requires verified loading/gameplay with identified presentation loss.
  It is an intermediate result, not completion of the user's fully verified pilot
- Red means a known unsupported requirement prevents loading or gameplay
- Gray means unknown or not required, distinguished in text. Partial scans,
  unknown assets and unresolved sections never become green by default
- Grayscale can be a tested presentation fallback. Opaque walls can change
  targeting; missing sounds can remove cues; omitted effects can break trigger
  references. These are not automatically cosmetic or safe for orange status
- Save/load, deterministic state/replay and multiplayer consistency are gates
  across every subsystem. Single-player results must label multiplayer unverified
  and cannot authorize XL network sessions. No broad multiplayer claim follows

The reporter emits only blocked/unknown/not-required and `playable: false`.
New categories remain unknown until native inspection provides specific evidence.
Survey records are research evidence, not browser lookups by mission title.
Source blockers aggregate, but uninspected levels/assets remain unresolved

## Implementation order and fixtures

1. Finish Ahayweh Gate's exact 15/33 path, resource closure and state isolation.
   Resolve the six loader differences against the reference. Do not defer pilot
   wall-opacity behavior or changing lights behind generic format work
2. Verify ordinary combat, all 13 triggers/branches, key/reactor/countdown/exit,
   matched lighting/material scenes, saves and return to stock. Keep assisted
   routing separate. A translated fixture alone cannot qualify production admission
3. The Apocalyptic Factor XL's 15 v15 levels are a second candidate family after
   per-level/resource inventory. Both it and D2-XL's 30 levels screen favorably
   for classic wall actions/counts; neither is certified
4. Add small version/width, event-chain, movement, environment and generator
   fixtures. Use Lunar Outpost Reloaded / Speed! for movement, Glass House /
   Crownpoint X for materials, and Sphere / Boiling Point for broader gameplay.
   Resolve Subzero's object-parser exclusion before treating it as a full fixture
5. Add effect/audio/resource fidelity slices and finally D.I: Monolevel /
   Anthology stress coverage. Move gameplay-affecting resources/effects earlier

Do not expand the pilot into a general XL port before its acceptance is complete.
Modern official 2.0 hub persistence and non-cubic geometry are outside this
historical corpus's evidence and require separate specification/version work

## Evidence boundaries

The survey references published Redux `1cadb4d3e5a39bd7fa08926df2e53725b1851b64`
and XL fork `2576a98b5de310e3ac7c9777df2677d1627b8bae`. It did not inspect our
unpublished layer or run missions. Our runtime/arithmetic investigation uses
local XL `11038eb53dd86f3bcdc027dddd3f93c60f02536e` and the pinned ar12 release.
These are distinct evidence sources, not one runtime specification

Keep PPG Hall light-table repairs, Uneasy4 performance, existing HAM/HXM repairs
and historical Belial failures in their original evidence categories. HMQ and
repair centers exist locally. Broad asset counts and historical PO2 PMH findings
do not establish new XL-only requirements

See the [pilot plan](../ai%20tool%20plans/mission%20metadata/d2xxl_first_mission_pilot_20261023.md)
and [browser plan](../ai%20tool%20plans/mission%20metadata/d2xxl_compatibility_browser_20261023.md)
for current gates and implementation evidence
