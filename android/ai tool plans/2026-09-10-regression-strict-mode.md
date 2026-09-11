# Regression metadata failure and strict-mode hardening

1. Fix shared descriptor parsing for the levelpack.7z d2x-name declaration and verify the whole archive
2. Normalize collection assignments and optional metadata fields in regression runners and shared helpers
3. Audit helper scope, sampling, cached records, and failure paths; enable explicit strict mode at regression entry points
4. Extend existing tests for strict mode, collection cardinality, optional/required fields, and failure cases
5. Run scoped quality checks, focused integration tests, and complete metadata plus guidebot simulation regeneration

Preserve existing user-generated regression changes and report concrete failures without dropping inputs to obtain a pass

Additional findings during full-corpus verification:
- Parallel metadata completion assumed optional reason/metadata_json fields and nonempty summaries; covered with real callback tests for success/failure/empty/missing/timeout/start errors
- levelpack.7z was incorrectly mounting every collection HOG for every descriptor; use the existing engine descriptor-specific loading for multi-mission archives
- Azure Catacombs XL declares level version 23, outside the native loader's supported versions; reject unsupported headers in native metadata before loading, retain explicit per-level diagnostics, and test both games
- Full metadata stage passed on 2026-09-10: 138 passed, zero skipped/failed; report run_20260910_090353
- Collection simulation publication reparsed the same file for every mission; load one fresh snapshot per publication. Real levelpack output is byte-identical, benchmark 8.2s -> 1.65s. Tests cover preserving other missions and observing updates on the next publication
- Full simulation stage passed on 2026-09-10: all 2,639 selected levels recorded, stage exit 0; report run_20260910_092347
- Final wrapper summary exposed cleanup text leaking into returned stage records when the report directory already existed. Route retention output through Out-Host; test existing directories, fail-and-continue, and the actual command-line completion block with a real child stage
- FFYL.rar passed metadata and produced all 27 simulation records; route diagnostics remain visible (23 ok, one failed, three timeout)
- Native D1/D2 builds and focused CTest, Kotlin parser tests, PowerShell integration tests, and scoped code quality checks passed. The full stages passed separately; the final wrapper-only output fix was verified with its focused integration test rather than repeating engine scans
