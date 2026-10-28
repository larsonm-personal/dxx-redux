# XL changing-light storage and activation

## Goal and base

Continue the singleplayer foundations with complete authored changing-light
tables and source-owned activation, rather than raising a stock count while
leaving narrow references or silently discarded contributions. Base:
xl_more_35_doors, PR #180, 99292e0239baaf3f2d0a23138eeaa137a584bffa.
The door tranche passed local native/source/Android acceptance and all seven CI workflows.

## Initial evidence

The original Dinter inventory contains 411 light-source indices and 32625 delta
records. Redux stores 500 indices and 32000 deltas, with signed-byte counts and
signed-short first indices. Existing XL translation splits a source count into
127-record chunks, creating another artificial 500-row limit and retaining a
narrow first index. Merely raising MAX_DELTA_LIGHTS would not complete this path.

Pinned reference 2576a98b5de310e3ac7c9777df2677d1627b8bae resizes both tables
from the source counts. Packed layouts retain wider counts and unsigned first
indices. lighting/light.cpp applies each source's deltas with per-corner lower
clamping and idempotent source-face enable state. Its renderer-method branches
and segment-light propagation require explicit scope, not a claim of universal
visual parity from a successful update.

## Implementation and acceptance

1. Audit and pin the source readers, allocation and changing-light arithmetic.
   Retain complete validated source records and reference widths. Check empty
   tables, 127/128/255 and packed counts, 32767/32768/65535 first indices, and
   ranges extending beyond the old 32000-delta boundary.
2. Prefer an XL-owned table/activation path over widening Redux structs and
   serialized layouts. Bind/release it with the admitted source. Ensure the
   private translated level cannot double-apply narrowed stock light records;
   distinguish source-owned runtime translation from a standalone classic file.
3. Verify authored off/on actions, repeated actions, actual UVL consumers,
   per-target clamping and all relevant source-profile calls. Preserve gameplay
   cues and source-face state. Document intentional fixes for unsafe reference
   lookup/overflow rather than copying undefined behavior.
4. Exercise disk through stock, rewind, failed admission and mission replacement.
   Restore must rebind the original tables before applying saved light switches.
   Extend metadata/introspection consumers to show actual XL source identities.
5. Add public contracts plus native/source integration using derived geometry,
   and an owned registered Android runtime test. Update existing native format
   checks to validate actual retained XL records and behavior, not an obsolete
   intermediate stock representation. Run relevant regressions and both stock
   controls, catalogs and scoped quality checks.
6. Re-audit original high-light candidates without promoting their independent
   unsupported features. Publish a stacked PR only after acceptance; no merges.

Flicker capacity, alternate renderer profiles, custom resource registries and
full original-mission qualification remain separately tracked requirements.

## Status

Owned runtime tables, full-width activation and schema-13 earned-light snapshots
are implemented. All 73 configured XL tests pass; additional propagation graphs
pass the affected integration again. MSVC/GCC extracted-source comparisons,
stock D1/D2, all 50 Android disk/rewind/stock steps and private-suite receipt
controls pass. Original Dinter and Sphere sp-03 lose only their lighting-capacity
blocker. Exact receipts, intentional reference corrections and retained renderer
limits are in [changing-light evidence](../../XL_compat/evidence/changing-lights.md).
Both automation catalogs and scoped quality checks pass. Ready for stacked
publication from xl_more_36_lights onto xl_more_35_doors; no merge is intended.
