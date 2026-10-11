# Expand public shareware regression coverage

## Goal

Audit the full automation and PowerShell inventory, move content-independent existing D1/D2 scenarios onto verified public data, preserve retail geometry/import/media coverage, and execute real native and Android regressions

## Plan

1. Inventory every test and identify edition-dependent assertions and reusable candidates
2. Add explicit, fail-closed automation data profiles with pinned dependencies and mission overrides; retain retail selection
3. Migrate a broad reviewed set of existing scenarios without removing assertions
4. Add host regression coverage for profile resolution, dependency isolation, unsupported profiles, and unchanged action/assertion bodies
5. Run native builds/runtime tests, catalog validation and scoped formatting; coordinate actual emulator execution with Android support work

## Status

Inventory complete: 698 original test files classified, 27 existing scenarios /
51 engine variants profiled. Host profile, staging and real source-hash tests
pass. Automation catalog and direct master catalog pass. Device execution is
explicitly pending the separate emulator-support segment. Retail secret/geometry/
media cases retained. Final formatting and commit verification in progress.
