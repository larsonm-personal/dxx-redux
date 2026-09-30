# LAN discovery diagnostic run

Implement an explicit client-side, 80-second experiment for the September 29
two-phone discovery failure. Preserve ordinary discovery outside the experiment

- Export QUERY/ANNOUNCE send and receive metadata, unique transmission IDs,
  echoed query IDs, socket generations, source ports, and handler duration
- Cycle subnet broadcast, limited broadcast, explicit/remembered unicast, and
  subnet broadcast after socket replacement in 10-second phases, twice
- Suppress ordinary client queries during the experiment, leave passive host
  announcements intact, and stop on backgrounding, joining, hosting or leaving
- Provide an optional host IP and progress/cancel controls in the LAN screen
- Format scoped files, compile Kotlin, run relevant tests, and document device
  validation limits and the one-run reproduction steps

Phone procedure: install this build on both phones, enable Network logging on
both, host a lobby on one, and on the other enter its IP in the diagnostics field
and run the test to completion without joining. Export both logs. A discovered
lobby does not end the experiment. Query IDs distinguish actual probe replies
from passive announcements, even if the UI retains a previously found lobby

## Implemented and verified

- QUERY/ANNOUNCE traces include attempted and accepted sends, actual receive
  endpoints, payload size, socket identity/generation, monotonic time and handler
  duration. Responses echo query identifiers, phase and originating timestamp
- The explicit diagnostic IP is not saved to recent addresses and never launches
  the engine. Ordinary discovery resumes after the test
- Socket replacement uses normal transport recovery, which also reacquires the
  multicast lock. A successful refresh implicates that combined transport path;
  it does not by itself distinguish the socket from the lock
- Debug app and instrumentation APKs built successfully, using existing native
  libraries because this change only affects Kotlin and the test runner
- All 17 lobby unit tests passed; scoped code-quality checks passed
- `android/tests/test_discovery_experiment.ps1` passed on emulator-5554. It runs
  the full 80-second sequence against a real local UDP responder, verifies both
  unicast rounds and two socket generations, exercises the real host query/reply
  handler and checks cancellation on backgrounding
- Emulator loopback validates the diagnostic mechanism, not physical Wi-Fi
  broadcast delivery. The original two-phone failure still requires the run above

Interpretation: match `trace_id` from client TX to host RX, then `reply_trace_id`
from host TX to client RX. Missing host RX localizes a forward delivery failure;
host RX without reply TX implicates handling; accepted host TX without client RX
localizes return delivery. Accepted sends alone do not prove network delivery
Compare passive ANNOUNCE reception independently of query replies. Phase markers
and echoed IDs allow correlation despite clock offsets and delayed replies
