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
- [x] Re-read and synthesize all prior D1/D2 shrink studies, execution plans, and tranche reports
- [x] Refresh the live `upstream/main` diff inventory for original D1/D2 files
- [x] Build a broad candidate catalog with concrete blocks, payoff, coupling, risk, target helper, and tests
- [x] Reconcile stale historical candidates against already-completed work and current source growth
- [x] Finish and validate the active `gamecntl.c` save/load dispatch tranche
- [x] Rank remaining candidates by net upstream reduction, semantic confidence, and validation cost
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

## Campaign refresh outcome
- Working catalog: `d1d2_diff_candidate_catalog_20260711.md`
- Completed active game-control reduction: 324 additions from inherited files
- Completed DXA-mask reuse: 37 additions removed from D2 OGL
- Completed masked bitmap scaler move: 97 additions removed and two inherited headers returned to the upstream form
- Completed OGL runtime texture controls: 302 inherited additions removed, with both GL-thread dispatch paths exercised in both games
- Completed coop multi-status move: 414 inherited additions removed, with all six Android game/ABI targets linked
- Completed Android EGL lifecycle: 326 inherited additions removed; both games passed background/resume with recreated surfaces
- Completed automap metadata overlay: 313 inherited additions removed; objective and secret labels passed focused dual-game coverage
- Completed Android scaled linear font renderer: 252 inherited additions removed; readability and scale regressions passed
- Stable aggregate before the active input-demo-probe tranche: 339 files, `+48217/-3887`, a reduction of 1,741 additions from the refreshed `+49958/-3886` inventory
- Completed input-demo diagnostic centralization: 1,053 inherited additions removed across collision, AI, FVI, controls, physics, and render probes
- Completed HUD-count extraction: 203 inherited additions removed
- Completed SDL-mixer diagnostics extraction: 174 inherited additions removed
- Completed live-difficulty extraction: 214 inherited additions removed
- Completed effect-runtime extraction: 168 inherited additions removed
- Completed coop-start fanout extraction: 126 inherited additions removed
- Completed coop restore remapping extraction: 130 inherited additions removed
- Completed shader-runtime extraction: 96 inherited additions removed
- Completed jukebox-name extraction: 62 inherited additions removed
- Completed Android fatal-error bridge: 57 inherited additions removed
- Completed texmerge-owner diagnostics extraction: 190 inherited additions removed
- Completed kconfig scaled-render extraction: 148 inherited additions removed
- Completed OGL viewport and keyboard-gap extraction: 136 inherited additions removed
- Completed safe OGL MSAA frame-lifecycle slice: 66 inherited additions removed
- Isolated reduction from these post-refresh tranches: 2,823 inherited additions
- Live aggregate after concurrent feature work: 339 files, `+46506/-3891` against `upstream/main`
- Current selected tranche: rerank the remaining low-risk feature seams against the 20-40-line endpoint
