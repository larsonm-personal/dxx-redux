# Version 21/game 40 qualification

This qualifies the bounded 21/40 layout through the existing modern runtime
translation. It does not qualify Dinter as playable. Adjacent versions 20, 22,
24, 26 and 28 and unqualified game-version pairs remain rejected. No classic
engine code or readiness cohort is changed.

## Reference and layout

Reference: arbruijn/d2x-xl commit
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. Existing source-oracle hashes pin
CRLF checkout bytes; the reference checkout used those line endings without
changing the pinned revision or hashes.

- `main/segment.cpp`, `CSegment::ReadExtras`: version 21 uses signed-byte producer
  and value fields, separate properties, two signed-short damage values, and
  fixed-point light. Its geometry has implicit canonical corners
- `objects/objectio.cpp`, `CObject::Read`: game40 includes the multiplayer header
  byte. Version21 omits lightning waypoint and width, defaulting to -1 and 3.
  Particle type and effect enable fields are present; effect identity
  normalization remains source-owned
- `main/wall.cpp`, `CWall::Read`: game40 walls occupy 25 bytes, with 16-bit flags
- `main/producers.cpp`, `ReadObjectProducerInfo`: modern producer rows occupy
  20 bytes, including both object bitsets and signed-short links

The existing decoders already implement these rules. Qualification adds 21/40
to the exact inspected pair and the shared modern-layout predicate. That
predicate also selects equipment-header removal, native wall/object conversion,
segment-function ownership, feature admission and vertex brightness. Merely
changing inspection's version whitelist would not select those paths.

## Controls

- 56 synthetic geometry profiles with bounded header/mine truncations, signed
  field boundaries and adjacent-version rejection
- Pinned geometry reader: 64 profiles, 8,316 comparisons, 512 vertex brightness
  maxima, including version21
- Pinned object reader: 150 profiles, matching all object ends/identities and
  120 effect payloads, including version21 defaults and normalization
- Game40 width controls deliberately declare wall24 and producer16 while
  serializing wall25 and producer20; verify native header/name offsets, wall
  fields, producer records/assignment and every producer truncation
- Pinned producer initialization: 24 four-layout/permutation cases, including
  version21's legacy declared row size; existing pinned wall-reader controls
  consume the complete 25-byte game40 record
- Explicit 21/40 malformed light-side rejection, including side34; wrong
  game-version pairs remain rejected
- Native derived 21/23/25/27 controls exercise geometry/normals/UVLs,
  objects/walls/triggers, resources, repair/protection, rejected-load state and
  disk save/restore through a classic level
- The five derived levels match 40,317 pinned geometry-reader comparisons and
  1,435 brightness maxima

`xl_layout_test --runtime-v21` generates the expanded native control.
The existing `--runtime` output remains byte-identical to Android's pinned
four-level HOG and descriptor. Version21's derived Neptune control explicitly
requires no lightning objects before changing its version; independently
serialized effect fixtures exercise the different lightning payload.
The native timeout increases from 100 to 150 seconds (outer CTest 120 to 170)
for four complete cycles instead of three; all assertions remain required.

Validation passed: 19 standalone CTests; five focused pinned-reference CTests;
native derived layout controls; automation catalog validation; `test_upstream_compat` and
`test_classic_trigger_demo`; scoped code quality and whitespace checks.
The first native run hit the old 100-second timeout during concurrent builds;
the direct rerun completed with exit0. Debug MEM diagnostics occur in the native
harness; this is not leak-sanitizer coverage. Android device and sanitizer runs
were not performed for this change.
`test_run_all_tests_catalog.ps1` failed with child-supervisor exit125 because
this host could not read `/proc/self/task/<pid>/children`; no catalog or
supervisor implementation was changed. The new geometry contract linked
against the original library exits1 with `unsupported level version 21`,
confirming that the regression control detects the previous gate.

## Exact original Dinter remains blocked

Private asset revision: `9dad21bd65ba05665ac6fe315f37fe7461e85725`.
Original `dinter-1.5.7z`, mission `dinter`, level1, member `dinter.rl2` SHA256:
`32c101c6c8a2177be8bc5ca74dcdb44dd1f27a2f637612e632ff30396d99f69b`.
No original bytes were edited.

Before: `unsupported level version 21`.
After: native route/readiness entry exits1 during `level_load` with
`invalid delta light 0`. No gameplay tick is reached. Full original geometry
and object comparison also stops at this rejection; synthetic and derived
controls must not be represented as original-mission qualification.

At byte offset 2233658, delta row0 contains segment5828, side34, padding0,
vertex bytes13/120/197/22. The reference `CLightDelta::Read` in
`lighting/light.cpp` marks this invalid because side34 is outside0..5.
4,592 of the 32,445 original records fail that reference validity predicate.
The reference loader retains validity and its changing-light consumer skips
invalid records. Redux currently rejects them during inspection; this change
preserves that guard. Supporting the reference's invalid-record semantics would
require a separate bounded audit of every consumer, index stability, native
translation and saved light state, not dropping rows or accepting bad indices.
Any subsequent original Dinter capability blockers remain unknown.
