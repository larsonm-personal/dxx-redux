# Modern equipment generators

Bounded modern 23/40 equipment makers now use a separate XL runtime. Stock Redux
retains its existing fuel/robot producer path. This is feature evidence from
controlled fixtures; no new original mission is qualified.

## Implemented behavior and bounds

- Modern function 11 binds row i through the segment's authored producer index.
  Redundant disk segment/station links remain visible and do not select the owner
- Initial enablement follows the combined producer list and the reference's
  destructible-wall/object-target discovery, including its index and ID quirks
- Authored activation toggles enablement without resetting elapsed/pending state.
  Production waits three seconds times difficulty plus one, then half the morph
  clip. Occupancy resets the first delay; failed effect allocation retries it;
  failed item allocation starts a new delay
- Selection uses two authored words and consumes RNG only for multiple choices.
  Bits above XL's 50 powerup types are inert. Unsupported multiplayer, addon,
  unused and release-only-missing pickup types reject with the item ID
- Actual items use loaded sizes/animations, an immortal lifetime, the source
  creator and ordinary Redux pickup behavior. Vulcan ammo contains 98 rounds
- Mixed robot/item makers run in source geometry order, preserving shared RNG and
  allocation order. Robot suspension leaves equipment production independent
- Checked schema 4 preserves enabled, pending and elapsed state through disk and
  authoritative rewinds. Geometry, generator rows, source objects and producer
  assignments bind the extension to its source. Validation precedes publication

This profile retains existing capacities, ownership/environment, expanded data,
unsupported payload and multiplayer gates. Equipment with modern water remains
unqualified. Legacy equipment encodings and XL slow-motion/bullet-time pickups
remain unsupported. The equipment checks do not establish full reference pickup,
AI, rendering or campaign parity.

The pinned XL disk reader calls SetEquipmentMakerStates after restoring producer
state and therefore reinitializes earned enablement. This port deliberately
preserves the captured enabled state instead, as it already does for earned
robot/boss/event state. That is a persistence correction, not reference save-format
compatibility.

## Reference and native controls

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae.
The reproducible `tests/reference_equipment.cmake` checks SHA256 pins for
main/producers.cpp, main/trigger.cpp, objects/createobject.cpp and
gameio/savegame.cpp. It compiles the actual production, selection, powerup
creation, initialization and trigger-discovery functions against engine/storage
adapters. These are isolated-function comparisons, not a full reference-engine
run.

All 210 production cases match timing, occupancy, zero/one/multiple selections,
allocation exhaustion/retry, suspension and complete RNG transcripts across five
difficulties and three seeds. Actual native allocation availability is an adapter
input; production controls exhaust the real allocator. Thirteen further cases
match combined-producer initialization, first-wall selection, missing overlays,
destructibility, one-shot effects and missing/retired object owners.

Native integration exercises authored mixed activation, permuted assignments,
exact delay boundaries, pending toggles, actual pickup rewards/replenishment,
pending disk restoration and a different-difficulty stock transition. A separate
mixed-order control compares selected actors, allocation slots and final RNG.
Twenty-two malformed extensions or producer links reject before changing any
wall, object or equipment state. Asset-free contracts retain decoded records,
reject malformed assignments/items and preserve inert high selection bits.

## Fixture and Android acceptance

The derived fixture retains unchanged original Neptune HOG SHA256
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303
as its input. It adds two equipment makers around a robot maker, permutes source
rows, relocates existing pickups and supplies authored wall/object controls.
These changes do not qualify original Neptune.

Derived HOG SHA256:
ebf2d9080dde9740cf18dd9ee2da3a48ddde08c11cfdd3dc21214af8c9dd4e8b

Descriptor SHA256:
6a4ca565352f3fe1bdce3b3249672c5ad2a2844816ac87173f21d8110960f6d0

Android acceptance passes 61/61 steps, including three capture/roundtrip cycles,
pending-phase disk restoration through stock, real energy pickup/replenishment
and simultaneous robot/item production. Each roundtrip waits for actual worker
completion before comparing the detached save byte-for-byte with its synchronous
capture. Protected device files have identical before/after hashes. Only
emulator-5554 was used.

Run ID: ca1fd5f3787343319bf5b238247535de, elapsed 143894 ms.
APK SHA256:
5f8dd9e90d1a661e1610e51ffe6c1e235be379f670219155d2b9f71854a66599

All 15 focused native regressions and 12 private-asset policy checks pass. Both
automation catalogs and scoped mixed formatting pass. The source oracle also
matches under Linux GCC; candidate regeneration preserves both evidence reports
byte-for-byte. Logs and machine-readable receipts are under
temp/xl-equipment-20261027 and temp/xl-equipment-android.

## Original missions

Fresh native inspection of all eleven hash-matched survey candidates still
rejects nine unsupported render payloads, Subzero's movement payload and pmlev1p's
17/35 layout. pmlev1p uses two legacy type-15 makers; the modern levels use
function 11. See [the candidate evidence](equipment-candidates.json).

Reproduce with `evidence/audit_spawn_candidates.py --feature equipment` and the
pinned survey bundle, original missions root and native inspector. Original HOGs
and level payloads are hash-checked and remain unchanged. Decoding alone does not
establish resource admission, gameplay or campaign qualification.
