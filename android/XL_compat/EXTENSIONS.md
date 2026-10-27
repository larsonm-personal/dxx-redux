# D2X-XL extension support catalog

The provisional singleplayer roadmap estimates 10-20 further comparable chunks,
with overlapping work areas and substantial uncertainty around trigger scheduling
and custom gameplay data. This is not a completion percentage. See the
[roadmap and wall-teleport plan](../ai%20tool%20plans/mission%20metadata/xl_wall_teleports_20261025.md)
for estimates, acceptance criteria and the next implementation tranche

The [maintained gameplay roadmap](../ai%20tool%20plans/mission%20metadata/xl_singleplayer_gameplay_roadmap_20261026.md)
tracks these overlapping areas and their acceptance requirements. Immediate wall
master chains and trigger enable/disable counters are now implemented, including
versioned source initialization and saved live state. Native and Android
controls pass; see [trigger-chain evidence](evidence/trigger-chains.md)

Delayed wall scheduling and permanent/alternate/autoplay flags are now implemented
for the supported actions. Native, disk-save and memory-checkpoint controls pass.
See the
[scheduler evidence](evidence/trigger-scheduler.md) for exact scope and remaining
flags/actions rather than interpreting a parsed timer as proof of support

Immediate singleplayer wall teleports now use detected requirements, including
destination validation, actor dispatch, reference-matched rotation and source
save restoration. Orientation/silent flags, object triggers, boosts and
multiplayer remain blocked. Derived-fixture qualification does not unlock an
original mission by itself; see [wall-teleport evidence](evidence/wall-teleports.md)

Normal OpenGL D2 builds admit levels by detected requirements and runtime asset checks,
without a mission-name or hash exception. Stock and XL missions share one binary; software rendering still rejects XL.
This is per-level eligibility, not campaign qualification. The original Ahayweh Gate
singleplayer pilot is now verified in the OpenGL runtime; see its
[acceptance record](evidence/pilot-acceptance-audit.md) and
[requirement reconciliation](evidence/pilot-requirements.md). This does not grant
campaign-wide compatibility. The native 15/33, 17/36 and 23/40 inventory and bounded
translation and Robotech water arena remain separate qualifications; inspection
alone does not admit a level. The executable catalog is `compatibility.cpp`;
the Android browser displays its per-source evidence. Keep descriptions and
support text synchronized with this catalog

The new 23/40 path admits only detected supported requirements. Modern no-damage
bit 8 now uses the existing protection behavior while preserving independent
segment functions. Derived native controls verify collision/firing, fusion,
boundary behavior and normal source-based save restoration. The normal Android APK
passes 45 derived-fixture protection, rendering-activity and lifecycle checks.
Modern water bit 1 and combined water/no-damage bit 9 now use ordinary admission
for the robot-free singleplayer profile. Native controls cover independent
functions, movement, turbulence, material boundaries, source saves and stock
isolation. The final normal APK passes 51 water and 45 no-damage checks, including
actual boundary draws, depth state, contact-loop playback and stock cleanup.
A matched-camera Android water surface/boundary capture also passes. Legacy water
remains experimental. Robots/generators in water, other property bits, signed
damage and multiplayer remain rejected. See the
[modern-property evidence](evidence/modern-properties.md) and
[implementation plan](../ai%20tool%20plans/mission%20metadata/xl_modern_properties_20261025.md).
Other modern properties/damage, multiplayer-only objects, wide wall flags and object
triggers remain blocked. Unchanged Neptune passes native geometry/record checks, ordinary
startup, all authored LGT entries, switched-light save/restore, reactor/door and
controlled exit dispatch, and stock-to-save restoration. Its 24 detected texture
emitters all use authored brightness values that differ from defaults. These
controlled checks do not establish traversal or rendered-scene fidelity. A normal
Android APK additionally passes 65 rejection, startup, shading, menu indication, save/restore,
stock-transition, emitter-view and POG checks. All seven live replacement payloads
match their original fingerprints and become GPU-resident. The pinned XL runtime's
287 baked vertex colors agree with the decoded table within capture precision.
Android rejects injected unsupported property bits, multiplayer-only objects and malformed
CLR with specific messages, then successfully loads unchanged Neptune. Bounded
rendering comparison and authored station/pickup/door checks are reconciled in the
[Neptune acceptance record](evidence/neptune-acceptance.md); its procedural halo
differs visibly from XL's bitmap glare. Detailed checkpoints remain in the
[Neptune plan](../ai%20tool%20plans/mission%20metadata/xl_neptune_support_20261025.md).
For 23/40, owned CLR is decoded and validated but the complete embedded texture
color table takes precedence, matching the pinned XL readers. Original,
conflicting and absent CLR controls preserve those colors. Foreign LGT/CLR and
malformed owned companions remain rejected before commit. Other D2 CLR profiles
remain blocked.

