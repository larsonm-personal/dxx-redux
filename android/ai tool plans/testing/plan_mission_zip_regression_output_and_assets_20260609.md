# Mission ZIP Regression Output And Assets - 2026-06-09

## Goal
Clean up mission ZIP regression JSON output, keep failed ZIPs represented by concise failure JSON, fix missing mission filenames, and include mission ZIPs in the hashed/tracked game asset flow.

## Plan
- [x] Find the JSON serialization paths for mission ZIP level metadata and failure handling.
- [x] Omit empty/noise fields from successful level rows.
- [x] Write concise failure JSON for failed ZIPs instead of empty arrays.
- [x] Trace and fix empty `mission_filename` values.
- [x] Locate hashed game asset manifests and include mission ZIPs in their generation/export flow.
- [x] Run focused generation checks and update asset hash JSON files.

## Notes
- Preserve useful non-ok statuses and non-empty problem/note fields.
- Do not delete `mission_filename`; fix its value.
