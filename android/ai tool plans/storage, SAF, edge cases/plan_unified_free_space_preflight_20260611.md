# Unified free space preflight

## Goal
- Survey every launcher path that writes extracted or temporary files before import, analysis, or launch.
- Reuse or replace the existing free-space check with one launcher-wide preflight and one consistent popup.
- Stop the pending operation before it starts when storage is insufficient.

## Plan
- [x] Map the current write-heavy paths: SAF copy, CD/GOG extraction, mission ZIP import, launch-time ZIP staging, level/music metadata analysis, and debug/demo staging.
- [x] Identify the existing free-space check and turn it into a reusable helper with testable calculation logic.
- [x] Add preflight calls before each operation that can create large temp or extracted files.
- [x] Use a shared Compose popup/state model so all failures look and behave the same.
- [x] Add focused tests for the new helper and at least one representative caller.
- [x] Run scoped formatting and relevant tests.
