# Headless corpus item count

- [x] Identify the worker-count helper's incorrect 1024-item validation cap
- [x] Allow any positive Int32 queue length, preserving worker throttling
- [x] Extend and run process-pool regression tests and scoped quality

The shared helper now accepts up to Int32.MaxValue queued items, for both GuideBot simulations and host metadata generation

Worker selection is unchanged: automatic half logical cores capped at 8, or the explicit requested limit, never exceeding the number of items

Process-pool integration tests pass, including item counts 1024, 1025, 1761, and Int32.MaxValue with automatic and explicit worker limits; scoped quality and all 45 existing D2 CTest tests pass

No native sources changed; a redundant Windows build check was stopped after discovering another Windows build already running in the workspace, to avoid interference
