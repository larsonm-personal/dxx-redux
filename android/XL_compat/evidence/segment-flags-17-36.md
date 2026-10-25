# Segment flag audit for the 17/36 inspection profile

This audit resolves Observatory's 37 previously unknown segment-flag records. It
does not qualify its level translation, descriptor/resources, no-damage behavior
or presentation. Runtime admission remains closed for 17/36.

Reference: `arbruijn/d2x-xl` commit
`2576a98b5de310e3ac7c9777df2677d1627b8bae`.

## Serialized byte and consumers

- `main/segment.cpp:154`, `CSegment::ReadExtras`, reads `m_flags` as one byte
  after function/producer/value and before average segment light. For level 17,
  `Upgrade` derives function/properties from the old combined function value;
  it does not derive them from, mask or reinterpret `m_flags`
- `include/segment.h:334` declares the byte separately from `m_props`.
  Lines 596-597 define only `S2F_AMBIENT_WATER=1` and `S2F_AMBIENT_LAVA=2`
- `main/findsegment.cpp:273`, `SetAmbientSoundFlagsCommon`, clears only the
  requested ambient bit, marks texture sources, propagates through passable
  neighbors, then sets that bit. Its two calls use exactly the water/lava bits.
  High bits survive recomputation and do not seed propagation
- `gameio/loadobjects.cpp:1326` invokes this recomputation after loading.
  `main/game.cpp:737-738`, `DoAmbientSounds`, consumes only the water/lava masks.
  The older equivalent implementation in `main/gameseg.cpp:806-852` uses the
  same masks. No consumer of the high six segment-flag bits was found
- The full reference search for `m_flags` covers 12 C++/header files. Apart from
  the segment readers/ambient code above, matches belong to render points,
  sound objects and movie libraries; those are different fields/types

The supported interpretation is therefore **preserved reserved bits**, not new
environment properties and not evidence of corruption. Do not clear these bytes
or reinterpret them as no-damage flags. No-damage comes from old function 13.
This conclusion is scoped to the audited 17/36 profile and pinned reference.
Unaudited flags in 15/33 retain the previous unknown-feature rejection.

Reference file SHA256 pins:

| File | SHA256 |
| --- | --- |
| main/segment.cpp | `eece1c8ba624bd42e6e77b9f2a3dd9a54489338139ee102245672b2b7353e60d` |
| main/findsegment.cpp | `dcacfd889edf869699eb49ac8e39c4027c1da48f7c2f95eb90581bf1e7701be7` |
| main/game.cpp | `a1a71d37ed4efc1341a62ab23d8c1375afc144fd7792bec35a143da83745b089` |
| include/segment.h | `4f43198ba394971128c81373940b5a58283cbbbc2851ca6a6e4187e78382a667` |

## Original Observatory evidence

The original HOG and RL2 SHA256 pins remain those in the
[Observatory plan](../../ai%20tool%20plans/mission%20metadata/xl_observatory_prerequisite_20261024.md).
Native inspection preserves every raw flag byte:

| Raw flag value | Segment count |
| --- | ---: |
| 0 | 73 |
| 13 | 1 |
| 49 | 1 |
| 65 | 1 |
| 69 | 5 |
| 70 | 1 |
| 101 | 2 |
| 149 | 1 |
| 185 | 1 |
| 202 | 21 |
| 203 | 1 |
| 221 | 1 |
| 255 | 1 |

The 37 nonzero records are reported as `reserved_segment_flags` in the optional
inventory `observations` array. Raw `segments[].flags` stays unchanged. This
retains visible evidence while separating it from unresolved requirements.
All 26 no-damage functions remain unsupported; 17/36 serialization still blocks
the gameplay translator, including otherwise neutral synthetic levels.

## Executable checks

- Public inventory tests exercise all 256 flag values in a synthetic 17/36
  level, verify raw preservation/observations, and require runtime rejection.
  A 15/33 negative control retains unknown-flag rejection
- The private original test requires 37 observations, the original section
  endpoints/counts, all 26 no-damage functions and gameplay rejection
- Optional `XL_REFERENCE_SEGMENT_FLAGS` points to the pinned reference
  `main/findsegment.cpp`. CMake verifies its hash and extracts the actual
  `AmbientMarkBfs`, `SetAmbientSoundFlagsCommon` and `SetAmbientSoundFlags` code
  into a local generated header; no reference source or mission bytes are vendored
- `xl_segment_flags_reference` runs that code with a minimal two-segment geometry
  adapter for all 256 flag bytes, four source texture masks, and open/closed
  propagation. It requires exact preservation of high bits and recomputed low
  bits independent of the initial byte. Geometry passability is an explicit test
  input; this is a flag/propagation oracle, not a rendering or collision oracle

The adapter uses the reference constants from `include/segment.h`,
`include/loadgamedata.h` and `include/wall.h`: ambient water/lava 1/2, texture
volatile/water 1/2, transparent doorway 4. It does not implement replacement
flag-update or traversal algorithms.
