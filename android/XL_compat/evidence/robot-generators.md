# Robot generator fidelity

Bounded robot-matcen assignment, production, contact and persistence behavior is
verified by the native/reference and Android controls below. Contact impulses,
damage and RNG consumption match 96 independent pinned-source comparisons.
Modern equipment makers have [separate acceptance evidence](equipment-generators.md).
Legacy equipment layouts and expanded gameplay data/capacities remain unsupported.
Object spawn actions now have [separate boss lifecycle evidence](boss-spawning.md);
matcen checks alone do not qualify them. This does not qualify a new original mission or finish the
singleplayer roadmap

## Assignment correction

The 25/27 runtime audit found that the earlier source comparison omitted an
initialization stage. LoadExtSegmentsCompiled calls CreateGenerator in geometry
order before reading generator rows. CreateRobotMaker and CreateEquipmentMaker
replace serialized producer indices with category-specific sequential indices.
AssignProducer then associates row i with that initialized owner; neither the
serialized producer index nor the row's redundant segment/station fields selects
the owner. The previous authored-index claim and its 2/0/1 expected spawn sequence
were incorrect. The expanded comparison reproduced the defect before correction

Translation now resolves rows in geometry order and preserves raw fields in the
inspection inventory. The source's combined station list includes fuel, repair,
robot and equipment producers but excludes reactors. Fuel/repair functions with
raw producer IDs >=500 are disabled during source initialization. Native Redux
station links are reconstructed separately because its list includes reactors.
Equipment startup targeting and boss resource selection use the same corrected
assignment. Saves containing the old generator identity reject before publication

Admission rejects a mismatch between initialized maker counts and disk rows, and
station counts outside the native signed-byte link range. Stale or duplicated
serialized producer IDs no longer reject otherwise valid assignments. No mission
name or hash participates in production admission

The source comparison now compiles the actual CreateObjectProducer,
CreateProducer, CreateRobotMaker, CreateEquipmentMaker and CreateGenerator
functions before ReadObjectProducerInfo and AssignProducer. It checks all six raw
index permutations in 15/33, 17/36 and 23/40 (18 cases), plus 24 mixed initialization
cases with signed wide fields, interleaved reactors and both maker kinds. Pins
also cover the geometry loader's call order and the source's limit definitions.
These are isolated-function comparisons, not a complete reference-engine run

The production runtime control retains deliberately stale source indices 2/0/1,
activates makers through normal wall trigger dispatch and now checks actual
morphed robot IDs 0/1/2, capacity consumption, creator/segment links, pending disk
saves and stock-to-source restore. Equivalent 25/40 and 27/40 variants pass too.
See [modern runtime evidence](runtime-layouts.md) for the current validation
record and the expanded Android checks

Temporary logs: temp/xl-generators-20261026 and
temp/xl-stack-review/generator-{assignment,runtime}. The original Neptune HOG
SHA256 remains 4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303

## Selection and production behavior

Empty lists can select from the level's initial nonboss robot IDs >=66; nonempty
lists keep their authored two-word selection. The source's short-circuit and
initialization ordering matter here. The mask is reconstructed on level load,
including source reload before save restoration, rather than following surviving
objects each frame. An independent extracted SetVertigoRobotFlags comparison
checks type, low-ID and boss exclusions. Actual expanded definitions remain a
separate production admission gate; the high-ID algorithm control deliberately
uses synthetic native definitions

Extracted GetObjProducerObjType and the handler's selection gate match 42 native
choice/RNG controls. Extracted default math-format-2 magnitude matches six
distance controls. StartObjectProducer matches 30 native difficulty/lives cases;
RobotDefaultShield matches 45 actual generated ordinary/thief/companion robots
across ordinary/secret levels and all difficulties

Production runtime checks cover the inclusive initial timer boundary, strict
half-morph boundary, light/effect allocation failure under a full object table,
successful retry after releasing one slot, player/robot occupancy damage, global
population-cap suppression/recovery, three exhausted waves/lives, final disable
frame and user one-round/paused modes. Stock dispatch retains its previous timer
boundary and generated health; real stock/source transitions also pass

