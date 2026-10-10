# Remaining checkpoint and frame-gap stutters

Objective: instrument the remaining stutters, measure them on the attached S21,
choose fixes from evidence, implement them, and verify significant progress in
co-op and the shared single-player save path

Baseline: the user's build 24630 log contains 36 intervals over 50 ms in 224.6
seconds of measured gameplay. Thirty recorded worst frames coincide with live
checkpoint capture; five have 82-101 ms outside the measured frame body

Plan:

1. Add sparse per-stage wall/thread-CPU capture diagnostics and partition time
   outside the game frame, including profiling, presentation and event handling
2. Build the isolated diagnostic package; record/restore device preferences and
   keep the production package intact. Reproduce on S21 with a connected peer,
   compare idle and active gameplay, and exercise the single-player path
3. Implement evidence-supported fixes, preserving coherent owned snapshots,
   independent peer history, save format and restore correctness
4. Run scoped formatting, both engine builds, relevant host tests and on-device
   save/restore integration. Compare before/after timing under matched conditions
5. Record evidence, limitations and any remaining work before completion

Status: implementation and verification complete

## Findings and changes

- Restoring a checkpoint was essential to reproduce the expensive captures. A
  fresh level initially captured in roughly 4 ms. After a restore, captures grew
  to 56.428 ms, almost all in world serialization, with comparable thread CPU
  time. The debug allocator searches its allocation table when freeing memory;
  the serializer allocated and freed a temporary record for every object and AI
  entry. Restore/texture allocation churn made these operations expensive
- Both engines now serialize these small temporary records from the stack,
  retaining explicit zeroing and the existing on-disk representation. This
  shared serializer improvement applies to single-player and co-op saves
- Profiling batch formatting and flushes used to run on the game thread. One
  measured profiling tail took 41.932 ms wall time and 41.099 ms thread CPU.
  Profiling now uses the existing bounded background diagnostic writer, with
  eight queue entries and nonblocking rejection when full
- Android raw packet logging previously wrote netlog.txt despite Network logging
  being disabled. Both engines now gate packet/comment writes on that category
- New sparse diagnostics retain wall/thread CPU times for capture stages and
  divide frame gaps into profiling, presentation, readback, swap, introspection,
  lifecycle, automation, input and dispatch. Capture stages require Profiling;
  outer-stage records accompany the existing stutter recorder

## S21 measurements

Tests used the isolated com.dxxredux.app.nsdtest package on SM-G996U with an
emulator peer, D1-in-D2 level 6, following a co-op restore. No per-frame ADB
polling occurred during measurement. Thermal status was zero. Production app
data was not changed

| Run                                             | Memory capture median / maximum | Disk capture median / maximum |
| ----------------------------------------------- | ------------------------------- | ----------------------------- |
| Before fix, representative post-restore samples | 19.678 / 56.428 ms (19)         | 20.647 / 48.160 ms (3)        |
| Stack-record fix alone                          | 2.511 / 4.486 ms (19)           | 2.597 / 4.533 ms (3)          |
| All fixes, post-restore movement and firing     | 2.514 / 2.931 ms (20)           | 2.581 / 2.615 ms (3)          |

The final 100-second observation ran from 00:45:19.134 to 00:46:59.277 on
2026-10-10. Nine complete interior stutter windows covered 82.935 seconds and
4,965 frame intervals. Exactly one interval exceeded 50 ms: 53.319 ms, including
48.135 ms wall / 47.440 ms CPU in the automation completion/verification stage.
No interval exceeded 100 ms. Checkpoint completions advanced from 10 to 33 with
zero failures or pending jobs. Player position, orientation, energy and shields
confirmed that movement/firing occurred

The raw network log size and modification time stayed unchanged through the
Network-disabled restore and active observation: 17367660 bytes, mtime
1791618195

Enabling Network in a separate co-op smoke test produced a fresh packet file
that grew from 198224 to 241351 bytes between consecutive checks. The full
independent-checkpoint runner resets preferences, so its prior run was not used
as evidence for the enabled-category case

## Validation

- Windows builds: D1 and D2 passed
- Selected native tests: 9/9 per engine, 18 total, including upstream save
  compatibility, checkpoint files/slots, save metadata/sets, co-op save format,
  stutter detector, transfer policy and argument defaults
- ARM64 and x86_64 diagnostic Android builds passed
- Kotlin category tests and blocked-writer test passed. The latter verifies that
  profiling callers return while the writer is blocked, FIFO batch ordering is
  preserved, and disabled profiling does not write
- S21 plus emulator D1-in-D2 independent-checkpoint integration passed, including
  differing peer histories and repeated authoritative restore/transfer
- S21 D1 unified checkpoint integration passed 47/47 steps, including the added
  capture/roundtrip after restore churn
- S21 D2 unified checkpoint integration passed 62/62 steps, also covering secret
  level companions, interrupted publication, ordering and the added roundtrip
- Scoped mixed-language formatting and both automation catalog checks passed

Device cleanup restores the backed-up diagnostic preferences byte-for-byte and
the original stay-awake settings (S21: 0; emulator: 1), stops the diagnostic
package, and returns both devices to Home. Verification is in cleanup.json

## Limits and evidence

The supplied log came from SM-S931U, whereas these experiments used the S21.
Controlled movement/firing stayed near the level entrance; this was not a full
level combat playthrough. Captures remain synchronous but are much cheaper;
these results do not prove all original outside-frame stalls are eliminated.
Enabled non-profiling log categories can still write synchronously. A longer
real gameplay log with the new stage diagnostics can identify remaining causes

Artifacts, APKs, scripts and raw logs are under
temp/remaining-stutters-20261010/. Timing analyses must respect each observation's
window JSON because a debug file can contain earlier runs. Do not aggregate an
entire reused file as if it represented only the named observation
