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