The 17/36 [segment-flag audit](evidence/segment-flags-17-36.md) resolves
Observatory's 37 high-bit records as preserved reserved data in the pinned
reference. Only the low ambient-water/lava bits have runtime consumers. The raw
bytes remain visible. No-damage function 13 is now admitted through the same feature
and resource checks as other implemented requirements; see the qualification below.

The bounded 17/36 translator removes the extra equipment header,
rebases retained offsets, and rebuilds every packed light-index record into
ordered stock-sized chunks. It requires empty equipment data and retains all
semantic blockers. Binary tests cover packed counts 0 through 8191. A native
loader control derived from Observatory verifies name, geometry, reserved bits,
light indices and delta payloads after explicitly replacing its 26 no-damage
functions with ordinary segments. That changed fixture is layout evidence only;
the unchanged original is tested separately through normal admission and save loading.

Konflict is the next original-distribution acceptance fixture for this layout.
Its unchanged d2x-name descriptor and 17/36 level now pass native startup,
geometry/light-record comparisons, switched-light save/restore, reactor countdown
and linked-door checks, and return to stock. Its LGT contains 82 differences from
the default brightness table. The runtime owns the entire validated table before
commit and uses it for texture-emitter selection; a foreign shadow fails without
replacing active geometry or lighting. Missing LGT retains the defaults; malformed
present companions fail, and D2 CLR overrides remain explicitly rejected.
There is no mission-name allowlist. This is bounded feature evidence, not an
uninterrupted playthrough or multiplayer qualification. See the
[tranche plan](../ai%20tool%20plans/mission%20metadata/xl_konflict_tranche_20261025.md)
for source hashes and verification. The Android acceptance runner passes 33/33:
startup, rendering, save/restore, XL menu indicator, stock switching and a
controlled authored-light face with actual corona draws and no depth failures.
Native controlled exit dispatch also passes; no route traversal is claimed.

Original Observatory now passes 45 normal-APK behavior/lifecycle checks and a
controlled rendered view. Its initially black mine exposed missing ambient
reconstruction: levels without registered baked colors now use the detected
method-0 geometry-light path. All 224 light sources and 328 ambient vertex colors
match the pinned running reference (maximum RGB difference 1.47e-7). Native
load/restore tests retain that comparison. This is bounded singleplayer evidence,
not a full playthrough or universal rendering parity.

The [no-damage semantic audit](evidence/no-damage-reference.md) records the implemented
function-13 protection behavior and its qualification limits. Reference behavior includes firing gates, collision
response and effects, with distinct flare-energy and queued-missile accounting.
It is not equivalent to player invulnerability. Headless controls now verify
final player-damage suppression and direct weapon impacts against ordinary
segments, using an owned original-property binding available only in the test
build. Native firing controls also cover primary and fresh missile/bomb refusal,
flare and queued-missile resource costs, parent/muzzle segment distinctions and
robot weapon creation. Synthetic wall-contact controls verify hit-segment damage
suppression, retained force-field response and lava/water classification with
thrust-dependent rotation. Paired radius-explosion controls verify preserved
force/rotation and flash-missile blinding while suppressing shield loss. Native
player/clutter and player/player controls check bump and sound-gate suppression
with receiver-segment distinctions. Optional persistent-debris behavior, full guided flight, general wall traversal
and exhaustive splash combinations remain unqualified. These controls establish
the listed cases, not universal combat parity.

