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
