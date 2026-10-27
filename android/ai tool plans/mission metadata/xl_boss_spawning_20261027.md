# XL object-triggered robot spawning

Implement on xl_more_9 after merged robot-generator PR #151. XL owns its boss
spawning implementation and per-boss state. Redux remains the alternate runtime;
share engine primitives where their semantics agree, not the two boss algorithms.

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae,
main/trigger.cpp, main/producers.cpp, ai/aiboss.cpp, objects/boss.cpp,
objects/createobject.cpp, main/segment.cpp and include/maths.h.

## Implementation and acceptance

1. Audit damage thresholds, linked/unlinked selection, cooldown/caps, placement
   retry, lifetime, AI/morph/audio/physics and ordinary-spew suppression
2. Implement a separate XL runtime with source-bound boss identities and per-boss
   gate times. Dispatch by active level capability, preserving stock behavior
3. Admit action 26 only with valid owners, targets and complete resource closure;
   preserve independent layouts, capacities, custom-data and multiplayer gates
4. Persist per-boss state through the checked event extension, disk saves and
   authoritative rewinds. Reject malformed state before publishing any event state
5. Compare production behavior with pinned reference extracts, exercise actual
   damage/spawns/combat, concurrent owners, failure/retry and stock isolation
6. Register/run Android integration and focused native regression controls;
   update evidence/catalog/roadmap with exact verified scope and remaining blockers

Use controlled fixtures and introspection. No autonomous 3D navigation or new
original-mission qualification from derived controls. Publication authorization
is recorded under Overnight continuation below.

## Progress

Source audit confirms action 26 selects from the first linked producer, or uses
arbitrary eligible robots when unlinked/empty. Spawn location belongs to the
owner. Gate time is per boss; population cap counts all boss-created robots.
Reference retries placement five times, adjusts failure cooldown to one quarter
interval, scales lifetime by difficulty, and suppresses ordinary spew whenever
the owner retains a spawn action, including disabled actions.

The production native integration passes two simultaneous boss owners,
linked and arbitrary selection, cooldown, morph/lifetime, disk restoration,
malformed owner state and a stock transition. The fixture enlarges original
Neptune geometry eightfold to give actual boss models room for placement retries;
it supplies no original-level qualification. Final Android acceptance passes all
66 steps with four capture/roundtrip cycles, real generated-robot morph/combat,
cloak/death restoration and identical protected-file hashes. The maintained
evidence records the tested APK hash and run ID.

An empty linked producer must use arbitrary selection until its ordinary handler
initializes the third selection word. The new runtime tracks this initialization
separately and includes it in schema-3 boss state. Unlike XL's disk reader, this
extension preserves the earned selection initialization across disk and rewind
restores; this is an intentional persistence correction, not byte-for-byte save
format compatibility.

The classic boss restore repair replaces negative shields with half health even
while the saved boss is dying; the pinned XL reader contains the same repair.
The separate runtime preserves the actual saved health and validates it against
owned death state. This intentionally avoids resurrecting a dying boss or counting
a second lethal hit. Stock restoration retains its existing repair policy.

Completed acceptance:

- Independent extracted release-profile comparisons match 50 production cases
  across five difficulties, including selection, placement/retry, timing/caps,
  allocation failure, lifetime, motion and RNG. Final native combat controls pass
- Separate cloak/teleport/death ownership now includes rendering, firing,
  hit clocks and death dispatch; native rolling-death persistence passes
- Final Android disk/rewind/stock checks pass and protected files restore exactly
- All 14 focused native regressions, 12 private-asset policy checks, both catalogs
  and scoped formatting pass. Executable support descriptions and maintained
  evidence now describe the verified scope; all seven original candidates remain
  blocked by independent decoding requirements

## Overnight continuation

The user authorized further chunks, branch/PR creation and publication for green
CI, with no merges. Finish this chunk and PR first, then continue bounded gameplay
support on separate branches/PRs. Keep each PR's CI green and preserve explicit
unsupported requirements. Likely follow-up areas are equipment generators and the
object payload layouts blocking original spawn-bot missions; audit reference data
before choosing each subsequent implementation.
