# Touch input capture

Preserve mouse-look behavior while diagnosing quantization using exported game logs only, without ADB

- [ ] Add an opt-in, 30-second capture starting at the first mouse-look gesture per game Activity
- [ ] Buffer timestamped touch history, drag/acceleration/drain values, native conversion and final engine commands
- [ ] Bound memory/output, report dropped records, flush periodically and on pause into the existing exportable logs
- [ ] Verify capture lifecycle and limits with host tests, build both Android engines, and run scoped formatting

Capture uses monotonic uptime milliseconds, sequence numbers and an explicit start/end record. Historical touch samples are observational only; do not feed them into the existing control calculation. Native records use CLOCK_MONOTONIC on the same time base. Keep native hooks Android-only in both games

User flow: Advanced > Debug Logging Categories > Touch Input Capture, launch a game, make normal aiming corrections for 30 seconds, return to the launcher and export the newest debug log. The switch arms one capture per game Activity; disable it after collecting the sample
