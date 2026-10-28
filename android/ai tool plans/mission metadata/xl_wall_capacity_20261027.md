# XL wide wall references

Continue the thirteen-hour foundations run with the pinned XL wall capacity of 2047. Trigger capacity is a separate change: native wall trigger/controller
fields are signed bytes and cannot be widened safely by changing a constant

## Source and design

Reference 2576a98b5de310e3ac7c9777df2677d1627b8bae defines MAX_WALLS 2047 in
include/wall.h. CSide::ReadWallNum in main/side.cpp reads an unsigned short at
level version >=13; SaveState/LoadState also use a short. Redux's live segment,
door, cloaking and stuck-object wall references already use shorts

The limiting translation writes one-byte references in a classic mine. Add a
named private compiled-mine encoding for wide references, consumed only while
the validated XL loader owns the load. Preserve classic encoding for ordinary
levels and keep classic acceptance limits explicit. The private cache is an
internal adapter, not a newly supported public XL dialect or a standalone stock
level. Bind and clear its load context on success, failure and developer preview

Expand physical wall storage without changing packed wall/save records. Audit
the loader, D1 import, restore counts, classic demo recording/read/rewrite and
metadata consumers. Classic demo cloak events currently truncate wall IDs to a
byte; address that consumer before qualifying high-index cloak behavior

## Acceptance

1. Extract the source wall-reference reader and compare byte/short identities,
   no-wall sentinels and 254/255/256/2046 boundaries
2. Native derived fixtures remap authored wall IDs while retaining geometry and
   linked actions. Exercise high-index doors, cloaking, collisions and triggers
3. Check disk and memory restoration through stock, source reloading, earned
   resources, rejected malformed references/counts and recovery
4. Keep D1 and stock limits/serialization unchanged. Verify classic demo high-ID
   handling and existing stock demo controls without claiming complete XL demos
5. Register and run an owned Android integration, format, validate catalogs,
   publish a stacked PR and verify latest-head CI
6. Reinspect original candidates independently. Capacity support does not erase
   their remaining resource, event, effect or layout requirements

The audit scratch record is temp/xl-capacity-audit.md. Native MAX_CLOAKING_WALLS
is 10 versus the pinned source's 100; producer concurrency and wall ID capacity
are distinct requirements and must not be conflated in support claims

## Results

Source comparisons pass under MSVC and GCC. Native boundary, door/cloak,
save-through-stock, demo boundary/rewrite and invalid input controls pass.
The registered Android integration passes 80/80 steps with protected-state
hashes unchanged. All 21 original high-wall members match the survey hashes;
17 decode and retain other runtime blockers, four retain independent reader
failures. See [wide-wall evidence](../../XL_compat/evidence/wide-walls.md)
