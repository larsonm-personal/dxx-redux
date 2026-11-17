# November 17 nightly evidence retention

1. Inspect run 44063788741 and published traversal state, keeping controller b90b11d separate from target fc6dc1eedda11f110c1c75a9eab4deb70f013631
2. Preserve sanitized route diagnostics through reconciliation and classify bounded stdout as well as stderr
3. Add regression checks for privacy, bounded reads and persisted failure evidence; run nightly tests and scoped formatting
4. Publish a draft PR and record remaining artifact-access and runtime investigation limits

No gameplay, baseline, coverage-credit or traversal-policy changes. Raw private logs remain local. Artifact downloads currently fail at the network proxy, so this change does not claim to identify the seven route failures or five Android failures.

## Findings and limits

- Reconciliation discarded sanitized `route_workers` and `route_infrastructure` fields. Preserve their fixed categories and exit codes in failed owner records
- Route-runner classification read stderr only. Read the last 16 KiB of each captured stream, still returning only existing fixed categories
- Sampled owners including `test_sound_trace` and `test_fov_demo_compatibility` do not emit `DXX_TEST_COMPLETION`. Empty case counts do not establish collector loss or full coverage. Coverage credit remains unchanged
- Shard 009 job 133972940422 failed resolving the pinned Kotlin JVM plugin before executing tests. Its log lacks the underlying HTTP failure. Other successful shards make an unavailable version unlikely, but do not prove the transient cause
- Published traversal state has 2587 done owners, including 77 new terminal credits (65 PASS, 12 FAIL) for this run and exact target. A read-only public planner check selects six next-undone owners deterministically and preserves cycle zero. Full retail replay still needs the original artifact asset identity
- Artifact download redirects are blocked by the environment proxy. The Android assertion causes and seven generic route failure causes remain unresolved; no gameplay fix is claimed
