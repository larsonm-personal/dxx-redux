# Regression throughput

1. Measure the current worker caps and serialized publication costs
2. Batch headless simulation publication with bounded checkpoints and a final flush, preserving all selected results and canonical ordering
3. Increase bounded automatic concurrency and schedule expensive work first
4. Extend integration coverage for checkpoints and worker sizing; run scoped quality checks
5. Measure a complete metadata and simulation regeneration against the prior 12:52 metadata and 42:33 simulation stages, and report remaining limits

Implemented and verified on the 16-thread, 32 GiB host:
- Automatic process workers use 75% of logical processors, capped at 16; metadata caps at eight because each archive worker also owns JVM/native processes. Explicit MaxParallel remains available
- Headless simulation publication checkpoints at 32 completed results per metadata file, or on a completion after 30 seconds; the final partial batch always flushes. Headed comparisons retain immediate publication
- Larger archives and higher simulation budgets start first; canonical serialization order remains unchanged
- Increased the default outer process watchdog from 180 to 360 seconds after Uneasy4 exceeded it under parallel CPU load. The engine simulation budget is unchanged; explicit watchdog overrides remain available

Measurements and verification:
- Metadata report run_20260910_101351: 138 passed, zero skipped/failed; processing 688.3s -> 505.5s (27% reduction). Full stage 12:03 included a substantial native rebuild
- The first optimized simulation pass took 13:51 but exposed Uneasy4's watchdog timeout; it is not a passing benchmark
- Final simulation report run_20260910_104021: 14:11, exit 0, all 2,639 levels, zero infrastructure failures (previously 42:33)
- All 140 simulation JSON files match the pre-optimization SHA-256 baseline byte for byte, including Uneasy4
- Successful stage durations total about 26 minutes versus 55 minutes previously on the same expanded corpus; stages were verified separately after the watchdog adjustment
- Whole-machine CPU samples during busy phases were 32-82%, including 54%, 75%, and 82% during simulation; these are samples, not a sustained utilization average
- Worker-pool, publication batching, schema, and runner integration tests passed, as did native builds and scoped PowerShell quality checks

Remaining serial costs include build/staging checks and descriptor scans within a single large archive
