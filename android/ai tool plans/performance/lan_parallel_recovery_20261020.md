# Parallel LAN loading recovery qualification

Starting revision: 06f9061de3e3c0db4970b0583fea42d11a51d120 (PR104,
stacked on PR103). Preserve the prior timing experiment; this phase qualifies
communication and recovery, not startup performance.

## Execution plan

1. Reuse the public disposable-emulator owner with a focused recovery mode.
   Bound every wait; retain native monotonic logs and observed state.
2. Reproduce silent host loss while initial client synchronization waits, and
   same-session client cancellation/reconnect. Keep force-stop distinct from
   cooperative cancellation, and never reset the surviving host/session.
3. Add debug-only packet fault injection for byte-identical admission replay,
   dropped/delayed request/proof packets and state-boundary delivery. Preserve
   signed purposes, freshness counters and state-bound proof verification.
4. Check actual old/new binaries both ways and three players with uneven loads;
   distinguish fallback from failure. Exercise failed level load separately.
5. Fix observed failures, run focused regressions for both engines, update PR104
   with exact evidence and remaining gaps. Do not merge or change cmake.

## Initial source hypotheses (not runtime results)

Initial client sync polling does not activate the join-in-progress watchdog;
therefore silent host loss may leave it waiting indefinitely. The host's 60s
initial-loading allowance bounds disappearance only after selection. Legacy
QUIT_JOINING is not an authenticated pending-admission cancellation and must not
be newly trusted as one. A reserved early slot while awaiting a third player
also needs bounded recovery. Tests must distinguish these states explicitly.

## Qualification checkpoint

PR103 remains open at 01401425aff549eefd8b5a9e9fd8a8458d01a9b8;
PR104 remains open at 06f9061de3e3c0db4970b0583fea42d11a51d120.
Current cmake is 84387ccdfb5fcd761f39bd0b4f6702ad92fa8150. This work
preserves the existing stack; it has not integrated that newer cmake revision.

The first control run, 39459895699 at 2f7b91b6a, failed in the harness:
callback-local log variables were read outside their PowerShell child scope.
All four cases stopped before host termination. This is NOT evidence of an
engine failure. Control e7907ae8a corrects the scope and requires a positive
terminal menu observation; its run is 39462987538.

The corrected control completed with four genuine host-loss failures: D1 and D2,
each with loss during host loading and during host readiness waiting. The host
was force-stopped in every case; no SYNC occurred. Clients continued sending
readiness requests beyond the 85-second controller observation bound and never
logged `wait_for_sync FAILED`. This establishes unbounded initial synchronization
over the observed interval, not a measured infinite duration. Raw evidence is
artifact 12284523605; annotation export 39467100603 preserves the logs and
controller events. The earlier harness failure is excluded from this result.

Candidate 1928d2f5d built both the candidate and actual old PR103 APKs in run
39462572572, but provisioning rejected Nexus5X_Light_3. No recovery case ran.
The creator and SDK-writer allowlists, third-AVD definition and explicit 5558
provisioning are addressed in the follow-up. Default provisioning remains dual.
Full candidate run 39465992978 uses engine revision
4a794c7c142b281a375846ab77fc2535199a3905. Follow-up c0ff3e61 changes only
the mixed-version reconnect observation to the common native auto-join marker:
the old APK has no newer `STARTUP-SYNC` instrumentation. A failure of that old
marker assertion in the pinned full run is a harness failure, not proof that
old-client reconnect failed. Targeted mixed-binary qualification is still needed.

### Candidate behavior under qualification

- Initial launcher client synchronization has a 60-second post-load deadline.
- Initial host selection has a 60-second roster deadline. A lost selected loader
  fails the initial start after the existing 60-second loading allowance instead
  of silently starting with fewer players. Error-dialog acknowledgement is user
  dwell, not network waiting.
- A new optional join-query capability advertises initial loading separately
  from live-world joining. A reconnecting selected client loads before sending
  ordinary readiness. Fresh identity, counter, route proof and level checks
  remain required. Legacy query fallback preserves old-host interoperability.
- A proven reconnect route updates the initial slot's destination before SYNC.
  No endpoint change occurs on an unproved request or stale proof.
- The fault fixture is debug-only. Retag testing suppresses the original
  admission, delivers its retagged bytes with an unseen counter, holds valid
  readiness for six seconds, and requires actual signature rejection.
- Reconnect evidence saves the old client log before launcher restart clears it,
  requires a new native join, and verifies the surviving host process and single
  initial level start. Escape cancellation is separate from process death.
- Three-player loss coverage requires both surviving peers to reach terminal
  menus without SYNC. Healthy clients may use their own watchdog; immediate
  authenticated cancellation propagation is not implemented.

The 60-second allowance is a liveness policy, not a way to identify CPU work
versus a dead process. A legitimate load exceeding the allowance can fail and
require retry. Client timing starts after its own load; the host's selected-peer
timer starts when it enters the readiness wait. Their deadlines therefore need
not expire together. The 20-second loader fixture crosses the former 15-second
timeout without claiming unlimited slow-loader support. No unauthenticated
heartbeat or legacy cancellation packet extends or releases a reservation.

