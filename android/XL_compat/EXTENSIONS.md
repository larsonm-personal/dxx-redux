# D2X-XL extension support catalog

Production XL admission remains unsupported. The exact original Ahayweh Gate
singleplayer pilot is verified in the opt-in build; see its
[acceptance record](evidence/pilot-acceptance-audit.md) and
[requirement reconciliation](evidence/pilot-requirements.md). This does not grant
campaign-wide compatibility. The native 15/33 and 17/36 inventory, bounded 15/33
translation and Robotech water arena remain separate qualifications; inspection
alone does not admit a level. The executable catalog is `compatibility.cpp`;
the Android browser displays its per-source evidence. Keep descriptions and
support text synchronized with this catalog

Pilot scope clarified by the user: faithful XL engine extensions; incidental
XL retuning of classic gameplay (for example, wholesale flare speed) need not
be copied. This does not mandate preserving every Redux gameplay choice.
Authored warps, speed boosts and other extensions must work wherever present.
Exact visual fidelity is not an acceptance requirement. Visual tweaks are
allowed; research into classic emitter formulas can inform implementation
without requiring exact image matching. Authored
extension data and its gameplay consequences remain mandatory

Latest pilot evidence: all 13 native trigger actions have host normal-input
coverage, with non-exit target persistence checked. All 14 authored-opacity
walls additionally pass native AI/object visibility and weapon-object queries
with solid-boundary controls. The opt-in Android x86_64 runtime now builds and
its explicit smoke test starts original Ahayweh Gate through normal menus,
retaining 38 robots and active XL shading. Its 34-step extension also verifies
quick-save/restore and pilot -> retail Counterstrike -> saved pilot isolation.
Controlled changes to one light and wall 6 survive both restores, with every
UVL/subtraction mask and restored material alpha checked. This is persistence
coverage, not normal-input operation of those fixtures. These checks do not
certify Android import or performance. A separate 22-step Android test now
verifies native exit from an earned checkpoint, the real score screen and bonus,
readiness-gated dismissal and usable main-menu return without level 2. The
checkpoint retains the native source-definition trailer; Android validation
was not weakened. This is not an uninterrupted Android playthrough.
Texture-colored coronas are now implemented, with Android pixel/color, real-wall
depth and object-pass depth checks. The reference `effects/glare.cpp` consumes
texture RGB even though legacy baked-vertex lighting does not recompute ambient
colors from that table. The isolated reference runtime now has enabled coronas
and loaded corona/glare resources. These observations do not establish exact
image parity, which is outside the requested scope. Current evidence and open
requirements are maintained in the [acceptance audit](evidence/pilot-acceptance-audit.md)

The experimental loader now explicitly rejects original and translated pilot
loads in multiplayer. Host lifecycle checks cover network/cooperative modes,
extension-state clearing, stock loading and subsequent single-player recovery.
The full 14-test progression/lifecycle sweep passes after this guard. This
closes a loader admission gap; multiplayer presentation remains unverified

This revision incorporates Dot's survey, checked on 2026-10-23. See
[evidence and qualifications](evidence/README.md) and the preserved
[original bundle](evidence/dot-survey.zip). Counts below are recomputed from its
decoded records, not a fresh scan of all mission bytes. The 131 distinct XL
payloads span 56 HOGs, level versions 11/15/17/18/19/21/23 and game-data versions
31/33/34/35/36/40. Our native reader inspects bounded 15/33 and 17/36 layouts; runtime probes remain 15/33-only

## Maintained categories

The 19 browser rows retain the original IDs and expose previously bundled work.
Dot's ten areas map to several rows where decoding, behavior and presentation
need separate evidence. A category can contain supported classic features and
unsupported XL extensions

