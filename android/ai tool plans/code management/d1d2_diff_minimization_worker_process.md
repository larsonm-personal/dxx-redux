# D1/D2 Diff-Minimization Worker Process

## Purpose

This process converts the live D1/D2 inherited-file diff into bounded implementation chunks. The root conversation remains the campaign orchestrator. Every implementation chunk belongs to one fresh `gpt-5.6-sol` worker at medium reasoning effort from claim through completion

The permanent artifacts are:

- This file, which defines the campaign method
- `d1d2_diff_minimization_ledger_20260811.md`, which records the round snapshot, candidate queue, claims, metrics, validation, and residual decisions
- `plan_d1d2_diff_minimization_chunked_round_20260811.md`, which records the overall plan and completion state

Generated inventories, command output, and test logs belong under `temp/diff_minimization_20260811/`. Do not create separate per-chunk plan files. The active ledger is the plan file for each worker chunk

## Durable goals

The primary objective is to reduce additions and deletions in D1/D2 files inherited from the upstream engine. Total repository line count is secondary. A successful extraction may move most implementation lines into a branch-added shared file while leaving only a small call, declaration, or build entry in an inherited file

The campaign also pursues these prior-round goals:

- Put Android-owned and branch-owned feature mechanisms in new files with clear ownership
- Remove D1/D2 duplication when one shared implementation is natural
- Keep authoritative file formats, configuration layouts, packet formats, game policy, private engine state, and genuinely cross-platform features in engine-owned C code
- Preserve desktop behavior and paired D1/D2 behavior
- Repair concrete correctness defects found at a boundary when they are required for a sound extraction and can be covered in the same coherent chunk
- Add focused fixtures or integration coverage before moving high-coupling code
- Remove dead declarations, stale duplicate implementations, implementation-header patterns, and accidental drift exposed by the move
- Fix warnings caused by branch-owned code, remove obsolete success-path diagnostics after their investigation is complete, and retain malformed-input, timeout, security, and hard-failure logging
- Reuse or parameterize existing test infrastructure when a chunk exposes duplicated tests, stale waits, or false-pass paths

## Round snapshot and metrics

Each round records these full Git object IDs:

- Survey head
- Current `upstream/main` tip
- Merge base between the survey head and `upstream/main`

Use two related measurements:

1. Branch-attribution view: merge base to survey head. This identifies changes introduced by the branch without including later upstream movement
2. Integration-pressure view: current `upstream/main` to the live worktree. This reflects the source a current merge or rebase must reconcile

An inherited file is a path present in the chosen base tree. A branch-added sink is a path absent from that tree. Report these separately. Do not rank a large branch-added sink as inherited-file churn, but do inspect it for duplicated implementations, misplaced ownership, and opportunities to receive extracted code

The live worktree can move while the campaign proceeds. Do not regenerate or replace the canonical ledger. Append a new survey generation when:

- `HEAD` moves beyond the recorded survey head
- `upstream/main` moves
- concurrent user work changes D1/D2 paths
- completed chunks materially change the rankings

Every completed chunk records its own live before and after target-file numstat against `upstream/main`. Aggregate figures are useful checkpoints but must not be presented as the isolated payoff when unrelated work changed concurrently

## Survey and chunk construction

Survey all inherited modified D1/D2 paths and all branch-added D1/D2 sinks. Classify the diff before ranking it:

- Android-owned or branch-owned mechanism embedded in inherited code
- Exact or near-exact paired D1/D2 implementation
- Thin hook, declaration, build wiring, or required platform adapter
- Authoritative engine policy or format logic that should remain local
- Substantive D1-only or D2-only feature work that should not be forced into a shared abstraction
- Upstream drift or stale branch change that may be restored to upstream behavior
- Branch-added sink that is correctly placed, duplicated, or overgrown
- Below-threshold residual

A chunk is one coherent ownership seam, not merely a file range. Its ledger row must name:

- Inherited source paths and approximate functions or regions
- Allowed new or existing destination paths
- Expected inherited-file reduction
- Why the destination is the correct owner
- D1/D2 similarities and intentional differences
- Coupling and risk
- Prerequisite fixtures or earlier chunks
- Focused validation
- Explicit non-goals

