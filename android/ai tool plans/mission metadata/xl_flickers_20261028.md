# Owned XL flicker timing and lifecycle

## Goal and base

Continue from xl_more_36_lights, cfa7f7561020d704bacc48ea34d4009284bda4a3,
PR #181. Implement XL variable-light behavior independently of Redux and retain
its phase through disk/rewind. Do not merely raise the stock 100-entry array.

## Initial source audit

Pinned reference 2576a98b5de310e3ac7c9777df2677d1627b8bae reads dynamically sized
flicker tables. `FlickerLights` rotates a 32-bit mask right; Redux rotates left
and uses platform-width unsigned long. Source disables an invisible/non-emitting
surface or zero delay. It consumes elapsed intervals but rotates only once per
frame. Explicit off/on controls affect the first matching source row. Negative
delays can hang the source loop and require a useful admission rejection.
Current saves do not retain earned flicker mask/timer phase.

## Implementation and acceptance

1. Own validated immutable source identities/delays and mutable 32-bit masks and
   clocks. Remove stock flicker records only from private runtime translation;
   standalone classic probes retain their capacity restrictions.
2. Hook ordinary frame processing and authored enable/disable calls. Use owned
   LGT/default XL brightness, visibility and exact right rotation, including
   disabled sentinel, zero delay, duplicate sources and large frame intervals.
   Correct overflow and unbounded catch-up loops explicitly; preserve Redux.
3. Extend checked source save state with exact phase, validating all rows before
   publication. Include flicker source records in the authored identity. Update
   geometry-light classification and introspection to use the owned records.
4. Add public layout/count/rejection contracts and extracted-source comparisons.
   Native integration must exercise more than 100 rows, real light updates,
   authored off/on, failed admission, disk through stock and malformed recovery.
5. Add an owned registered Android test with exact disk/rewind phase checks and
   protected-state restoration. Run affected regressions, stock controls, both
   catalogs and scoped quality checks. Reconcile original candidates without
   claiming campaign qualification. Publish a stacked PR only after acceptance.

The initial profile uses ordinary flickering with the reference's epilepsy-
friendly option off. Full alternate renderer/settings support stays on the
overall completion roadmap; no autonomous level navigation is planned.

## Status

Implementation and acceptance complete within the recorded default profile.
All 74 configured XL tests, both stock controls, 5432 source comparisons and all
50 Android lifecycle steps pass. Twelve malformed saved states reject atomically;
168 large-clock controls exercise bounded arithmetic. See
[flicker evidence](../../XL_compat/evidence/flickers.md) for hashes, source scope,
original survey reconciliation and the corrected restore-boundary test timing.
Development schema 14 replaces 13; no compatibility reader is added.
Publication and CI are the remaining delivery steps.
