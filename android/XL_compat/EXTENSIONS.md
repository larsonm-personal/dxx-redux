# D2X-XL extension support catalog

The target is complete singleplayer fidelity in dependency order. The earlier
10-20-chunk estimate was for broadly useful support, not completion. Resource
ownership, data layouts and capacities come before the gameplay systems that
depend on them; original missions and persistence checks run throughout

XL flickers now use an owned full-size table and separate XL timing, including
right-rotating patterns, first-matching authored controls and exact phase across
disk/rewind. Source/native and all 50 Android controls pass; Redux retains its
original behavior. See [flicker evidence](evidence/flickers.md) for zero-delay,
visibility, emission, malformed-state and original-survey coverage. Alternate
renderer/settings profiles and original campaign qualification remain open.

Native D2 archive mounting and inspection now share a streaming DHF/D2X reader,
including per-member long names and exact resource ownership through save/load.
The Android launcher now uses that native format reader for import, catalogs,
metadata and music, including bounded streaming of Pumo Mines' large HOG.
See [container evidence](evidence/extended-hog.md) and
[launcher evidence](evidence/extended-hog-launcher.md). Container support does
not qualify contained gameplay. All fourteen Pumo levels now decode as 27/40,
with canonical geometry and source-reader comparisons; independent gameplay
requirements still block them. The [modern-layout evidence](evidence/modern-layouts.md) records the
complete requirement inventory, retained failures in other packages, and a
correction to the older light-index source adapter. Explicit side geometry and
fog now have browser rows. Canonical 25/40 and 27/40 runtime translation now has
native geometry, gameplay, resource and persistence controls plus all 87 Android
transition/save/rewind checks, documented in
[runtime-layout evidence](evidence/runtime-layouts.md). Noncanonical geometry
remains blocked. Authored fog now has a separate implementation and measured
GPU capability admission; see [fog evidence](evidence/fog.md) for source math,
actual rendering, reference quirks and remaining qualification. The expanded source comparison
also corrects generator initialization: rows follow geometry order after source
renumbering, not stale serialized producer IDs. Equipment startup and boss
resource selection use that corrected order.

The [maintained gameplay roadmap](../ai%20tool%20plans/mission%20metadata/xl_singleplayer_gameplay_roadmap_20261026.md)
tracks these overlapping areas and their acceptance requirements. Immediate wall
master chains and trigger enable/disable counters are now implemented, including
versioned source initialization and saved live state. Native and Android
controls pass; see [trigger-chain evidence](evidence/trigger-chains.md)

The [XL CI coverage](evidence/private-ci.md) requires public source comparisons
on Windows and Linux plus a controlled private Linux lane for runtime behavior,
original formats, saves and Redux recovery. Fixed membership rejects missing or
skipped tests and omitted source oracles. Original mission admission and human
campaign playtesting remain separate requirements.

Owned wall/object robot-disarming actions now suppress gunfire, melee and thief
decisions through XL-owned state bound to robot lifetimes. Missing/nonrobot
targets are inert; generated robot slots can be targeted. Save/rewind preserves
earned neutral state and pending actions. See [disarming evidence](evidence/disarming.md)
for actual attack consumers, source quirks and stock-isolation controls.
Reprogramming remains separate and rejected. One Pumo action rejection is
removed; all fourteen original levels still have independent blockers.

Owned wall/object countdown actions now retain their authored seconds and use
separate XL timing, warning, rotation and terminal-death behavior. Saved state
includes displayed seconds, extended duration and partial cadence. See
[countdown evidence](evidence/countdowns.md) for executable source comparisons,
the explicit simulation-clock boundary, actual output and persistence controls.
Pumo loses nine countdown rejections; all fourteen original levels retain
independent blockers and remain unqualified.

Owned wall/object texture-change actions now preserve XL's packed texture
values, retained overlay orientation and addressed-side behavior. Admission
checks future material dependencies; normal rendering and collision consumers
handle cleared rotated overlays correctly. See [texture-action evidence](evidence/texture-actions.md)
for source semantics, actual contact/rendering and delayed persistence controls.
Pumo loses fifteen action rejections in three levels; all fourteen levels retain
independent blockers and remain unqualified.

