# S21 production checkpoint measurements

## Plan

1. Add profiling-category completion records separating whole submission,
   snapshot capture, worker encoding/publication, and main-thread completion
2. Build/install the isolated ARM64 diagnostic app and run D1-in-D2 level 6
   in single player and with a connected co-op peer
3. Observe natural checkpoint cadence and frame-hitch windows without per-frame
   ADB polling; benchmark remaining synchronous disk saves separately
4. Record distributions, failures and device conditions; restore device settings
5. Audit single-player save transactions and identify reusable worker components

The measurements distinguish game-thread time from worker latency. A worker
completion time is not a frame stall. Test fixtures are stationary and cannot
prove that every stutter in active play has disappeared.

## Method and single-player results

SM-G996U (S21), Android 15, isolated `com.dxxredux.app.nsdtest`, ARM64 Debug
build. D1-in-D2, First Strike level 6, Trainee, stationary pilot. Profiling and
automatic slowdown capture enabled. No per-frame ADB polling during observation;
introspection and thermal snapshots bracket each observation interval.

An initial unprotected setup died under robot fire and is excluded. The repeated
single-player run enabled the engine's invulnerability cheat and observed 310
seconds, including a natural five-minute autosave. It completed 62 rewind jobs
with no failures, deferrals or sidecar failures. Checkpoints were about 1.22 MB.

Milliseconds, nearest-rank p95:

| Single-player rewind phase      | Median |    p95 | Maximum |
| ------------------------------- | -----: | -----: | ------: |
| Game-thread capture/preparation |  6.013 |  6.164 |   9.299 |
| Main-thread completion callback |  0.253 |  0.833 |   0.918 |
| Worker encoding                 | 18.771 | 25.302 |  41.583 |

Capture/preparation includes path/attachment preparation where applicable and
world capture, ending just before atomic queue publication and wakeup. Completion
includes the rewind history copy/allocation; it excludes the diagnostic log write
and slot release. These phases can occur in different frames. Worker encoding
includes its encoded-size validation. Worker duration is not a frame stall.

Thirty complete stutter windows within the observed checkpoint range cover
17,904 frame intervals, averaging 16.696 ms. There was one interval above the
50 ms detector threshold: 75.320 ms at the periodic disk save. No intervals
exceeded 100 ms. Its frame body was 74.000 ms: simulation 71.454 ms, rendering
2.220 ms, rewind 0.007 ms. The save file `player.sg4` was modified at
18:35:35.258924 PDT, and the hitch was logged at 18:35:35.296 PDT.

The separate synchronous save probe then ran eight samples per mode, rotating
mode order with one-second waits. Every save succeeded:

| Synchronous single-player path | Median | Maximum |
| ------------------------------ | -----: | ------: |
| Reused memory buffer           | 23.744 |  26.274 |
| Disk, blank thumbnail          | 39.317 |  51.838 |
| Disk, rendered thumbnail       | 64.644 |  73.044 |

The thumbnail stage itself had a 23.524 ms median. The remaining metadata size
walk was about 2.95 ms; world serialization was about 5 ms. Besides moving
encoding and disk writes, a shared backend could cache the fixed encoded size.
The disk probe tests primary-file serialization/validation/publication, not the
outer slot/secret-companion transaction. The natural autosave above exercises
that actual periodic slot path with no secret companion present. These are
current-build measurements, not a controlled old/new APK frame-time comparison.

## Connected co-op results

The S21 hosted the same mission/level/difficulty with `emulator-5586` connected
over direct LAN. Both pilots received the existing 2,000-shield idle-test seed;
no Guide-Bot was deployed. The observation lasted 190 seconds. The host remained
alive and had two connected players at both boundaries. It completed 38 rewind
jobs and seven natural 30-second disk saves, with no failures, deferrals, pending
jobs at either boundary, or sidecar publication failures.

| Co-op phase                                    | Median ms | p95 ms | Maximum ms |
| ---------------------------------------------- | --------: | -----: | ---------: |
| Rewind capture/preparation                     |     4.148 |  4.254 |      6.954 |
| Rewind completion callback                     |     0.261 |  0.904 |      0.959 |
| Rewind worker                                  |    19.742 | 49.785 |     51.991 |
| Disk capture/preparation                       |     4.151 |  4.265 |      4.265 |
| Disk encoding on worker                        |    17.018 | 39.774 |     39.774 |
| Disk publication on worker, including sidecars |    15.298 | 16.078 |     16.078 |
| Disk worker total                              |    31.086 | 55.852 |     55.852 |

