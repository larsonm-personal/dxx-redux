# XL missile firing state

Continue #396 on a separate draft. Pickup admission remains disabled.

## Implemented scope

- Qualify missile last-shot timing and the strict five-second world-clock deadline
  against pinned controlweapon.cpp. Ordinary shots mark the timestamp; empty or
  guided-release attempts do not. Level initialization resets it alongside the
  existing next-shot timestamps. Classic and network branches remain unchanged.
- Persist the relative last-missile timestamp in the XL trailer (schema 31), with
  strict signed/unsigned/type/arity/range validation before any state publication.
  The prerelease trailer replaces schema 30; classic save bytes are unchanged.
- Compare 512 mandatory native deadline cases against exact pinned source fragments
  and test real missile firing, malformed-state atomicity and disk restoration.

Pinned source: 2576a98b5de310e3ac7c9777df2677d1627b8bae,
weapons/controlweapon.cpp:239 and weapons/fireweapon.cpp:898-917. The deadline
comparison preserves #396's relative-epoch adaptation for Redux's 64-bit clock.
Existing Redux frame-overhead scheduling is retained, not claimed as a complete
source firing implementation. The primary/flare tests and previous Omega increment
controls remain mandatory.

## Omega investigation: blocked on the correct clock producer

An unpublished implementation initially used game_get_simulation_frame_id for
Omega's current/previous-frame recharge gate. Source-predicate and native tests
passed, but tracing the producer disproved that mapping: the pinned appData frame
counter advances in render/renderframe.cpp:735 (also end-level rendering at348 and
render/automap.cpp:504), not the simulation loop. gameio/loadgame.cpp:913 resets it
on level loading. A headless simulation-frame pass cannot qualify these semantics.

The unqualified implementation is preserved locally at
/workspace/scratch/xl-slow-motion/weapon-state-with-omega-wip.patch, and none of its
Omega production or save changes are included in this draft. A correct next slice
must map render/automap/end-level/frame-skipping behavior, test actual rendering,
and then coordinate Omega's saved eligibility with that verified clock. Existing
Omega recharge gates, consumption and deadlines are unchanged here.

Still outstanding: Omega state, slow-clock/audio persistence, wall/object effect
timing, controls and genuine pickup rendering. No new original readiness pass is
claimed, and no asset or cohort change is included.

## Final missile-only validation

Passed: 512 new mandatory source/native deadline cases plus all inherited clock
controls; eight corrupted/missing/duplicate transcript negatives; source-oracle
ASan/UBSan (not a sanitizer build of the full native engine); eight malformed save
shapes/types/ranges with atomic rejection, actual ordinary/empty missile attempts,
and disk restoration of the last-shot offset. All 14 selected native/save-heavy
owners passed: named audio, trigger chains, object events, boosts, spawns, invalid
changing-light rows, invalid flicker rows, cloaks, projectiles, lava, water actors,
boss spawning, player resources and equipment generators. The broader first run
lacked several reference executables; building those prerequisites resolved the
setup failures without source/assertion/deadline changes.

Both classic tests, all 55 public XL contracts, 33 asset-policy tests, scoped
quality and catalog validation passed. Catalog execution remains blocked by the
existing Linux child-supervisor /proc/self/task/.../children limitation (child
exit 125). Windows/hosted qualification remains pending. Native compilation emits
only the previously observed const-string warnings.

The derived controls used unchanged Neptune HOG SHA-256
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303.
No rendered/original readiness improvement is asserted. The private FAST cohort
and public test set are unchanged.
