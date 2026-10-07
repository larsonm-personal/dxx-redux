# Touch input capture

Preserve mouse-look behavior while diagnosing quantization using exported game logs only, without ADB

- [x] Add an opt-in, 30-second capture starting at the first mouse-look gesture per game Activity
- [x] Buffer timestamped touch history, drag/acceleration/drain values, native conversion and final engine commands
- [x] Bound memory/output, report dropped records, flush periodically and on pause into the existing exportable logs
- [x] Verify capture lifecycle and limits with host tests, build both Android engines, and run scoped formatting

Capture uses monotonic uptime milliseconds, sequence numbers and an explicit start/end record. Historical touch samples are observational only; do not feed them into the existing control calculation. Native records use CLOCK_MONOTONIC on the same time base. Keep native hooks Android-only in both games

User flow: Advanced > Debug Logging Categories > Touch Input Capture, launch a game, make normal aiming corrections for 30 seconds, return to the launcher and export the newest debug log. The switch arms one capture per game Activity; disable it after collecting the sample

## Capture format and limits

- Every record has `[touch-capture]`, a capture ID, sequence number and monotonic `t_ms`; native records also include `native_ms` at the emission point
- `begin` includes mouse settings and geometry; `motion` includes original event times and historical samples; `drag` includes the actual processing time, delta, acceleration history, multiplier and pending output
- `drain` includes emitted, remaining, edge and clamped output; `jni`, `publish`, `dispatch`, `quantize`, `axis` and `binding` expose the native conversion path and mailbox generations
- `controls` identifies intermediate input-event results; `consume` records the commands actually used by physics, with game time and frame duration. Native command/time values use the engine's 16.16 fixed-point units
- A background writer flushes once per second. Each batch is capped at 1 MiB of characters and the capture at 120,000 accepted records or 30 seconds. `gap` records identify lost-record counts and time ranges; `end` includes totals and the stop reason
- Pause and Activity.finish() synchronously flush the tail. The finish override is necessary because native shutdown kills the game process immediately after finish() returns
- No per-sample disk flushes or capture logcat output. The log's wall timestamp identifies each batch; source timestamps inside the records identify sample times
- Acceleration, sensitivity, event consumption, clamping and control precision are unchanged

## Validation

- Android arm64 debug APK assembled with both D1 and D2 engines
- 22 selected JVM tests pass: capture lifecycle/concurrent producers/expiry/overflow, category labels, existing acceleration and edge movement behavior
- Windows D1 and D2 builds pass
- Scoped mixed-language formatting and whitespace checks pass; no new warnings in the changed native files
- No ADB or device interaction used. Phone capture and export still need the user's normal-play session
