# Modern XL door marker flags

Branch 95 retains modern 16-bit wall flags in XL-owned source metadata while
translating the mutable native wall flags to their low byte. Bit 256 controls
whether map markers obstruct a door. Higher reserved bits remain retained in
the source; they do not acquire invented runtime behavior. Independent additive
wall rendering and other requirements remain gated.

## Source and geometry contract

Pinned D2X-XL revision: `2576a98b5de310e3ac7c9777df2677d1627b8bae`.
The existing close-door reference target extracts `CSegment::DoorIsBlocked`.
It now also extracts `CSide::Masks` from hash-checked `main/side.cpp`:
`5f8552e6fb177b24a624c47c9c644029acf95eda9f7001624c7f28e7972b4989`.

The pinned obstruction routine walks the front segment's object list and tests
each retained object against both doorway sides. Weapons, fireballs, static
explosions and effects do not block it; markers are conditional on bit 256.
Modern XL now follows that traversal. The previously implemented legacy XL
two-list path and classic Redux behavior remain separate.

Source poke geometry excludes zero-radius objects. A face intersects the sphere
when `distance - radius < -250` and `distance + radius >= -250`. Both faces must
intersect for a planar two-face side; one suffices for an outward-folded side.
The adapter reuses the compiled XL side anchors and normals already used by
particle geometry. Redux's general segment mask cannot substitute for this
test: it also marks spheres wholly beyond the plane as outside.

Windows and Linux reference runs pass 1,620 sphere/plane boundary cases,
336 object-filter/list-membership/overlap comparisons, 3,072 close transitions,
and 660 animation cases. Native geometry/lifecycle coverage also passes the
90-entry door boundary, full-pool reversal, compaction, wait deadlines,
malformed saves, stock isolation and disk restoration.

## High-flag persistence evidence

The native owner generates two derived fixtures: `mpblock` has flags `0xfe10`,
and `mpignore` has `0xff10`. Their low-byte automatic-door behavior is identical.
Marker/actor controls run before and after a full save, stock mission load and
restore: 192 cases pass. This proves the ignored marker bit remains available
after source rebinding, while native mutable low flags remain intact.

Android `test_xl_close_door_runtime.ps1 -MarkerFixture` passes 17/17 steps in
28.994 seconds, including live obstruction checks, energy 73/save/disturb to 1/
restore to 73, then repeated obstruction checks. Protected before/after file
hashes match. Its fixture differs from the current baseline HOG at exactly two
bytes: the high flag byte of each door face. This is derived geometry, not an
authored Pumo door traversal.

Portable contracts verify original high flags survive translation for modern
23/25/27 layouts and native flags retain their low byte. A separate flags-260
case continues to reject unsupported additive rendering. The particle/skybox/
save lifecycle owner passes after factoring the shared side compiler.

## Android lifecycle and original route

The complete Android door lifecycle passes 104/104 steps in 99.361 seconds:
actual single-axis crossings, paired closure, source front-list obstruction,
motion audio, disk/rewind and stock-to-XL restoration. Protected hashes match.
Transition checks pause simulation to observe a potentially one-frame closing
state before resuming to completion. Post-crossing checks explicitly reopen
doors that may already have automatically closed. Closed visibility is 6
(transparent, not flyable), matching installed texture flags; this does not
claim opacity. Closing audio follows the source zero-timer rule, and restored
playback counters are checked independently from pre-restore counters.

The original Pumo route through `tulev1.rl2` passes 41 Android steps, including hub checkpoint
recovery, after correcting placed-mine model admission and fresh hit histories.
See [placed-mine evidence](placed-mines.md). This uses controlled authored exit
dispatch and does not qualify the whole campaign or a human playthrough.

Receipts are under `temp/xl-skybox-20261031`: `door-linux-reference.log`,
`door-reference-test.log`, `door-geometry-native-runtime.log`,
`admission-test.log`, `android-marker-numeric-runtime.log`, and
`android-door-final-stock-restore.log`. Native details
are under `temp/xl-particles-20261030/contracts-windows64/close_door`;
Android marker artifacts are under `temp/xl-marker-door-android`.