Prefer independently verifiable chunks that remove roughly 40 to 400 inherited lines. Larger opportunities must be split by mechanism, state owner, or validation boundary. Smaller chunks may be grouped only when they share one destination, one behavior, and one validation path. Never group unrelated small cleanups solely to fill a worker call

The queue order is:

1. Restore accidental drift or remove dead duplicate code with very low behavior risk
2. Exact paired mechanisms with an existing shared owner and focused tests
3. Android-only bodies with a compact new shared boundary
4. Branch-added sink deduplication that enables later inherited-file reductions
5. Higher-coupling persistence, networking, replay, rendering, or private-state work after fixtures exist
6. Evidence-backed retained-policy and below-threshold classification

## Candidate acceptance and stopping rules

Accept an implementation candidate only when:

- The moved body is branch-owned or has a clear shared cross-platform owner
- The destination and public API are materially smaller and clearer than the duplicated inherited bodies
- The move does not require exposing broad private structures or building a callback table comparable in size to the removed code
- Exact ordering, ownership, flags, errors, cleanup, ABI, and platform guards can be preserved or a separately justified correction is covered by tests
- D1/D2 differences remain explicit and reviewable
- Appropriate validation can be run in the current environment

Reject or retain a candidate when it would move canonical engine policy out of its source of truth, hide required build or API surface, manufacture cross-game similarity, or trade a small inherited reduction for larger coupling and adapter churn

The campaign endpoint is reached when every surveyed candidate is complete, rejected, deferred with specific prerequisites, or classified below threshold, and the remaining credible seams are individually about 20 to 40 inherited lines or carry disproportionate risk. Line count is not permission to extract unsafe networking, serialization, private-state, or render-order code

## Worker model and isolation

Each chunk uses a fresh worker with:

- Model: `gpt-5.6-sol`
- Reasoning effort: `medium`
- Conversation fork: `none`

One worker owns one chunk for its entire lifecycle. The root may send follow-up instructions to that same worker to correct incomplete validation or scope mistakes, but the worker is retired when the chunk reaches a terminal ledger state. Do not reuse a worker for a different chunk

Run only one product-editing worker at a time. Parallel workers are prohibited even for apparently disjoint paths because they share a worktree, build products, format locks, test devices, and the ledger. The root may perform read-only Git and ledger checks while a worker runs

## Single-writer and dirty-worktree rules

The active chunk worker is the only writer during its turn. It may edit only:

- The exact inherited paths named in the chunk
- The named destination shared files or a narrower replacement agreed in the ledger
- Direct declarations and build wiring required to compile the move
- Focused tests or automation scripts required by the chunk
- The active ledger
- Scratch output under `temp/diff_minimization_20260811/`

Before claiming a chunk, the worker records `git status --short --untracked-files=all` and distinguishes campaign changes from pre-existing user changes. It must not edit, format, revert, stage, or delete unrelated paths. If an allowed path already has unrelated live edits, the worker must preserve them and record the overlap. If the overlap prevents a safe change, mark the chunk `DEFERRED` with the exact conflict instead of overwriting it

Do not commit, amend, reset, checkout, clean, stash, or rewrite history. Do not broad-format D1/D2. Never delete handmade comments merely to reduce the diff

## One-chunk lifecycle

Each worker performs exactly one ledger chunk:

1. Read `AGENTS.md` if present, `.github/copilot-instructions.md`, this process, the round plan, and the full active ledger
2. Confirm the recorded Git objects exist and inspect the current worktree status
3. Claim only the named `TODO` item by changing it to `ACTIVE`; resume it if it is already `ACTIVE`
4. Reconfirm live functions, paired D1/D2 differences, callers, declarations, build wiring, shared owners, and tests
5. Record a short boundary decision in the chunk note before product edits. If the proposed API is not a net clarity win, reject or narrow the candidate with evidence
6. Record exact before metrics for every inherited target path
7. Implement only the named seam, using normal compiled translation units and the smallest complete API
8. Add or extend focused integration coverage when behavior is centralized or a correctness defect is repaired
9. Run scoped code quality, `git diff --check`, affected host tests, Windows D1/D2 builds, configured Android ABI builds, and focused emulator tests as appropriate
10. Record exact after metrics, files changed, test output paths, limitations, and residual follow-ups
11. Mark the row `DONE`, `REJECTED`, `DEFERRED`, or `BLOCKED`, append the completion record, and stop

