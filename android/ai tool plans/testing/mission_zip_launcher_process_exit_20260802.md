# Mission ZIP launcher process-exit recovery

## Goal

Fix mission ZIP regeneration failures where SetupActivity initially reports ready but the launcher process exits before automation writes a result.

## Plan

- [x] Reconstruct the trainng.zip activity and process timeline from batch artifacts
- [x] Trace the gap between launcher readiness, automation broadcast delivery, and result monitoring
- [x] Compare the mission batch handoff with the process-verified extraction startup fix
- [x] Implement targeted same-ZIP recovery without extending timeouts
- [x] Extend existing mission batch recovery coverage
- [x] Run focused validation and scoped code quality
- [x] Record root cause and focused rerun guidance

## Result

The failed batch reached SetupActivity and dispatched automation, then observed an ADB health hiccup and found that the launcher process had exited before any durable automation result was written. The runner already classified this as recoverable, but it restored the emulator only after recording the current ZIP as failed.

The mission ZIP batch now gives this infrastructure-only automation failure one same-ZIP retry. It restarts ADB, verifies the emulator, reinstalls the APK, restores the standard base data, clears the partial run artifacts, and repeats preparation and automation. App-produced analysis failures remain final and are not retried.

Validation:

- `trainng.zip` normal focused metadata run: passed all 8 steps
- Injected launcher termination after `SETUP_AUTOMATE`: first attempt detected the missing process, recovery completed, second attempt passed all 8 steps
- Existing mission ZIP preparation recovery test: passed
- Scoped PowerShell analysis and formatting: passed
