# Plan: D1/D2 Diff-Minimization Campaign 2026-07-11

## Goal
- Build a substantial, evidence-backed catalog of remaining D1/D2 cleanup and extraction candidates
- Process the best candidates in measured tranches until the remaining strong candidates offer only about 20 to 40 lines of upstream-owned reduction each

## Existing work to preserve
- Preserve completed shared-helper extractions and upstream-sync work from earlier rounds
- Preserve the active `gamecntl.c` save/load dispatch extraction and finish its validation first
- Preserve unrelated mission metadata, level metadata, bug-list, and workspace changes
- Keep desktop behavior and upstream compatibility intact

## Campaign phases
- [ ] Re-read and synthesize all prior D1/D2 shrink studies, execution plans, and tranche reports
- [ ] Refresh the live `upstream/main` diff inventory for original D1/D2 files
- [ ] Build a broad candidate catalog with concrete blocks, payoff, coupling, risk, target helper, and tests
- [ ] Reconcile stale historical candidates against already-completed work and current source growth
- [ ] Finish and validate the active `gamecntl.c` save/load dispatch tranche
- [ ] Rank remaining candidates by net upstream reduction, semantic confidence, and validation cost
- [ ] Process high-value low/medium-risk candidates one tranche at a time
- [ ] Re-measure and rerank after every completed tranche
- [ ] Stop only when the remaining good seams are approximately 20 to 40 lines each or are too coupled to justify extraction
- [ ] Record the final residual catalog, deferred risks, validation evidence, and aggregate reduction

## Candidate acceptance criteria
- Prefer branch-added behavior in files original to upstream
- Prefer mirrored or behaviorally identical D1/D2 blocks
- Require a shared boundary materially smaller than the duplicated implementation
- Keep private engine structs and inherited converter/render logic local unless a stable adapter already exists
- Avoid abstractions whose callbacks or declarations erase most of the line-count payoff
- Preserve ordering, flags, ownership, cleanup paths, and platform guards exactly
- Require focused integration coverage for behavior centralized under `android/`

## Validation policy
- Run scoped format and lint checks without broad-formatting upstream files
- Build Windows D1 and D2 after every source tranche
- Build both Android games across configured ABIs for shared native changes
- Run focused D1/D2 emulator coverage sequentially
- Record environment-only validation blockers separately from source failures
- Preserve and explicitly identify unrelated dirty-worktree changes

## Campaign baseline
- D1/D2 aggregate before the active `gamecntl.c` tranche: 341 files, +49969/-3880 against `upstream/main`
- Active `gamecntl.c` candidate baseline: D1 +455/-9, D2 +728/-13
- Active extraction target: 310 additions from the paired dispatch bodies, plus safe redundant include cleanup