| Browser ID / category                                 | Inventory and corpus evidence                                                                                                                     | Current production support / investigation                                                          | Acceptance slice                                                                                                           |
| ----------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| `container` Archive/descriptor dialect                | DHF versus D2X, signed member sizes, long names, `d2x-name`; survey reports one D2X HOG and 48 XL descriptors                                     | DHF and d2x-name discovery exist; extended archives and XL gameplay admission unsupported           | Bounds, truncation, name collisions, descriptor precedence and dependency closure                                          |
| `serialization` Versioned layouts                     | Separate level/game versions; v13 wall-width, v21 segment-layout and color/object transitions                                                     | Production levels 1-8; native inspection 15/33 and 17/36                                            | Exact section ends, malformed references and version fixtures; never just relax version checks                             |
| `capacity` Tables and reference widths                | 21 levels exceed 255 walls; two exceed 1000 objects; two exceed 100 triggers                                                                      | Stock: 9000 segments, 36000 vertices, 255 walls, 1000 objects, 100 triggers; no general XL widening | Indices 254/255/256, sentinels, allocation headroom, saves/demos/network and metadata consumers                            |
| `segments` Physics, properties, ownership             | Old combined types versus v21+ split properties; blocked, water/lava, outdoor, no-damage, shield/energy damage, teams/groups                      | Classic repair centers exist; XL semantics unverified                                               | Equivalent old/new encodings, damage timing, traversal and ownership                                                       |
| `walls` Wall types and flags                          | Extended types/flags; paired sides and door/open/close/cloak transitions                                                                          | Classic subset; XL parity unverified                                                                | Collision, projectile passage, activation and persistence                                                                  |
| `triggers` Stateful events and wall triggers          | 99 masters, 14 enable and 19 disable wall actions; delayed/permanent/alternating/autoplay flags; v39 master-value interpretation is a source lead | Classic actions exist; XL scheduler/event semantics unsupported                                     | Finite event interpreter, dependency chain, delay/repeat, cancellation, save/reload; no general scripting language assumed |
| `object_triggers` Object activation                   | 212 triggers in 22 levels; damage/destruction activation and stable references                                                                    | Unsupported                                                                                         | Threshold/destruction, ordering, once/repeat flags, restore and network identity                                           |
| `objects` Objects and AI                              | Type/control/movement/render variants, identities and robot behavior                                                                              | Classic behavior exists; field agreement is not AI parity                                           | Ordinary combat, collisions, contained items and object lifecycle                                                          |
| `gameplay_assets` Custom game data                    | Robot/weapon/model dependencies, HAM/HXM/PMH and expanded tables                                                                                  | Substantial classic HXM/extra-HAM exists; XL alternate formats/counts unverified                    | Reference closure, failure rollback and custom weapons/robots; never alias PO2 PMH into stock HAM                          |
| `baked_lighting` Embedded and sidecar colors          | Non-gray vertex/side/texture tables in 124/58/129 levels; CLR is a separate resource source                                                       | Inspected 15/33 and 17/36; 15/33 static preview only; production unsupported                        | Float/integer transitions, resolved color precedence and matched reference frames                                          |
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

Track four independent questions for each category: was the exact serialized
requirement decoded, is its runtime behavior implemented, was that behavior
exercised, and what presentation difference remains? A single support percentage
cannot answer these questions. Record source hash, field/record location,
reference version, test artifact and remaining uncertainty with each claim

Use `not required` only after complete inspection of the relevant level and
resource scope. A missing parser section is `unknown`; a decoded but unimplemented
requirement is `unsupported`; an implemented path without behavioral evidence is
`unverified`. These are evidence distinctions, not new browser status IDs

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

For the pilot, retain these evidence boundaries when updating any row:

| Evidence available                                       | What it establishes                                                   | Required next evidence                                                 |
| -------------------------------------------------------- | --------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| Survey and native inventory agree on exact source bytes  | Selected decoded fields corroborate each other                        | Resolve every required field and transitive resource dependency        |
| Translated fixture loads and draws                       | A bounded experimental path reaches rendering                         | Original-source admission with validated behavior and failure handling |
| Extracted reference arithmetic and lifecycle checks pass | Tested formulas and state transitions agree within their stated scope | Required authored extension behavior in live gameplay and persistence  |
| Assisted route reaches an exit                           | A route exists under the recorded assists                             | Ordinary combat, all pilot branches, countdown and native transition   |
| One level meets its gates                                | That level and tested profile can qualify                             | Separate evidence for remaining campaign levels and multiplayer        |

