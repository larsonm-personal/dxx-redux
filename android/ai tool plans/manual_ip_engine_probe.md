# Manual IP engine verification

Implement parallel launcher discovery and read-only native engine information
queries. Prefer a fresh launcher lobby response; after the lobby query window,
allow engine-only joining only with a validated compatible response. Identify D1
and D2 independently, retain advertised non-default ports, and report timeout or
version mismatch without launching. Do not occupy a player slot

Keep protocol encoding and decoding in native code compiled against each engine's
headers. Use isolated UDP sockets, bounded retries, cancellation and sender checks
in Kotlin. Preserve resume identity filtering and existing advertised-game joins

Verify native builds for both engines/all APK ABIs, scoped formatting, socket
fixtures for loss/malformed/mismatch/cancellation, and two-emulator manual IP
joins against actual D1 and D1-in-D2 hosts with launcher discovery disabled

Completed September 29

- Manual IP now runs D1/D2 versioned engine information queries in parallel with
  launcher queries, preferring a fresh launcher lobby and retaining advertised
  game ports and metadata when they agree with the verified engine
- Native JNI codecs compile against each engine's headers. Independent connected
  UDP sockets distinguish the engines, reject other source ports, retry loss, and
  close on cancellation. No admission request or player slot is used
- An engine response can launch the game even without launcher discovery. Silence,
  malformed packets, version rejection and unavailable engine states stay in the
  launcher with diagnostics. Resume's identity-filtered lookup remains separate

Validation

- Full debug APK and instrumentation APK build passed, including both native
  engines for arm64-v8a, armeabi-v7a and x86_64
  (`android/temp/engine_probe_build.log`)
- Scoped mixed-language formatter/lint passed (`android/temp/engine_probe_quality.log`)
- `tests/test_manual_ip_engine.ps1 -Game d1` and `-Game d2` passed against two
  emulators. Hosts ran directly without JSON lobby discovery. Actual engine replies
  were captured and replayed for loss, truncation, wrong packet type/source port,
  lobby preference, engine-only fallback, version rejection, silence and cancellation
- Hosts retained one player throughout probing. Manual IP then joined the real
  engine lobby, both peers entered gameplay, client was force-stopped, and manual
  IP rejoined the still-running game using fresh introspection output
  (`android/temp/manual_ip_d1_test.log`, `android/temp/manual_ip_d2_test.log`)
- Existing lobby latency/recovery instrumentation passed
  (`android/temp/engine_probe_lobby_regression.log`)
- Test corrections: clear old introspection before each new client launch; omit
  D2's mission identifier when hosting native D1's built-in campaign

APK: `android/app/build/outputs/apk/debug/app-debug.apk`
