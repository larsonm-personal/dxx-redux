# Extract paired saved-player inventory restoration

Diagnosis-only candidate plan for GQ2-CHUNK-0122. No product edit, build or test execution. Canonical GQF-0276 OPEN / GQR-0262 TODO admitted by chunk0122 publication. Product implementation and acceptance remain pending.

## Measured opportunity

Frozen D1 state.c L2873-L2891 and D2 state.c L3829-L3847 each contain a19line loop restoring active saved-player inventory, life and restore serial into live slots. Current counterparts are L2900-L2918 and L3856-L3874. Their18line bodies are identical after whitespace normalization; the D2 loop adds the world-only restore exclusion. Earlier commentary estimated20lines per loop; exact counting corrects this to19.

Replace the D1 loop with one shared call and D2 with an explicit two-line !coop_world_restore_active() guarded call. Preserve handmade comments at the call sites or with the moved implementation. Expected inherited source reduction is18+17=35lines across two edited hunks, not an applied saving or removal of the larger surrounding hunks. No reduction of native serialization. Confirm isolated before/after Git metrics at implementation time; full state-file numstat includes unrelated changes.

The natural owner is existing shared coop_save.c/.h: it already owns metadata lookup, native record application, absent inventory and recovery integration. A helper such as coop_restore_active_players_from_metadata(const coop_save_metadata *meta) needs no callback, copied schema, new source registration, extra traversal or allocation. Retain the game-format parsing and PHYSFS lifetime in inherited state.c.

## Ownership reconciliation

DMR1-CHUNK-007 concerns the D2 direct-restore filename-slot parser and is a separate deferred extraction. Existing BR0206 concerns coordinated restore failure, BR0195 packet authority, and GQF0254 final native ammo grants; this extraction does not resolve their acceptance. No active canonical extraction row for this saved-player loop was found in the inspected finding/remediation and DMR tables or targeted earlier plan search. Canonical admission now records this extraction separately as GQF-0276/GQR-0262; refresh overlap and prerequisites before implementation dispatch.

The larger recovery-bearing frozen state hunks contain36added/7removed lines in D1 and43added/2removed in D2, including includes and broader optional/strict gear handling. The measured candidate is only the19line loop in each, avoiding collateral extraction of distinct D2 world restore and failure/close policy. Frozen multi/powerup/gameseq lifecycle/codec hooks remain at natural engine observation boundaries; do not add wrapper tables around these narrow calls merely to move lines.

## Implementation and acceptance

- [ ] Reread live paired state loops, shared metadata lookup/application and concurrent checkpoint changes. Limit edits to the two loops and one shared declaration/definition, plus maintained focused validation. Do not alter neighboring async checkpoint work.
- [ ] Move the complete loop into the existing engine-compiled shared save owner. Preserve MAX_PLAYERS traversal order, client_id-first/callsign fallback lookup, active0..7 acceptance, copy-before-adjustment, recovery_alive before inventory application, nonpositive shield/energy reset, same_level=1 native application, and original_slot-based restore serial/life after application.
- [ ] Keep D2 world-only restore exclusion explicit at its inherited call site. Do not fold it into a shared helper that silently changes D1 or unrelated callers. Keep optional gear/source validation and absent-player restoration in their current order around the call.
- [ ] Preserve current unmatched/absent/disconnected-slot behavior and existing old_slot checks. Admission hardening, missing-record policy and outer transaction changes require their own evidence and owners; they are not part of a behavior-preserving extraction.
- [ ] Exercise actual metadata lookup and native record application with remapped saved slots, stable ID preference, callsign fallback, unmatched/absent records, dead saved ship normalization, current local object shields, remote inventories, life and restore serial. Compare complete pre/post player/recovery state, not source text or a duplicate implementation oracle.
- [ ] Validate ordinary D1/D2 full restore and D2 world-only restore exclusion, absent late return and source gear rejection controls through maintained integration. Ensure metadata is not mutated and iteration applies each live slot exactly as before. Preserve native formats and desktop builds.
- [ ] During implementation run relevant maintained fixtures/integration, paired Windows/Android builds and scoped quality. Measure isolated35line inherited reduction against the live baseline and actual original-file diff; record total shared-code effect separately. No such runtime/build evidence is claimed now.

## Provisional rating

57 (H/M/B/C/R=12/21/7/10/7): concrete paired merge-pressure/maintenance candidate,35 removable inherited lines/two edited hunks, exact shared owner and focused state-equivalence validation. Canonical writer must confirm this after admission. Recovery freeze liveness is a separate correctness owner and must not be bundled into this extraction.
