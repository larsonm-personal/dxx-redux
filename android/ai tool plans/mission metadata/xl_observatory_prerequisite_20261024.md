# Observatory XL: bounded serialization prerequisite

Water is a separate parent slice on `xl-level-expansion` at
`416a87de10a4f36c51398df0c763d71846542f27`. Begin implementation only after
that head's CI completes successfully. Use a new child branch and stacked draft
PR; do not modify `d2xxl_compat` or the active Counterstrike controller.

## Decision and sequence

No additional non-Counterstrike 15/33 level was found in the retained XL corpus
beyond Robotech. Apocalyptic Factor is 15/34: its first level is `sdg22.rl2`, not
`fbasin.rl2` (level 10). Its actual light indices have classic byte-side/count
layout while the newer pinned reference expects packed indices for game 34.
Do not infer a historical profile or silently reinterpret that discrepancy.
Neptune's 23/40 layout also adds segment/object/wall fields and CLR precedence.

Observatory is a useful small target but is not a one-feature runtime unlock:

1. Decode its bounded 17/36 structure for inspection, preserving unsupported
   segment functions and keeping gameplay translation/admission closed
2. Qualify original descriptor/resource loading and version-specific runtime
   translation without discarding no-damage properties
3. Implement the reference no-damage behavior as a separate semantic slice,
   including damage/firing boundaries, native saves and deterministic replay
4. Qualify authored emission data and presentation against an explicit lighting
   profile; do not replace HAM lighting with LGT brightness

The first PR implements only step 1. Observatory has no exit trigger; later
acceptance is arena behavior, not campaign completion.

## Actual sources and baseline

Assets checkout `d9fbeaf6eb059fd4504308bfd6364734775b7cba`, archive
`missions/d2xxl_downloads/levelpack.7z`, members
`missions/kruel/obsrv-xl.hog` and `missions/kruel/obsrv-xl.mn2`.

- HOG: 82014 bytes, SHA-256 `e89acaad09cefa0c09fc4b487c56b6077477cfe20169f364fe54f627040b01e7`
- RL2: 52176 bytes, SHA-256 `edf4bcf7db90a979028f883ba9b72d5c584821f0ef22b00cadf6009e409be5df`
- Descriptor: 297 bytes, SHA-256 `8a2946c7f786f4834e1c9e303a8d4248b419b89479a750d0d6c1917fbe1b6317`
- LGT: 3640 bytes, SHA-256 `7729cbdcb089ecc295c96f194b134dc9a5dd7cba1ef4ea3bd523e95094f9471c`

The water-parent native inventory rejects `unsupported level version 17`. Retained
research and actual section offsets indicate 328 vertices, 110 segments, 54
walls, 44 objects, 26 old-type-13 segments, no triggers/equipment/light deltas,
and a neutral six-byte object-trigger tail (zero triggers plus zero sparse refs).
The original HOG also has a POG; native resource closure remains unverified.

Reference source is `arbruijn/d2x-xl@2576a98b5de310e3ac7c9777df2677d1627b8bae`:
`gameio/loadobjects.cpp`, `gameio/loadgeometry.cpp`, `objects/objectio.cpp`,
`main/wall.cpp` and `lighting/light.cpp`. Qualify the actual bytes as well as the
reference; a version number is not proof of layout compatibility.

## Lighting finding

Original Observatory LGT changes used textures 26, 45, 67, 307, 353 and 687.
Kaffeine is a smaller classic-layout LGT-only resource probe (7/31, 203 segments)
and loads in the unchanged native engine, with the existing four-byte HXM
trailer warning. It has no authored flickers, as do the other inspected candidates.
The reference keeps LGT brightness separate from HAM `TmapInfo.lighting`:
`ChangeSegmentLight` and light-switch gates still use HAM, while emission
construction and flicker classification use the separate brightness table.
Thus overriding stock HAM lighting would be incorrect, and an LGT-only change
cannot honestly be claimed to unlock this legacy runtime profile. Retain these
files as future emission-profile evidence rather than inventing a blocker.

## First-slice completion criteria

- Explicit supported inspection pairs only: 15/33 and 17/36
- Decode added equipment header, integer texture colors, game-36 neutral sparse
  reference tail and packed light-index counts with strict bounds
- Keep unsupported nonneutral sections visible or rejected; no silent dropping
- Synthetic public fixtures cover layout differences and malformed boundaries
- Optional private test verifies the original HOG hash, complete inventory and
  section endpoints, the 26 unsupported no-damage segments, and unchanged inputs
- Gameplay probe explicitly remains 15/33-only; production remains blocked
- Original Robotech inspection remains byte-for-byte identical to the parent
- No new save/deterministic state, runtime behavior, private mission binaries,
  generic interpreter or changes to the active Counterstrike controller

## Implementation findings

Water's exact parent head completed all five workflows successfully before this
implementation began. Child branch: `xl-observatory-inspection`.

The original native decode agrees on complete geometry/object boundaries and
preserves all 26 old-type-13 functions. It additionally exposes 37 segment flag
bytes outside the audited ambient-water/lava mask, including 65, 202 and 255.
The retained static report did not inventory these flags. They remain unchanged
and explicitly unknown; no historical interpretation or harmlessness is assumed.
Resolve them before any runtime admission. This is further evidence against
calling Observatory a single-feature unlock.

Public fixtures cover the added header, integer texture colors, zero sparse-ref
count, packed light count 300, invalid sides/counts/truncation and bounded
unsupported equipment records. The gameplay translator explicitly rejects 17/36,
even for otherwise neutral synthetic geometry. Production remains blocked.

## Validation

Linux GCC 14 Debug builds with warnings treated as errors. All three tests pass,
including the hash-pinned original archive integration. Robotech's complete
inspection JSON is byte-for-byte identical to the water parent. The retained
survey audit passes. No runtime behavior or save format changed; the new version
is rejected by the gameplay translator and still marked non-playable by the
production compatibility report.
