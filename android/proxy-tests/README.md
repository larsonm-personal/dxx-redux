# Proxy failure diagnostics tests

Run from `android/` with JDK 21:

```powershell
./gradlew.bat :proxy-tests:test
./gradlew.bat :proxy-tests:test -PcompareProxyHistory=true
```

On Linux use `./gradlew` instead

## Google matchmaking HTTPS integration

`GoogleLoginHttpsTest` runs the actual Android login helper against the Rust
authentication router and signed-token verifier, through an owned loopback HTTPS
terminator. It generates a temporary certificate with JDK `keytool`; the client
trusts only that fixture certificate and retains hostname verification. The Rust
signing/lookup helpers are compiled only into the test binary, not the server.
The test starts/stops its own processes and listeners and needs no Google account.
Without `googleFixtureBinary` it is explicitly skipped, not provider-qualified.

From the repository root, with JDK 21 configured:

```powershell
$artifacts = cargo test --locked --manifest-path server/Cargo.toml --target-dir server/target/security-auth --lib --no-run --message-format=json | ForEach-Object { $_ | ConvertFrom-Json }
if ($LASTEXITCODE -ne 0) { throw 'Rust fixture build failed' }
$fixture = $artifacts | Where-Object { $_.reason -eq 'compiler-artifact' -and $_.target.name -eq 'dxx_matchmaking' -and $_.executable } | Select-Object -Last 1 -ExpandProperty executable
if (-not $fixture) { throw 'Rust fixture executable missing' }
./android/gradlew.bat -p android :proxy-tests:test --tests com.dxxredux.app.multiplayer.GoogleLoginHttpsTest "-PgoogleFixtureBinary=$fixture"
```

Coverage includes signed login, nonce binding, credential lookup, logout and
challenge-based cleanup after a real issued response is deliberately dropped.
It does not exercise Google UI, Play Services, Android lifecycle events, the
deployed reverse proxy or real OAuth signing configuration.

## Proxy diagnostics coverage

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
its shared receiver and sockets open. The current implementation keeps both
forwarding directions working after a transient keepalive send failure.
This test does not establish the cause of the original device send error

With Network logging enabled, exports now include `[PROXY]` creation, shutdown
callers, socket identities/endpoints, operation failures with full exception
causes, and successful gameplay/keepalive send and receive counts and ages.
An age of `-1` means no success has been observed. Repeated failures of the same
operation are limited to one exported trace per five seconds
