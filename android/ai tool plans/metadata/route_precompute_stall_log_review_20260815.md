# Route precompute stall log review

Date: 2026-08-15
Status: completed

## Plan

- [x] Reconstruct coordinator state transitions represented by the supplied log
- [x] Trace lifecycle, wake, cancellation, and priority behavior in the launcher
- [x] Check whether Android can run this work while the screen is off or the app is backgrounded
- [x] Identify concrete defects and the instrumentation or fixes needed next

## Scope

- Diagnose the supplied log and current implementation
- Do not change scheduling behavior until the failure mechanism is established

## Findings

- SetupActivity.onPause stops the coordinator, so screen-off and background execution are intentionally cancelled
- Fill work is throttled to one percent CPU duty and a low FVI budget
- Partial jobs remain eligible immediately and can be selected repeatedly instead of yielding to other levels
- Metadata viewer priority entries are only log records; they do not retarget the background coordinator
- Cancellation, pause, retry, and heartbeat events are absent from the persistent log, making active slow work indistinguishable from a stopped coordinator
