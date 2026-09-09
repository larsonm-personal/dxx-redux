# Coop restore status completion

- [x] Trace status lifecycle in shared restore code and both games
- [x] Broadcast completion and order host status updates within a session
- [x] Exercise delayed status messages and paired D1/D2 restore flows
- [x] Run Android native build, scoped quality checks and mark the bug complete

Waiting and failure are broadcast, but completion only clears the local banner.
The two-byte status packet cannot distinguish delayed waiting from a new restore.
Use a monotonic host status revision, retain it on clients for host migration,
and reset it with multiplayer session initialization. Authenticate the sender
before accepting a revision. Keep the existing local restore completion hooks.

The retained level-start checkpoint path also omits the host status completion
entirely. Its transfer-finished callback must report success or failure, including
the no-client path. Exercise it through a real retained checkpoint restore.

Validation blocker: two D2 runs crashed on solo restore with
`OverlappingFileLockException` in `RouteMetadataLedger`. Concurrent ledger
instances need a shared process monitor around their existing cross-process
file locks. Add a concurrent read/update regression and rerun the device tests.

Completed validation:

- Android x86_64 D1/D2 native build and APK assembly passed
- All 12 RouteMetadataSchedulingTest tests passed, including concurrent ledger access
- D2 paired `-SavedLateJoin -RestoreStatus` passed: solo full-save restore, late
  join, network completion broadcast, actual retained-checkpoint restore, stale
  and duplicate status rejection, sender validation, and subsequent status changes
- Scoped mixed-language formatting/lint passed
- D1 paired `-SavedLateJoin -RestoreStatus` passed with the phases separated
  so tests do not depend on observing brief idle intervals between restores
- The retained-checkpoint host callback now clears the banner on success and
  reports failure; status packets include a six-byte schema mirrored in D1/D2
- `git diff --check` passed
