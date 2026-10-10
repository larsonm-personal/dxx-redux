# Save optimization adversarial review

Scope: the complete Android asynchronous save pipeline, including detached metadata, rewind, independent co-op checkpoints and transfers, and single-player periodic saves with secret-level companions. Review only; preserve unrelated work.

Plan:

1. Trace ownership, scheduling, all file consumers, and transition/drain boundaries against the actual source
2. Challenge queue ordering, publication failures, stale contexts, companion-cache recovery, and co-op restore authority with targeted failure/stress experiments
3. Run relevant existing tests where they establish useful coverage, recording limitations rather than extrapolating happy-path results
4. Record prioritized actionable findings with source locations, reproductions, and suggested regression tests

Status: complete. Production code was not changed by this review. Reviewed the save work through `a42fbb9f9` (including `a56718695`), plus current callers. Unrelated concurrent code-management work was preserved.

## Findings

### 1. P1: an interrupted D2 periodic save can permanently block its slot

`checkpoint_file_publish_pair` uses `android_file_pair_publish`, which first moves the old primary and companion to `.bak`, then installs the new companion and primary. There is no restart recovery. Any leftover backup causes all future attempts to return failure, before publishing anything.

Locations:

- `android/app/src/main/cpp/shared/checkpoint_file.cpp:90` selects this transaction for asynchronous paired saves
- `android/app/src/main/cpp/shared/android_file_pair_transaction.c:29` rejects leftover backups
- `android/app/src/main/cpp/shared/android_file_pair_transaction.c:39` begins moving the only published copy away
- `android/app/src/main/cpp/shared/state_android_shared.c:1276` rotates periodic slots only on success

Reproduction: execute the production transaction in a child process and terminate immediately after each successful rename. After renames 1-3 the primary is missing from its normal path. After rename 4 the new pair is present, but old backups still block subsequent saves. A fresh call to the real filesystem publisher fails at all four interruption points. This also reproduces with no secret companion: D2 still uses the pair transaction when companion absence is the captured state. Thus D1-in-D2 is affected too; native D1's asynchronous single-file rename is not.

An injected backup-deletion failure also reports success while leaving the next healthy attempt permanently blocked. An injected failure during rollback leaves both ordinary paths missing with old bytes retained only in backups. Those are retained recovery material, not a usable/recovered save.

The transaction weakness predates the optimization; the new periodic path inherits it. On failure the scheduler retries the same slot every ten game seconds and never advances to the other slot. An older alternate slot can remain usable, but periodic progress protection stops advancing.

Suggested fix: use immutable save generations with an atomically replaced manifest identifying both files, or introduce an explicit recoverable transaction protocol with startup/read-time recovery. Simply deleting `.bak` on retry could destroy the only surviving old save. Include directory durability in the protocol if power-loss durability is intended.

Regression tests: kill/restart after every rename and cleanup operation, with old/new companion present and absent; run another periodic save and load it; inject both the original operation failure and rollback failure. Assert a coherent old or new pair remains discoverable and retries recover.

### 2. P2: one failed companion read disables periodic saves for the current context

`state_checkpoint_refresh_companion` invalidates and clears the cache before opening the live secret file. If opening/reading fails, `state_checkpoint_submit_slot` keeps refusing captures. The ten-second retry repeats submission without refreshing the cache. Only another context reset or explicit secret-companion change retries the read.

Locations:

- `android/app/src/main/cpp/shared/state_checkpoint.cpp:295` invalidates the cache before I/O
- `android/app/src/main/cpp/shared/state_checkpoint.cpp:242` refuses an invalid cache
- `android/app/src/main/cpp/shared/state_android_shared.c:604` refreshes on context reset
- `android/app/src/main/cpp/shared/state_android_shared.c:1322` retries submission without refreshing

Production-worker probe: fail exactly one `PHYSFS_openRead`, restore healthy storage, then submit five times. All five attempts fail and the source-open count remains one. An explicit refresh immediately restores successful saving. A zero-length/unreadable companion has the same invalid-cache consequence. Open failure also bypasses the detailed capture-failure log at the bottom of the refresh function.

Suggested fix: retain an explicit dirty/error state and retry companion capture with backoff, safely at a context boundary or asynchronously with generation validation. Do not silently treat unreadable state as absence or reuse stale companion bytes.

