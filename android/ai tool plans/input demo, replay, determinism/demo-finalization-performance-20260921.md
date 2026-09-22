# Demo finalization and level 7 slowdown

- Inspect recording finalization and quantify recorded frame times in the supplied level 7 demo
- Measure host finalization/replay costs and identify expensive work before changing behavior
- Rework the shared recording path to reduce stop latency while preserving demo contents and replay semantics
- Investigate boss-room slowdown using recorded timings and accelerated replay evidence
- Extend integration coverage, run relevant Windows builds/tests, and run scoped code quality
- Document measured results and any remaining device validation

## Findings and implementation

- The supplied demo has 2,253 frames, 137.188 seconds of recorded time, 77,780,118 bytes of demo JSON and 2,028,865 bytes of RNG trace
- Finalization used to build a second frame collection, serialize the entire demo into discarded validation text, then serialize it again to write it. Every pass reparsed and rebuilt the large diagnostic objects (360 keys per frame in this recording)
- The recorder now encodes each frame as it arrives. Diagnostic JSON is appended directly from the engine encoder, and staged/late events are appended from the canonical event encoder. Finalization writes the prepared lines between the existing header/checkpoint and result formats, without a second complete demo allocation or JSON parsing pass
- Rewind resizes the prepared frames alongside control history. Per-frame flags track diagnostics and events, including late additions, so truncation still selects the correct format version. Failed output opens leave the recording available for retry; file close failures are checked
- No diagnostic fields, RNG events, input timings, or checkpoint contents were removed. Checkpoint compression and RNG sidecar writing still run at stop
- The five-minute integration fixture records 3,000 frames at approximately 10 FPS with complete diagnostics. It reads them all back and checks per-frame contents, input timing coalescing, ordered staged/late events, rewind replacement and retry after a failed write

## Boss-room evidence

Recorded effective FPS by ten-second interval:

| Start (seconds) | FPS |
| --- | --- |
| 0 | 24.9 |
| 40 | 16.9 |
| 60 | 11.2 |
| 70 | 13.2 |
| 80 | 7.1 |
| 90 | 9.8 |
| 100 | 5.7 |
| 110 | 12.7 |
| 120 | 14.9 |
| 130 | 24.5 |

The median recorded frame is 41 ms; the maximum is 321.014 ms at frame 1349 (62.52 seconds). The long frame times are part of the inputs. Real-time replay deliberately waits for them in `input_demo_delay_replay_frame_shared`; changing them would change simulation behavior

Native D1 accelerated replay on Windows completed all 2,253 frames in 9.008 seconds without presenting and 10.556 seconds with the default renderer at 640x480, including loading and teardown. Temporary SDL tick instrumentation around simulation and rendering measured 2,252 frames (the terminal frame closes before the timing write): 473 ms total simulation, 870 ms render calls, 1 ms simulation p99 and 2 ms render p99. During recorded seconds 100-110 the largest combined measured frame was 2 ms. The isolated 73 ms simulation outlier was at frame 222, before the slowdown interval

These are host wall-clock CPU timings at millisecond resolution, not GPU timings. Presentation/event dispatch is outside the individual buckets and included in the runner's total elapsed time. Temporary instrumentation was removed; its patch and CSV remain under `temp/demo_perf_instrumentation.patch` and `temp/demo_perf_render.csv`

The terminal state matched the recording except `level_summary.endlevel_completed` (recording false, replay true). This run used capture-only comparison and does not establish full deterministic parity. The slowdown is demonstrably embedded, and the PC did not reproduce sustained expensive boss-room frames. Determining why the phone was slow still needs its profiling/slowdown log or a connected device; `adb devices` was empty

## Validation

- Initial native D1 benchmark: finalization 27.709 s before, 0.256 s after; the baseline overlapped a build, so this is not a controlled speedup ratio
- Separate D2 baseline: capture 2.693 s, finalization 8.218 s
- Subsequent native D1 benchmark with rewind and failure/retry coverage: capture 3.081 s, finalization 0.228 s
- Scoped code quality passed
- Final D1 benchmark: capture 3.103 s, finalization 0.181 s
- Final D2 benchmark: capture 3.012 s, finalization 0.237 s (about 35x less stop latency than the separate D2 baseline)
- `run-windows-build.ps1 -Target both` passed without new compiler warnings
- Both engines passed all six selected CTest cases: `test_input_demo_controls`, `test_input_demo_fixture`, `test_input_demo_recorder`, `test_input_demo_result`, `test_input_demo_replay`, and `test_input_demo_rng_mode`
- Logs: `temp/demo_perf_build_final.log`, `temp/demo_perf_ctest_d1.log`, `temp/demo_perf_ctest_d2.log`, `temp/demo_perf_quality.log`
- No Android device was connected, so phone stop latency and the original phone-side boss-room bottleneck remain unmeasured