Contact handling uses the reference's default math-format-2 normalization,
random-direction axis and draw order, player direction perturbation, and
floating-point mass scaling. Ninety-six production/reference samples compare
the exact impulse, damage and RNG transcript for players and robots, offset
positions, four RNG seeds, fractional/zero mass, nonphysical movement, automap
and closed exits. The Android control also exercises production contact and
checks the player's shield change from 100 to 96 and resulting movement

The pinned per-station population expression uses a signed creator byte before
XOR. Redux retains that behavior rather than silently imposing a different limit;
the global 15-extra-robot cap is verified. Introspection interprets creator bytes
as unsigned to report actual spawned identities; it does not change production
counting

## Persistence and Android

Generator source rows, segment assignments and initial robot IDs enter the
authored identity in the existing bounded event extension. Such sources require
that extension. Native saved generator counts, contents and table links are
validated against the loaded source before publishing extension event state.
Seven malformed count/contents/link controls reject without publishing saved
wall state. The stock save layout remains unchanged

The historical Android run passed 53/53 steps, run ID 5852fd1b2af5450f8c6a1947b1edd1f7, elapsed
152589 ms. Two captures each undergo three real authoritative rewind restores,
covering pending generation and spawned robots. Native combat, pending disk
restore and stock-to-source restoration pass. Protected files have identical
before/after hashes; emulator-5556 was not used. Its row-selection expectations
predate the initialization correction. The current script checks the corrected
0/1/2 sequence; its rerun is recorded in [modern runtime evidence](runtime-layouts.md)

The corrected runner passes 53/53 on the normal Android APK with the same fixture,
actual 0/1/2 spawns and exact protected-file restoration. That current acceptance
supersedes the historical row-selection expectations above.

The test's artificial robot-suspension control is reapplied after restoration:
it is cleared by loading, and delaying inspection otherwise lets production
generation continue. The initial failing run and the successful corrected run
both restored protected files exactly

Tested APK SHA256:
a9df1c3e05375a96e368b641a3cd049307686c06a6d299e774a5ef901d842b22

Derived fixture HOG SHA256:
1cdbbe99c38ebca641bdd5184d342de21172163f051c3379592b3468f8ff3e77

Derived descriptor SHA256:
9dc10214bc91ff022d4eac6cc61b1f101d3b788d9bf31a8c05eb4c99ec6e5fb4

Android artifacts: temp/xl-robot-generators-android. Trusted native CI now requires
both generator assignment/reference and runtime controls (46 required passes);
the runner-policy unit tests pass 12/12

The final rebased/formatted native sweep passes 13/13 in 52.63 seconds. Both
automation catalogs pass with 392 top-level entries and 445 support scripts.
The retained native contact reference again matches all 96 impulse/damage/RNG
cases. Final local logs use the final-* names in temp/xl-generators-20261026

## Original missions and remaining work

Fresh native inspection of five original HOGs decoded all 34 levels: Ahayweh
Gate, Arena, Observatory, Konflict and GlassHouse. The
[preserved requirements](robot-generator-candidates.json) retain source hashes,
failed-decode fields, counts and assignment order without making admission claims.
Ahayweh Gate contains 74 robot centers across 30 levels. Its d2leve-2.rl2 uses
serialized producer IDs 2/0/1/3, which initialization replaces with 0/1/2/3.
This confirms that ignoring stale serialized indices matters for original data.
The candidate report now distinguishes raw indices from initialized row order.
That 15/33 level still has four texture-ID-910 findings; fixing generator
assignment does not waive those requirements or qualify the level.
The other four inspected HOGs contain no robot centers

Object spawn-bot actions require the owner's boss gate identity, cooldown/caps,
intersection retries, lifetime, AI/morph/audio/physics state and normal spew
suppression. Activating a matcen does not implement that action. See the
[active chunk plan](../../ai%20tool%20plans/mission%20metadata/xl_generators_20261026.md)
for source pins and acceptance requirements
