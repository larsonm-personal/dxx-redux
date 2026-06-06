[x] Audit resume-save and restore debug logs added during crash investigation
[x] Keep durable launcher/startup restore logs that explain user-visible failures
[x] Remove short-lived D2 breadcrumb probes now that build 15350 confirmed the cause
[x] Run scoped formatting and validation

# Resume Save Logging Cleanup Plan

## Goal

Now that launcher `Load Last Save` works, trim the investigation logging down to a stable set that helps diagnose future resume failures without flooding Game Logs during ordinary save loads.

## Keep

- Pending resume launch write/read/reject/clear logs in the launcher.
- Startup resume prepare/begin/result logs in D1/D2 startup.
- Android fatal `Error()` logging, since it turns controlled fatal exits into useful Game Logs.
- A compact D2 secret companion summary for direct restore paths and build failures.

## Clean Up

- D2 `state_restore_all()` probe markers around `stop_time()`, `start_time()`, and `state_restore_all_sub()`.
- Verbose D2 secret companion breadcrumbs that were only used to bracket the null `Current_mission` dereference.
- Per-call secret filename builder logs in the shared helper once the D2 caller records the useful outcome.