Guided dispatch controls verify that fresh protected fire retains control, normal
release works after exit, and an already queued protected event releases without
spending ammo or creating another projectile. These use explicit boundary
placement and queue setup; original-source persistence is verified separately below.

Controlled shared-face physics now verifies both directions through an original
Observatory portal, with identical movement/RNG to neutral-property controls and
immediate damage/firing gates on arrival. Separately compiled, hash-pinned XL
functions match 12 native firing-gate and four contact-dispatch cases. This is
isolated-function evidence, not full reference gameplay or collision-impulse
parity. Reference actor/debris contacts are conditional on optional persistent
debris; the default disables them. Original-source save/restore now reconstructs
all 26 properties through the normal loader in the native harness, with continued
fusion and guided behavior after restoration and stock isolation. No private
admission switch is needed. Normal-APK qualification is recorded in the no-damage
audit; this is bounded singleplayer support, not a full mission playthrough.

A full-frame fusion control reproduces and fixes queue loss after protection:
detected no-damage profiles retain a charged pending shot on released input,
recharge for one frame on exit as in the reference, then emit with the retained
charge multiplier and normal energy costs. Profiles without this property retain
classic behavior. Original-source persistence is verified; other fusion timing
cases remain unqualified.

Fresh protected fusion input and native weapon-switch cancellation also have
frame controls. XL's general manual-selection charge refund is a known unported
policy: Redux clears charge on switching without refunding it. This is separate
from serialized no-damage properties and is not claimed by those controls.

OpenGL descriptor selection now reloads verified D2 base resources without treating
d2x-name as an extra-HAM dialect. Native tests verify replacement of stale prior
definitions, capture of an owned baseline, shadow HAM/default-PIG rejection and
D1 -> XL descriptor -> D2 recovery. Software-renderer builds retain discovery-only behavior.
Supported extended levels can now load under this descriptor after feature and
resource preflight; stock-format levels under d2x-name remain guarded.

Mission selection now owns a bounded descriptor snapshot. Native field
parsing and XL resource-option checks consume the same bytes, with DOS/Mac line
endings normalized once and embedded NUL bytes rejected. Admission does not
reopen a selected mission's descriptor; edits take effect on reselection.
Failed selection preserves the prior snapshot with its mission and resources.

Mission identity allocation now precedes Android preparation and prior
mission teardown. Native fault controls cover allocation of both the Mission
object and its path, retaining the previous mission/archive/resource baseline.
This closes that allocation window; later mounts and resource initialization
are not thereby transactional; supported extended levels still require their
own feature/resource preflight before native loading.

Visible native companion HOGs now undergo a private mount probe before teardown.
Unreadable or foreign-owned archives reject while preserving the prior mission
and search-path order; the mission's own mounted archive can be reselected.
Native commit acquires the candidate mount before teardown, so disappearance or
corruption after preflight also preserves the previous mission and search order.
Reselection transfers the owned mount without closing open archive members.
If an open member prevents releasing the previous archive during a different
selection, the candidate mount is removed and the previous state is retained.
Android catalog activation and later resource initialization still have separate
failure boundaries; this does not establish full mission-switch rollback.

Rejected XL level candidates now retain the active extension state until admission
and translated-byte validation finish. Native tests verify preserved water,
shading/materials, geometry, objects, light masks and source identity. Successful
stock loads still clear XL state; multiplayer mode changes explicitly deactivate
singleplayer extensions. This does not yet roll back native-load failures after
commit or restore a previously selected mission's descriptor/mount context.

