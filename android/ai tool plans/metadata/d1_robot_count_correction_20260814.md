# D1 Robot Count Correction Plan

## Objective

Reconcile the D1 robot-name catalog with the robot types actually loaded by the engine and
remove unreachable catalog entries if the 30-entry assumption is incorrect.

## Work

- [x] Trace D1 stock robot counts across data loading, constants, and preview requests
- [x] Determine that IDs 24 through 29 are unused capacity, not registered D1 robot types
- [x] Correct the D1 JSON catalog, parser count, and tests
- [x] Load the renamed `.jsonc` catalogs through comment-aware parsing
- [x] Cover line comments, block comments, and comment markers inside JSON strings
- [x] Run focused validation, formatting, and Android build/tests