Owned level MSG text and wall/object message actions now have source/native
controls and all 80 Android display, modal, disk/rewind and stock-transition
checks. Timed text, modal pause, resource identity and saved phase use a separate
XL runtime. The browser counts these actions while retaining load-time text
checks. See [message evidence](evidence/level-messages.md), including the
same-frame save correction and all 21 original Pumo message dependencies.
SND parsing alone does not implement sound playback; Pumo's independent gameplay
requirements remain. Public Windows and private Linux coverage now require
38 and 94 tests respectively at that increment. The subsequent
[stereo PCM foundation](evidence/stereo-pcm.md) preserves both authored WAV
channels through shared bounded validation, with independent comparisons of
all 25 original Pumo WAVs and unchanged classic mono controls. It adds one
required public/private test; named playback was still gated at that increment.

The [named-audio source contracts](evidence/named-audio-source.md) execute the
pinned sound action, resource lookup, repetition, frame and restore behavior.
All 60 original Pumo sound actions resolve to owned SND/WAV resources on both
hosts. This establishes inputs and reference behavior, not audible playback.
That source-only increment required 40/41 public and 96 private contracts.

Owned named-sound playback now uses a separate XL voice bank and real stereo
output, with wall/object actions, repetition counts, source cooldowns, autoplay,
pause and exact saved playheads. Resource preparation rejects missing, malformed
or foreign SND/WAV files before installation. Restored voices wait for an
unpaused gameplay frame. See [named-audio runtime evidence](evidence/named-audio-runtime.md)
for actual SDL/Android sample comparisons, volume controls, disk/rewind and stock
transitions. Ambient sound objects and shared-channel
priority under mixed exhaustion remain unqualified. The browser retains a
resource-dependent partial row; this does not qualify the Pumo campaign.

The owned named-voice pool now uses XL's circular allocation cursor, half-pool
class restriction, persistence priority and recorded-volume tie rules. Saves
retain the cursor and captured priority as well as voice playheads. See
[channel evidence](evidence/audio-channels.md) for source comparisons, full-pool
controls and the explicit global-slider output difference. Ordinary and ambient
consumers still require a unified XL pool and their own lifecycle/spatial checks.

The mixer now also has source-verified per-channel gain and pan controls,
including different startup/update quantization and priority-volume updates.
Actual SDL output comparisons cover independently panned simultaneous voices.
See [channel-control evidence](evidence/audio-controls.md). These primitives
prepare shared ordinary/ambient playback; those consumers and their saved
nondefault channel state remain gated and unfinished.

The owned library now has a 500-entry sound registry, shared-channel owner kinds
and checked completion/replacement cleanup. Source and mixed PCM controls cover
compaction, full capacity and lifetime reuse. It intentionally repairs a proven
source deletion bug that loses unrelated sounds. See
[ownership evidence](evidence/audio-ownership.md). Engine routing for ordinary
and ambient sounds, spatial updates and saved owners are still unfinished;
this library foundation does not change mission admission.

Owned mission, normal-level and secret-level sound folders now preserve XL
resource priority and use the incoming signed mission ordinal, including repeated
level filenames. Native and all 65 Android publication/output/persistence checks
pass. The original Pumo resource audit resolves its 644 ambient objects to 41
owned external WAVs, with independent PCM comparisons. See
[sound-folder evidence](evidence/audio-scopes.md). This supplies resources for
the future ambient runtime; spatial playback, global addons and the remaining
external replacement consumers are still unqualified.

Ordinary D2 sound replacements now have separate source and production resource
checks. The selected output-rate mission bank suppresses external WAV lookup;
otherwise incoming normal/secret files precede mission files. All 1,988 Pumo
level/sample selections resolve to 196 WAVs with independent source and PCM
comparisons. The loader explicitly rejects unapplied classic replacements and
unsupported custom banks rather than silently using stock sounds. See
[classic sound-bank evidence](evidence/classic-audio-bank.md). This is resource
preparation and honest admission, not classic playback qualification.

The complete 183-sample stock bank and XL spatial audio calculation are now
implemented as preparation for ordinary playback. Stock PCM, routing, ear-angle
math and decay/pan have pinned-source comparisons; 2,232 outputs using loaded
mine geometry match on Windows and Linux. See [spatial audio evidence](evidence/spatial-audio.md).
The installed bank now shares 128 channels between stock samples and named WAVs.
Controlled Windows, Linux and Android output verifies mixed playback, class-aware
replacement, gain/pan, pause and saved unlinked channels. Source-content checks
guard reuse of converted stock PCM. See the shared-playback section of that
evidence for limits and platform results. Ordinary/ambient gameplay dispatch,
moving owners, wall changes and linked-owner saves remain unfinished, so classic
replacements remain gated.

