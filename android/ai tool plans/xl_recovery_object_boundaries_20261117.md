# Boiling Point 4 object boundary audit

The original subzero.rl2 contains two Fusion powerups with control/movement 255. They are actual authored records, not a displaced decoder boundary. The
hash-pinned CObject::Read consumes all 134 objects consecutively from 829945
through 851653, exactly the next section's offset. Object 4 spans 830809-830898;
object 6 spans 830991-831080. Each is 89 bytes: 80 header bytes and nine render
bytes, with no control or movement payload. Both produce two source warnings.

The original HOG SHA256 is
f0c7363c992bdcf0222873e63bcf7fff699c6f53ca28ab8ea9437f953b190ce2;
subzero.rl2 SHA256 is
41a119e267513dff3e0f5ce399e4eab666f8d380bf2ae32f7694c6e4cc094b1e.
The pinned reader is revision 11038eb53dd86f3bcdc027dddd3f93c60f02536e,
objects/objectio.cpp SHA256
528f64e09469b5a452e9c6cf0ae8e9d5dd3eaffeaf5478378669f0783deaf68f.

## Implementation and limits

Inventory now decodes this specific powerup/vclip sentinel shape without
normalizing its authored bytes. It emits a powerup_sentinel_controller blocker
for each record. Other unknown movement/control shapes still reject. Gameplay
admission is unchanged: no readiness credit is claimed by this checkpoint.

The existing source-reader executable has an explicit --object-stream diagnostic
that takes original section bounds, count and versions. It executes the actual
pinned reader sequentially without obtaining object boundaries from Redux.
Truncation, excess count and incomplete consumption return failures.

The full original Boiling Point HOG now inventories all seven levels. Source
comparison matches all 509 object endpoints/identities and 193 effect payloads.
Subzero's current structural runtime blockers are exactly the two sentinel
records; asset-aware load validation may expose further dependencies. This does
not qualify every runtime consumer or imply all seven levels are playable.

Source Verify changes powerup control to CT_POWERUP but leaves movement 255.
UpdatePosition has no matching movement action. Native Redux read_object instead
asserts on unknown movement/control. Before runtime admission, compare source
initialization, pickup/control updates, collisions and save/restore continuation;
retain the authored identity and prove any native representation choice. No
guessed payload, scan, silent movement normalization or admission exception is
introduced here.

## Evidence

Private receipts: E:/dxx-redux-builds/xl-recovery-20261117:
subzero-object-stream.json/csv, object-stream-negatives.json,
boilpnt-inventory.json and boilpnt-source-reference.log. The modern-layout owner
checks adjacent sentinel and ordinary records in versions 21/23/25/27, unchanged
raw bytes, admission blocking and four malformed-shape controls per version.
The execution ledger will bind publication and applicable CI to the exact head.
# Modern CI witness follow-up

The PR 562 trusted run 44024601658 preserved 8/8 original readiness and completed
58/64 expanded readiness in 252.765 seconds. Its separate Linux asset owner failed
at `modern-probes`: the owner still required the historical Subzero decoder rejection.
This is an acceptance-contract omission in the inventory change, not a passing CI run.

The separately pinned modern HOG has SHA-256
`5cde12e3188fca12b655de3be424871f61bd464e9318b0da6bcdbc65b79551f3`.
Its v27 Subzero member has SHA-256
`77d0f94d30c903371cb0ba4e03015bc1692fcff09186c5e12afeba4784603041`.
The actual pinned object reader consumes all 134 records from 922671 through 944379.
Records 4 and 6 occupy [923535,923624) and [923717,923806), respectively; both
retain type 7, ID 16, control/movement 255, render 5 and no control/movement payload.
The inventory retains both explicit runtime blockers and remains unplayable.

The modern CI assertion now requires the complete object count, both exact raw
sentinel shapes and payloads, both blockers, and an additional pinned layout-reader
comparison of all 3566 segments. Historical witness counts remain seven format
witnesses and three v27 source witnesses; the added comparison is supplemental.
Six negative cases cover accidental admission, truncation, normalization, invented
payload, dropped blockers and omitted supplemental source comparison.

Validation: 53 Python asset/receipt tests pass with CMake on PATH; the real modern
HOG probe returns `(7, 3)` with the newly built reference executable. Initial runs
without CMake on PATH and with an older statically linked reference executable
failed and remain retained. Private evidence is under
`E:/dxx-redux-builds/xl-recovery-20261117`, including `modern-object-audit/`,
`modern-ci-tests-02.log`, `modern-ci-original-02.log` and the failed CI artifact.
