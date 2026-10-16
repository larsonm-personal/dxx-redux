# Bad level and mod data fixes

## Goal

Begin remediating the highest-risk findings from `android/security_scan_bad_level_mod_data_20261015.md`, preserving D1/D2 behavior for valid levels while rejecting hostile counts and references before they can write outside fixed arrays.

## Initial tranche

1. Add release-build validation for compiled mine vertex and segment counts in D1 and D2
2. Add release-build limits for level game-data section counts before section readers write global arrays
3. Harden immediately consumed object, model, and trigger references where a narrow validation can be applied without redesigning the loader
4. Add focused malformed-level regression coverage
5. Format changed files and run the relevant host tests/builds
6. Update the security ledger with implemented and remaining work

## Progress

- [x] Re-read repository guidance and the security ledger
- [ ] Trace loader return conventions and existing validation helpers
- [ ] Implement the first native validation tranche
- [ ] Add regression coverage
- [ ] Run formatting, focused tests, and build verification
- [ ] Update the fix ledger and record follow-up work
