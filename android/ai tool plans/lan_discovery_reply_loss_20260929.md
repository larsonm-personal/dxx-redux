# LAN discovery and manual IP reply loss

Evidence from debuglog_20260929_212404.txt (host 192.168.88.163) and
debuglog_20260929_212304.txt (client 192.168.88.21), build 23521 / 7563cd0f

- Host sends 21 broadcasts, every three seconds; client receives none
- Some client broadcast queries reach the host, which replies in 0-5 ms
- All four manual IP queries reach the host and receive immediate replies, but
  none reach the client during its one-second probe
- The first received host packet is a unicast query reply at client 21:25:22.169
  and the lobby is published two milliseconds later, about 20 seconds after the
  manual attempt began
- No incompatible-version reply, transport exception, or slow handler is logged
- Earlier manual IP behavior fell through to the engine after the short lobby
  probe; the new verified-join flow turns the same short deadline into failure
- These logs localize missing packets to delivery before the client receive loop
  but cannot identify Wi-Fi, routing, or device filtering as the underlying cause

Implementation and validation

- [x] Repeat host announcements by unicast to recently heard discovery clients,
      with bounded, expiring state, so discovery can recover when host broadcasts and
      initial query replies are lost
- [x] Allow a cancellable 30-second manual join, retain short resume probes, and
      return promptly for a verified running engine after the launcher preference window
- [x] Report no response separately from an actual compatibility rejection
- [x] Extend real UDP tests for lost replies, delayed manual replies, cancellation,
      peer expiry/cleanup, and prompt engine success
- [x] Run scoped quality checks, Android builds and relevant emulator integration tests

Physical Wi-Fi delivery must still be confirmed on the affected phones

Validation

- Scoped code quality passed
- Debug app and instrumentation APKs built, reusing unchanged native libraries
- 19 focused lobby/launch unit tests passed
- Extended real UDP instrumentation passed: unicast announcements without further
  queries, peer expiry, a launcher reply after 1.5 seconds, cancellation before a
  late reply, and the existing save-latency and launch-commit cases
- Build log: `android/temp/lan_reply_loss_build.log`
- UDP test log: `android/temp/lan_reply_loss_udp.log`
- Live D1 and D2 engine probes passed: loss/malformed/wrong-source replies,
  launcher preference, prompt success without waiting for the other engine,
  version rejection, silence and cancellation
- Both engines passed full two-emulator direct IP joins and rejoins with no
  launcher announcement service (`android/temp/lan_reply_loss_engine_d1.log`
  and `android/temp/lan_reply_loss_engine_d2.log`)
- Two-emulator D1-in-D2 launcher discovery/join/readiness and concurrent launch
  preparation passed (`android/temp/lan_reply_loss_launcher.log`)
- APKs used for validation are under `android/app/build/intermediates/apk/`,
  which is the current Gradle output; `outputs/apk/debug/` contained an old APK
