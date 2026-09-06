# Coop process exit and in-game rejoin

1. [x] Correlate the two September 6 logs and trace the rejection
2. [x] Preserve reconnect signing identity and monotonic request counters across game process restarts
3. [x] Record Android historical game process exit reasons in exported logs
4. [x] Run scoped formatting, reconnect regression tests, and relevant builds

## Evidence

- Actual inputs are Downloads/debuglog_20260906_142859.txt (client) and debuglog_20260906_142906.txt (host)
- Client logging stops after the 14:54:08.262 coop autosave metadata write; at 14:54:10.221 the launcher reports no game process
- No fatal signal, stack trace, graceful game exit, or transport timeout explains this process loss
- Host accepts Player68 at 14:54:35.295 but immediately sends a dump; client receives reason 9 (DUMP_DUPNAME) at 14:54:35.655
- UdpReconnectIdentity generates a process-lifetime key, so a restarted client cannot authenticate as its previous slot
- Native request counters also start randomly, which would make persisted-key reconnect unreliable unless counters survive restart too

## Changes

- Store the installation signing key and replay counter under noBackupFilesDir/udp_reconnect, shared by D1 and D2
- Lock storage across processes and atomically replace the committed state; reserve counters durably before signing
- Keep callsign collision checks and signed route challenges intact
- On launcher resume, export Android 11+ historical game process exit reason, signal/status, timestamp, memory usage, and description
- No speculative crash fix: the client has no normal engine-return message, while the host later logs `jni startup main returned` at 14:55:01.960

## Validation

- Scoped mixed-language formatting passed; test Kotlin files also formatted directly because the helper only scans main/java
- Four multiplayer source-contract checks passed
- Current reconnect-authentication, initial-sync-retry, and save-transfer-policy tests built with CMake/MSVC Debug and all three passed
- Seven Kotlin reconnect tests passed, including identity reload, counter continuity, signature verification, and interrupted-write recovery
- Android storage uses same-directory rename, available on API 23; host tests inject the equivalent atomic replace operation because Windows File.renameTo refuses replacement
- Final `:app:testDebugUnitTest --tests 'com.dxxredux.app.multiplayer.UdpReconnect*Test' :app:assembleDebug` passed, packaging arm64-v8a, armeabi-v7a, and x86_64
- Windows D1 and D2 builds passed through run-windows-build.ps1
- No new compiler warnings in changed files; existing engine/dependency warnings remain
- Live two-device rejoin remains unverified; no Android devices were connected
- Start a fresh session with the updated build before testing, because an old session retains its process-only signing identity
