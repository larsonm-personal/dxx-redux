# Save review fixes

Objective: fix all three findings from `save_adversarial_review_20261009.md`, preserving independent peer saves and authoritative load transfer.

Requirements:

1. Interrupted paired publication must recover a coherent old/new save before reading or retrying. Cleanup and rollback failure must remain recoverable, including companion absence. Add deterministic interruption/restart tests
2. Failed secret-companion capture must recover automatically after transient I/O failure without changing level. Preserve immutable generations and avoid stale/absent substitutions
3. Co-op capture must stop repeatedly validating/encoding/checksumming dormant worlds on the game thread. Retain owned immutable generations, finish assembling/checksumming on the worker, and preserve exact save/restore bytes

Plan:

- Implement recoverable publication and integrate recovery into launcher discovery, native restore and subsequent writes
- Implement companion-cache retries and failure-injection coverage
- Cache immutable campaign generations at mutation boundaries and defer archive assembly to the worker
- Extend host and Android integration tests; verify both engines, actual secret-level restoration, queue/order behavior and S21 archive timings
- Run scoped formatting, builds, selected tests and automation catalogs; audit all three requirements against final evidence

Status: complete. All three fixes are implemented and verified. Unrelated code-management edits belong to concurrent work and were preserved

Implementation:

- Paired publication records old primary/companion presence in a flushed pending journal before moving either old file. Renaming the journal to committed marks publication; cleanup and rollback are repeatable after interruption. A per-slot filesystem lock also covers recovery through the separately linked launcher/engine libraries. Both worker and explicit synchronous saves use this protocol, with staged-file and parent-directory flushing on Android
- Launcher directory discovery recovers journaled slots before enumerating saves, including missing primary files. Native slot lookup and direct restore recover before opening files. Unrecognized backups without this protocol's journal remain untouched; there is no migration for the previous disposable pre-release format
- An invalid companion cache is refreshed when the existing periodic scheduler retries submission. Healthy captures keep immutable cached bytes; an unreadable file is never treated as a valid absent companion
- Campaign mutation boundaries validate and encode one immutable generation. Recurring captures retain its ownership and offset without reserving/copying the full archive. The worker inserts the archive and computes the outer checksum before publication. The resulting save format is unchanged; explicit synchronous saves still assemble a complete save before returning

Verification completed:

- Scoped mixed formatter/linter passed. Final ARM64/x86_64 Android diagnostic builds and Windows D1/D2 builds passed
- Windows CTest: final 8 selected save/checkpoint/format/compatibility tests passed per engine; total times were 3.80 seconds for D1 and 137.21 seconds for D2, including upstream compatibility
- ASan/UBSan: 5,376 fake-filesystem publication/recovery interruption cases passed. Every recovered state is a coherent old or new pair and accepts a subsequent save. Real filesystem tests also recover pending and committed transactions, discover a missing `.sg4` primary, preserve failed-publication data, and successfully publish the next companion-absent generation
- S21 single-player integration on final builds: native D1 43/43 steps (run `5d013beea0034bdbb044ad1c41bc37b0`); native D2 58/58 (run `274b60c1a7ff44f097d30a0db2442d98`). D2 exercises damaged companion repair without a context change, unreadable recovery-record rejection, interrupted pair recovery through native restore, restored secret-world contents, slot rotation, companion absence and asynchronous/synchronous ordering
- Production worker probe under ASan/UBSan and on S21: open, length, short-read and close failures all recover on the next submission; three queued jobs retain different archive generations across cache replacement/clear; full queue rejection is safe; growing buffers preserve exact bytes/footer/trailer/checksum through 15 MiB. Production worker, queue, rewind buffer, publisher and checksum sources are used unchanged with controlled game/PhysFS dependencies
- Automation catalog passed (95 standalone JSON, 382 support scripts, 219 standalone PowerShell); suite catalog passed (318 top-level entries, 382 support scripts)

S21 archive component measurements (NDK r30 ARM64 `-O2`, 3 warmups and 31 measurements per size):

