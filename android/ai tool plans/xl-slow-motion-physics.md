# XL slow-motion displacement and lifetime integration

Layer on #390. Connect reference UpdateOffset scaling after fluid displacement,
selecting the console player's clock by identity and the world clock for other
objects. Scale finite object lifetime using the world factor, preserving immortal
objects and classic/network behavior. Reset transient state on mission release.
Drive the real state machine from native test controls; do not wire gameplay
input, relax pickup admission or claim disk persistence. Audit rotation, drag,
collisions and other consumers separately before enabling gameplay.

Compare actual native actor offsets and lifetime with pinned source code across
world/player factors, dry/water segments, signs, repeated frames, core transitions,
multiplayer bypass and stock-level transition. Preserve existing water/drag owners
and original bytes. Run relevant source/native/classic controls and scoped quality.

## Marker compatibility boundary

Redux drop_marker_object assigns IMMORTAL_TIME-1 and lighting.c uses its changing
low bits to flash. This equals XL's ONE_FRAME_TIME sentinel numerically, but XL
DropMarkerObject instead assigns IMMORTAL_TIME and uses its own rendering path.
Do not apply the XL sentinel rule to Redux markers: keep their existing real-time
lifetime clock, including after its initial sentinel value has decremented.
Native drop_marker_object plus three object_move_one calls verify this exception.
This preserves the existing marker presentation, not full XL marker-render parity.

## Qualification evidence

The existing water-actor owner now requires 768 canonical native displacement and
finite-lifetime cases plus 144 sentinel/nonpositive-time cases against extracted
pinned UpdateOffset and object-update source. It covers all four core-generated
clock profiles, console identity independently of object type, dry/water rooms,
positive/negative velocities and multiplayer isolation. Five transcript mutations
(missing, duplicate, unknown profile, changed displacement and trailing fields)
are rejected. The source oracle also passes ASan/UBSan on the native transcript.
The native engine itself was not sanitizer-instrumented in this qualification.

Water actors, lava, projectiles and cloaks pass with unchanged deadlines. Existing
production, combat, save/restore and stock-transition assertions remain mandatory.
The tests restore a prior save while clocks are active and assert reset, not disk
persistence of slow motion. The real dropped marker keeps advancing its existing
flash clock for three updates. Public source/GPU/audio owner55 and Python owner33
pass. Catalog validation passes; execution is blocked by the environment's missing
/proc/self/task/.../children support (exit125). Classic upstream and trigger-demo
checks are required separately before publishing this stage.

Private Neptune source HOG SHA-256 remains
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303.
Fixtures are derived controls; no original campaign/readiness case is changed.

Remaining: rotation, active collision/force interactions, weapon/AI/effect clocks,
input, pickup resources, audio and persistence. Gameplay activation is still
absent, and powerups48/49 remain rejected. No new rendered original/readiness pass
is claimed. Verified genuine atlas resources are separately available in draft
assets PR4 at d09f199c3b66bab71970364b1939b1658851ea81 for a later stage.