All authored wall-key bytes now pass source/native comparisons and retain their
exact values through loading and saves. Nonstandard values are observations;
player hits, closed-wall passage and robot door tests keep their distinct
semantics. See [wall-key evidence](evidence/wall-key-bytes.md), including all 90
Android controls. Pumo loses 73 key-value rejections across nine levels, while
every level retains independent blockers. Wider wall flags remain gated.

Owned single-frame RGBA POG replacements now preserve source dimensions, pixels,
alpha/key behavior and averages through native installation, GPU upload, collision
and destructible-texture consumers. All 26 original Pumo replacements and Arena's
nineteen match the pinned pixel reader and desktop merge shaders. All 58 Android
GPU-readback, disk/rewind, context-loss and stock-transition steps pass. See
[POG runtime evidence](evidence/truecolor-pog-runtime.md) for exact coverage and
remaining animation, cockpit and renderer/settings limits. The
[earlier defect](evidence/truecolor-pog-admission.md) remains a historical audit;
its old mechanics checks were not texture evidence. HXM and INI application still
require complete package qualification, and every Pumo level retains independent
gameplay blockers. POG support alone does not admit or qualify the campaign.

XL wall tables now retain up to 2047 identities through validated native loading,
doors, cloaking, save/restore and cloak demo events. Stock and D1 import retain
their 255-wall limit. Native boundary/rejection controls and 80 Android steps
pass; see [wide-wall evidence](evidence/wide-walls.md). The 21 original high-wall
survey members are hash-matched, with 17 fully decoded and four still rejected
by independent readers. None is newly qualified. Wall-trigger capacity now has separate controls below;
object allocation capacity has separate controls below

XL wall-trigger tables now retain 254 rows and unsigned byte identities through
actual wall dispatch, chains, scheduling, metadata and disk/rewind restoration.
Stock and imported D1 retain 100. Source/native checks and all 77 Android steps
pass; see [trigger-capacity evidence](evidence/trigger-capacity.md). Object-action
count bounds now match the source's 254-row persistent limit. Dinter and Antholgy
retain their independent blockers; neither is newly qualified

XL object storage now follows authored-count headroom, up to 32768 runtime
slots, with separate allocation-pressure reclamation and 32-bit lifetime signatures. Redux and imported D1 retain 1000 active slots and classic save
widths. Native/source and all 53 Android checks cover full-pool boundaries,
high-ID combat/AI/events, exact disk/checkpoint allocator restoration and stock
isolation. See [object-capacity evidence](evidence/object-capacity.md). Both
hash-matched high-object originals retain independent blockers; wider object-slot
references, other capacities and broad performance remain unfinished.

XL cloak transitions now have separate activation/frame behavior, 100 concurrent
entries, checked saved owners/timers and source fade arithmetic. Redux retains its
ten-entry alternate. Native/source comparisons, stock controls and all 81 Android
steps pass, including actual draw alpha/audio and active disk/rewind restoration.
See [cloak lifecycle evidence](evidence/cloak-lifecycle.md) for the method-0/default-
speed scope and intentional reference corrections. Other wall/material profiles
and full demo fidelity remain unfinished

Delayed wall scheduling and permanent/alternate/autoplay flags are now implemented
for the supported actions. Native, disk-save and memory-checkpoint controls pass.
See the
[scheduler evidence](evidence/trigger-scheduler.md) for exact scope and remaining
flags/actions rather than interpreting a parsed timer as proof of support

Object-triggered robot spawning now has a separate XL boss runtime with per-owner
spawn, cloak, teleport, hit and death state. Redux retains its stock implementation.
Admission validates boss ownership and the complete selection-resource closure;
legacy equipment layouts, expanded data/capacities and dynamic boss creation remain gated. See the
[boss evidence](evidence/boss-spawning.md) for reference comparisons, lifecycle
controls, intentional restore corrections and final Android acceptance. Original
spawn-bot missions retain independent runtime and resource blockers.

