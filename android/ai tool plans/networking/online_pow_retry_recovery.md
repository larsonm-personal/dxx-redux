# Bounded installation proof retry and recovery checkpoint

## Plan

Reconstruct from verified published online-restoration, diagnose the production
proof timeout, bound fresh-challenge retries without changing the protocol or
work difficulty, exercise producer and service cancellation/staleness, and qualify
D1 against the actual Android provider. Keep PR 113 draft and release UI disabled.
D2 workflow dispatch remains pending explicit approval. No deployment or public
probing/load tests are authorized.

## Reconstruction

Disposable workspaces can break permanently. Bound troubleshooting to basic
command/access checks and one targeted recovery attempt; if still unusable,
reconstruct in a new directory from verified remote commits. Do not depend on
inaccessible unpublished files or claim work resumed before command/build access
has been checked. Re-read published repository instructions and relevant
.agents/skills when present. Keep enough evidence in published checkpoints for
another fresh reconstruction; never store secrets here.

This checkout was cloned afresh at d7ba5f8debbf33448b7db177a8e15aaf73563526.
Remote branch and draft PR head matched; GitHub identity was larsonm-personal.
AGENTS.md and .github/copilot-instructions.md were read; no published .agents
folder exists. Java 21 and g++ were available. The checked-in Gradle wrapper
has no wrapper jar/properties; bootstrap the pinned Gradle 9.8.0 distribution
from its official release rather than using previous workspace binaries.

## Policy and quantitative tradeoff

Keep each producer's 12-second budget and server difficulty unchanged. On work
exhaustion, retire the old socket and jobs before scheduling a fresh connection;
never replay a challenge or proof. Permit at most three challenge computations
per explicit connection operation, with a 45-second wall deadline from the first
challenge across retries and network stalls. AUTH_OK ends that budget; socket
upgrades and reconnects do not reset it. Explicit cancellation retires the timer,
proof and reconnect work. Generation/socket ownership rejects stale callbacks.
Provider and malformed-challenge errors remain terminal.

At 327567 hashes/s and independent 20-bit SHA-256 trials, the 12-second miss
estimate is exp(-327567*12/1048576) = 0.02355. Two full attempts miss about
0.0005547; three about 0.00001306 (0.001306%). Expected computation with up to
three attempts is about 3.20 seconds, while worst-case hashing is 36 seconds,
plus bounded scheduling/check overhead. Three attempts consume at most three
server challenges and add reconnect backoff; slow devices, scheduling and network
stalls can still fail. This is an estimate, never a substitute for adversarial
fixed-vector and actual-device evidence. Server absolute preauth remains 20s per
connection; the client reconnects instead of extending that deadline.

## Prior qualification evidence and preserved gates

At d7ba5f8 all five public CI lanes were green. D1 run 39648996309 failed before
gameplay: artifact 12371140764 / job 119397193075 recorded fresh sample 9
unsolved after 12001ms and 3931136 hashes; eleven other fresh identities passed.
Three fixed 4243438-hash vectors took 10971/10897/10814ms. A lucky rerun is not
qualification of the tail behavior.

The corrected long-outage oracle permits same-attempt host-only gameplay with
zero peer leases while requiring joiner timeout cleanup. Preserve precleanup
diagnostics and its strict negative controls. Earlier native successes do not
qualify that corrected oracle because d7 never reached gameplay. Emulator
recycling after failure is not evidence of disappearance during the assertion.

## Implementation and local evidence

The service now retries only work exhaustion, retains the three-attempt budget
across socket replacement, and uses a generation-owned 45-second timer plus
monotonic deadline checks before work/retry/proof submission. Explicit disconnect,
replacement connect and successful authentication cancel the timer. The producer
also checks cancellation and its deadline after signing. Identity transcript,
difficulty validation and server code are unchanged. Native qualification waits
now allow the bounded retry window and record the final attempt count.

Host tests exercise the actual service and producer with deterministic clocks:
three unsolved attempts terminate with no proof submission or later retry; total
timeout and cancellation leave no jobs; cancelled old work and stale socket
messages cannot submit on or disrupt a replacement; fixed expensive work verifies
the signature transcript. The shared Android instrumentation checks now also
force exhaustion and cancellation after 512 actual hashes, independently of luck
or device speed. These instrumented device checks still require a D1 campaign.

Fresh pinned Gradle/JDK/Rust tools were bootstrapped outside the checkout; the
Java proxy and system trust store were configured locally without weakening TLS.
The initial host suite passed 19 tests, with three optional integrations skipped.
The catalog validator passes (95 standalone JSON, 431 support, 262 standalone
PowerShell tests). Online admission/outage controls pass, including all strict
host-only survival negative controls. Scoped mixed formatting passes. The master
catalog runner remains blocked by this executor's missing /proc/self/task/.../
children interface (exit 125); do not treat this as a passing catalog execution.

The final host suite, with the freshly built locked Rust server supplied, passed
21 tests; only the opt-in historical comparison was skipped. This includes actual
service authentication/reconnect/pagination against production auth (local test
configuration difficulty 8) and the corrected real-server long-outage ordering
case. The independent fixed producer vector uses difficulty 20. No claim of
Android performance follows from these host results. Full D1 device qualification
and the five public CI lanes must be checked on the resulting published commit;
D2 remains pending explicit user approval. Do not merge or enable release UI.
