# Robot generator fidelity

The final contact audit is still in progress: damage amounts are verified, but
the newly discovered XL knockback normalization/RNG path needs independent
comparison and integration. Earlier controls below retain their recorded scope.
Equipment generators, expanded gameplay data/capacities and object spawn actions
remain unsupported. This does not qualify a new original mission or finish the
singleplayer roadmap

## Assignment correction

XL AssignProducer associates disk row i with the robotmaker segment whose
authored producer index is i, ignoring the row's redundant segment/station fields.
Redux's native mine loader renumbers robotmaker segments in geometry order.
The old translation copied disk rows unchanged and therefore swapped robot
contents when those orders differed. The new translation appends a table in
native order with resolved segment/station links and retains the raw source rows
in inspection evidence

Admission rejects missing, duplicated and out-of-range source assignments, and
station counts outside the native signed-byte link range. Ambiguous assignments
are rejected rather than reproducing the reference's silent first-match/discard
handling. No mission name or hash participates in production admission

The inventory regression first reproduced the previous swapped-contents result.
It now passes 15/33, 17/36 and 23/40 controls with deliberately inconsistent row
metadata, nonmonotonic segment indices, an interleaved fuel station and twelve
malformed assignment controls. Hash-pinned extracted ReadObjectProducerInfo and
AssignProducer agree with the translator for all six permutations in each of
the three layouts (18 cases)

The production runtime control derives three robotmakers from unchanged Neptune
geometry, assigns source rows in order 2/0/1, activates them through normal wall
trigger dispatch and checks actual morphed robot IDs 2/0/1, capacity consumption,
creator/segment links, pending disk saves and stock-to-source restore. The focused
native selection passes 13/13, including the preceding object events, scheduling,
teleports, modern properties/water and original Neptune/Observatory regressions

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

Android passes 50/50 steps, run ID 6c855bcc862b4d2580374bdbfb4ca907, elapsed
129572 ms. Two captures each undergo three real authoritative rewind restores,
covering pending generation and spawned robots. Native combat, pending disk
restore and stock-to-source restoration pass. Protected files have identical
before/after hashes; emulator-5556 was not used

The test's artificial robot-suspension control is reapplied after restoration:
it is cleared by loading, and delaying inspection otherwise lets production
generation continue. The initial failing run and the successful corrected run
both restored protected files exactly

Tested APK SHA256:
485d72a608a627a9418a71fd50068bb3bbaa2544f8b5e1e06642754108f511a6

Derived fixture HOG SHA256:
1cdbbe99c38ebca641bdd5184d342de21172163f051c3379592b3468f8ff3e77

Derived descriptor SHA256:
9dc10214bc91ff022d4eac6cc61b1f101d3b788d9bf31a8c05eb4c99ec6e5fb4

Android artifacts: temp/xl-robot-generators-android. Trusted native CI now requires
both generator assignment/reference and runtime controls (46 required passes);
the runner-policy unit tests pass 12/12

## Original missions and remaining work

Fresh native inspection of five original HOGs decoded all 34 levels: Ahayweh
Gate, Arena, Observatory, Konflict and GlassHouse. The
[preserved requirements](robot-generator-candidates.json) retain source hashes,
failed-decode fields, counts and assignment order without making admission claims.
Ahayweh Gate contains 74 robot centers across 30 levels. Its d2leve-2.rl2 uses
source order 2/0/1/3 and demonstrates that the remapping fix addresses real
authored data. That 15/33 level still has four texture-ID-910 findings; fixing
generator assignment does not waive those requirements or qualify the level.
The other four inspected HOGs contain no robot centers

Object spawn-bot actions require the owner's boss gate identity, cooldown/caps,
intersection retries, lifetime, AI/morph/audio/physics state and normal spew
suppression. Activating a matcen does not implement that action. See the
[active chunk plan](../../ai%20tool%20plans/mission%20metadata/xl_generators_20261026.md)
for source pins and acceptance requirements
