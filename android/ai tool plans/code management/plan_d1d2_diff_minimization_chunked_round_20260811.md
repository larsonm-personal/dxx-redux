# Plan: Chunked D1/D2 Diff-Minimization Round 2026-08-11

## Goal

- Continue reducing branch-owned churn in files inherited from the original D1 and D2 trees so the merge request is reviewable and future upstream merges remain tolerable
- Put Android-owned and branch-owned feature bodies in new files, normally under `android/app/src/main/cpp/shared/`, while leaving compact engine-owned hooks and policy at original call sites
- Continue the earlier secondary goals: remove duplicate D1/D2 implementations when there is a natural single owner, repair correctness defects discovered at extraction boundaries, clean branch-owned warnings and obsolete diagnostics, simplify duplicated tests where a chunk exposes them, preserve canonical C ownership of game formats and policy, and strengthen focused integration coverage
- Process the work as bounded chunks, each implemented by a fresh `gpt-5.6-sol` worker at medium reasoning effort under root orchestration

## Prior work and rules retained

- Measure inherited-file conflict surface separately from total repository size
- Compare against both the frozen merge base for branch attribution and `upstream/main` for current integration pressure
- Classify branch-added sink files separately from inherited modified files
- Prefer one normal shared translation unit over duplicated bodies, implementation headers, or direct `.c` inclusion
- Keep game-specific policy, private engine structures, serialization layouts, packet layouts, and cross-platform feature implementations local when extraction would obscure ownership or increase coupling
- Preserve exact ordering, flags, cleanup, error behavior, ABI, and platform behavior during diff-only movement
- Require a shared boundary to be materially smaller and clearer than the inherited bodies it replaces
- Keep D1 and D2 hooks paired where behavior is paired, and preserve Windows, Linux, macOS, and Android behavior
- Use focused host or emulator integration tests for behavior centralized under `android/`
- Preserve unrelated dirty-worktree changes and never broad-format inherited source files
- Remove only branch-owned warnings, dead declarations, stale compatibility aliases, and noisy success-path diagnostics; do not churn inherited warnings or useful failure logging
- Reuse and parameterize test helpers when a chunk needs coverage rather than adding another near-duplicate runner

## Campaign artifacts

- Durable process: `d1d2_diff_minimization_worker_process.md`
- Active round ledger: `d1d2_diff_minimization_ledger_20260811.md`
- Scratch surveys and command output: `temp/diff_minimization_20260811/`
- Historical inputs: the original shrink study, phase 2 and phase 3 plans, May refresh, July candidate catalog, July high-coupling campaign, and adversarial review process and orchestration documents

## Plan

- [x] Read repository instructions and identify the prior diff-minimization and adversarial-review artifacts
- [x] Recover the durable goals, acceptance criteria, validation policy, stopping rules, and worker-orchestration lessons
- [x] Freeze the round identifiers and record the existing dirty-worktree ownership boundary
- [x] Survey the current inherited D1/D2 diff, branch-added sinks, paired D1/D2 duplication, stale historical candidates, and recent feature regrowth
- [x] Create a ranked chunk list with exact paths, boundaries, estimated inherited-file payoff, coupling, prerequisites, and validation
- [x] Define the single-worker-per-chunk claim, implementation, verification, and recovery protocol
- [x] Dispatch one fresh `gpt-5.6-sol` medium worker for the first eligible chunk
- [ ] Verify each worker's scope, diff, tests, metrics, and ledger update before dispatching the next chunk
- [ ] Rerank after every completed chunk and append delta chunks when live branch or worktree changes invalidate the survey
- [ ] Close only when every surveyed candidate is complete, rejected, deferred with evidence, or below the stopping threshold, and the final residual surface is recorded

## Initial repository state

- Branch: `cmake`
- Frozen starting `HEAD`: `0498798fc927581626c3f5978e219c68e64990c0`
- Current `upstream/main` tip: `6e76c5d8dafd02dc32a1c6312ce9440dc95b1aca`
- Merge base: `fb555eec75e1ed12c8348805ab335afb4c721b06`
- Live D1/D2 integration-pressure diff against `upstream/main`: 362 files, `+52729/-5055`, comprising 37 added and 325 modified paths
- Existing uncommitted work at campaign start is outside D1/D2 and must be preserved; exact paths will be recorded in the ledger

## Completion standard

- Every inherited modified D1/D2 path is represented by a candidate chunk, an explicit retained-policy classification, or a below-threshold batch
- Every branch-added D1/D2 sink is checked for unnecessary duplication or misplaced feature ownership without treating its raw size as inherited-file merge cost
- Each processed chunk records before and after inherited-file metrics, changed paths, tests, limitations, and the next reranked candidate
- No chunk leaves only one of a paired D1/D2 path changed without an explicit game-specific reason
- Remaining credible extractions are individually about 20 to 40 inherited lines, are high-risk relative to payoff, or would move authoritative engine policy out of its proper owner
- Windows D1/D2 builds, configured Android ABI builds, scoped quality checks, and focused integration coverage pass for the final accumulated state, with any environment-only gap called out explicitly
