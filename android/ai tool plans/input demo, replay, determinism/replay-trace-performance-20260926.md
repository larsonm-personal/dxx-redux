# Replay trace performance

The no-render D1 replay previously measured 394 fps without traces and 18 fps
with full state/RNG diagnostics. Preserve every diagnostic record and SIM RNG
behavior while reducing capture cost.

1. Profile current native D1 and imported D1 captures, separating JSON snapshot
   construction, serialization, compression/write, and flush time
2. Optimize the measured dominant costs without reducing trace coverage or
   changing replay semantics; preserve progress visibility and error reporting
3. Build both engines, run focused tests, and repeat the same full captures
4. Compare decoded state traces, RNG traces, and terminal results byte for byte
   against each engine's baseline, and report measured speed and limitations

Evidence lives in temp/replay-trace-performance. The existing long corpus uses
its frozen binaries and helper; do not alter it. Launcher/music edits belong to
separate work and must remain untouched.

## Findings and implementation

Temporary scoped timers showed that compression/writing and per-record flushing
were small costs: native D1 spent 2.67 and 1.31 seconds respectively. World JSON
construction, comparison, copying and destruction dominated. A narrower profile
attributed 53 seconds to world-field construction outside the AI and segment
builders, despite those other fields containing little data.

The ordered JSON object's backing vector holds const-key pairs. Growing the
vector copies existing values, including their large nested snapshots. Reserve
capacity before inserting the AI/world fields, sparse AI locals, object-slot
deltas and world-field deltas. This is five reserve calls in shared diagnostic
code; game simulation, RNG, trace schema, field order, flushing and failure
handling are unchanged.

An intermediate experiment retained encoded world fields instead of JSON trees.
It passed exact comparisons but only reduced native elapsed time from 137 to
121 seconds. It was reverted in favor of reserving capacity with the existing
serializer. Temporary profiling hooks were also removed from production code.

## Measurements

Same 2,006-frame level-15 recording, accelerated full executable with rendering
disabled, current Windows builds. Baselines include lightweight profiling timers;
final captures do not. Elapsed time includes engine startup but excludes wrapper
setup. Concurrent workspace activity means these are individual measurements,
not guaranteed throughput or a console-runner comparison.

| Capture | Baseline | Optimized | Improvement |
| --- | ---: | ---: | ---: |
| Native D1, full traces | 137.427 s / 14.60 fps | 62.179 s / 32.26 fps | 2.21x |
| D1-in-D2, full traces | 172.129 s / 11.65 fps | 80.577 s / 24.90 fps | 2.14x |

Untraced controls completed in 5.097 seconds (393.60 fps) for native D1 and
24.190 seconds (82.93 fps) for imported D1, including their different startup
costs. Both terminal results match their fully traced captures byte for byte.
Full diagnostic capture remains substantially more expensive than simulation
without tracing; no 500-1,000 fps full-trace claim is made.

## Verification

- Both Windows game binaries and relevant native test targets built successfully
- Native D1 capture: all 742,608,842 decoded state-trace bytes, RNG trace and
  terminal result match its baseline exactly
- Imported D1 capture: all 772,206,794 decoded state-trace bytes, RNG trace and
  terminal result match its baseline exactly
- Reusable integration: android/tests/test_replay_trace_equivalence.py accepts
  --game d1 or d2 (imported D1), --exe, --demo, --data, --reference and --output
  It uses a fresh capture directory, records binary/demo hashes and timing, and
  fails if any decoded state, RNG or terminal bytes differ
- Scoped formatting and BOM checks passed; Python syntax compilation passed
- Native D1 upstream/replay CTests: 7/7 passed
- Imported/D2 upstream/replay CTests: 7/7 passed
- Both Android game libraries compiled for arm64-v8a, armeabi-v7a and x86_64
  using the existing CMake build directories, with no new compiler warnings
- The initial Gradle native-build invocation stopped before compilation because
  its workspace-cleanup guard waited 60 seconds for the concurrent host tests
  Direct CMake builds completed after those tests; no device runtime benchmark
  or APK installation was performed for this diagnostic-only change

Primary evidence: temp/replay-trace-performance/baseline-d1 and baseline-d2,
reserve-d1/report.json, reserve-d2/report.json, untraced-d1.log and untraced-d2.log
The discarded experiment and scoped profiling evidence are retained separately
under world-d1, profile2-d1, profile2.json and the profiled source snapshots