Regression tests: fail open, length, short read and close once each; repair the source without changing levels; advance through the scheduler retry deadline and verify a complete pair is saved and slot rotation resumes.

### 3. P2: accumulated co-op worlds still cause substantial synchronous capture work

The detached worker moves Guide-Bot metadata encoding and file publication off the game thread, but `coop_write_save_payload` still validates, allocates/encodes and checksums the campaign archive during capture. The archive encoder checksums every stored world, and the outer save then checksums the whole encoded archive again. This happens for rewind captures as well as disk checkpoints.

Locations:

- `android/app/src/main/cpp/shared/coop/coop_save.c:1073` validates stored worlds during capture
- `android/app/src/main/cpp/shared/coop/coop_save.c:1075` encodes the campaign synchronously
- `android/app/src/main/cpp/shared/coop/coop_save.c:1090` checksums it again
- `android/app/src/main/cpp/shared/coop/coop_campaign.c:269` checksums each world inside encoding

S21 component experiment: compiled the production campaign encoder and checksum with NDK r30, ARM64 `-O2`; 3 warmups and 31 measured iterations per size. Each synthetic stored world contains 1.25 MiB, representative of the earlier roughly 1.28 MB D2 saves. The archive shape passes `coop_campaign_valid`; embedded bytes are synthetic, so this is a component benchmark, not an end-to-end valid campaign/frame test. Content does not affect the checksum loop's work.

| Stored worlds | Encoded bytes | Encode median | Outer checksum median | Combined median | Combined p95 |
| ------------- | ------------: | ------------: | --------------------: | --------------: | -----------: |
| 0             |            40 |      0.001 ms |             <0.001 ms |        0.001 ms |     0.002 ms |
| 1             |     1,310,776 |      2.048 ms |              1.933 ms |        3.981 ms |     4.044 ms |
| 3             |     3,932,248 |      6.210 ms |              5.843 ms |       12.061 ms |    12.112 ms |
| 7             |     9,175,192 |     14.745 ms |             13.604 ms |       28.347 ms |    29.667 ms |
| 12            |    15,728,872 |     25.287 ms |             23.321 ms |       48.619 ms |    50.585 ms |

These times exclude live-world serialization, stored-world validation, copying into the final save, callback copying into rewind history, and normal frame work. The large archive is within the 16 MiB format limit and represents a pathological custom campaign. Three stored secrets are a more ordinary native-D2 case. The earlier D1-in-D2 level-6 timing does not cover this path with accumulated native-D2 secret worlds. This is a remaining bottleneck, not evidence that the optimization made this component slower.

Suggested fix: retain immutable archive generations and cached per-world checksums; capture references on the game thread and assemble/hash the final owned save on the worker. Measure total main-thread submission and collection cost as the archive grows. Keep the existing save format and authoritative transfer contract.

Regression/performance tests: valid end-to-end campaigns with 0/1/3 stored secrets and near-limit custom archives; simultaneous rewind and disk deadlines; low-memory allocation failures; compare restored dormant-world contents and record full-frame p95/max, capture, and collection timings.

## Confirmed consistency limitation

Co-op sidecars remain best effort after primary publication (`state_checkpoint.cpp:149`). A deterministic staging failure for history publication produces a successful callback with new primary bytes, while history still labels that slot with the old level. `MultiplayerScreen.kt:1036` reads that history directly and does not cross-check the save's embedded metadata. It can therefore show an old label for new contents, or omit a new checkpoint entirely.

This is an explicitly documented tradeoff, and the previous synchronous implementation also published these files separately. It is not classified as a newly introduced regression here. It matters for progress recovery: logging a sidecar failure alone does not repair discoverability. Add index reconstruction from validated save trailers/generations, or make the index the commit point. Test failure of each scoped/global history, info and last-save-set sidecar separately, including restart before repair.

## Checks and experiments completed

