# PR 113 conflict resolution and test continuation

- Verified online-restoration dd6e56bceba6dc59627f89a0e9d3b7945f0c03fb and fetched cmake 4be58740e29440a2c2b944b31bcfd2d72c0d325b
- Merge cmake into online-restoration without rewriting history; retain draft PR
- Resolve workflow, sweep planner, and policy conflicts by retaining online campaigns and incoming three-player LAN requirements, strict leaf evidence, and per-child exits
- Run focused planner, receipt, catalog, and demo-listing tests; publish merge after validation
- Follow current D2 recording CI, then address late AUTH_OK deadline and deterministic proof exhaustion/reconnect coverage, followed by D1 requalification
- D2 online campaign remains unauthorized; preserve LAN cases/defaults and defer additive route modes until preceding fixes settle

## Merge resolution checkpoint

- Three textual conflicts: `.github/workflows/android-nightly-tooling.yml`, `android/tests/nightly_tooling_policy.json`, `android/tests/nightly_tooling_sweep.py`
- Retained incoming strict four-case LAN evidence, per-child exit codes, failure diagnostics, target-plan validation and third-emulator provisioning alongside online preview requirements
- Combined planner initially failed the existing scheduled three-player assertion: the D2 online preview's 2100-second estimate could displace the 900-second LAN owner
- Keep both online preview campaigns explicit opt-ins, retaining the scheduled LAN default and strengthening its test to exclude online preview selections
- Initial planner suite: 80/81 passed; corrected default-selection test then passed all 32 target-order fixtures; full corrected suite running
- Public receipt tests: 14/14 passed
- Automation catalog validation passed (95 standalone JSON, 431 support scripts, 264 standalone PowerShell)
- Demo-listing host fixtures passed, including exact count, stdout/status, wrong/nested/duplicate paths and bounded evidence
- Master catalog test blocked by host `/proc/self/task/<pid>/children` absence, including outside sandbox; no oracle change
- Scoped mixed-language formatter invoked; repository scope reported no newly eligible files, so pinned Ruff and Prettier were also applied directly to resolved files
- Native build/CTest and workspace-local JVM setup continuing; prior-head D2 sample 39689703879 attempt 2 passed provisioning and reached APK compilation, existing LAN run 39689703881 passed

## Follow-up fixes

- Merge published as `199703d8fba81f99005725d33b888afa295c3e89`; GitHub reported MERGEABLE and draft, base remained unchanged
- The complete corrected planner run passed 80/81 cases; the remaining online receipt fixture assumed implicit online selection. It now explicitly selects `online-preview`, and its unchanged failure-status/diagnostic assertions pass independently
- Reject AUTH_OK after the monotonic installation deadline before clearing the budget, even if the deadline coroutine has not run
- Added delayed-timer negative and live-budget positive controls, retaining disconnected identity/session and job cleanup assertions
- Added actual local WebSocket retry coverage: exhaust the real producer after 512 hashes at difficulty 20, observe cancellation with no submitted proof, allow the actual reconnect to create a new socket, challenge with a distinct nonce, independently validate its 20-bit proof and P-256 transcript signature, and accept authentication with the same persisted identity
- Synchronize the existing exhaustion test's socket-retirement observation with the service callback so scheduler advancement cannot race reconnect scheduling
- Full proxy JVM suite passed: 24 discovered, 21 executed/passed, 3 optional integrations skipped (historical comparison and two production-server fixtures). Production-server and Android D1 requalification still require the owned CI campaign
- Scoped code quality checks passed
- Native verification exposed an existing PhysFS portability error: PHYSFS_sint64* is not int64_t* on this host. Corrected only output variable types for the shared backward-demo-link helper in both games; no casts, seek changes or LAN changes

- Prior-head PR sample 39689703879 attempt 2 completed successfully at dd6e56bc; Android execution and required-selection reconciliation both passed after the earlier SDK provisioning failure
- Merge-head tooling run 39692227326 failed only the online receipt fixture described above on Linux and Windows; its explicit-campaign correction is included in this follow-up
- Both native builds and CTest completed: D1 89/89 and D2 98/98 passed, with no skips
- Current plan/code validation preserves exact recording-count and both-level-header assertions; no shared ADB harness or LAN scenario changes
