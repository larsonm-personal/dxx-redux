# XL boss spawning and lifecycle

Object-attached action 26 uses a separate XL runtime. Boss spawn clocks, hit
clocks, cloak/render times, normal teleport destinations and death rolls belong
to their source-bound owner/signature. Stock Redux keeps its existing boss path.
This is bounded singleplayer feature evidence, not original-mission completion.

## Semantics and admission

- A first linked matcen supplies robot selection; spawning happens near the
  owner, not at that matcen. Missing links or a negative first segment select an
  arbitrary eligible robot. Extra links and serialized sides are inert
- Empty matcens use their implicit high-ID word only after the ordinary producer
  handler initializes it. Uninitialized empty producers select arbitrarily
- Spawn actions require a loaded D2 boss definition (flags 21 through 28), a
  valid selection source and complete model/weapon/sound/item dependencies
- Selection, inclusive cooldown, strict population cap, five placement attempts,
  allocation failure/retry, difficulty lifetime, AI/morph state and initial
  tumble/motion follow the audited default XL profile
- Attached spawn actions suppress ordinary boss spew even when disabled. Damage
  thresholds and consumed/permanent values share the object-event interpreter
- Boss AI owns its cloak/teleport/hit clocks and destination traversal, including
  source groups. Cloaked rendering and firing/death dispatch use that same owner
- Disk and real rewind restoration use checked schema 3. Geometry/groups, source
  owner records and generator assignments contribute to source identity. All
  wall, object, producer-selection and boss fields validate before publication

Legacy equipment layouts, expanded data/capacities, generated/contained bosses, other
boss profiles, boss object-trigger teleports, unsupported object payloads and
multiplayer remain independently gated. Native classic combat mechanics remain
the base engine's policy; this does not claim complete reference AI or visual
parity, optional critical-hit subsystem damage, or old-demo qualification.

## Reference and native controls

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae.
The reproducible extraction in `tests/reference_boss.cmake` checks SHA256 pins for
ai/aiboss.cpp, ai/ailib.cpp, main/segment.cpp, include/vecmat.h and include/maths.h.
It compiles the actual release-profile CreateGatedRobot/BossSpewRobot, placement
reader and vector operations against storage/engine adapters. The pinned debug
branch's unconditional gated-spawn refusal is not the production profile.

All 50 production samples match extracted selection, cooldown, population,
placement, allocation failure, lifetime, health, motion and RNG behavior across
all five difficulties. These are isolated-function comparisons, not full-engine
reference runs. Native allocation availability and loaded definitions are inputs
to the adapters; native allocation exhaustion is exercised separately by the
production sampler.

The object-event oracle additionally covers 728 spawn-bot threshold/consumption
cases, bringing its total to 2184. Native integration exercises two simultaneous
owners, actual local damage, repeat cooldown, disabled ordinary-spew suppression,
cloak/render clocks, normal teleports, lethal boss combat, a rolling-death save,
retirement, a different-difficulty stock transition and stock gate lifetime.
Twenty-six malformed extensions reject before changing wall/object/boss state.

Intentional corrections are explicit: source-uninitialized hit clocks start at
zero, a degenerate zero-facing teleport retains orientation, initialized producer
selection survives disk/rewind restore, and a dying boss retains its saved
negative health. Both Redux and the pinned XL disk reader otherwise repair that
health to half strength; that repair can resurrect the dying boss. Stock repair
behavior is unchanged.

## Fixture and Android acceptance

The fixture derives from unchanged original Neptune HOG SHA256
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303.
Its geometry is enlarged eightfold to accommodate actual boss models and
placement retries. It adds two bosses, linked/negative-sentinel spawn actions,
and two deliberately invalid levels for admission rejection/recovery. These
changes never qualify original Neptune.

Derived HOG SHA256:
718b507ed3866a73f36fed6472810c2310d99dc1861aece09002101575c60d9c

Descriptor SHA256:
27f367855bbb7748536b29b3fbf84faf8583c216f3276a220eed2be959070ed2

Final Android acceptance passes 66/66 steps and four authoritative
capture/roundtrip cycles, including spawning, real morph completion, generated
robot damage/death, cloaking, lethal boss damage, death-roll restoration and a
different-difficulty stock-to-XL disk restore. Protected files have identical
before/after hashes. Only emulator-5554 was used.

Run ID: 555104601327494295cb4188e1701353, elapsed 176766 ms.
APK SHA256:
9f1d3a2418fa90812c3d10315d45e2088f6533eefeae070fb85588b4fc1c25e1

All 14 focused native regressions pass, covering event decoding, scheduling,
matcens, bosses, teleports, modern properties/water and existing mission controls.
The 12 private-asset policy checks, both automation catalogs and scoped mixed
formatting pass. Candidate report regeneration preserves its bytes. Logs and
machine-readable receipts are under temp/xl-boss-spawning-20261027 and
temp/xl-boss-spawning-android.

## Original missions

The hash-matched research survey identifies action 26 in seven levels across
Dinter, Sphere, Anthology and Boiling Point distributions. Fresh native inspection
of the original bytes still rejects each: Dinter, Sphere and most Anthology
levels contain unsupported render payload layouts; Anthology D3 has an invalid
trigger target; Subzero has an unsupported movement layout. See the retained
[candidate evidence](boss-spawn-candidates.json) and reproducible
`audit_spawn_candidates.py`. No original mission is promoted by this chunk.
