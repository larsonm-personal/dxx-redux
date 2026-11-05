# Invalid authored XL light contributions

Reference: arbruijn/d2x-xl `2576a98b5de310e3ac7c9777df2677d1627b8bae`,
`lighting/light.cpp`: `CLightDelta::Read`, `ChangeLight`, `AddLight`,
`SubtractLight`, `ApplyAllChangedLight`; `gameio/loadobjects.cpp`:
`ReadLightDeltaInfo`. Existing pinned source hashes are unchanged.

## Semantics and consumers

The reference reads every eight-byte delta row and derives validity from its
segment and side bounds. It retains invalid rows and logs them. Changing-light
processing skips each invalid contribution before dereferencing its target;
valid rows retain order, overlap and lower-clamping semantics. Source-face
state and segment-light propagation still execute. Reference save restoration
replays changed-light masks through this same path.

Inspection now retains every raw field, padding byte, row and index and reports
invalid targets as observations. `valid_light_delta` implements the reference
predicate without normalizing signed/unsigned raw bytes. Declared source-face
and first/count range checks remain strict. Runtime binding checks those bounds
again before replacing ownership. The source-owned path guards each target;
its getter returns the raw row plus explicit validity. The Android diagnostic
consumer checks validity before indexing its expected-contribution array.
Other raw-row consumers either compare/serialize/hash values or now guard their
expected-light indexing. Non-source gameplay probes still reject invalid rows.
No classic engine code changes.

The stock shadow has zero active light-index and delta counts for source-owned
levels, preventing duplicate application. Source identity still hashes every
raw row, including invalid payloads. Snapshot format and identity calculation
are unchanged. Disk/rewind restoration retains exact earned face/ambient light
state, then continues through guarded source-owned processing.

No rows are dropped, no indices or offsets are shifted, and no packed-count
anomaly is repaired. This follows the existing unsigned encoded-count policy;
it does not claim literal equivalence to the reference's signed-byte count bug.

## Qualification

- Six known XL layouts: exact raw-row and overlapping-range retention,
  validity diagnostics, truncation and source/range rejection, and refusal of
  invalid rows by non-source gameplay probes
- Pinned reader: 1,280 segment/side boundary and raw-payload comparisons
- Pinned changing-light consumer: 524 UVL samples, including 16 interleaved
  invalid/valid and overlapping-range samples, plus 56 attenuation samples
- Separate native invalid-row mode: retained high first/count indices,
  diagnostics/getters, repeated authored add/subtract, clamping, disk through
  classic, malformed save rejection and invalid-payload source identity
- Separate native invalid-row flicker mode: active flicker, disable/restart,
  exact saved phase/light continuation and classic transition
- The existing ordinary native modes remain required. Separate validity modes
  preserve the existing Android fixture output. Both new controls require
  built reference oracles in the private runner; they cannot be omitted from
  successful receipts

The identity fixture initially failed because its display title exceeded the
mission descriptor limit. Its title was shortened; no runtime admission rule
was changed to accommodate the fixture.

## Exact original Dinter

Asset commit: `9dad21bd65ba05665ac6fe315f37fe7461e85725`.
Original `dinter.rl2` SHA256:
`32c101c6c8a2177be8bc5ca74dcdb44dd1f27a2f637612e632ff30396d99f69b`.
Original bytes are unchanged.

All 32,445 delta rows remain present. 4,592 have invalid targets; 4,275 of those
are referenced by declared ranges. They are not assumed to be unused sentinels.
Pinned geometry comparison now completes: 6,744 segments, 40,464 sides,
569,595 comparisons and 19,257 vertex-brightness maxima. The pinned object
reader matches 1,857 ends/identities and 377 effect payloads.

Native unassisted load/120-tick entry still exits1 at `level_load`, before any
tick. The next independent requirements are 44 blockers: one segment-damage
record (segment2215, 3/5), nine object-trigger type16 records, 11 unsupported
particle profiles, six related effect-object type requirements and 17 lightning
requirements. These gates are unchanged. No original-mission readiness pass or
navigation coverage is claimed.

Local validation passed: 19 standalone CTests; four native ordinary/invalid-row
changing-light and flicker controls with their pinned references; two classic
compatibility tests; 30 private-runner policy tests; automation catalog
validation; scoped code quality and whitespace checks. The invalid-row identity
control was rerun successfully after correcting its fixture title.
Android-device and sanitizer execution are not covered by this local pass.
