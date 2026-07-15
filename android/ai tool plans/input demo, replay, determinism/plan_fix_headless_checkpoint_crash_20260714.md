# Fix Headless Checkpoint Demo Crash Plan

Date: 2026-07-14
Status: Complete

## Objective

Find and fix the common `0xC0000005` crash affecting all committed D2 headless checkpoint demos, then rerun the complete headless regression corpus.

## Plan

- [x] Reproduce the failure across the complete 11-demo D2 headless corpus
- [x] Resolve the crashing instruction and call stack from dump data or targeted restore tracing
- [x] Identify the invalid state or initialization ordering that causes checkpoint restoration to crash
- [x] Implement the smallest symmetric or shared fix required
- [x] Use the complete committed D2 headless corpus as focused integration coverage
- [x] Run scoped code quality, Windows builds, native tests, Android assembly, and all 11 headless demos
- [x] Record the root cause and complete verification results here

## Root Causes

MSVC compiled `Mission` with different packing in different translation units. Checkpoint restore and mission loading read `level_names` at offsets 74 and 76 respectively, producing an invalid level filename pointer. D1 and D2 now give `Mission` an explicit MSVC packing scope so its layout is independent of include order.

After that restore crash was fixed, the dedicated headless runner completed simulation and wrote its result, then crashed because replay completion closed `Game_wind` from inside the final simulation frame. Headless replay completion now leaves the synthetic window open for process teardown.

## Headless Regression Result

All 11 committed D2 checkpoint demos passed their embedded-result comparisons in 90.688 seconds using the standard headless regression wrapper.

## Final Verification

- Scoped code quality passed for all changed C and C++ files
- Windows D1 and D2 builds passed
- Native CTest passed 21 of 21 D1 tests and 24 of 24 D2 tests
- Android `:app:assembleDebug` passed
- D2 headless regression corpus passed 11 of 11 demos