Opt-in builds now parse non-builtin D2 descriptors before discarding the current
mission, using the same caller-owned parser as actual loading. On Android this
check reads the published discovery descriptor before asset preparation. Native
tests verify that malformed candidates retain the previous descriptor, mounted
archive and owned resource baseline. Loading consumes the owned validated fields;
a native source-replacement test verifies that later file changes cannot replace
the level list or dialect. Ordered briefing/ending lookup remains deferred until
mounting. Activation/resource failures after this check still require rollback.

Android catalog preparation now probes candidate mounts in an isolated virtual
namespace before reset. Corrupt archives, duplicate paths and mounts owned by
another context are rejected without changing the active search path. Native
tests use the actual catalog/PhysFS code with reset spies and verify repaired
archive activation and reselection. Preparation now acquires all candidate mounts
before destructive reset; activation transfers ownership without reopening them.
Late acquisition failure removes candidate mounts while preserving the old owner,
generation, search order and reset counts. Reselection retains open handles.
Published mission contexts are disjoint; overlapping contexts are rejected.
Activation requires a successful preparation and consumes that transition once.
The 59-step Android water test now injects second-mount failure during gameplay
and verifies preserved water/audio state, subsequent movement, save/restore and
stock switching. Its old mission has base catalog ownership; imported-context
retention still has native coverage only. This is not complete playable-mission
rollback: old-mount release, loose companion archives and later resource
installation failures still need coverage. Directory contents are not snapshotted.

Changing-light translation splits 15/33 counts above 127 into ordered records
that fit the stock signed count. It preserves source data and rejects expanded
tables above 500 indices. Native tests verify 128- and 255-target fixtures through
normal admission, decoded records and every source's light-off/on UVLs, including
ordered clamping. The 17/36 path also rebuilds packed counts into these chunks;
the original Konflict fixture verifies the native records and persistence.

XL resource admission checks both base sound banks against their startup contents,
including bank absence, alongside the base HAM identity. Identical mounted copies
are allowed; changed banks are rejected before resource reset with the bank name
in the error. Native tests verify unchanged active resources and recovery after
unmounting the override. Selected level palettes and PIGs are likewise compared
with startup content before XL preflight and loading; native tests cover changed
files, unchanged active graphics state and new-game recovery. Custom base resources,
immutable check-to-read handling and full mission rollback remain separate requirements.

The separate experimental `-xl-water-physics` profile now detects water requirements
without mission-name, filename or source-hash admission rules. Native tests cover a
renamed/repacked/changed-hash fixture as well as explicit rejection of unqualified
water/robot combinations. A 57-step Android emulator probe verifies controlled
water movement, authored portal crossing from staged poses, live waterfall mixer state,
dry-space sound cleanup, wet/dry save restoration, stock isolation and disabled-profile
rejection. Multiplayer, robot combat/generation, full Android authored traversal
and reference water presentation parity remain outside this profile's qualification. This does not
promote water to generally supported status in the browser catalog

An optional Android input-replay phase now flies from the original dry spawn to
water segment 187 using 1,235 recorded control frames, with no pose injection.
The complete terminal result matches the fresh-process native replay. Recording
and playback use the same 1920x1080 projection; validation is not bypassed.
The launcher forwards an explicit experimental-water opt-in for replay only,
honored by XL-enabled native builds. This adds one authored route's cross-platform
simulation evidence, not whole-arena coverage or heard/rendered presentation parity.

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
with solid-boundary controls. The normal Android x86_64 runtime now builds and
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
31/33/34/35/36/40. Our native reader and OpenGL translator support bounded 15/33
17/36 and 23/40 layouts; other layout combinations remain rejected.

## Maintained categories

The 19 browser rows retain the original IDs and expose previously bundled work.
Dot's ten areas map to several rows where decoding, behavior and presentation
need separate evidence. A category can contain supported classic features and
unsupported XL extensions

