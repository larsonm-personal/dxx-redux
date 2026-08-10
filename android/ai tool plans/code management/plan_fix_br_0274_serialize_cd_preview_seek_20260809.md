# Fix BR-0274 CD preview seek serialization

## Plan

- [x] Re-read project instructions, compare remaining P1 shortlist items, and confirm the live CD preview seek race
- [x] Trace preview render, callback, seek, pause, resume, stop, and state ownership plus the fixed MIDI preview design
- [x] Give decoder, ring, and OpenSL queue mutation one ordered owner with synchronized state snapshots
- [x] Add deterministic synchronization coverage for seek during rendering and stale pre-seek sample exclusion
- [x] Run scoped quality, native tests, Android all-ABI builds, relevant launcher integration, and diff validation
- [x] Move BR-0274 to the done ledger with resolution evidence
- [x] Mark this plan complete
