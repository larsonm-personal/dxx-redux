# Owned XL changing-light tables and earned state

This tranche replaces the production runtime's narrowed stock shadow with the
complete admitted XL index and delta tables. Redux retains its existing path.
It qualifies table retention, authored UVL updates and persistence, not every XL
renderer mode or an original campaign.

## Source and storage

Reference revision: 2576a98b5de310e3ac7c9777df2677d1627b8bae.
`tests/reference_changing_light.cmake` pins `lighting/light.cpp`,
`include/segment.h` and `gameio/loadobjects.cpp`. The reference uses dynamically
sized tables, unsigned 16-bit first indices and, in packed layouts, 13-bit
counts. A range can start at 65535 and extend to 73726 records.

The owned runtime keeps the validated source inventory and a stable sorted
lookup of its source faces. It applies every matching row and contribution in
order, with per-corner lower clamping. Private runtime translation gives the
stock loader empty light sections, preventing double application. The standalone
classic probe still rejects tables it cannot represent; its output is not a
substitute for the source-owned runtime. Existing 15/33, 15/34, 17/36 and 23/40
native checks now compare actual owned records instead of split stock shadows.

Twenty-four public contracts cover three layouts, 501 indices, counts
0/127/128/255/8191, first indices 32767/32768/65535, full end ranges and truncated
payload rejection. The classic probe keeps its signed first-index and bounded
table restrictions. Admission no longer imposes those restrictions on XL.

## Behavior and explicit reference differences

The extracted `FindDLIndex`, `ChangeLight` and `ApplyLightToSegment` functions
match 508 production UVL samples and 56 segment attenuation samples under both
MSVC and strict GCC with undefined-behavior checking. The adapter pads the
reference's duplicate-index rewind before its logical table; production uses a
bounded lower-bound lookup. Reference samples exclude signed-overflow cases.
Another 52 production cases verify explicit saturation instead of undefined
signed overflow.

The separate XL segment-light fallback uses exact fixed-point distance, the
80-unit cutoff, two-link propagation and the source's +/-65535 contribution
clamp. Redux's alternate retains its existing quick-distance behavior. Native
graphs exercise 13 and 34 unique segments in both directions, including cycles,
duplicate paths and an unreached third-depth destination. A level-sized visited
set replaces the reference's unsafe 30-entry scratch array.

One intentional behavior correction remains explicit: release XL returns early
when its sorted UVL scan reaches the next source face, bypassing segment-light
fallback for earlier groups. The port completes the matching range and applies
the fallback consistently. Its segment sum also saturates on positive overflow.
These are not claims of bit parity with undefined behavior or every release
branch. The oracle isolates UVL arithmetic and fallback attenuation; it does not
reproduce the reference light manager, `segDeltas` renderer accumulator, alternate
lighting methods, float-math setting or final rendered images. Those profiles
remain separate fidelity work.

The high-capacity fixture retains original Neptune geometry/resources and adds
501 index rows and 73726 deltas. Two authored permanent wall actions switch the
first 500 rows and the final 8191-record range together. Every resulting UVL is
checked, including repeated subtraction and a complete off/on cycle whose lower
clamping makes the earned brightness differ from initial level brightness.

## Persistence and isolation

Development schema 13 replaces 12. The trailer stores all 24 UVL values and the
ambient value for each segment. Restoring only light-switch masks cannot recover
an earned off/on cycle, so the validated snapshot is applied after normal source
reconstruction. The source identity now includes geometry, initial light values
and complete authored light records. All trailer state validates before light
publication.

Native disk-through-stock restoration preserves the exact earned snapshot.
Nine corrupt payloads cover segment counts, row width, positive/negative numeric
overflow, a real JSON boolean, a fractional value, a missing lighting array and
wrong source identity. All reject without changing live brightness; valid state
then recovers. The source pointer and lookup clear on stock replacement.

The registered Android runner `test_xl_changing_light_runtime.ps1` passed 50/50
steps in 70744 ms, run 1f1b849ace124fddb81b73a743074268. It checks authored
contributions and clamping, exact disk restoration through Counterstrike,
repeated restore, and memory checkpoints after deliberate high-range light
disturbance. Generic checkpoint verification compares all UVLs and ambient values
immediately after each restore, before simulation advances. Protected
player/configuration/mission hashes match after cleanup. No navigation or
playthrough claim is involved.

- Original Neptune HOG: 4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303
- Derived xlights.hog: 2619769d8e623bb92cb798afad4307c7dbad4874bd66144df93166dd5ea8e394
- Derived xlights.mn2: 1c8f805de715407d4cadc98a9ff28e612ff6f94309c027e76a07d86279cba4ad
- Android APK: 7a77d14c5af8b4f5711eef43dbce4bdcee293d91902b142dc3cbb6a4ad18b61d

All 73 configured XL tests pass in 284.94 seconds. The added propagation graphs
then pass the affected integration again in 2.63 seconds. Stock D2 passes in
45.71 seconds and stock D1 in 1.66 seconds. The normal APK builds both games.
The required private-suite receipt now includes this test and six previously
omitted capacity/HXM integrations; all twelve receipt/policy tests pass.
Both automation catalogs pass: 415 top-level entries, 471 support scripts,
99 standalone JSON and 302 standalone PowerShell tests. Scoped quality and
pinned PowerShell analysis pass.

## Original missions and remaining work

[changing-light-originals.json](changing-light-originals.json) records fresh
production admission checks against hash-matched survey members. D.I: Monolevel
has 411 indices and 32625 deltas; Sphere's sp-03 has 234 and 32769. Both lose the
lighting-capacity blocker and retain independent effects/events or other
gameplay blockers. Neither gains playable or campaign qualification.

Flicker capacity and its full lifecycle, other renderer profiles, complete
changing-source visual comparisons, custom resource registries and original
campaign qualification remain unfinished. The existing 8 MiB trailer bound
also remains; these controls do not claim unbounded save payloads.