Modern 23/40, 25/40 and 27/40 equipment makers now have separate XL production and checked
disk/rewind state. Native/reference and 61-step Android controls cover authored
activation, timing, allocation, pickups, replenishment and mixed robot/item
production. Legacy layouts, unsupported item types and independent mission
requirements remain gated. See [equipment evidence](evidence/equipment-generators.md).

Effect payload inspection now covers particle, lightning, sound and empty thruster
records, with raw identity preservation and separately exposed reader defaults.
Five of seven spawn-bot and seven of eleven equipment candidates now decode
completely; none gains runtime qualification. Unknown layouts and independent
requirements still reject. The browser shows detected effect counts as
unsupported. See [effect inspection evidence](evidence/effect-layouts.md) for
source comparisons, production rejection/recovery and Android dialog checks.

Immediate singleplayer wall teleports now use detected requirements, including
destination validation, actor dispatch, reference-matched rotation and source
save restoration. Bits 32/64 now have action-specific handling; multiplayer
remains blocked. Derived-fixture qualification does not unlock an
original mission by itself; see [wall-teleport evidence](evidence/wall-teleports.md)

Boss object teleports now rebuild only that owner's cached destinations, with
source-matched ordering and checked disk/rewind state. Same-segment
actions retain the earned list even after walls change. Native/source controls
and 48 Android steps pass within the [recorded scope](evidence/boss-transport.md).
Other owner/action variants and independent mission requirements remain gated.

Delayed descendants of nonplayer object masters now retain event context and
actor identity. Geometry/resource work survives the owner's removal; teleports
require that original live actor and cannot move a reused slot. Native/source
and 45 Android steps cover nested deadlines, disk through stock, rewind and
malformed-state rejection. See [delayed-chain evidence](evidence/object-chain-delays.md)
for the intentional reference lifecycle corrections. The boost tranche below
replaces development schema 6 with schema 7.

Speed boosts now have per-owner XL velocity constraints, orientation and saved
state, plus actor-specific boost-area collision rules. Wall teleports normalize
speed and stop boosts; object teleports preserve them. Native/source controls
cover actual crossings, player/robot collisions, release and stock isolation.
All 49 Android disk/rewind and transition steps pass. See [boost evidence](evidence/speed-boosts.md)
for the default ship/strength scope and intentional reference corrections.

Authored spawn points now use a separate XL table and the ordinary death/respawn
path, including named SPAWN marker precedence and checked saved transforms.
Source, native lifecycle and all 56 Android controls pass within the tested scope.
See [spawn-point evidence](evidence/spawn-points.md). This replaces development
schema 7 with schema 8; surveyed originals retain independent requirements.

Authored segment groups with neutral ownership now constrain XL boss destinations
across open connections. Group labels are preserved independently of unsupported
team ownership; the default label is not a wildcard. Native/source and lifecycle
controls are recorded in [boss group evidence](evidence/boss-groups.md). This
supersedes the earlier combined owner/group gate without qualifying a new
original mission.

Shield-damage and energy-drain wall actions now use the XL resource path, including
percentage drain, replenishment/caps, authored protection bypass, scheduling and
lethal continuation. Source/native and 62 Android steps pass for the default D2
profile; Redux remains the alternate. Direct object actions and optional ship
scaling/recharge remain unqualified. See [resource action evidence](evidence/player-resources.md).

Trigger flag bits 32/64 are now admitted for implemented actions. Set-orient is
inert outside the boost action. Silent suppresses forcefield-off
audio but enables the explicit illusion-off cue, matching the pinned source's
opposite conditions. XL has its own bounded illusion toggles; Redux keeps the
classic path. See [trigger flag evidence](evidence/trigger-flags.md) for source,
runtime and persistence controls and the intentional connected-side correction.

XL persistent projectiles now use separate hit histories and impact lifetime,
with source/native collision and physical-travel controls plus 53 Android
memory/disk/reuse steps. See [projectile evidence](evidence/projectile-history.md)
for explicit overflow/save corrections and remaining combat requirements.
Redux retains its existing behavior; expanded XL object capacity has separate
controls described above

