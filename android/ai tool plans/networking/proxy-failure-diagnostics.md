# Proxy failure diagnostics

- Add exported Network diagnostics for proxy/socket lifetime, operation failures, exception causes and stack traces, and monotonic ages of successful traffic
- Preserve existing forwarding, exception handling, keepalive timing, and shutdown behavior
- Exercise the real current proxy and the version before commit 0563e726e with a controlled keepalive IOException on loopback; verify traffic and socket lifetime instead of assuming the historical version recovers
- Run scoped formatting, the controlled JVM test, and Android Kotlin compilation
- Record results and remaining on-device evidence needed

Existing unrelated workspace changes belong to other work and must be preserved

## Findings

- The current proxy exports the injected keepalive IOException with operation,
  socket endpoints/state, cause chain, and recent successful traffic, then closes
  the proxy as it did before these diagnostic changes
- The actual pre-audit implementation also loses outbound forwarding under the
  same injected error. In the shared-socket fixture its independent inbound worker
  remains usable and its sockets remain open. This does not establish that the
  September 28 change introduced the original disconnection
- A gameplay-send IOException is exported and forwarding continues under the
  existing catch policy
- Keepalive timing, forwarding policy, and user-facing failure messages are unchanged
- Normal JVM testing includes the current proxy checks. Historical comparison is
  opt-in with `-PcompareProxyHistory=true` and uses the pinned parent Git object

## Validation

- Current keepalive failure, historical comparison, and continued gameplay send
  tests passed on real host loopback sockets
- Scoped code quality and both automation catalog checks passed
- Android Kotlin compilation and the default proxy test run passed after the
  shared receiver diagnostic counter adjustment
- Still needed: a phone reproduction with Network logging enabled to identify
  the real failing worker and exception; the injected test does not explain the
  original send error or earlier join failures
