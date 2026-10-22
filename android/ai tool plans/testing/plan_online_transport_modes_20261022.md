# Additive online transport scenario reuse

Base: online-restoration 5e888904f042d9e9612a8378df64cfbab21ece36; PR113 remains unchanged
Follow-up branch: online-transport-test-modes; stack against online-restoration while PR113 is open

1. Extract the existing native co-op death/respawn scenario and its automation helpers verbatim; retain LAN defaults and selection
2. Add opt-in two-player server-direct, authenticated-relay and initial-direct-failure fallback setup to the online runner using an explicit co-op fixture
3. Require current per-peer route epochs and fresh native traffic before and after the shared scenario; retain online host-closure policy
4. Inject initial probe failure only on disposable emulators with app-UID scoped UDP filtering, preserving owned STUN/relay and loopback; require real dropped probes and completed current fallback reports
5. Register opt-in catalog entries and manual campaign without changing existing schedules; exercise route negative controls, default selection and unchanged scenario bodies
6. Run scoped quality, tooling, server/JVM checks and appropriate native D1/D2 campaigns; publish a stacked draft with exact evidence and limitations

No production protocol, LAN scenario semantics, Rust pins, deployment or release UI changes are planned
Initial fallback is not mid-game failover; three-player mixed routes are outside this first slice

## Implementation and local evidence

- Three explicit catalog variants: D1 authenticated relay, D2 server-arranged direct, D2 failed initial probes followed by relay
- Shared co-op scenario and four automation helpers compared by PowerShell AST extent against 5e888904: all five bodies unchanged
- Initial fallback uses a unique app-UID scoped IPv4 UDP chain on disposable emulators; permits local native proxy, owned STUN and relay bridge, checks actual drops, removes owned rules in cleanup
- Current room/player/peer/network/check provenance required before fallback startup; exact route kind/peer slot/runtime epoch and fresh native traffic checked around both-role death/respawn
- Online host closure remains distinct from LAN host migration
- 173 Rust tests pass; unchanged JVM test task is up to date (prior 30 tests, one historical skip)
- Online admission/route host controls pass; fault bridge/filter controls: 5 pass; focused planner campaign/default-selection controls: 3 pass
- Automation catalog validates 95 standalone JSON, 431 support scripts, 264 standalone PowerShell owners
- Master-catalog controls reach the existing local unsupported /proc child-supervisor failure (exit125); do not weaken it
- Scoped quality: ShellCheck, shfmt, Prettier and PowerShell parsing pass; PSScriptAnalyzer1.25.0 provisioning unavailable. Pinned Ruff formatter exercised separately because the quality wrapper replaces PYTHONPATH
- Full planner suite and remote native campaign evidence to be recorded in the PR/checkpoint; no native qualification claim until receipts pass

Native scope is owned emulator topology only. The filter covers IPv4 candidates used by this fixture; the direct positive control must also pass. This is initial fallback, not mid-game failover. No three-player mixed-route claim is made by this first slice

## Published checkpoint

Draft PR116 targets online-restoration. Initial implementation 5f6c59af73e3fa66e48d20cf3231ede1bc636a35
Full planner suite: 82 pass; unchanged LAN public-profile controls pass
Native campaign39731864363 was dispatched once at that initial implementation and passed selection; execution remains pending
A focused follow-up corrects the work-evidence device count for the two-player slice (8 fresh identities and 2 device vector suites), with a positive host control; the original three-player campaign remains 12 identities and 3 device suites
