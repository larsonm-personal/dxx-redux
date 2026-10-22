# D2 online direct readiness

- Base/head f5c01779546eff1eef67681c85acd3261412f784, failure run 39705491539: native gameplay worked over relay, strict direct qualification failed
- Early StartGame can precede probes and cancel them during handoff; historical causation remains unproven because pre-start logcat was cleared
- Expose bounded current-room/network/probe result diagnostics and actual peer route kind/epoch
- Gate only the opt-in direct fixture on both completed current-peer probes, then require server-reported direct connectivity in the existing ready acknowledgement before StartGame
- Retain zero-relay plus native bidirectional traffic checks, normal early-start fallback, host consent and LAN defaults
- Extend existing host controls with deterministic delayed-probe and stale/mismatched-result cases; validate JVM diagnostics and scoped formatting, then publish fast-forward and dispatch one exact-head D2 campaign
- Preserve Rust compiler/component and dependency pins; no Rust upgrade or server policy change

## Validation and publication

- JVM suite: 25 tests, 22 passed, 3 optional server-backed tests skipped locally; real delayed UDP probe test passed
- Existing online admission/profile host controls pass, including delayed both-peer completion, stale epochs/room/peer, relay rejection and initial-start ordering
- Automation catalog validation passes (95 JSON owners, 431 support scripts, 264 PowerShell owners)
- Catalog integration blocked locally by unsupported `/proc/self/task/<pid>/children`; exact three-player scheduling/target/timeout controls passed before that host limitation
- Scoped mixed-language formatter: Kotlin and applicable checks passed; pinned PSScriptAnalyzer 1.25.0 unavailable from configured package repository (installation failed), so PowerShell analyzer remains for CI
- Fresh refs remain cmake 4be58740e29440a2c2b944b31bcfd2d72c0d325b and online-restoration f5c01779546eff1eef67681c85acd3261412f784 before publication; PR113 mergeable and user-set non-draft state preserved
- Prior f5c0177 D1 online 39693846692, ordinary sample 39693830711, LAN 39693830643, tooling 39693830689 and engine checks all succeeded; D2 39705491539 failed actual direct qualification while relay gameplay worked
- D2 qualification will run on the published fix; direct reachability and the historical race remain unproven until device evidence is available

## D2 probe-delivery investigation

- Run 39709354374 at 5bc012b failed before Ready/StartGame: both current checks completed with relay; joiner timed out nine pairs in 3000ms without a reported send exception
- The readiness guard worked; early start alone cannot explain this failure
- Fixture STUN reports loopback reflexive addresses; shared Wi-Fi uniqueness does not prove UDP reachability
- Add bounded actual send/receive/request/echo/response counters and fixture Wi-Fi candidate membership checks, keeping raw endpoints out of owner logs
- Confirm counters through the real delayed UDP service test and host fixture controls; do not alter candidate selection, socket routing, probe timeout, LAN defaults, Rust pins or direct-only oracle based on incomplete evidence
- Artifact download through both available paths returns HTTP 403 locally; parent reviewer supplied retained joiner evidence, while host logcat was unavailable
- Diagnostic validation: JVM suite 26 tests, 23 passed and 3 optional server-backed skips; real delayed successful probe and unanswered UDP timeout counter tests pass
- Host controls verify Wi-Fi candidate membership, actual per-pair send totals and sanitized bounded owner output; catalog validation passes
- Local catalog integration still blocked by `/proc/self/task/<pid>/children`; pinned PowerShell analyzer unavailable locally, Kotlin formatting passes
- This follow-up is instrumentation, not a claimed reachability fix; the next exact-head campaign is a targeted measurement of candidate membership versus UDP delivery, preserving the failing direct-only oracle

## Exact winning endpoint binding

- Measurement 39712660405 at 0193fdc proves both Wi-Fi probes succeeded, while launch selected a different host candidate and native traffic did not arrive
- Concrete defect: reports retain only type/ping and StartGame recomputes the first host endpoint; historical causation remains qualified because the exact winning endpoint was not previously retained
- Issue fresh server-owned offer IDs, binding authenticated reporter, peer and candidate endpoint; validate current candidate ownership and retire offers on acceptance, replacement, departure and end-game
- Preserve the accepted directional endpoint in launch assignments; unconfirmed directions fall back to authenticated relay, including late joins that lack reverse proof
- Verify UDP response source equals the probed endpoint and ignore own-token requests; add wrong-source/self-echo/multi-candidate tests and server stale/replaced-peer/forged-offer rejection tests
- Correct only resend-sensitive sent-count assertion, retaining exact response/match checks
- Preserve compiler/component/Cargo pins and known-good native LAN behavior
- Final endpoint validation: `cargo +1.94.1 test --locked` passes 170 tests (60 unit, 94 integration, 13 installation auth, 3 UDP reconnect), including real WebSocket second-host-candidate launch addresses and offer ownership/staleness rejection
- JVM validation with the rebuilt server: 27 tests total, 26 passed, only the opt-in historical proxy comparison skipped; wrong-source/self-echo/multi-candidate UDP, real auth and outage tests pass
- Host fixture controls and catalog validation pass; catalog integration remains blocked by the local `/proc` supervisor limitation
- Scoped Kotlin formatting and pinned rustfmt 1.96.1 pass; local PSScriptAnalyzer 1.25.0 provisioning still unavailable
- Compiler/component/dependency pin files unchanged; protocol v6 rejects earlier clients before authentication, and the campaign supplies matched builds
- Publish fast-forward from 0193fdc and run one exact-head D2 campaign; native qualification remains pending that result

## Protocol negotiation and expired offers follow-up

- Candidate offer IDs are a breaking schema change: raise Android protocol and server current/minimum protocol together from 5 to 6
- Reject v5 at authentication with explicit protocol/update guidance before auth or PoW challenge; test old-schema rejection, Android/server parity and current-version acceptance
- Share the 30-second holepunch deadline between cleanup and report acceptance; clear unused issued offers when expiring an attempt while preserving candidate-bound accepted routes
- Fence delayed reports and StartGame even before the cleanup worker runs; verify expired-unused offer rejection, accepted-route preservation and fresh-attempt acceptance
- Preserve active D2 run 39716307780 at 74ec15b for evidence; do not dispatch a duplicate or deploy
- Follow-up validation: 172 Rust tests pass (61 unit, 95 integration, 13 installation auth, 3 UDP reconnect); 26 matched-v6 JVM tests pass, with only the historical proxy comparison skipped
- Scoped format/lint and catalog validation pass; catalog integration still hits the local `/proc` child-supervisor limitation
- Existing D2 run 39716307780 remains active on 74ec15b; no cancellation, duplicate dispatch or deployment

## Preserve successful routes beyond offer timeout

- Offer acceptance deadline and accepted route lifetime are distinct: timeout retires unused offers but keeps validated exact endpoints while peer/candidate ownership is unchanged
- Keep report state/age rejection and all candidate-refresh, peer-departure and room-end invalidations
- Add real WebSocket launch coverage after the deadline, both before and after cleanup, with zero relay capacity and strict exact direct endpoints; advance the stored monotonic start time instead of sleeping
- Retain expired-unused report rejection and fresh-attempt positive tests
- Prior D2 run remains untouched; no duplicate dispatch or deployment

- Route-lifetime validation: all 173 Rust tests pass (61 unit, 96 integration, 13 installation auth, 3 UDP reconnect), including delayed direct launch before/after cleanup with no relay capacity and stale unused report rejection
- Scoped formatting passes; no Android code, wire version, dependency pins or endpoint ownership invalidation paths changed in this follow-up