In particular, the current live lighting slice covers robot, debris, explicit
light, ordinary weapon, marker, indexed explosion, muzzle-flash and normal
single-player ship/headlight and flare sources. Alternate
player profiles and multiplayer are unverified. An unresolved-emitter count of zero is not a
rendering acceptance test. A saved reference profile is evidence of intended
settings; verify the running reference before accepting frame comparisons.
Frame parity and XL tuning of classic emitters are not pilot completion gates

The isolated original-source admission and real mission save/restore checks now
provide stronger evidence than translated-fixture loading alone. They preserve
mission identity, robots, light state and opacity across a controlled restore.
They do not establish ordinary combat, branch coverage or level completion, and
do not promote the production browser's status

Subsequent normal-input tests provide a separate, stronger progression result:
the player earns the red key, restores that earned save without replenishment,
and destroys the reactor while alive with the normal 45-second countdown.
The recorded pair of runs agrees. These tests use ordinary movement, weapons,
damage, pickups and physical trigger activation; the controller reads level
topology to choose inputs, so this is automated gameplay evidence rather than
a blind human playthrough. The subsequent `xl_normal_escape` test also reaches
native completion alive in 407 frames with 38.2 seconds remaining, twice, then
checks stock-state isolation. It does not prove every authored branch or
interactive completion presentation. See the pilot plan for exact artifacts
and superseded earlier runs

All 12 non-exit trigger tests now pass twice, with native activation, observed
linked-target passability and actual save/restore of target state and earned
player resources. Together with the native exit, this covers all 13 authored
actions. It does not establish physical traversal of every linked portal or
an uninterrupted all-secrets run. The pilot plan preserves the earlier failures
and the current passing artifacts; production admission remains unsupported

The next qualification sequence is therefore:

1. Retain the verified interactive completion boundary: the Android 22-step
   earned-checkpoint test passes, including score presentation and menu use.
   Level 2 remains outside admission; broaden device/performance evidence
   separately rather than treating the emulator result as universal
2. Retain the verified activation, passability and persistence coverage for all
   13 actions. Keep any unexercised physical branch traversal explicitly
   unverified; visiting a segment alone is not activation evidence
3. Close authored side/texture-color semantics. Retain the all-wall native
   visibility/targeting/projectile-query checks, distinguishing their controlled
   objects from live projectile damage. Loaded resources and geometric passage
   alone do not establish those behaviors
   The [pilot color audit](evidence/pilot-authored-colors.md) identifies 24
   initial chromatic corona candidates and traces side-color registration.
   The old reference screenshot has coronas disabled and remains insufficient;
   later resource-backed observations and Android occlusion tests are separate
   evidence. The two-segment ambient-state difference is now traced to the
   reference's paged-out bitmap flags during startup. A controlled opaque-boundary
   test reproduces it; Redux retains its existing PIG transparency flags rather
   than copying this classic loading artifact
4. Retain the Android ZIP import/startup evidence: the 15-step original-byte
   import passes with manually staged files moved aside and extracted hashes
   matching the source. The 34-step persistence/isolation test also passes
   through this imported path, including retail Counterstrike and pilot restore.
   Retain the private-fixture prerequisites and do not infer other campaign
   levels or production compatibility approval from these results

These gates refine the first pilot, not the corpus-wide implementation order.
Do not port teleports, generators or wider tables merely because the survey
finds them elsewhere; retain them as explicit later fixtures unless this pilot's
complete requirement inventory demonstrates a dependency

## Implementation order and fixtures

1. Finish Ahayweh Gate's exact 15/33 path, resource closure and state isolation.
   Classify the six loader differences by whether they lose an XL extension or
   reflect acceptable classic normalization; do not require incidental bit parity.
   Do not defer pilot
   wall-opacity behavior or changing lights behind generic format work
2. Verify ordinary combat, all 13 triggers/branches, key/reactor/countdown/exit,
   authored lighting/material semantics, saves and return to stock. Keep assisted
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