| Browser ID / category                                 | Inventory and corpus evidence                                                                                                           | Current production support / investigation                                                                                                                    | Acceptance slice                                                                                                                                 |
| ----------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| `container` Archive/descriptor dialect                | DHF versus D2X, signed member sizes, long names, `d2x-name`; survey reports one D2X HOG and 48 XL descriptors                           | DHF and d2x-name supported with per-level feature/resource checks; extended archives unsupported                                                              | Bounds, truncation, name collisions, descriptor precedence and dependency closure                                                                |
| `serialization` Versioned layouts                     | Separate level/game versions; v13 wall-width, v21 segment-layout and color/object transitions                                           | OpenGL builds: stock 1-8 and bounded XL 15/33, 17/36 and 23/40                                                                                                | Exact section ends, malformed references and version fixtures; never just relax version checks                                                   |
| `capacity` Tables and reference widths                | 21 levels exceed 255 walls; two exceed 1000 objects; two exceed 100 triggers                                                            | Stock: 9000 segments, 36000 vertices, 255 walls, 1000 objects, 100 triggers; no general XL widening                                                           | Indices 254/255/256, sentinels, allocation headroom, saves/demos/network and metadata consumers                                                  |
| `segments` Physics, properties, ownership             | Old combined types versus v21+ split properties; blocked, water/lava, outdoor, no-damage, shield/energy damage, teams/groups            | Classic repair centers exist; XL semantics unverified                                                                                                         | Equivalent old/new encodings, damage timing, traversal and ownership                                                                             |
| `walls` Wall types and flags                          | Extended types/flags; paired sides and door/open/close/cloak transitions                                                                | Classic subset; XL parity unverified                                                                                                                          | Collision, projectile passage, activation and persistence                                                                                        |
| `triggers` Stateful events and wall triggers          | 99 masters, 14 enable and 19 disable wall actions; delayed/permanent/alternating/autoplay flags; versioned master-value initialization  | Wall scheduling, permanent/alternate/autoplay and saved counters implemented; orientation/silent flags and object events unsupported                          | Finite event interpreter, dependency chain, delay/repeat, cancellation, save/reload; no general scripting language assumed                       |
| `object_triggers` Object activation                   | 212 triggers in 22 survey levels; bounded 15/33, 17/36 and 23/40 action/owner decoding; [current evidence](evidence/object-triggers.md) | Nonplayer leaf events and immediate masters verified natively and on Android, including disk/rewind; delayed descendants and other variants gated             | Threshold/destruction, ordering, once/repeat flags, restore and network identity                                                                 |
| `objects` Objects and AI                              | Type/control/movement/render variants, identities and robot behavior                                                                    | Classic behavior exists; field agreement is not AI parity                                                                                                     | Ordinary combat, collisions, contained items and object lifecycle                                                                                |
| `gameplay_assets` Custom game data                    | Robot/weapon/model dependencies, HAM/HXM/PMH and expanded tables                                                                        | Substantial classic HXM/extra-HAM exists; XL alternate formats/counts unverified                                                                              | Reference closure, failure rollback and custom weapons/robots; never alias PO2 PMH into stock HAM                                                |
| `baked_lighting` Embedded and sidecar colors          | Non-gray vertex/side/texture tables in 124/58/129 levels; CLR is a separate resource source                                             | OpenGL 15/33, 17/36 and 23/40 embedded colors; 23/40 embedded table supersedes CLR                                                                            | Float/integer transitions, resolved color precedence and matched reference frames                                                                |
| `dynamic_lighting` Changing lights and modes          | Switchable/flickering lights, dynamic sources, reactor/seismic flash, per-pixel/lightmap/shadow settings                                | Arithmetic comparison exists; full integration/modes unverified                                                                                               | Light on/off, shot sources, flash, save/reload; pin profile, never extrapolate across modes                                                      |
| `visual_assets` Texture/model formats                 | TGA/OOF, layered/animated materials; broad-corpus counts are not exclusively XL                                                         | Individual formats/paths need verification                                                                                                                    | Orientation, filtering/alpha, model rendering and disclosed fallback                                                                             |
| `mission_flow` Campaign behavior                      | Secret origins/returns, directed exits, dependencies and objectives                                                                     | XL campaign behavior unverified; no modern hub-persistence claim                                                                                              | All branches, transitions/returns and saves; one level never certifies 30                                                                        |
| `runtime` Qualification                               | Combat, progression, presentation, persistence, isolation, replay and multiplayer                                                       | Bounded OpenGL pilots with explicit evidence; no general campaign or multiplayer claim                                                                        | Scope and evidence for each claim; all required categories must qualify                                                                          |
| `navigation` Teleports, boosts and spawn destinations | 108 wall teleports, 281 boosts, 20 spawn-setting actions; object-trigger transport also occurs                                          | Singleplayer wall teleports include delays and stateful flags; orientation/silent flags, boosts, spawn actions and route-analysis edges remain unsupported    | Reference transforms, actor crossings, arrival collision and source save restoration verified; campaign and guidebot routing unqualified         |
| `generators` Equipment and spawning                   | 53 equipment generators in 11 levels; robot assignment permutations occur in original Ahayweh Gate levels                               | [Bounded robot matcens verified](evidence/robot-generators.md) natively/on Android, including disk/rewind; equipment, expanded data and object spawning gated | Assignment, activation, selection, health, contact impulse/RNG, allocation retry, exhaustion and restore; original missions separately qualified |
| `wall_materials` Authored opacity/materials           | Type-8 colored walls (Glass House: 126), opacity on ordinary walls; pilot has 14 nonzero cloak/opacity records                          | Classic cloaking/bitmap transparency exists; XL semantics unverified                                                                                          | Visibility, collision, targeting and projectiles independently; fixed opacity and layers                                                         |
| `effect_objects` Particles, lightning, ambient sound  | In 129 decoded sections: 330 particle, 151 lightning, 1335 sound records                                                                | XL payloads and behavior unverified                                                                                                                           | Versioned payload ends, preserved IDs/references, activation, looping-audio cleanup; cues may affect gameplay                                    |
| `resource_scope` Precedence and isolation             | Mission/level textures, models, music, sounds, CLR and custom-data overrides                                                            | Standard resource paths and HMQ dispatch exist; XL precedence/cleanup unverified                                                                              | Mission A -> stock -> mission B, failed overrides, saves and audio/model/texture cleanup                                                         |

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
   Level 2 is eligible in the OpenGL runtime but remains unqualified; broaden device/performance evidence
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

