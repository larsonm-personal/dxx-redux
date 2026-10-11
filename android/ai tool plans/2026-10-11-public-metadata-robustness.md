# Public metadata robustness coverage

## Scope

- Migrate existing native metadata worker lifecycle and malformed-level-header integration tests to pinned D1/D2 shareware by default, with explicit retail mode retaining original level names and assertions
- Reuse the existing public fixture resolver; never download implicitly, alter pinned source data, or weaken worker retirement/recovery/reuse checks
- Generate invalid level copies only in the existing disposable per-run fixture directory
- Exercise both engines in Linux CI using the already-built metadata workers; preserve public replay and full-campaign baseline checks
- Keep Android service lifecycle coverage separate from native host results; no device claim without a serially coordinated run

## Validation plan

- Run real worker initialization failure, recovery, invalid request and stable healthy reuse for both public games
- Run unsupported version, truncated header, invalid signature and zero-version cases against both public games, then verify subsequent valid requests on the same worker
- Evaluate imported D1 and profile switching only when the public engine supports the same semantics
- Run scoped quality and both required catalog checks; report the known local supervisor namespace blocker without changing its guard
- Stack after the emulator-support segment, publish as a separate draft and check its exact-head CI; do not merge

## Results so far

- Existing Debug metadata workers passed both native lifecycle integrations and all eight malformed-header cases, with same-process valid recovery after each rejection
- Imported D1 in D2 passed lifecycle/reuse and D1 -> D2 -> D1 switching, including new worker PIDs and identical restored D1 metadata
- Public D2 requires the existing config's `d2demo` mission identity; simply replacing its first-level filename was insufficient and correctly failed the recovery assertion
- Retail/custom fixture execution and Android service execution are not claimed; no private media was supplied and no engine source changed

- Same-directory public profile switching also passed after strengthening fixture isolation; the content-game guard cannot be masked by changing data paths
- Native worker catalog classification corrected from emulator to host-only while retaining explicit caller-selected executable; catalog regression asserts both properties
- Automation validator passed; direct catalog discovery passed. The required child-supervised master catalog test reproduced the existing `/proc/self/task/.../children` namespace failure (exit 125), with safety guard unchanged
- Scoped mixed-language quality and actionlint passed using existing pinned tools; final checks repeated after catalog classification change