- D2: metadata codec, queue, checkpoint publication, paired transaction, save metadata, save-set paths, co-op save format, campaign codec, rewind policy and transfer policy tests all passed (10 tests)
- D1: metadata codec, queue, checkpoint publication, paired transaction, rewind policy and transfer policy tests all passed (6 tests)
- Production worker, queue and publisher compiled byte-for-byte with controlled game/PhysFS dependencies under Linux AddressSanitizer and UndefinedBehaviorSanitizer. Game serialization and metadata encoding were stubbed in this harness; their correctness is covered separately, not by this worker probe
- Worker probe held real `fsync` behind a deterministic gate: three slots accepted, fourth rejected without blocking, no premature callback; releasing the gate completed FIFO callbacks on the main thread. Changing the source and companion cache while queued preserved each captured generation, including absence
- 100,000 further worker submissions with interleaved polling/draining passed buffer/token and callback-thread checks. Publication failure retained old bytes and a later healthy retry succeeded
- Production pair transaction exercised process interruption after each rename, failed cleanup, and failed rollback, revealing finding 1
- Production worker companion-cache recovery probe revealed finding 2
- Real metadata codec accepted extreme floating values and extreme epoch arguments; 250 deterministic multi-bit corruptions were rejected without altering the destination. No ASan/UBSan errors were reported
- Inspected the snapshot schema against the replaced live serializer: field order matches, owned snapshot fields are copied on the game thread, and the worker codec does not read live game globals
- Inspected authoritative co-op load: host retains and applies the same owned bytes sent to peers even if its disk slot changes; transfer failure does not command peers to load independent local histories. Independent peer histories are intentional and are not a finding
- The S21 ran only a temporary standalone benchmark under `/data/local/tmp`; it was removed afterwards. No APK, app data, preference, production game, or device setting was changed for this review

ThreadSanitizer could not run: WSL aborted with `unexpected memory mapping` before executing the queue test. The stress test is useful evidence, not a proof of race freedom. This review did not rerun the earlier complete two-device game integrations, nor emulate sudden power loss on an actual filesystem. Process-exit tests exercise interruption recovery, not storage-device durability.

## Remaining high-value adversarial integration cases

1. Delay publication while changing pilot/mission/level, changing host, loading another save and rewinding backward. Assert old callbacks do not populate the new history or alter its scheduler; assert captured file paths stay scoped to the original save set
2. Queue rewind and disk captures together, then exercise manual save, minimize, exit and immediate restore. Prove older queued jobs cannot overwrite the newer explicit save or its index
3. Disconnect the host at transfer start, mid-chunk, after receiver acknowledgement and before host apply. Include unreliable-transfer refusal. Both peers must fail/recover explicitly, without choosing their unrelated same-number slots
4. Exhaust storage during stage write, flush, primary rename and each sidecar; restore free space without leaving the level. Assert automatic recovery and correct UI labels
5. Race secret entry/return/reactor destruction with a queued periodic capture, including companion absent -> present -> absent and multiple retained generations. Run actual restore/revisit and verify saved robot/player state, not just file equality
6. Stalled disk I/O can hold later rewind encoding behind the single FIFO worker. Test recovery and history age under multi-second stalls; verify queue saturation remains bounded. Explicit drain is intentionally blocking, so measure lifecycle latency too
7. Maximum objects, remembered players, pickup/recovery ledger and dormant worlds; allocation failure while growing all three job buffers and the 12-entry rewind ring. Do not infer a worst-case frame guarantee from the stationary level-6 measurements

## Reproduction artifacts

Local experimental sources and logs are under `temp/save-adversarial-20261009/` (ignored scratch, intentionally not registered as game automation):

- `prepare.py`: copies production sources byte-for-byte and provides controlled dependency declarations
- `worker_probe.cpp`, `worker-results.txt`: cache failure, stalled I/O, ownership/FIFO/drain stress, failed publish/recovery, stale sidecar
- `pair_probe.cpp`, `pair-results.txt`: isolated child-process interruption and injected transaction failures
- `codec_probe.c`, `codec-results.txt`: real detached-codec boundary/corruption checks
- `campaign_bench.c`, `s21-campaign-results.txt`: production archive component timings
- `queue-tsan-results.txt`: explicit sanitizer-runtime limitation
- `host-more-results.txt`: additional D2 CTest results

The pair probe retains interrupted files for inspection and gives each execution a unique PID directory. An initial rerun reused earlier crash artifacts and failed fixture setup; the final probe uses isolated directories. No production change was made to turn a failing behavior into a passing test.
