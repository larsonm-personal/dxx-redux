# OOF 2300 static metadata qualification

This change qualifies typed SPCL and timed RANI decoding only for the existing
opaque static presentation profile. Rotate/turret/facing properties, facing
lists, other runtime chunks and nonopaque newly admitted models remain rejected.
No OOF animation, general visual parity or readiness improvement is claimed.
Raw extension bytes remain in their original order alongside typed metadata.
Texture names and resource/path validation are unchanged.

## Pinned source

Reference commit: [`2576a98b5de310e3ac7c9777df2677d1627b8bae`](https://github.com/arbruijn/d2x-xl/tree/2576a98b5de310e3ac7c9777df2677d1627b8bae).
The CMake oracle pins hashes before extracting these actual source functions:

- `models/oofread.cpp:235`: timed frame information; `:256` rotation frame;
  `:306` rotation records; `:456` special point; `:478` special list
- `models/oofread.cpp:1405`: `AssignChildren`, including self-parent root normalization
- `models/oofread.cpp:1530`: static hierarchy bounds
- `render/oofrender.cpp:532`: vertex transform; `:573` submodel rendering;
  `:601` draw; `:651` model rendering and clock handling
- `3d/oof2model.cpp:42`: optimized model conversion

SPCL has storage/read/destruction semantics and no runtime consumer in the
pinned tree. RANI has normalized axes, signed integer angles and timed records;
the reader also builds matrices/tick maps. Neither pinned render path consumes
those matrices/maps for this property-free profile. Rotation/turret flag handling
is outside the admitted profile. Angles are retained exactly, not reinterpreted
as normalized turns. Finite/count/range/padding limits deliberately reject
unqualified inputs rather than attempting repairs.

The direct-path oracle executes the pinned rendering clock and CPU transforms
with controlled GL/view adapters. The optimized oracle executes the pinned
converter. This is geometry/state evidence, not a GPU pixel, shadow or lighting
oracle. Changed authored RANI, ticks 0/1/17/1000, bounds, hierarchy, positions,
UVs and alpha preserve the static result. Previously supported Redux vertex
normals/output are explicitly preserved.

## Original inputs and known limitation

Boiling Point and Warp Shock each contain the same 7063-byte `model132.oof`:
SHA-256 `0fb25898a740b0849ffdce92c6549a6e18901da1dcbd5a1b22532c052d6d5047`.
Version 2300, four submodels, empty submodel properties, no facing lists and
opaque alpha. SPCL holds `$dummy` with zero-padded metadata strings. RANI has
frame counts 2/1/1/1; the first track has time/angle pairs 0/0 and 1/65536.
Both unchanged originals pass the pinned reader and static time-variation
comparisons.

**Existing normal mismatch remains:** optimized reference face normals differ
from Redux stored vertex normals at 209/212 model132 corners (maximum component
difference 1.69437). A previously admitted model114 also differs at 76/76 corners.
The explicit `xl_oof_render_reference --require-normal-parity <original>`
diagnostic fails and preserves this limitation. Ordinary invariance checks
require that authored RANI changes do not alter that mismatch or Redux output.
The original failed diagnostic remains available separately in local evidence.

A byte-level serialization of every mesh field for all 20 previously supported
original models in the two missions is unchanged: 103984 bytes, SHA-256
`ab45ee5c28b1a482d829d774ed386629446ffffbb20e9c1454578ee9e5e800f4`.

## Validation scope

All 43 standalone/reference CTests pass, including strict truncation, duplicate,
count, finite-value, padded-string, ordering and unsupported-profile negatives.
Native ownership, source identity, malformed preparation atomicity and disk
restore controls extend the existing countdown owner; they use explicitly
synthetic assets and do not alter original missions or trusted cohort counts.
Native ownership controls pass for 23/40 and 27/40: typed metadata/skin
ownership, authored RANI source identity changes without output changes,
malformed preparation atomicity, classic isolation and disk-through-stock
restoration. Existing native model slots preserve stock collision bytes while
OOF owns rendering; this is checked through classic transition and restoration.
The standalone generated-geometry equivalence control also remains mandatory.

Unchanged-original attempts on this branch based on cmake `e145e553` both exit 1
at `level_load`; neither reaches 120 ticks. Boiling Point level 7 stops at
`invalid reactor trigger count` (the independent #381 fix is not included).
Warp Shock level 6 advances beyond the OOF rejection to missing genuine
`computer.wav`; the sanctioned mission/mod/global lookup reports no file.
No asset substitution or download was attempted.

Original level identities: control2.rl2 SHA-256
`cfdfdd6d46eb2a4e933478f9ae37ea5cd7533ec2f7e53e649221fb6fc67c30ef`;
warplab.rl2 SHA-256
`be4f85abafe25df133bce95e3e7ce73e87a355e3040aa87f753fca76e61cc7da`.
The before/after original HOG hashes are unchanged: Boiling Point
`f0c7363c992bdcf0222873e63bcf7fff699c6f53ca28ab8ea9437f953b190ce2`,
Warp Shock `2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c`.
No readiness cohort or expected load/tick success was changed.

Both `test_upstream_compat` and `test_classic_trigger_demo` pass. Decoder controls
also pass ASan/UBSan (leak detection disabled in this environment). Scoped
quality and automation catalog validation pass. Catalog invocation is blocked
with exit 125 by the existing Linux supervisor's unavailable
`/proc/self/task/<pid>/children`; no checks or time limits were relaxed.

## Exact line-ending pins

The supported `reference_sources.py` checkout explicitly sets
`core.autocrlf=true`; existing broader oracles still require those CRLF working
bytes. This narrow follow-up does not change that setup or those validators.
The three OOF files now accept exactly two raw SHA-256 values each: the pinned
Git blob (LF) and its byte-for-byte CRLF conversion. No content is normalized
before hashing; mixed endings, BOMs, truncations and other edits are rejected.
The full constants are in `tests/reference_oof_pins.cmake`, derived directly
from Git revision `2576a98b5de310e3ac7c9777df2677d1627b8bae` and verified against
the supported CRLF checkout. This does not make the broader reference suite
accept an entirely LF checkout.

`xl_oof_source_pins` exercises both equivalent representations and rejects edits,
wrong source, truncation, mixed endings and BOMs independently for all three
files. This adds a standalone source-pin control without altering the trusted
original-witness cohort or its receipt counts.

The render oracle now executes pinned `AssignChildren` instead of approximating
its hierarchy setup. A valid opaque, property-free self-parent root fixture
reproduced the previous adapter-only bounds failure and now participates in
the complete authored-RANI/time-variation comparisons. Invalid parent indices
and a two-submodel cycle remain rejected; production geometry is unchanged.

Follow-up validation: all 44 standalone/reference CTests pass with the supported
CRLF checkout; all four OOF controls pass with only the three OOF files replaced
by their exact upstream LF blobs. All three extracted oracle headers are
byte-identical between setups. Both unchanged original models pass reader and
render/time-variation comparisons in both setups, and the explicit normal-parity
diagnostic remains failing. Each pin-control run verifies four positive outcomes
and 30 hash-guard rejections. Updated decoder controls pass ASan/UBSan.