Disk completion callbacks took at most 0.001 ms. With seven disk samples, the
nearest-rank p95 is the maximum; this is a small-sample diagnostic, not a latency
guarantee. Independent phase medians need not add to the total median.

Seventeen complete frame windows cover 10,190 intervals, averaging 16.695 ms,
with no intervals above 50 ms; the maximum was 38.180 ms. Thus the longest
background jobs did not turn into equally long frame stalls in this fixture.
Single-player and co-op differ in gameplay, scheduling and CPU operating state;
their capture-time difference should not be attributed to the mode alone.

Thermal status remained 0 at all four observation boundaries. This test does not
reproduce active combat/navigation, Wi-Fi congestion, host migration, or a save
with a large accumulated multi-level archive. The earlier integration tests
cover restore correctness; this run measures the production capture path.

## Evidence and validation

- `temp/checkpoint-runtime-20261009/summary.json` contains distributions and the
  single-player hitch's subsystem breakdown
- `single-protected.log` and `coop-natural.log` contain per-checkpoint records;
  matching `*-debug.txt` files contain frame windows. Complete windows are
  selected between the first and last observed checkpoint timestamps, avoiding
  partial boundary windows and earlier sessions in the same debug file
- `*-before.json`, `*-after.json`, and `*-thermal-*.txt` retain device/fixture
  state. The `*-window.json` timestamps use `/proc/uptime` (boot time), not the
  profiler's monotonic clock; analysis uses checkpoint monotonic timestamps
- `sync-single.json` retains all 24 successful synchronous samples and stage
  timings. The run's generated scripts and durable PASS records are retained
- `run_actions.py`, `observe.py`, `start_coop.py`, and `analyze.py` retain the
  exact setup, observation and analysis procedures. The co-op helper's device
  IDs and host LAN address are fixture-specific
- Both Android ARM64 engines built with the new sparse completion diagnostics;
  no new compiler warnings. The new records require the Profiling category
- Diagnostic games were stopped; the S21's profiling, automatic slowdown,
  skip-intro and stay-awake settings were returned to their previous behavior.
  The production app was not modified

The immediate follow-up is to move single-player periodic disk saves onto the
same owned-snapshot worker, with paired-file publication and thumbnail handling.
The measured residual 4-6 ms capture cost is also real; reaching a 1-2 ms total
game-thread budget would require moving more world serialization off-thread.

## Single-player applicability and sharing

- Rewind already uses the shared worker in single player and co-op, in both
  engines. The D2 metadata size-pass optimization also benefits synchronous saves
- Single-player periodic disk saves still call `state_android_save_to_slot` every
  300 game seconds. That path renders/readbacks a thumbnail, serializes the live
  world and metadata, writes/validates staged files, publishes the slot, and
  updates the launcher pointer on the game thread
- Native D1 needs one primary save file. D2 additionally associates each slot
  with an optional `secretc` companion holding the revisit state of a secret mine
  and must also represent companion absence, so an old companion is removed
- Reuse `state_checkpoint` for owned buffers, bounded queue, worker lifetime,
  metadata encoding, completion and diagnostics. Extend a disk job to an owned
  save bundle with a primary file and optional binary companion; keep scheduling
  and the decision to wait for durability at the caller
- Reuse `android_file_pair_publish` and its failure-injection tests through a
  worker-safe filesystem adapter. Its current game-thread adapter uses PHYSFS;
  workers should use captured absolute paths and independent file operations
- The companion must be frozen at capture time. Merely retaining its filename,
  or opening a descriptor while other code can truncate that file in place, is
  insufficient. An immutable cached companion generation, replaced when the
  secret mine is saved, can be retained by the worker without a disk read on
  each periodic capture
- Thumbnails need separate treatment: capture/reuse owned pixels on the render
  thread or give periodic checkpoints an explicit no-thumbnail policy. Moving
  file writes alone does not remove the synchronous GL readback
- Manual and lifecycle saves can share the same backend while preserving their
  completion/durability contract. Background submission must not acknowledge a
  lifecycle checkpoint as committed before its files have actually published
- If the remaining world-capture cost matters, extend the same detached-snapshot
  approach to the remaining world serializer. This needs a coherent field audit
  and engine-specific capture adapters, not a second worker or a blanket mutex