The full candidate's planned matrix is 15 dual-peer cases per game, two
three-peer cases per game, and both actual mixed-binary directions per game.
This is an intended matrix, not a pass count. A case failure can prevent later
cases within a grouped helper from running; final claims must come from recorded
case receipts and logs. Follow-up 6ebeeb3aa also requires a positive host terminal
menu after canceled, missing-third, and failed-load starts. Those strengthened
assertions are absent from the pinned full run and need focused qualification.

Mixed-binary coverage checks initial startup and reconnect after both peers have
entered the running level. It does not qualify reconnect during initial loading
with an old peer. The new initial-loading query phase is advertised only to a
capable client; that recovery path requires both new endpoints. Older binaries
retain serialized initial startup and their existing live-game join protocol.
This compatibility scope is distinct from the all-new same-session loading
reconnect test and must not be generalized to every mixed-version interruption.

Local native authentication/deadline and wire-fixture tests pass in both engine
build trees. Linux engine objects compile. The SDK-writer regression and master
catalog execution cannot run on this local host: its child supervisor cannot
read /proc/self/task/PID/children. Automation catalog validation passes. Runtime
and compatibility claims remain pending actual successful CI evidence.

The equivalent existing CI tooling owner subsequently passed on Ubuntu 24.04
and Windows 2022 in run 39471168051 at
71c38cfe258a317b525d60e7e2be1bcc547a0ce5. Its bounded tooling suite includes
`test_sdk_writer_lock`, `test_run_all_tests_catalog`, and
`test_validate_automation_catalog`; workflow validation and planner/ledger checks
also passed. The local supervisor limitation remains local and is not an
outstanding qualification blocker for those checks.

## First candidate runtime evidence

Run 39465992978 completed its owner in 34:35, before the 40-minute owner
timeout. Raw artifact 12287219949 and annotation export 39473966888 preserve
the evidence. There are 23 observed case directories and 15 explicit passes:
11 D1 and four D2. These are case receipts, not a complete matrix pass.

- Same-session process restart/reconnect passed in both engines. Each host
  retained its process and single initial level start; fresh authentication
  proved a changed route (`initial_route_proven changed=1`) before readiness.
- Both engines passed admission replay, the discriminating retag/signature
  rejection test, and two dropped readiness requests.
- D1 additionally passed proof loss/delay/replay, delayed admission, the
  20-second client hold, canceled-loader timeout, and missing-third timeout.
- All four host-loss cases logged `initial_sync_timeout` and returned from
  `StartNewLevel`, but the harness waited for category-gated `net_log_comment`
  output. Therefore its terminal-menu assertion was never reached. This is
  evidence that network waiting stopped, not yet a complete observed-UI pass.
- Escape likewise returned from initial loading in both games, but the same
  gated marker prevented the cancellation/reconnect check from continuing.
- The D1 missing-file fixture reached the real fatal error
  `Can't open file <missions/__missing_startup_test_level__>`; the harness
  expected different loader wording and stopped before checking host recovery.
- During D2 proof-loss testing, emulator operations stopped succeeding and no
  final native logs were captured. Later D2 resets, three-player verification,
  and mixed installs could not execute reliably. The cause of the emulator
  connection failure is not established; these are unqualified cases, not
  demonstrated packet-protocol failures.

The follow-up adds a forced `sync_wait_aborted` diagnostic, matches the actual
loader error, and still requires positive terminal-menu observations. It also
stops a matrix when emulator health is lost and reports remaining cases as
blocked instead of producing cascading reset failures. Authentication, readiness
and timeout behavior are unchanged by these follow-up diagnostics.

The UI assertion rejects the ordinary `Wait...` dialog, even though it reports
`screen_mode=menu` and `in_game=false`. After the native transition it requires
either the specific failure text in a non-game-front dialog, or main-menu items
with `is_network=false`. Host-only negative controls exercise that distinction;
an error dialog awaiting acknowledgement is reported separately from a resumed
game. Raw menu-activation breadcrumbs are retained in subsequent evidence exports.

Focused runs may qualify a dual case by game, for example
`-f recovery_cases=d2/drop-proof,d2/delay-proof`. This preserves earlier valid D1
results while finishing the D2 cases that lost their emulator. Three-player and
actual mixed-binary qualification run on separate fresh runners; no KVM
permission change or attempt to revive the unavailable prior runner is needed.

Focused reproduction uses the existing workflow, not a new unmerged workflow:

```sh
gh workflow run public-lan-smoke.yml --ref TOPIC_BRANCH -f recovery_checks=true
# Optional bounded subset, preserving the same owner and data collection:
gh workflow run public-lan-smoke.yml --ref TOPIC_BRANCH -f recovery_checks=true \
  -f recovery_cases=same-session-reconnect,cancel-reconnect,retag-admission
```
