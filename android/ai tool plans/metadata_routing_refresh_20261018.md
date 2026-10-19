# Mission and routing metadata refresh, 2026-10-18

Request: rerun mission and routing metadata after the recent Dot changes and inspect the resulting local JSON.

- Capture the current HEAD, clean/dirty state, relevant source hashes, and exact existing mission/routing JSON bytes and timestamps before publication
- Run full host-native mission metadata regeneration with a fresh build, then full canonical headless GuideBot routing regeneration with WriteRegression
- Use the maintained producers with no archive/level filters, sampling, NoBuild, or semantic baseline acceptance
- Log both actual stage exits and producer summaries. Monitor disk space and stop only owned children above the user100GB floor if necessary
- Compare pre/post JSON, retain producer diagnostics and report failures, source drift, scope counts, and changed publication paths

Current run: temp/metadata_routing_refresh/run_20261018_091329
Initial HEAD:9302a5a4b; initial worktree clean

Coverage detail verified from runner selection and emitted records: full default scan covers133archives,5CD sources and2built-ins,140publications total. The full scan automatically includes FirstStrike and Counterstrike when no archive filter is set. Both already passed. Removed the initially planned redundant built-in pass before it ran. No filter, NoBuild, sample or extra regeneration pass is needed.

Final results, 2026-10-18:

- Fresh builds completed from unchanged HEAD9302a5a4b439858fb7ee558a3f1c42cc3cf0bf5d
- Mission metadata:140/140 passed, no skips or failures
- Routing:2639/2639 levels selected and recorded; full producer exit1 for af_d1_beta level7/cefispar.rdl, with native error Robot30 unavailable in selected D1 edition
- Changed JSON:140mission publications and136routing publications;11existing JSON files retained exact bytes and timestamps
- Three level status changes: levelpack target116/incontin timeout->ok; Harqyjia level2 ok->timeout; af_d1_beta level7 ok->infrastructure_error. Route input hashes did not change. Failures retained for review
- All2040recorded source hashes unchanged; original287JSON snapshots verified. Exact140mission and140routing post-run snapshots retained
- Regeneration supervisor and native workers exited; power hold released. Approximately120GB free. Cleanup preview only; no caches were deleted
- comparison.json and RESULTS.txt under temp/metadata_routing_refresh/run_20261018_091329 provide full counts, changed paths/hashes, statuses and artifact navigation
- Only requested canonical metadata/routing JSON and this owned plan are changed. No production source edits or replay oracle acceptance