Counterstrike level 2 (Turnabout Bore) now has a native prerequisite test using
the original archive: direct invocation of the next-level path, all 69 authored
robots retained, two seconds of ordinary thrust/fire, and native checkpoint
restore after a level-one restart. Identity, player position/energy/shields,
robot count and light UVLs survive restore. The transition is invoked directly,
not earned through level one's exit. Its 39 triggers (including ten generator
activations), three generators, full combat/branch route, countdown and Android
presentation remain separate qualification work. These results do not extend
the verified pilot's completion claim to level two.

A separate earned campaign-transition test now follows the normal key/reactor/
escape route into original level two and restores its arrival checkpoint. This
exposed a native divide-by-zero: the old object loop continued after the exit
replaced the level and reset FrameTime. Both engines now stop object iteration
and the old frame at a level change. The D2 earned route verifies this boundary;
it does not establish level-two completion or Android transition presentation.

The ordinary-input continuation from earned arrival reaches level two's blue
key alive and restores its earned checkpoint. The added xl_level_two_keys
acceptance test remains failing at the red-key continuation: the controller
dies in combat. Its logs preserve these failures; passing blue-key evidence is
not a claim that both keys, generator behavior or level completion are verified.

Generator-specific route observations now distinguish segment targets from wall
targets. The red-key attempt activates the segment-233 generator and observes
five robots whose IDs match its authored mask. A checkpoint after its first
spawn preserves masks, station mapping, capacity, lives, timers, robot provenance
and earned resources; the observed remaining spawn sequence matches the run
without that checkpoint. The other two generators, repeat/exhaustion semantics
and reference behavior remain unqualified, and the overall route still fails.

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

