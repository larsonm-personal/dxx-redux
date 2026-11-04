# XL swept-hull crossing paths

The unchanged original Crauphel prefix intermittently reached the terminal cell
without firing its last boost/exit. This is not yet attributed to a source defect.
A separate native fixture found a concrete FVI metadata bug: a swept hull can
return a terminal segment while omitting intermediate segments from its path.

1. Retain failing native executable and original-derived diagnostics privately
2. Reproduce independently with generated corridor cells, speeds100/460 and nine
   frame intervals. Keep these controls in the existing native boost gate
3. Repair only malformed XL crossing metadata by tracing the center to the already
   accepted point. Preserve collision fate, point, normal, object and physics
   results. No trigger dispatch, movement extension or stock/D1 behavior changes
4. Compare baseline/fixed collision receipts and actual movement paths. Run boost
   lifecycle/save/restore/stock isolation plus scoped quality and test catalogs
5. Commit a coherent child PR, document exact evidence and the unproven link to the
   Android failure. No APK builds. Continue original earned progression separately
