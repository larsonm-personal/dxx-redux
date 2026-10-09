# Proxy failure diagnostics tests

Run from `android/` with JDK 21:

```powershell
./gradlew.bat :proxy-tests:test
./gradlew.bat :proxy-tests:test -PcompareProxyHistory=true
```

On Linux use `./gradlew` instead

The normal test is included in `tests/test_gradle_unit_tests.ps1`. The optional
comparison requires Git object `b2e5c617b42df337d4d6d678a4fff935a5593b56`, the parent
of the September 28 proxy change. It compiles that exact historical source under
a different package; it does not emulate the old exception handling

Tests compile the production proxy and diagnostics with host-only Android log
sinks. They use real loopback UDP sockets and inject one plain `IOException` on
the first keepalive send. No device, Wi-Fi interruption, or production injection
switch is involved. Each keepalive test takes about 15 seconds and has a 30-second
deadline. Port 42424 must be free on the test host

The comparison uses shared-socket mode so the real socket can be substituted
without a production fault-injection hook. It confirms that the old implementation
also loses outbound forwarding after the injected keepalive error, while leaving
its shared receiver and sockets open. The current implementation closes them.
This test does not establish the cause of the original device send error

With Network logging enabled, exports now include `[PROXY]` creation, shutdown
callers, socket identities/endpoints, operation failures with full exception
causes, and successful gameplay/keepalive send and receive counts and ages.
An age of `-1` means no success has been observed. Repeated failures of the same
operation are limited to one exported trace per five seconds
