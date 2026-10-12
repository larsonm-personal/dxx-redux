# GQR-0272: Preserve LAN mission retry generations, 2026-10-10

Diagnosis and implementation plan only. Proposed GQF-0286/GQR-0272; admission occurs with chunk0345 canonical publication. No implementation or runtime reproduction in this tranche.

## Source-backed defect

A client can exhaust the three automatic mission-download attempts, select Resume mission download, complete the resumed transfer, and still be refused Ready by its host. The reset attempt ordinal is being used as a lifetime ordering generation.

- LobbyService.kt:186-199 rejects report.attempt below previous.attempt before considering terminal state
- handleMissionStatus at1698-1734 applies that predicate to the host's retained member report
- requestMissionDownload at1736-1743 resets clientTransferAttempt to1 on every manual activation
- sendMissionTransferRequest at1767-1798 creates a new request UUID, but publishes a queued report containing only the reset attempt ordinal
- Grant timeout at1799-1829 and transfer failure at1925-1935 retry only while attempt<3; failed attempt3 becomes FAILED_RESUMABLE
- LanDiscoveryTab.kt:246-267 offers Resume mission download for FAILED_RESUMABLE and calls requestMissionDownload directly
- Host handleMissionTransferRequest at1845-1882 updates liveness and grants a token without resetting or replacing the retained mission-status ordering owner
- MissionTransferService.kt:158-239,348-517 propagates the given attempt through download, verification, import and the successful MATCH report
- handleReady at1673 accepts ready only when the host's retained report is MATCH; startGame at2112-2123 likewise requires matching, connected, ready members

Concrete admitted sequence: the host retains FAILED_RESUMABLE/attempt3 for the current member and requirement; a new manual series succeeds on attempt1 or2; each new report is valid for the same requirement but fails the host's lower-attempt check, including MATCH. Successful completion stops automatic retries, leaving the client's completed download inconsistent with the host. The existing member heartbeat preserves that member report. Leave/rejoin can replace membership, so this is not an assertion of irrecoverable process-wide failure. A manual retry that reaches attempt3 can also behave differently; the demonstrated failure is success below the previously retained ordinal.

This is a source-derived ordinary path, not an executed network test. Assigned frozen hunk16 adds the policy at152-200 (+49/-0), SHA256 0febc89f71be9a9ada0eb695adbf3e43c1d6957b84cc119917838c1710af36ca. Whole current/frozen LobbyService.kt SHA256 e8a90a8454ad56498214f9d4416d4743aa48bd6addc489d59b63b37037e30fda.

## Ownership and intended behavior

The original third_party_coop_mission_sync_20260829.md:189-195 distinguishes an authorized attempt series from its connection attempt ordinal;330-360 and500 explicitly require manual Resume after automatic exhaustion while preserving verified partial data.

This root is distinct from BR-0433 authoritative lobby configuration, BR-0342 sender/session authentication, BR-0490 membership/start serialization, GQR-0102 source-byte generations and GQR-0209 delayed foreground expiry. Preserve those owners and the existing stale-grant request UUID, sender, lobby, revision and attempt checks. Preserve native match/readiness admission; do not allow Ready based solely on successful socket completion.

Proposed priority44 MEDIUM (H/M/B/C/R23/0/4/10/7), P2 correctness/protocol-state. The blocked resume path has a leave/rejoin workaround; do not claim a crash, data loss or demonstrated exploit.

## Implementation plan

- [ ] Give each accepted manual transfer series an ordering identity scoped to the current lobby membership and requirement, separate from its bounded local attempt ordinal
- [ ] Carry that identity through the existing transfer request/grant/report path and retain the host's currently admitted series with the member. Admit a new series only through a validated request; an arbitrary status report must not replace the owner
- [ ] Order reports within that series using the existing attempt/phase/progress rules. Reject delayed reports, grants, duplicate requests and delayed old-series requests after replacement; preserve repeated delivery of the active request without regressing progress
- [ ] Keep automatic retry limits and backoff local to one series. Reset that local ordinal on manual Resume, while preserving the new series identity through success, failure, cancellation and callbacks
- [ ] Clear or replace the series on the same explicit membership/requirement lifecycle as its owner. Use the existing session/state serialization boundary; coordinate with BR-0342/0490 rather than adding a parallel transfer session framework
- [ ] If a monotonic series counter is chosen, define its bounded representation and explicit exhaustion behavior without silent wraparound. If an opaque identity is chosen, prove that a delayed old request cannot be mistaken for a fresh series
- [ ] Update shared MissionStatusReport producers/consumers and both protocol endpoints together. Audit online consumers because the report is shared. Launcher schemas are disposable; no compatibility readers or migration layer

Simply removing the lower-attempt comparison reopens stale-report rollback. Simply incrementing the current attempt indefinitely conflicts with attempt<3 retry policy, request/grant coercion to1..10 and report validation0..100. Reusing a per-connection token as the entire series identity also requires proving queued, renewed-token, rejected-grant and timeout states.

## Required future acceptance

- [ ] Exercise the actual LobbyService request, grant, status, roster, Ready and start path: exhaust three grant timeouts, manually Resume, succeed on attempt1 and separately attempt2, then verify both endpoints show MATCH and host accepts Ready/start
- [ ] Repeat with transient in-transfer failures and verified retained chunks. Require the same content bytes and successful native catalog match after import
- [ ] Delay/reorder status and request/grant packets from the prior series around replacement, terminal publication and Ready. Require no rollback, stale success or duplicate download
- [ ] Cover same-attempt terminal failure, duplicate manual activation, cancel, host loss, leave/rejoin, changed requirement and member replacement
- [ ] Repeat enough manual series to cross the old10/100 ordinal boundaries without consuming the new series's bounded three-attempt retry allowance; cover the chosen generation representation boundary
- [ ] Preserve permanent-error no-auto-retry, verified partial recovery, phase/progress monotonicity and client request-identity checks
- [ ] Extend meaningful service/UDP integration coverage and the maintained end-to-end LAN runner; register any new top-level automation according to repository instructions. Focused predicate tests may supplement, not replace, the actual host/client sequence
- [ ] After implementation run scoped quality, relevant JVM/protocol tests, Android build and real two-client integration. Read-only diagnosis does not satisfy those gates

LobbyMissionRefreshTest currently uses attempt1 throughout. MissionTransferResumeTest covers partial boundaries, corruption rollback, throughput and retention of attempt2 in one completion report. Neither covers a new manual series after host-retained attempt3. No fixtures were executed or modified here.

