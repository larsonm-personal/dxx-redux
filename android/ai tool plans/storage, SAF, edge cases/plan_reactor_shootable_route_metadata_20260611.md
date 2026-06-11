# Reactor shootable route metadata

## Goal
- Fix false `target unreachable or blocked by unsupported door` reports for levels where the reactor can be shot through a transparent/render-past wall from a reachable segment.

## Steps
- [x] Inspect the scanner view callbacks and game adapter doorway APIs.
- [x] Add a general shootable-reactor check without mission-specific special cases.
- [ ] Regenerate or rerun Plutonian Shores metadata and confirm levels 1 and 5 no longer report partial route failures.
- [ ] Run focused formatting/tests for the touched files.