## Distribution packages and scoped-resource pilot

The package follow-up uses Dragon Pit Outpost from the original `levelpack.7z`
as a separate resource-support pilot. Its classic D1 geometry does not make the
package equivalent to a stock mission: it supplies scoped boss WAVs and LGT/CLR
lighting companions. Native D1 and imported-D1 behavior require separate evidence.
See the [package pilot plan](../ai%20tool%20plans/mission%20metadata/xl_package_pilot_20261024.md)
and the imported [package-layout study](../ai%20tool%20plans/mission%20metadata/xl_mission_package_support_20261024.md).

The browser now separates package identity, scoped textures, sounds, music,
models, companion lighting, configuration options and distribution extraction.
Level-only reports leave those categories unknown. A ZIP/7z/RAR distribution is
distinct from its mission HOG, and reading its catalog does not prove decoding.
Extracted mission targets now attach a native report from the verified extraction
record and the launch catalog's ownership/deduplication policy. It lists candidate
paths and SHA-256 values for scoped sounds, textures, music, models, options and
loose lighting companions. These are catalog candidates, not active-HOG or
ordinal selection, decoding or qualification evidence. Zero candidates remains
unknown because embedded archive members and other resource sources are outside
this inventory. Direct unextracted archives and loose-file targets do not yet
provide this report. Embedded HOG lighting inspection remains separate.
An Android original-package witness verifies Dragon Pit's two boss WAV paths and
hashes through the extracted-target/JNI/report pipeline and production dialog;
its lighting restrictions remain present. Long table evidence is previewed with
an ellipsis and available in full in the feature explanation. This is browser
evidence, not active resource selection or gameplay qualification.
Reports now carry an explicit `blocks_analysis` flag, separate from displayed
support status. Only Boolean false permits informational reports to bypass that
restriction; existing detected level/descriptor/lighting reports retain it.
Package evidence cannot suppress a separate level blocker or promote support.
The original Dragon Pit distribution exposed a BCJ2 decoder limitation; its
targeted native 7z fallback now extracts the admitted catalog in one pass with
path, byte, CRC and completion checks. Original-package Android import passed,
and the extracted descriptor, HOG and two boss WAVs match the audited originals.
This proves import for that package, not all 7z methods or gameplay support.
Launcher base-file compatibility dispatch recognizes 7z/RAR owner signatures,
including extensionless downloads, before attempting a DXA ZIP manifest read.
This reads at most eight bytes and preserves ZIP/DXA checks; full archive
validation remains part of extraction. The original-package Android lifecycle
check passes with the final dispatch and no owner-as-DXA ZIP warnings. This
does not add nested-DXA base-requirement validation or change admission status.

The opt-in imported-D1 WAV runtime path selects only active-context files using
the mounted HOG identity and incoming signed level ordinal. It explicitly
converts PCM and records installed-byte evidence; unsupported scoped sound banks
reject instead of implying WAV fallback support. Controlled Android boss probes
played both original WAVs, with matching installed PCM and mixer fingerprints.
The death WAV also reached natural SDL_mixer completion; a save/restore negative
control stopped a second playback without counting it as completed. Debug-only
`sound_playback` introspection exposes process-cumulative per-sample attempts,
natural completions, stop requests, failures, and last callback elapsed time/bytes.
These are playback diagnostics, not mission support flags.
The logging-gated mod_usage snapshot additionally records actual scoped WAV
selection against the mounted HOG, signed ordinal and mission key/generation,
with a current-context check. Android save/restore and mission-to-stock-to-mission
tests verify these records and invalidation. They remain separate from static
browser candidates and do not promote a package's qualification status.
Native D1 death scheduling is retained; the pinned XL reference reads an empty high-resolution
length slot and uses a one-second fallback. Exact reference death timing, normal
combat and native-D1 parity remain unqualified.
The probes used teleportation and scripted lethal damage; they do not establish
normal traversal or combat. No successful startup or decoder test
may promote Dragon Pit to fully supported without those gates and normal-input
mission completion.

