# Large Level Metadata Progress, Timeout, and Performance

## Goal

Make metadata analysis usable for extremely large levels such as `uneasy4.zip` by reporting meaningful progress, extending the deadline while analysis continues to make sufficient forward progress, and reducing avoidable analysis cost without changing metadata results.

## Phase 1: Trace and measure

- [x] Locate the launcher, JNI, and native metadata analysis control flow
- [x] Identify the current timeout boundary and available units of completed work
- [x] Reproduce or benchmark the `uneasy4.zip` analysis path where local data permits
- [x] Record the dominant costs and select low-risk optimizations

## Phase 2: Progress and adaptive timeout

- [x] Add native progress reporting with a stable total-work definition
- [x] Surface progress in the launcher as a determinate progress bar and percentage
- [x] Replace the fixed deadline with a forward-progress policy, targeting at least 5 additional percentage points per 10 seconds
- [x] Preserve cancellation, failure reporting, lifecycle safety, and smaller-level behavior

## Phase 3: Performance

- [x] Implement measured optimizations in the shared/native analysis path
- [x] Keep D1 and D2 behavior consistent and metadata output unchanged
- [x] Add diagnostics sufficient to distinguish slow progress from a stalled analysis

## Phase 4: Regression coverage and verification

- [x] Add or extend high-level tests for progress, adaptive timeout, stall timeout, and output parity
- [x] Run scoped code formatting and lint checks for touched files
- [x] Run the relevant host/unit/integration tests
- [x] Run the required CMake build and test suite
- [x] Update this plan with results and any remaining device-only validation

## Results

- The original launcher checkpoint advanced only between levels, so one very large level appeared stuck at 0%. Native analysis now reports task-local completed/total work for secret scanning, topology, summaries, route planning, switch firing paths, and objective visibility. JNI throttles checkpoint writes while preserving meaningful percentage changes.
- The original timeout was a fixed 30 seconds. The launcher now keeps that initial deadline, grants each new task a 10-second window, and extends the deadline only when the current task earns at least 5 additional percentage points per 10 seconds. A stalled or too-slow task still expires.
- Route visibility now tries cheap segment samples before expensive face, vertex, and edge samples, stops when Dijkstra distance proves later segments cannot improve the result, and caches aggregate sample outcomes within one plan. The traced Uneasy4 workload fell from 1,566,643 to 863,768 wall-shot requests, a 44.9% reduction.
- On the final formatted D2 host build, three Uneasy4 runs took 3,875 ms, 3,603 ms, and 4,012 ms, averaging 3,830 ms versus the 5,415 ms baseline, a 29.3% improvement. Every generated JSON document was semantically identical to baseline and remained 10,964 bytes.
- Focused Kotlin progress/deadline tests passed. The D1 and D2 route snapshot and level metadata scan executables passed. Both Windows CMake builds passed. The complete Android debug unit test suite and APK build passed for arm64-v8a, armeabi-v7a, and x86_64.
- Emulator automation imported `Uneasy4.zip`, analyzed its metadata, and passed all 8 steps without timeout. Final UI smoothness and thermal behavior should still be checked on the target flagship phone because the available emulator cannot reproduce that hardware.
