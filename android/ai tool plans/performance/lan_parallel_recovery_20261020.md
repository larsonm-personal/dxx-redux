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
