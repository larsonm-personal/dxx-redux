# Owned snapshots and asynchronous checkpoints

## Objective

Implement the measured detached-snapshot design for Android saves and rewind,
including independent periodic coop checkpoints and authoritative transfer at
restore time. Demonstrate immutable snapshots and stable repeated save/load
cycles with isolated codec tests and engine integration tests.

## Requirements and implementation sequence

1. Extract a reusable native navigation snapshot and pure encoder/decoder.
   Preserve the current portable field order. Test repeated load/save/load/save,
   frozen input ownership, invalid-input non-mutation and meaningful populated
   planner/workspace values, including floating point state.
2. Build a bounded persistent worker with explicit slot ownership. Capture live
   state only on the game thread. Encode detached metadata and perform disk I/O
   on the worker. Queue-full periodic work defers without waiting. Keep completed
   saves valid until atomic publication; failures preserve the prior checkpoint.
3. Integrate rewind capture, preserving snapshot timestamps, demo timelines,
   campaign/level identity and cancellation after restore or level changes.
4. Integrate periodic coop disk saves with captured launcher metadata and no thumbnail
   rendering. Consolidate coop timers, checkpoint independently on each peer,
   and retain authoritative transfer on load. Audit legacy slot-based fallback.
5. Test queue saturation/failure/lifetime, repeated save/load cycles, rewind,
   disk save/load and coop restore with intentionally different local histories.
   Register reusable integration runners and run catalog validation.
6. Build both games on Windows and Android and run applicable isolated tests and
   integration tests to completion. Retain the S21 baseline and snapshot timing
   evidence; rerun production capture timing there when the device is unlocked.

## Design constraints

- No worker reads or mutates live game globals
- Pending buffers cannot be reused before worker completion
- Native snapshots are internal; disk/network saves remain explicitly encoded
- Preserve desktop behavior and paired D1/D2 paths
- Distinguish exact codec immutability from gameplay changes caused by advancing
  frames; integration assertions must compare the captured state at a controlled
  restore boundary
- Experimental probes and existing unrelated worktree edits are preserved until
  a production replacement makes a specific probe redundant

## Evidence / progress

- Baseline inspected; existing experiments found 0.052-0.115 ms median metadata
  capture and 60/60 exact delayed encoding and decode/re-encode matches
- Native metadata codec extracted; 32 encode/decode/re-encode cycles, populated
  planner data, source immutability and rejected-input non-mutation pass in both
  Windows builds
- Three-slot producer/worker/collector ownership queue passes saturation,
  abandonment, FIFO and 20,000 concurrent wraparound jobs in both builds
- Staged file replacement passes publication and failure-preservation tests in
  both builds
- Rewind captures now use a persistent worker; history callbacks retain capture
  clocks and reject stale level/restore generations
- Periodic coop saves run independently on each peer; duplicate host timers and
  periodic save packets removed. Disk jobs own paths, launcher metadata and
  sidecar records. Explicit saves drain older jobs to preserve publication order
- Engine comparison found one wall-clock-relative cache-poll field changing
  between two separately paused saves. Holding a common outer pause produces
  byte-identical complete saves; three engine load/save rounds also preserve
  their input and restore captured inventory and position
- Android single-player integration passed 36/36 steps in each engine: exact
  same-epoch complete-save comparison, published disk bytes, actual disk restore,
  three repeated memory load/save cycles per capture, immutable restore input,
  and the normal rewind action restoring energy from background-captured history
- Independent coop checkpoint integration passed in native D1 level 6 and
  D1-in-D2 level 6. Both peers published their own valid saves, the selected slot
  deliberately contained different bytes, and two authoritative restores
  recovered both players' original energy, score and ammo
- Five selected tests pass in each Windows build: upstream compatibility,
  metadata codec, queue ownership, file publication and rewind policy
- Mixed asynchronous/synchronous history testing exposed the old 2 KiB history
  reader limit. Its buffers now hold the entire five-slot history; filling the
  ring on the worker and then saving synchronously preserves all five entries
- Consecutive automation runs now carry unique run IDs so a previous PASS cannot
  complete a later request. The checkpoint fixture waits for previous jobs to
  be collected before submitting another explicit test capture
- Cold host-swap integration passes: the former client restores its own local
  save, transfers it to the former host, and remaps Guide-Bot ownership from
  player slot 0 to slot 1 while preserving unexplored guidance intent
- ARM64 and x86_64 D1/D2 Android builds pass. Automation and suite catalogs pass
  with the new single-player test and reusable two-device checkpoint runner
- A D1 fixture initially assumed the built-in mission directory was `d1`;
  inspection showed `default`. The runner was corrected and passed on rerun
- During the D1-in-D2 coop run, the host recorded six completed checkpoints,
  no failures, 1.362 ms latest capture and 1.472 ms maximum capture. This is
  emulator evidence, not an S21 before/after performance result
- S21 production timing rerun is pending normal device unlock. Its diagnostic
  app contains the ARM64 build, its original stay-awake setting (0) is restored,
  and its production application was not changed

## Scope and follow-up

The worker handles frequent rewind captures and 30-second coop checkpoints.
Explicit manual/transition/lifecycle saves retain their synchronous completion
contract and drain older background writes before publishing. Single-player
periodic disk saves retain the existing D2 main/secret-companion transaction;
the cheaper metadata size pass also benefits that path. Moving that pair to the
worker requires coherent ownership of the companion file and a separate pair
publication test, rather than reading a mutable file after capture.

Restore transfer is mandatory for Android coop loads; a failed transfer does
not fall back to commanding peers to load their independently populated slots.
Primary-file publication failures preserve the prior checkpoint. Sidecar
publication is best effort after the primary file commits, with failures logged
separately rather than misreporting a committed save as failed.

## Reproduction

Use the isolated diagnostic package with the matching APK already installed:

```powershell
$env:DXX_TEST_PACKAGE = 'com.dxxredux.app.nsdtest'
./android/helpers/run_test.ps1 -ScriptName test_async_checkpoint_unified.jsonc -Game d1 -Serial emulator-5586 -TimeoutSeconds 360
./android/helpers/run_test.ps1 -ScriptName test_async_checkpoint_unified.jsonc -Game d2 -Serial emulator-5586 -TimeoutSeconds 360
./android/tests/test_independent_checkpoints.ps1 -HostDevice emulator-5584 -JoinDevice emulator-5586
./android/tests/test_lan.ps1 -Game d2 -HostDevice emulator-5584 -JoinDevice emulator-5586 -SkipBuild -GuidebotSlotRemapRestore
```

Run device tests serially. The independent-checkpoint wrapper defaults to native
D1 and D1-in-D2 level 6 and verifies two authoritative loads from unequal peer
histories. The Guide-Bot case also fills the background history, appends an
explicit save, restarts both processes with swapped slots, and checks that
ownership follows player identity.
