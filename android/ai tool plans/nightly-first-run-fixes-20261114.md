# First scheduled sampler failure

Base: cmake merge 755b368bffd2ed22fd1cf86eb5ebd549bdaa2b21. Run 43584661983.

- Verify immutable run receipts, published traversal and setup logs. Keep genuine geometry/baseline/test failures visible; do not regenerate baselines.
- Fix stable Rust toolchain selection for dual-device setup using rust-toolchain.toml, not the multi-tool inventory.
- Explicitly distinguish the complete fixture-independent Gradle core from named unavailable integration leaves; retain leaf counts and precise missing prerequisites, never certify skipped leaves.
- Retain stable runtime capability exclusions separately from traversal completion, requalifying on code/fixture/definition changes and cycle rollover. Transient setup/budget blocks remain pending.
- Exclude nested phase build trees from public artifacts while preserving diagnostic receipts/logs.
- Preserve one-hour elapsed target, 16 host shards, five quick core owners, pseudorandom ordering and cross-revision receipt progress.
- Focused regressions, bounded artifact inspection and lint; new branch/draft PR and current-head CI. No merge, cmake push, history rewrite or full dispatch.

## Admission and host-target follow-through

- PR496 head a8744574e timed out in the shared four-minute replay admission step after passing replay failure controls and starting catalog integration; the same catalog integration passed in aggregate script CI.
- Move all fifteen media-free admission commands into a separately bounded job alongside the native build, with explicit command groups and a required dependency before native save/replay execution. Preserve failure propagation, diagnostics and native workload limits.
- Reproduce the first-night host test_android_test_target failure. Correct its fake ADB protocol responses, covering an empty console plus valid boot-property fallback, missing identity and mismatched replacement identity without weakening the real guard.
- Run every admission command, scoped quality and workflow lint, then publish an ordinary follow-up commit and inspect exact-head CI. Retain genuine corpus/readiness failures. No merge or full nightly dispatch.