Normal OpenGL D2 builds admit levels by detected requirements and runtime asset checks,
without a mission-name or hash exception. Stock and XL missions share one binary; software rendering still rejects XL.
This is per-level eligibility, not campaign qualification. The original Ahayweh Gate
singleplayer pilot is now verified in the OpenGL runtime; see its
[acceptance record](evidence/pilot-acceptance-audit.md) and
[requirement reconciliation](evidence/pilot-requirements.md). This does not grant
campaign-wide compatibility. The native 15/33, 15/34, 17/36 and 23/40 inventory and bounded
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
remains experimental. Modern water with stock robots and admitted robot/item
generators now has separate [combined controls](evidence/water-actors.md).
Modern lava bit 2 and its water/no-damage combinations now have a separate XL
contact path, reference-matched motion/damage/scrape impulses, and a saved relative
scrape clock. See [lava evidence](evidence/lava-volumes.md) for the current
acceptance record. Development schema 9 replaces schema 8. Original lava missions
retain independent requirements and qualification. Modern blocked bit 4 now
has actor-specific entry restrictions, allowed exits/projectiles, source automap
color and disk/rewind restoration. Native/source and 66 Android checks pass;
see [blocked-segment evidence](evidence/blocked-segments.md). Other property bits,
signed damage and multiplayer remain rejected. See the
[modern-property evidence](evidence/modern-properties.md) and
[implementation plan](../ai%20tool%20plans/mission%20metadata/xl_modern_properties_20261025.md).
Modern repair centers now use XL's default 11-unit shield rate, initial-shield
cap, source automap cue and separately saved audio cadence. Native/source and
70 Android controls pass; see [repair-center evidence](evidence/repair-centers.md).
Development schema 10 replaces schema 9. Redux retains its existing 25-unit rate;
17/36 legacy repair centers are now covered separately by
[legacy repair evidence](evidence/legacy-repair.md). The 15/33 repair encoding
and optional settings remain independently gated.
Closed walls now have an XL-owned actor-specific path: exactly matching blue or
red keys allow the owning player through; gold and combined masks do not.
Actorless visibility and nonplayers retain closed-wall behavior. Source/native
and all 90 Android crossing and persistence checks pass; see
[keyed-wall evidence](evidence/keyed-closed-walls.md). Blocked and boost constraints
still apply. Redux retains its original doorway behavior; no original mission
is newly qualified by these derived controls.
All admitted wall types now use a separate XL doorway decision, including closing
door transparency, wall-type-specific flags and signed cloak bytes. This replaces
the former additions to Redux's result. Source/native animation, collision and
persistence controls are recorded in [doorway evidence](evidence/doorways.md),
including the corrected signed-field adapter from the keyed-wall tranche.
Authored XL close-door actions now override an automatic door's waiting period
using their own transition handler. Slot/timer reuse, two-sided obstruction and
persistence controls are recorded in [close-door evidence](evidence/close-doors.md).
Force does not bypass blocking actors. Separate XL opening/frame processing now
uses all 90 source active-door entries, with own-face animation, correct waiting
clocks/compaction and checked saved ownership. See
[door lifecycle evidence](evidence/door-lifecycle.md) for paired-face corrections,
full-table and physical controls. Redux retains its original alternate; optional
settings and other wall behaviors remain separate work.
Other modern properties/damage, multiplayer-only objects, wide wall flags and unimplemented
object-event variants remain blocked. Unchanged Neptune passes native geometry/record checks, ordinary
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

The 15/34 reader now decodes packed light indices and retains audited reserved
segment bits. [Format evidence](evidence/format-15-34.md) separates the derived
native/Android controls from original-mission status: fourteen Apocalyptic Factor
XL levels have invalid declared packed ranges. Alarlof's original HXM now passes
the [custom gameplay controls](evidence/hxm-gameplay.md): source-matched model
radii, authored robot initialization, AI firing, combat and restoration through
stock. The generic HXM path validates dependencies before installation; 42 Android
steps also cover memory checkpoints and disk saves. Signed/D1 boss profiles,
expanded definitions and alternate custom-data formats remain separate work.
No original campaign is newly qualified.

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

Changing lights now retain complete XL-owned tables and full unsigned first/count
widths; production no longer splits them into a 500-row stock shadow. Native and
source comparisons cover authored updates, clamping, 501 rows and 73726 deltas.
Schema 13 preserves actual earned UVL/ambient values, including non-invertible
off/on cycles. All 50 Android disk/rewind/stock checks pass. See
[changing-light evidence](evidence/changing-lights.md) for source corrections,
full-table controls and retained renderer limits. Owned flicker timing and persistence
now have separate controls below. The standalone classic
probe keeps its own representation limits. Dinter and Sphere sp-03 lose only
their lighting-capacity blocker; neither gains mission qualification.

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
31/33/34/35/36/40. Our native reader and OpenGL translator support bounded 15/33,
15/34, 17/36 and 23/40 layouts; other layout combinations remain rejected.