`BLOCKED` is reserved for missing authority or evidence that cannot be obtained after safe attempts. Uncertainty, a difficult implementation, or a failing test is not by itself a blocker. A worker should diagnose and fix chunk-caused failures within its chunk lifecycle

## Validation policy

Use the narrowest validation that establishes the moved behavior, then include platform coverage proportional to the boundary:

- Run `android/helpers/stop-stale-formatters.ps1` before a new formatting pass if a prior pass timed out or was interrupted
- Run one scoped `android/run-code-quality.ps1 -Fix -Paths ...` call for new or branch-owned files. Avoid formatting untouched inherited regions
- Run `git diff --check`
- Build Windows D1 and D2 for changes to shared native or inherited engine code
- Build both Android games across configured ABIs for shared native changes
- Run focused host fixtures before emulator work
- Run emulator tests sequentially with cleared logcat and file-backed output
- Run two-emulator coverage only for multiplayer or host-migration behavior
- Run deterministic state, RNG, and replay comparisons for simulation or replay boundaries

An environment limitation is recorded separately from a source failure. A chunk is not complete if its own code fails a relevant available check

## Root orchestration

The root conversation:

1. Maintains the queue and identifies the first eligible item whose prerequisites are complete
2. Confirms no other worker or long-running formatter/build/test owns shared resources
3. Spawns one fresh medium-effort Sol worker with no inherited turns and a referential prompt naming the chunk and canonical files
4. Avoids editing shared files while the worker runs
5. Verifies the worker processed only one chunk, respected allowed paths, preserved pre-existing changes, and left a complete ledger record
6. Reviews the resulting diff for scope and boundary quality without silently reimplementing the worker's chunk
7. Sends correction work back to the same worker when the chunk is incomplete
8. Retires the worker after the chunk reaches a valid terminal state
9. Refreshes the ranking and dispatches a fresh worker for the next chunk

If a worker is interrupted after claiming a chunk, resume the same worker when possible. Otherwise a replacement worker may resume only that `ACTIVE` chunk and must record the recovery. Never treat a partial implementation as completed coverage

## Standard worker prompt

```text
Process exactly <CHUNK-ID> as the sole product and ledger writer using the live worktree

Read AGENTS.md if present, .github/copilot-instructions.md, android/ai tool plans/code management/d1d2_diff_minimization_worker_process.md, android/ai tool plans/code management/plan_d1d2_diff_minimization_chunked_round_20260811.md, and android/ai tool plans/code management/d1d2_diff_minimization_ledger_20260811.md. Follow the process exactly. Claim or resume only <CHUNK-ID>. Preserve all pre-existing and out-of-scope changes. Reconfirm the candidate against the live diff, record before metrics, and reject or narrow it if the proposed boundary is not a real clarity win. Otherwise implement the smallest complete extraction or cleanup, add focused coverage, run the required scoped quality, builds, and tests, record after metrics and exact evidence, set a terminal status, and stop. Do not commit or process another chunk

Report the chunk status, inherited-line reduction, files changed, validation, limitations, whether any out-of-scope file changed, and the recommended next eligible chunk
```

## Root verification checklist

After every worker lifecycle, verify:

- Exactly one chunk row changed from `TODO` or `ACTIVE` to a justified terminal state
- No unrelated or pre-existing user path was altered
- The actual diff matches the chunk's ownership seam and non-goals
- D1/D2 paired behavior is either preserved in both games or intentionally game-specific
- New code has a conventional declaration and implementation boundary
- No new source-of-truth duplication, broad private-state exposure, or adapter layer erased the payoff
- Before and after inherited metrics are exact and isolated as far as possible
- Required tests ran and their outcomes are recorded honestly
- The residual queue was reranked or confirmed still valid
