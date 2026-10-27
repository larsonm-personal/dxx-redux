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
original-mission qualification from derived controls. No commits or pushes requested.

## Progress

Source audit confirms action 26 selects from the first linked producer, or uses
arbitrary eligible robots when unlinked/empty. Spawn location belongs to the
owner. Gate time is per boss; population cap counts all boss-created robots.
Reference retries placement five times, adjusts failure cooldown to one quarter
interval, scales lifetime by difficulty, and suppresses ordinary spew whenever
the owner retains a spawn action, including disabled actions.

The first production native integration passes two simultaneous boss owners,
linked and arbitrary selection, cooldown, morph/lifetime, disk restoration,
malformed owner state and a stock transition. The fixture enlarges original
Neptune geometry eightfold to give actual boss models room for placement retries;
it supplies no original-level qualification. Android integration is in progress.

An empty linked producer must use arbitrary selection until its ordinary handler
initializes the third selection word. The new runtime tracks this initialization
separately and includes it in schema-3 boss state. Unlike XL's disk reader, this
extension preserves the earned selection initialization across disk and rewind
restores; this is an intentional persistence correction, not byte-for-byte save
format compatibility.

Remaining before completion:

- Complete independent reference comparisons for placement/retry, timing/caps,
  failure, lifetime, motion and RNG; expand real combat and owner-release controls
- Finish the separate XL boss lifecycle where existing cloak/teleport/death
  globals would otherwise couple concurrent owners. Inspect rendering, firing,
  hit-time and death dispatch consumers as well as spawn dispatch
- Complete Android disk/rewind/stock checks on the final APK and verify protected
  files restore exactly; run formatting and focused regressions/catalog checks
- Update executable support descriptions and maintained evidence only after the
  complete acceptance scope passes