| Synthetic dormant worlds | Archive bytes | Capture median | Capture p95 | Capture max |
| --- | ---: | ---: | ---: | ---: |
| 0 | 0 | 1 us | 1 us | 1 us |
| 1 | 1,310,720 | 4 us | 8 us | 14 us |
| 3 | 3,932,160 | 6 us | 9 us | 10 us |
| 7 | 9,175,040 | 2 us | 2 us | 3 us |
| 12 | 15,728,640 | 1 us | 2 us | 2 us |

These isolate production submission/archive retention using a small synthetic live-state serializer, not full game frames. They demonstrate removal of archive-size-dependent capture work; the earlier 15 MiB encoder/checksum component cost was 48.619 ms on the S21. Encoding at campaign mutation boundaries and worker copying/checksumming still cost time. Raw artifacts and the reproducible probe are under `temp/save-review-fixes-20261009/`

During the live native-D2 test, a fresh post-restore S21 snapshot reported 23 completed jobs, zero failures, a 1,212,380-byte cached archive and a 20.258 ms latest/maximum capture. This is one integration-run observation under transfer/logging load, not a controlled frame benchmark. The optimization removes the measured repeated archive work; it does not establish that every complete native-D2 save capture fits a 16.7 ms frame

Co-op integration and final reader audit:

- The first S21-host native-D2 secret travel test timed out at the existing 90-second prepared-campaign deadline. Logs show the 1,213,780-byte source checkpoint progressing through all 2,810 chunks and reaching the peer, so this was a transfer deadline failure before the archive checks, not a save-worker failure
- Extended that phase to 300 seconds and the paired travel runner to 600 seconds. An attempted emulator-host run failed the LAN reachability preflight (the S21 cannot route to the emulator's private address), before gameplay. The current run uses the reachable S21-host arrangement
- Full native-D2 S21-host/emulator-client `test_lan.ps1 -InitialLevel 8 -SecretSaveRestore -AllowSecretWarps -NoCoopQol` passed to completion. Both devices passed the added archive fixture after entering the secret mine and returning to the base: async memory/disk bytes equal synchronous saves and recurring captures do not rebuild the campaign generation. The scenario also saved/restored both players' distinct inventories and the dormant base, then verified the actual base-world return. Evidence: `coop-archive-3.txt` ends with `LAN MP TEST PASSED`, exit 0
- The final reader audit found a legacy menu caller that ignores filename-builder failures. Failed recovery now clears the returned path, launcher discovery omits unrecoverable slots, and periodic slot metadata lookup attempts recovery. Malformed-journal regression coverage, both Android architectures, both Windows engines and the final S21 single-player runs passed

Completion audit:

1. Interrupted-save recovery: journal/commit protocol is shared by synchronous and worker publication; native lookup, direct restore and launcher discovery recover before reading. Interruption matrix, combined publication/rollback failure, malformed journal, real filesystem discovery and on-device restore all pass; the next save succeeds after recovery
2. Companion retries: all four read failure modes recover on the next submission in the production-worker probe; the S21 periodic scheduler fixture repairs a bad source without a context change, saves a complete pair and restores the saved secret world
3. Archive work: all campaign mutation sites refresh immutable generations; worker insertion and checksum preserve exact bytes. Ownership/queue tests pass through 15 MiB under sanitizers and on S21; both co-op peers compare memory/disk bytes after entry and return, and the full campaign restore scenario passes

Diagnostic preferences and original stay-awake values are restored after testing, with preference file hashes and settings verified. Both diagnostic apps are stopped, the devices are on Home, and the temporary S21 benchmark is removed. The production package was not changed

Limits: interruption tests model process termination and failed filesystem operations, not physical power removal. Files and parent directories are flushed on Android, but no claim is made about arbitrary storage-controller failure. ThreadSanitizer remains unavailable in this WSL environment from the earlier review; ASan/UBSan and controlled ownership tests are the concurrency evidence
