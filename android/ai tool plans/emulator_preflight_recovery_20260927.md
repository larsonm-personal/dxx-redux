# Emulator suite preflight recovery

- Remove the unconditional ADB reconnect after successful stale-state inspection
- Allow an existing emulator time to reconnect before recycling only its serial
- Recover online emulators that fail boot readiness, preserving the existing scoped shutdown safeguards
- Verify transient disconnect, stuck process, failed shutdown and boot failure paths with controlled tests
- Exercise reconnect recovery on the live primary emulator and run scoped code quality checks

## Results

The healthy-then-failed sequence was caused by preflight's unconditional ADB
reconnect racing the immediate online check, not evidence of a zombie emulator
after idle time. Removed that reconnect. Existing offline processes now get 30
seconds to reconnect before serial-scoped graceful shutdown/forced termination
and relaunch. Failed shutdown prevents a duplicate launch

Six controlled startup cases passed, along with the existing launcher preflight
tests. Three live reconnects reproduced the transient offline path and recovered
without changing the emulator boot ID. Package and app-private storage readiness
passed afterward. Scoped formatting/lint passed. The new host test is included in
the suite's no-infrastructure tier