## Maintained categories

The 19 browser rows retain the original IDs and expose previously bundled work.
Dot's ten areas map to several rows where decoding, behavior and presentation
need separate evidence. A category can contain supported classic features and
unsupported XL extensions

| Browser ID / category                                 | Inventory and corpus evidence                                                                                                           | Current production support / investigation                                                                                                                                                                                                                                                                                                                                                                    | Acceptance slice                                                                                                                                 |
| ----------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| `container` Archive/descriptor dialect                | DHF versus D2X, signed member sizes, long names, `d2x-name`; survey reports one D2X HOG and 48 XL descriptors                           | DHF and d2x-name supported with per-level feature/resource checks; extended archives unsupported                                                                                                                                                                                                                                                                                                              | Bounds, truncation, name collisions, descriptor precedence and dependency closure                                                                |
| `serialization` Versioned layouts                     | Separate level/game versions; v13 wall-width, v21 segment-layout and color/object transitions                                           | OpenGL builds: stock 1-8 and bounded XL 15/33, 15/34, 17/36 and 23/40                                                                                                                                                                                                                                                                                                                                         | Exact section ends, malformed references and version fixtures; never just relax version checks                                                   |
| `capacity` Tables and reference widths                | 21 levels exceed 255 walls; two exceed 1000 objects; two exceed 100 triggers                                                            | XL 2047 walls with native/Android persistence controls; stock 255. XL 254 triggers with source/native/Android controls, stock 100. XL authored-count pools up to 32768 slots with wide lifetime signatures and persistence controls; stock 1000. Other limits remain 9000 segments, 36000 vertices; 100 XL / 10 stock concurrent cloaks verified; owned full-width XL light tables; other widening unfinished | Wall indices 254/255/256/2046 and trigger IDs 99/100/127/128/253; sentinels, allocation headroom, saves/demos/network and metadata consumers     |
| `segments` Physics, properties, ownership             | Old combined types versus v21+ split properties; blocked, water/lava, outdoor, no-damage, shield/energy damage, teams/groups            | Modern water/lava/blocked/no-damage, 17/36 and 23/40 repair behavior, plus neutral boss groups, have native/source/Android controls; legacy encodings, signed damage and team ownership retain gates                                                                                                                                                                                                          | Equivalent old/new encodings, damage timing, traversal and ownership                                                                             |
| `walls` Wall types and flags                          | Extended types/flags; paired sides and door/open/close/cloak transitions                                                                | Separate XL doorway decisions, blue/red closed-wall keys and authored waiting-close behavior verified; 100-entry method-0 cloak lifecycle and draw alpha verified; colored/team walls and wide flags remain separate                                                                                                                                                                                          | Collision, projectile passage, activation and persistence                                                                                        |
| `triggers` Stateful events and wall triggers          | 99 masters, 14 enable and 19 disable wall actions; delayed/permanent/alternating/autoplay flags; versioned master-value initialization  | Wall scheduling, permanent/alternate/autoplay and saved counters implemented; byte flags and action-specific cues implemented; delayed object descendants retain context and actor identity                                                                                                                                                                                                                   | Finite event interpreter, dependency chain, delay/repeat, cancellation, save/reload; no general scripting language assumed                       |
| `object_triggers` Object activation                   | 212 triggers in 22 survey levels; bounded 15/33, 17/36 and 23/40 action/owner decoding; [current evidence](evidence/object-triggers.md) | Nonplayer leaf events and immediate/delayed masters verified natively and on Android, including disk/rewind; other owner/action variants gated                                                                                                                                                                                                                                                                | Threshold/destruction, ordering, once/repeat flags, restore and network identity                                                                 |
| `objects` Objects and AI                              | Type/control/movement/render variants, identities and robot behavior                                                                    | Separate XL persistent projectiles and owned boss/generator state have controls; remaining AI/combat variants unqualified                                                                                                                                                                                                                                                                                     | Ordinary combat, collisions, contained items and object lifecycle                                                                                |
| `gameplay_assets` Custom game data                    | Robot/weapon/model dependencies, HAM/HXM/PMH and expanded tables                                                                        | Classic HXM gameplay and lifecycle verified within loaded registries; alternate formats/counts and unsupported boss profiles remain gated                                                                                                                                                                                                                                                                     | Reference closure, failure rollback and custom weapons/robots; never alias PO2 PMH into stock HAM                                                |
| `baked_lighting` Embedded and sidecar colors          | Non-gray vertex/side/texture tables in 124/58/129 levels; CLR is a separate resource source                                             | OpenGL 15/33, 15/34, 17/36 and 23/40 embedded colors; 23/40 embedded table supersedes CLR                                                                                                                                                                                                                                                                                                                     | Float/integer transitions, resolved color precedence and matched reference frames                                                                |
| `dynamic_lighting` Changing lights and modes          | Switchable/flickering lights, dynamic sources, reactor/seismic flash, per-pixel/lightmap/shadow settings                                | Owned full-width delta tables, authored UVL updates and exact earned disk/rewind state verified; fallback/source differences documented; owned XL flicker phase and full-table lifecycle verified; full renderer modes remain unfinished                                                                                                                                                                      | Light on/off, shot sources, flash, save/reload; pin profile, never extrapolate across modes                                                      |
| `visual_assets` Texture/model formats                 | TGA/OOF, layered/animated materials; broad-corpus counts are not exclusively XL                                                         | Individual formats/paths need verification                                                                                                                                                                                                                                                                                                                                                                    | Orientation, filtering/alpha, model rendering and disclosed fallback                                                                             |
| `mission_flow` Campaign behavior                      | Secret origins/returns, directed exits, dependencies and objectives                                                                     | XL campaign behavior unverified; no modern hub-persistence claim                                                                                                                                                                                                                                                                                                                                              | All branches, transitions/returns and saves; one level never certifies 30                                                                        |
| `runtime` Qualification                               | Combat, progression, presentation, persistence, isolation, replay and multiplayer                                                       | Bounded OpenGL pilots with explicit evidence; no general campaign or multiplayer claim                                                                                                                                                                                                                                                                                                                        | Scope and evidence for each claim; all required categories must qualify                                                                          |
| `navigation` Teleports, boosts and spawn destinations | 108 wall teleports, 281 boosts, 20 spawn-setting actions; object-trigger transport also occurs                                          | Singleplayer wall teleports include delays and stateful flags; byte flags admitted with per-action semantics; boost direction/constraints/release/area/saves implemented; spawn actions and route-analysis edges remain unsupported                                                                                                                                                                           | Reference transforms, actor crossings, arrival collision and source save restoration verified; campaign and guidebot routing unqualified         |
| `generators` Equipment and spawning                   | 53 equipment generators in 11 levels; robot assignment permutations occur in original Ahayweh Gate levels                               | [Bounded robot matcens verified](evidence/robot-generators.md) natively/on Android, including disk/rewind; modern equipment separately verified; legacy/expanded data gated; boss spawning has separate lifecycle evidence                                                                                                                                                                                    | Assignment, activation, selection, health, contact impulse/RNG, allocation retry, exhaustion and restore; original missions separately qualified |
| `wall_materials` Authored opacity/materials           | Type-8 colored walls (Glass House: 126), opacity on ordinary walls; pilot has 14 nonzero cloak/opacity records                          | Doorway visibility/collision verified; rendered opacity remains bounded by per-mission evidence; colored walls remain gated                                                                                                                                                                                                                                                                                   | Visibility, collision, targeting and projectiles independently; fixed opacity and layers                                                         |
| `effect_objects` Particles, lightning, ambient sound  | In 129 decoded sections: 330 particle, 151 lightning, 1335 sound records                                                                | [Bounded payloads inspected](evidence/effect-layouts.md); runtime activation/audio/interactions unsupported                                                                                                                                                                                                                                                                                                   | Versioned payload ends, preserved IDs/references, activation, looping-audio cleanup; cues may affect gameplay                                    |
| `resource_scope` Precedence and isolation             | Mission/level textures, models, music, sounds, CLR and custom-data overrides                                                            | Owned classic HXM staging and stock/save transitions verified; other XL override formats and folder precedence need separate qualification                                                                                                                                                                                                                                                                    | Mission A -> stock -> mission B, failed overrides, saves and audio/model/texture cleanup                                                         |

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
