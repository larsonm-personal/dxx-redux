# General Code-Quality Survey and Remediation Process

## Purpose

This process combines the repository's broad adversarial review method with its reusable cleanup practices. It covers the complete branch change surface and relevant whole-codebase context, while retaining `DMR1` as the inherited D1/D2 diff-minimization subtrack

The campaign separates four objects that the first DMR1 queue conflated:

- Coverage unit: a frozen path, range, hunk, mechanical batch, preflight, or cross-file sweep assigned for read-only review
- Finding or investigation: one stable evidence-backed root cause or one bounded unanswered hypothesis
- Remediation chunk: the smallest complete live-worktree change and validation boundary
- Coverage record: explicit evidence that an assigned unit was reviewed, including clean dimensions and gaps

Hundreds of coverage units are expected. Hundreds of fixes are not assumed. Clean coverage is a valid result

## Canonical artifacts

- Plan: `plan_general_code_quality_chunked_round_20260811.md`
- Active ledger: `general_code_quality_ledger_20260811.md`
- Done archive: `general_code_quality_ledger.done.md`
- D1/D2 extraction subtrack: `d1d2_diff_minimization_ledger_20260811.md`
- Generated manifests and worker outputs: `temp/general_code_quality_20260811/`

The active ledger is the only canonical writer-owned progress file. Generated manifests and survey outputs are evidence, not alternate ledgers

## Scope and review domains

Every branch-modified path belongs to a primary domain and may also be revisited by sweeps:

1. Android launcher, Compose UI, lifecycle, persistence, and touch-only operation
2. Shared Android native code, JNI, cross-thread and cross-language interfaces
3. Inherited D1 changes, inherited D2 changes, paired behavior, and upstream merge pressure
4. Branch-added D1/D2 sinks and game-specific Android owners
5. Parsers, import, extraction, storage, save, metadata, and data publication
6. Graphics, audio, simulation, replay, determinism, and hot paths
7. Networking, matchmaking, multiplayer clients, and server Rust
8. Tests, runners, fixtures, automation, and emulator orchestration
9. Scripts, developer tools, build logic, dependencies, packaging, release, and CI
10. Documentation, generated material, artifacts, provenance, licenses, and PR hygiene

Applicable review dimensions include correctness, security, resource lifetime, concurrency, performance, compatibility, portability, warnings, diagnostics, API/data-format ownership, duplication, dead code, naming, maintainability, test quality, and diff minimization

## Freeze and generation rules

Each survey generation records:

- Exact target tip, merge base, and survey head
- Dirty-worktree ownership boundary and digest
- Sorted path/blob inventory and digest
- Generator settings and rubric version
- Parent generation and reason for generation change

Read-only survey reviews frozen Git objects, not mutable worktree copies. Remediation always rereads the live worktree

Do not regenerate away an active generation. When `HEAD`, target, assigned blobs, callers, interfaces, or fixtures move, append `GQ2`, `GQ3`, and later delta generations. Unrelated drift is recorded; assigned or context-changing drift causes `RECHECK` or `STALE`

## Deterministic coverage construction

Use `android/helpers/new_adversarial_review_ledger.ps1` as the mechanical path/range inventory seed. For the initial general survey use the frozen merge base to frozen head, source ranges of roughly 600-750 high-risk lines, roughly 900 test/documentation lines, and batches of no more than 40 mechanical paths

The generated queue is then normalized into `GQ1-COV-*` coverage records. Preserve its path order and exact range/path lists. Add:

- Three preflight units for composition, architecture, and change history
- Domain sweeps for D1/D2 parity, native/JNI boundaries, Android lifecycle, files/data, server/network, concurrency/resources, build/portability, tests, determinism, performance, errors/logging, interfaces, maintainability, and PR hygiene
- A cleanup-specific sweep for branch-owned warnings, diagnostics, dead code, stale aliases/references, script duplication, test runtime, and inherited-file minimization
- Closure reconciliation against the final live head

Every final-head changed path must map to a non-partial current coverage record. Mechanical classes are real review work and require provenance, reproducibility, schema, license, secret, or consumer validation appropriate to their kind

## Survey worker protocol

Survey workers are read-only. Every coverage unit uses a fresh `gpt-5.6-sol` worker at medium reasoning effort with no inherited turns. A worker processes exactly one coverage unit:

1. Read `AGENTS.md`, `.github/copilot-instructions.md`, this process, the active and done ledgers, and the frozen generation manifest
2. Confirm the frozen objects and assigned scope fingerprint
3. Inspect every assigned range or path plus enclosing functions, relevant callers and callees, tests, owners, paired D1/D2 code, and interfaces
4. Apply all applicable general review and reusable cleanup dimensions
5. Search the adversarial active and done ledgers, DMR1, earlier cleanup plans, and earlier GQ generations for duplicates, prior fixes, incomplete work, or regrowth
6. Write one raw result to the assigned `temp/general_code_quality_20260811/` path
7. Record exact scope and context checked, commands, atomic observations, explicit clean dimensions, evidence gaps, and scope fingerprint
8. Stop without editing product code or a canonical ledger

Read-only survey shards may run concurrently when their assigned outputs and frozen scopes are disjoint. Every shard still belongs to only one fresh worker. They must not use mutable live files as evidence while a product writer is active

## Finding admission and normalization

A single canonical-ledger writer imports raw outputs in coverage-ID then local-observation order. Each raw observation becomes one of:

- `GQF-*`: supported finding
- `GQI-*`: bounded investigation because essential evidence is missing
- Duplicate or evidence extension of an existing `GQF`, `BR`, or DMR1 item
- Rejected observation with a specific admission failure

Admit a finding only when it has:

- Branch causation or material worsening, or current regrowth in a changed area
- Concrete trigger, state, input, or maintenance failure
- Identified control flow, data flow, ownership, or build/test mechanism
- Specific impact and narrow verifiable location
- Plausible fix direction and validation

Severity is P0 through P3 and confidence is high or medium. Low-confidence concerns become investigations. Categories include correctness, security, resource-lifetime, concurrency, performance, compatibility, build-release, api-data-format, test-gap, maintainability, naming, documentation, and pr-hygiene, with cleanup-specific subcategories as needed

Do not admit generic warnings, formatter-only noise, unchanged upstream defects, personal style preferences, or duplicate symptoms. A 20-40-line extraction threshold never suppresses correctness or cleanup findings

Every coverage unit receives a `GQC-*` record with outcome `CLEAN`, `ISSUES`, `INVESTIGATION`, `PARTIAL`, or `SUPERSEDED`. A bare `none` is insufficient; clean records name dimensions and context checked

## Deduplication and historical reconciliation

Deduplicate first by path/symbol/category/trigger keys, then by root-cause reasoning. Search:

- `general_code_quality_ledger_20260811.md` and `.done.md`
- `branch_adversarial_review_ledger.md` and `.done.md`
- `d1d2_diff_minimization_ledger_20260811.md`
- Prior cleanup and next-30 plans

Normalization results are `NEW`, `EXTENDS`, `DUPLICATE`, `REGROWTH`, or `HISTORICAL-CLOSED`. Historical completion is evidence, not a permanent waiver. Regrowth receives current evidence and a new generation-specific finding linked to the historical ID

Maintain an immutable provenance row for every imported observation so normalization decisions can be audited and reversed without ID reuse

## Remediation chunk formation

Only supported findings and resolved investigations form `GQR-*` remediation chunks. One chunk contains:

- One root cause, or a tightly coupled set with the same owner, allowed paths, and validation
- Exact allowed product, test, build, and ledger paths
- Explicit non-goals, prerequisites, conflicts, and accepted boundary
- Expected quality or inherited-diff result
- Focused and platform validation appropriate to risk

Never batch unrelated issues to meet a call or line quota. One broad finding may use ordered child chunks, but remains open until its entire acceptance boundary is complete. Link extraction-only work to DMR1 instead of duplicating it

## Remediation workers and single-writer rule

Every `GQR-*` chunk uses one fresh worker with:

- Model: `gpt-5.6-sol`
- Reasoning effort: `medium`
- Conversation fork: `none`

One product writer runs at a time. The worker owns one remediation chunk for its entire lifecycle and may receive correction follow-ups only for that chunk. Retire it at terminal state and never reuse it for a different chunk

Before edits, record live HEAD, status, allowed-path hashes, and overlap classification:

- `SAFE_DISJOINT`: recorded non-overlapping edits permit work
- `NEEDS_REBASE`: re-anchor the patch to live content
- `USER_OWNED`: defer pending authority
- `ACTIVE_WRITER`: do not dispatch
- `SAME_ROOT`: merge or sequence related chunks before dispatch

Workers must not commit, stash, reset, checkout, clean, delete unrelated paths, or broad-format inherited sources

## Remediation lifecycle

1. Claim exactly one `GQR-*` row
2. Read linked findings, investigations, coverage, historical IDs, and live code
3. Reconfirm root cause and complete acceptance boundary
4. Narrow, reject, defer, or mark stale if current evidence differs
5. Implement the smallest complete root-cause fix
6. Add or improve focused coverage at the production boundary
7. Run scoped quality, `git diff --check`, and proportionate native, Windows, Android, JVM, script, server, emulator, multiplayer, replay, or data-regeneration checks
8. Record exact changed paths, before/after signals, limitations, and out-of-scope audit
9. Update remediation and finding state precisely and stop

Finding lifecycle is `OPEN`, `PLANNED`, `REMEDIATING`, `VERIFYING`, then `FIXED`, or terminal `DISMISSED`, `DEFERRED`, `DUPLICATE`, `RESOLVED_EXTERNAL`, or `OBSOLETE`. Partial mitigation never closes a finding

## Root orchestration

The root conversation:

1. Freezes generations and maintains the canonical queue
2. Assigns read-only survey shards and single-writer normalization
3. Verifies scope fingerprints, explicit coverage, admission, deduplication, and ID uniqueness
4. Ranks supported findings by severity, locality, confidence, prerequisites, and testability
5. Dispatches exactly one fresh product worker for the first eligible remediation chunk
6. Performs read-only diff, scope, metrics, and validation acceptance after each worker
7. Returns incomplete work to the same worker and retires it only at terminal state
8. Appends delta generations when repository movement invalidates coverage

## Standard prompts

Survey:

```text
Survey exactly <GQ-COVERAGE-ID> read-only using gpt-5.6-sol at medium effort. Read the repository instructions, general quality process, active and done ledgers, generation manifest, and assigned frozen scope. Inspect the assigned ranges plus necessary callers, callees, tests, paired D1/D2 files, owners, and interfaces. Apply the full general-quality and reusable-cleanup rubric. Do not edit product code or canonical ledgers. Write one raw survey result to the assigned temp path with concrete observations, explicit clean dimensions, commands, gaps, and scope fingerprint. Stop after this unit
```

Normalization:

```text
Normalize exactly the pending raw outputs in deterministic coverage-ID order as the sole canonical-ledger writer. Apply the admission standard and search both GQ ledgers, both adversarial ledgers, DMR1, prior cleanup records, and earlier generations for duplicates or regrowth. Assign stable IDs only after deduplication. Create atomic findings or investigations and one explicit coverage record per unit. Do not edit product code or form unrelated remediation batches
```

Remediation:

```text
Process exactly <GQ-REMEDIATION-ID> as the sole product and ledger writer using a fresh gpt-5.6-sol worker at medium effort with no inherited turns. Read repository instructions, the general quality process, active and done ledgers, all linked findings, and live code. Record live HEAD, status, allowed-path hashes, and overlap before edits. If evidence or boundaries drifted, narrow, reject, defer, or mark stale. Otherwise implement the smallest complete root-cause fix, preserve unrelated work, add focused coverage, run required validation, update states precisely, and stop. Do not commit or process another chunk
```

## Closure

The campaign closes only when:

- Every final-head changed path maps to current or explicitly superseded non-partial coverage
- Every coverage unit and sweep is terminal with an explicit `GQC-*` record
- Every raw observation has a normalization action
- Every finding and investigation is terminal or explicitly deferred with owner, reason, accepted risk, and recheck trigger
- Every completed remediation has root acceptance and exact validation
- No active writer, path claim, formatter lock, stale worker, or unreviewed head delta remains
- DMR1 and adversarial findings reconcile without duplicate ownership
- Counts reconcile across coverage, clean units, observations, findings, investigations, duplicates, remediations, and dispositions

Report campaign scale as a tuple:

`coverage units / clean units / investigations / open findings / terminal findings / remediation chunks`

This prevents a large coverage queue from being misrepresented as hundreds of predetermined fixes