LGT/CLR companion decoding is separately tested against the pinned 910-entry
layout and the original Dragon Pit HOG. The bounded readers reject incomplete
or oversized tables and preserve signed brightness/color values and the serialized
CLR byte for inspection. XL resets that byte when applying texture colors.
The opt-in imported-D1 runtime now applies these tables through owned ambient
lighting state and the vertex renderer hook. Native and refreshed Android lifecycle
probes cover mission release, stock transitions and save/restore after the latest
geometry corrections. Matched-viewport native
comparison agrees with all 1,737 nearest-light lists and vertex RGB values within
0.0000002 of a separately labeled reference with its uninitialized visibility
buffer corrected. See [lighting comparison evidence](evidence/dragon-lighting-comparison.md).
This verifies the bounded calculation profile. A separate native still-view
comparison now measures five static surface regions after matching effective
projection and texture filtering. Four regions have mean channel errors below
0.066 on the 0-255 scale. The remaining warped floor differs because normal
rendering preserves its compiled triangle split while the reference's non-mesh
renderer uses a four-corner fan. An explicit diagnostic fan draw reduces that
region's error from 9.408 to 0.056, leaving the other four regions unchanged.
The original comparison retained the compiled split; this documented visual difference
is not a reason to alter collision geometry. These bounded captures do not verify
other views, dynamic lighting, Android rendering or normal mission completion.
Companion lighting remains unqualified in the browser until the remaining gates
pass.

HOG summaries now inspect embedded LGT/CLR members and expose their decoded
counts or decoding errors as blocked companion requirements. D1 MSN descriptors
follow their sibling HOG like D2 MN2 descriptors. Detection does not establish
which level consumes a member, or whether it is equivalent to defaults; those
remain explicit verification requirements.

A controlled Android still-view capture now provides a matched five-region
lighting witness with exact camera/viewport receipts and hashed pixels. Four
regions have mean channel errors0.010..0.035 (0..255 scale); the warped floor
remains9.403 with normal compiled face splits. This is bounded diagnostic
rendering evidence, not full visual or gameplay qualification. Details and
provenance are in [the comparison evidence](evidence/dragon-lighting-comparison.md).
Package/mission support status is unchanged.

Warped-face drawing is also profile-dependent in the pinned XL source: its
no-mesh lighting path uses four-corner fans, while mesh paths preserve side
splits or subdivide eligible quads. Treat this as renderer-profile fidelity,
separate from file-format and collision support. A classic warped side alone
must not become an XL-only admission blocker. The current floor discrepancy is
still recorded; no mission is promoted by this distinction.

The still-only Android fan diagnostic now has matched start, oblique and
mixed-facing reference views. At the start, the warped-floor mean RGB error
drops from 9.403 to 0.030; the oblique floor regions drop from 10.050/8.009 to
0.043/0.026. The mixed-facing projected triangle footprint has mean error
0.035 over 2675 pixels. Captures label the diagnostic explicitly and retain
normal compiled collision geometry. These bounded results justify further
work on a named renderer profile, not automatic promotion of companion
lighting or package support. Missions without prepared classic lighting retain compiled splits;
normal-input mission completion and broader authored-feature coverage remain
open. See the comparison evidence for hashes, fixtures and limitations.

The prepared experimental classic companion-lighting runtime now selects its
named `legacy_fan` visual profile during ordinary single-player D1 gameplay,
without a mission-name check or still-camera override. This requires successful
companion preparation; merely requesting lighting or loading a warped classic
side does not activate it. The 43-step lifecycle test verifies activation,
save/restore, stock Counterstrike isolation and restored pilot rendering with
both diagnostic flags off. Frame receipts report the effective profile
separately from diagnostic flags. This remains within the experimental
classic-lighting opt-in; admission and browser support status are unchanged.
