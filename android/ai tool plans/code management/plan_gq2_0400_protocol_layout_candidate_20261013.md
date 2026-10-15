# Packet diagnostic protocol-layout candidate, 2026-10-13

Admitted source-static GQF-0309/GQR-0295 under GQ2-CHUNK-0400. Diagnosis-only publication PASS5648; local47MEDIUM/P2high23/0/4/10/10. Implementation and actual runtime validation remain future; no source/test changes or executed failure.

D2 writes a 65-byte asset identity after kind/token/count and uses a 74-byte object header. Both tools start entries at 9. Ordinary D2 identity is zero. Complete native END is 83 bytes, local -2 at 74, player at 78 and total at 79. Tools read zeros at 9 as local0/owner0/remote0, expect a 264-byte body at18 and mark it truncated because282>83. The receiver logs structurally admitted RX bytes. This is a source-derived witness, not synthesized/decoded bytes or a captured runtime failure. Frozen Python changes are formatting-only AST equivalents; this is a context-derived interface mismatch, not a formatting regression. Fixture constructors cover header9 only.

## Later implementation

Select explicit game/protocol layout in both CLIs: native D1 header9, current D2 header74 with zero/32/64 asset identity profiles. If automatic selection is retained, validate complete candidates and require unique admission; reject ambiguous/unsupported or incomplete data rather than guess. Never treat truncated header74 as header9 because it superficially fits. Keep native wire/security bytes and asset admission unchanged; report identity as diagnostic data without duplicating native asset-file digest authority.

Preserve kind/token/count, signed owners (-128/-1/0/127), INIT/END, remote IDs, packed264 key fields, native bracketed prefixes, complete even-length hex matching, declared length checks, zero/one/many collection shape, nonzero decode error status and suppressed loss claims for incomplete data. BR-0609 session/token/endpoint/generation partition is distinct and remains OPEN; corrected offsets cannot close duplicate/order/missing-marker/token-reuse obligations. Preserve FIXED BR-0608 and GQF-0242/0243/0244.

## Later acceptance

Extend maintained paired CLI controls using actual native producer layout authority. Cover D1 and D2 object/INIT/END packets, zero/32/64 identity profiles, signed owners, invalid identity length/padding, short common/header/entry/body data, mixed layouts and ambiguous admission. Require matching selected layout and packet/object fields and intentional status/diagnostics from both CLIs, without false truncation of complete records. Keep existing raw/log/diff and zero/one/many/bracket-prefix/incomplete/declared-length controls. Header9-only fixture success must not qualify current D2 support. Native receiver identity admission must remain unchanged.

Run focused paired CLI runner and scoped quality in a later implementation tranche; complete required automation catalogs if tests change. No tests/builds/probes executed here.

## Cleanup and disposition

Both assigned paths are authored Android tools and absent at 1996 original: zero inherited saving and no native wire simplification proposed. Consider deleting only proven unused expected_full/remaining/tx_init assignments with preserved diagnostic contracts. bytes_lost subtracts identical raw lengths, requiring a deliberate output decision rather than an invented loss estimate. No broad decoder rewrite/helper unification proposed or implemented.

Admitted local rating: P2/high correctness/diagnostics,47 MEDIUM (23/0/4/10/10), after consolidated dedup. Do not inherit unrelated extraction candidate score71 or add BR-0609's priority. No implementation or inherited saving.

Consolidated dedup/publication admits GQF-0309/GQR-0295 at47MEDIUM; publication PASS5648. Native base-to-frozen layout addition established; no decoder formatting-regression or executed failure claim.
