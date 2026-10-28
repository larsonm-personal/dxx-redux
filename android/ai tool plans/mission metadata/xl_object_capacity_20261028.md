# XL object storage, allocation and persistence

## Goal and scope

Support the pinned XL singleplayer authored-count-sized pool, including authored
objects, dynamic allocation, high identities and earned state. Keep Redux and
D1-in-D2 at their existing 1000-object capacity, save widths and network bounds.
Use separate XL reclamation semantics where the source differs from Redux.
This does not qualify full missions or expand other capacity families.

Base: xl_more_33_projectiles / PR #178, commit
7f4ac6cfeef8c19e50d513826766d592d3f1eaf8. Its public/native/Android acceptance
passed; prerequisite #178 has all seven workflows green. Root cmake stays clean.

## Implementation and acceptance

1. Audit physical arrays versus active limits, packed signed identities,
   projectile bitmap width, AI storage, allocator serialization, classic demos,
   network validation, metadata snapshots and D1 translation. Pin XL source
   object limits, reclamation ordering and allocation headroom.
2. Provide 32768 physical slots in XL-capable D2 builds, with active limits
   chosen from verified load context and the loaded mission. Preserve the
   classic bitmap width; the separate XL projectile table is already available.
   Clear inactive storage on mission replacement and preserve stock allocation.
3. Implement XL reclamation and allocation pressure through production object
   cleanup, with source comparisons and actual full-pool exhaustion/recovery.
4. Update all save readers/writers/preflight together, retaining classic widths.
   Restore high object/AI/trigger/projectile identities and allocator order;
   reject malformed references and recover with valid state. Development XL
   save layouts are disposable; no compatibility readers for earlier tranches.
5. Exercise original-geometry derived high-count levels, high-ID combat,
   linked lists, robot AI, rendering/metadata and physical contacts. Verify
   source admission boundaries and unchanged original-mission independent gates.
6. Add owned Android disk/checkpoint/stock-transition acceptance with protected
   state, public no-assets CI controls, meaningful native controls, stock and
   D1-in-D2 regressions. Format changed files, run both catalogs, build both
   Android games, then publish a stacked PR and monitor CI. No autonomous flight.

## Initial audit

At the start of this tranche MAX_OBJECTS was 1000 and serves unrelated physical and serialized
purposes. Blind widening would make the embedded hit bitmap quadratic, expand
stock AI/allocator/afterburner saves and traces, and relax network bounds.
The initial audit misread MAX_OBJECTS_D2X=20000 as the runtime limit. The correction below
records the actual authored-count initializer. XL retains 20 slots of normal headroom,
reclaims at capacity minus two before allocation and cleans dead objects.
Its reclaim order is debris, unattached fireballs, flares, then other weapons.
Source reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae.

## Review correction and current implementation

Final source review found LEVEL_OBJECTS is dynamic: max(authored count + 1000,
authored count * 2). MAX_OBJECTS_D2X=20000 is a separate static helper limit.
The initial fixed-pool implementation and its passing Android run are superseded.
The current runtime follows the source initializer, with a 32768-slot backing
pool and admission requiring full authored headroom. More than 16384 authored
records remain blocked pending wider object slot references.

The maximum-pool test also exposed the 16-bit lifetime-signature dependency:
with every signature occupied, a replacement cannot have a distinct identity.
XL now uses a separate 32-bit sequence; Redux keeps its 16-bit behavior. Audio
ownership stores the full signature, and demo records have a private wide
signature wrapper while preserving classic bytes. Source overflow and exact
reclaim occupancy corrections are explicit in the evidence.

Current acceptance includes 20002 source capacity expressions, 57 actual native
lifetime samples, 2016 matching reclamation samples and 32 exact-occupancy
corrections. Native controls cover four authored-count loads, full allocation,
pressure cleanup, high-ID AI/combat/death actions, exact disk state and malformed
core count recovery. Demo codec, complete-frame, rewrite and malformed input
controls pass, including exact rewriting, explicit malformed-input decode errors
and recovery. All seven reference source hashes are checked at configuration.

Final formatted Android run 701294cdbf5f4c0e8afec303e03f8c2f passes all 53 steps in 91597 ms
with wide signatures, actual linked playback, source-sized headroom, production
disk restores, three exact checkpoint cycles and restored protected hashes.
Earlier run 09bcc3dcd5404d28bac244bd17e9a29d used the superseded fixed pool.
No physics workaround is installed. Final D1 and D2 stock integration tests pass,
as do all 72 configured XL tests across the full run and focused reruns. The
reruns restore missing private assets and correct the boss test's availability
sample to use the active pool. Its 50 reference cases pass without runtime changes.

Dinter (1857 authored objects) and Anthology (1279) lose only the object-count
blocker; independent effects/properties/actions remain unsupported. See
../../XL_compat/evidence/object-capacity.md for the current scope and limitations.

Both automation catalogs and explicit PowerShell analysis pass. Scoped formatting
passes; publication and CI are still pending.
