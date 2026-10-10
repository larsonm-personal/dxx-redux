# GQR-0038 STi2 method 15 peak-memory budget plan

Date: 2026-08-12

## Objective

Close `GQR-0038` / `GQF-0051` by enforcing the native 128 MiB memory
ceiling against the complete STi2 method 15 live set. The calculation must
include the retained archive, output, decoder workspace, decoder state, and
the public STi2 entry-list caller state. It must reject an over-budget input
before either large allocation.

No D1 or D2 source change is expected.

## Work phases

- [x] Read repository instructions and canonical finding/evidence
- [x] Trace STi2 method 15 allocations and direct/nested callers
- [x] Replace per-block block/transform allocation with one bounded workspace
- [x] Apply one overflow-safe admission calculation before output/workspace allocation
- [x] Carry retained outer and nested StuffIt buffers through the same memory budget
- [x] Add exact-limit, one-byte-over, block-size, and allocation-failure coverage
- [x] Run focused host tests, scoped quality checks, diff checks, and Android ABI compile
- [x] Record final paths, metrics, validation, and limitations here

## Intended ownership

- `android/app/src/main/cpp/extract/sti2_extract.c`
- `android/app/src/main/cpp/extract/sti2_extract.h`
- `android/app/src/main/cpp/extract/test_sti2.c`
- `android/app/src/main/cpp/extract/stuffit_extract.c`
- `android/app/src/main/cpp/extract/CMakeLists.txt` only if test registration requires it
- this plan

The general quality ledger and the root next-ten plan are owned by the
orchestrator and will not be edited in this item.

## Completion evidence

- Method 15 reads its block-size header before large allocation, charges the
  output, decoder state, entry-list caller state, block, and transform to the
  attempt memory budget, and allocates one reusable block/transform workspace
- Direct STi2, matching STi2, outer SIT5, and nested STi2 paths charge their
  retained archive buffers to that same attempt budget
- Exact 128 MiB and one-byte-over checks cover 9-bit and 24-bit blocks;
  deterministic injection covers output and workspace allocation failures
- Windows host: `test_sti2` 12/12, StuffIt corpus 6/6, malformed SIT5 pass;
  real D1/D2 Mac media and method-15 corpus hashes pass
- Android native builds pass for arm64-v8a, armeabi-v7a, and x86_64
- Scoped formatting/BOM checks and `git diff --check` pass

The complete APK task was also attempted. Native arm64-v8a and armeabi-v7a
completed, but concurrent build-directory access caused an x86_64 Ninja deps
log permission error and a Kotlin incremental-output deletion error. A serial
native-only rerun then passed all three ABIs. No emulator heap profiler or
concurrent extraction stress run was performed.

### STi2 production reservation ownership reopened, 2026-10-09

- Imported GQR-0038 production reservation ownership diagnosis reopening 20261009 with exact source/test/plan identities. Frozen/current method15 reservation sequence unchanged; incomplete acceptance, no new source regrowth or duplicate finding
- Existing GQF-0051 reopened FIXED to OPEN and GQR-0038 DONE to TODO. Failed reserve preserves retained charge, then cleanup wrongly releases an unacquired request; nested StuffIt can swallow failure and continue. Prior workspace/CRC/helper/OOM/media/ABI history preserved
- Plan separates requested/acquired ownership, keeps returned payload charged to actual free and reserves retained buffers before materialization. Actual production rejection/continuation/lifetime acceptance remains deferred. No code/test change or executed physical overrun/security claim
- Totals266 findings/252 remediations, now78 DONE/173 TODO/1 DEFERRED. GQ2 remains159 DONE/486 TODO, chunk0019 still in progress; GQ1 remains818 DONE/1 TODO
- Additional0019 context read: current STi2 extraction/writer/resource/matching lifetime, complete CD preview/fingerprint JNI and strict string codec, paired protocol prefix and partial Kotlin EngineQuery consumer. Finish query caller/protocol/build/owner reconciliation before terminal report
