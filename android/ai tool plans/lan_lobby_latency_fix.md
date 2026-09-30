# LAN lobby latency and launch regression fix

Evidence: September 29 two-phone diagnostic logs show host QUERY handling taking
1.1-2.2 seconds with save slot 6 selected, versus 0-2 ms once in-game. Save checks
scan the mission catalog in packet handlers. Host launch preparation takes 4.2 s
before START is sent; client preparation then takes another 2.3 s

Requirements

1. Remove save scanning from discovery and membership packet handlers. Compute
   warning state asynchronously for each hosted lobby and publish a cached result
   without allowing an old lobby's result to overwrite a new lobby. Preserve fresh
   validation during host launch preparation, outside the transport lock
2. Start client preparation alongside the host, but wait for host Activity launch
   confirmation before entering the engine. Correlate prepare/commit/abort messages
   by launch attempt, retry over UDP, reject duplicates/stale or wrong-host packets,
   and retain cancellation, timeout and retry behavior
3. Retry manual IP probes within the existing one-second window, report a failure
   in the launcher instead of automatically entering the engine on timeout
4. Keep the diagnostic packet logs and add preparation timing to exported logs
5. Verify real UDP discovery and join responsiveness with a slow save validator,
   launch preparation overlap and confirmation/abort handling, loss/reorder/duplicates,
   manual-IP retry and timeout, existing lobby tests and relevant device tests

Validation: scoped formatter, Kotlin/APK builds (reuse unchanged native libraries),
unit tests and Android instrumentation, plus a two-emulator lobby launch check
where practicable. Physical Wi-Fi broadcast loss remains unproven; do not claim
that removing the measured application stall fixes every missing radio packet

Completed September 29

- Hosted save checks now run asynchronously and publish cached warnings. Packet
  handlers do not scan missions; save changes trigger refresh, old results are
  discarded, and host launch performs a fresh check outside the transport lock
- PREPARE starts both launchers' work; START commits after host Activity launch,
  START_CANCEL aborts. Attempt IDs reject duplicates and reordered old packets;
  repeated sends and membership heartbeats recover missing decisions
- Manual IP retries queries within one second and reports silence in the launcher
  instead of automatically entering the engine. Exported logs include preparation
  steps and elapsed times

Verification

- Scoped formatter/lint: passed (`android/temp/lobby_fix_quality.log`)
- Debug APK, instrumentation APK and 18 focused unit tests: passed
  (`android/temp/lobby_fix_build.log`). Native libraries were reused; no native
  source was changed for this fix
- Real UDP instrumentation: three QUERY replies each below 750 ms and JOIN_ACK
  while a deliberately blocked save validator remained blocked; only one check
  ran. Prepare/abort/retry/commit, duplicate and reordered packets, wrong sender,
  heartbeat commit recovery, lost manual-IP query, timeout, and stale validation
  publication all passed (`android/temp/lobby_latency_test.log`)
- Existing multiplayer recovery and co-op host migration suites passed
  (`android/temp/lobby_recovery_test.log`, `android/temp/lobby_coop_session_test.log`)
- Save compatibility device fixture passed: invalid and unsupported saves blocked,
  supported versions accepted, Start fresh cleared the warning
  (`android/temp/lobby_save_compatibility_test.log`)
- Two-emulator D1-in-D2 launcher test passed: discovery, join and ready; client
  completed preparation in 1,877 ms while host launch was deliberately withheld;
  no client engine process before confirmation; both entered after confirmation,
  with exactly one client preparation (`android/temp/lobby_launch_preparation_test.log`
  and `android/temp/lobby_launch_emulator-*.log`)

Build: `android/app/build/outputs/apk/debug/app-debug.apk`. Install on both phones
to exercise concurrent preparation. Physical Wi-Fi behavior still needs a device
run; the diagnostic experiment and packet timing logs remain available
