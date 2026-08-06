# Server and engine preferences failure investigation

## Goal

Determine and fix the remaining `test_server_integration` and
`test_engine_prefs_unified` failures from report `20260805_090321`, without
masking races with retries or timeout increases and without relying on isolated
emulator success.

## Plan

- [x] Create the investigation plan
- [x] Trace the server port-bind failure and its concurrency assumptions
- [x] Implement and test a race-free server test harness fix
- [x] Inspect engine-preferences script, runner artifacts, and predecessors
- [x] Reproduce engine preferences with relevant accumulated state
- [x] Fix or disposition the engine-preferences failure from durable evidence
- [x] Run ordering-sensitive validation and scoped quality checks
- [x] Record final evidence and remaining limitations

## Result

`test_server_integration` exposed the documented sequential NAT port race. The
simulator selected a released ephemeral port, added 200 with wrapping
arithmetic, returned successfully, and later attempted the real bind inside a
detached task. It now reserves a contiguous 32-socket range before startup
returns, reports collisions and invalid wrapping ranges synchronously, and
uses the reserved sockets for mappings. Startup now returns `io::Result`.
Focused tests cover occupied and wrapping explicit ranges.

`test_engine_prefs_unified` reproduced the batch's blank-log behavior only
when the emulator changed to `offline` before automation was staged. The
captured file contained only the health helper's Boolean output, and no durable
automation result existed. After the emulator transport recovered, the exact
`test_double_launch` predecessor followed immediately by the complete D1 and
D2 engine-preferences test passed twice. This is an infrastructure failure,
not an engine-preference assertion or stale-state failure.

The launcher test runner now suppresses the health helper's Boolean return and
retries game-data resolution only when the first resolution failed and a
health check confirms the emulator became unhealthy. Recovery occurs before
automation begins. Genuine dependency failures and all failures after
automation starts retain their existing failure behavior. No timeout changed.

Validation completed:

- Focused NAT simulator suite passed all 13 tests
- Required Rust lint, build, and full test pass completed successfully
- Exact top-level server integration test passed all 104 tests
- Exact predecessor plus unified engine preferences passed for D1 and D2
- Final D2 durable automation result passed 44/44 steps
- Scoped PowerShell and repository quality checks passed
