# Plan: General Code-Quality Chunked Round 2026-08-11

## Goal

- Reduce branch impact on 1996-original and other inherited files as the dominant objective, using roughly 80 percent of survey effort for diff attribution, extraction, consolidation, revert, and retained-boundary decisions
- Use the remaining roughly 20 percent for the broader branch code-quality survey without discarding concrete high-severity defects
- Survey the full branch-owned change surface and relevant whole-codebase context for correctness, safety, ownership, duplication, maintainability, portability, warnings, diagnostics, tests, automation, scripts, build logic, documentation drift, and upstream merge cost
- Produce a review-scale ledger expected to contain hundreds of atomic findings or explicit coverage records, comparable in granularity and auditability to the branch adversarial review
- Process accepted remediation chunks one at a time with a fresh `gpt-5.6-sol` worker at medium reasoning effort, under a single-product-writer rule

## Scope correction

- The existing `DMR1` queue is retained as the inherited D1/D2 extraction subtrack, not treated as the general cleanup queue
- Previous survey stopping rules such as a 20-40-line extraction threshold apply only to diff-minimization opportunities. They do not suppress small correctness, cleanup, warning, test, diagnostic, portability, or dead-code findings
- High coupling is a risk attribute and may require smaller investigation or fixture chunks. It is not a blanket code-quality disposition
- Every reviewed path or coherent path family must receive explicit coverage, even when it yields no remediation finding
- Finding count is not a target by itself. The survey must use adversarial-review granularity so unrelated issues are not collapsed into a few broad categories

## Plan

- [x] Record the user-requested scope correction before beginning the expanded survey
- [x] Analyze prior cleanup work logs, reusable cleanup instructions, warning/lint campaigns, next-30 queues, and the adversarial process and ledger
- [x] Diagnose exactly why the initial queue produced only nine implementation decisions
- [x] Freeze a new survey generation against the live HEAD, target ref, dirty-worktree boundary, and branch diff
- [x] Define review domains and coverage units for the full repository and branch-owned diff
- [x] Run initial independent methodology, process, and live broad-survey passes across every repository domain
- [x] Normalize the initial live observations into 23 atomic findings, 50 historical rechecks, ten bootstrap coverage records, and ten bounded remediation candidates
- [x] Create a general code-quality process, active ledger, and done archive modeled on the adversarial review, retaining DMR1 as a linked subtrack
- [x] Deduplicate the initial live findings against adversarial findings and prior cleanup work without suppressing regrowth or incomplete fixes
- [x] Generate and inventory 750 deterministic range/path chunks and 819 total initial coverage calls for 3,136 changed paths
- [ ] Process every deterministic coverage and recheck unit with one fresh read-only `gpt-5.6-sol` medium worker per chunk
- [ ] Normalize each completed coverage output and create explicit clean, issue, investigation, or partial records
- [ ] Reconcile every final-head path against current non-partial coverage through delta generations
- [x] Dispatch a fresh `gpt-5.6-sol` medium worker for the first eligible general remediation chunk after preflight and live overlap checks: `GQR-0001`
- [ ] Dispatch fresh `gpt-5.6-sol` medium workers for the first eligible remediation chunks, one chunk per worker and one product writer at a time
- [ ] Root-review every terminal chunk, rerank after repository-generation changes, and append new findings discovered during remediation
- [ ] Close only when every survey unit and finding has a terminal evidence-backed disposition

## Current progress

- [x] Process and normalize `GQ1-PREFLIGHT-001`; result `ISSUES`, one new P1 supply-chain finding, one retained P0-risk investigation, and duplicate/evidence extensions without ID inflation
- [x] Process `GQ1-PREFLIGHT-002` with a fresh read-only `gpt-5.6-sol` medium worker
- [x] Process `GQ1-PREFLIGHT-003` with a separate fresh read-only `gpt-5.6-sol` medium worker
- [x] Process deterministic coverage unit `GQ1-CHUNK-0001` with a third fresh read-only `gpt-5.6-sol` medium worker
- [x] Normalize `GQ1-PREFLIGHT-002` as the sole canonical-ledger writer
- [x] Normalize `GQ1-PREFLIGHT-003` as the sole canonical-ledger writer
- [x] Normalize `GQ1-CHUNK-0001` as the sole canonical-ledger writer after the preflights
- [x] Root-review and accept terminal `GQR-0001`; correct its inherited diff from net +15 lines to five one-line substitutions with zero net line growth

## First 30 deterministic coverage tranche

- [x] Count `GQ1-CHUNK-0001` as already completed and normalized
- [x] Process `GQ1-CHUNK-0002` through `GQ1-CHUNK-0030` with one fresh read-only `gpt-5.6-sol` medium worker per chunk, up to three disjoint workers concurrently
- [x] Normalize every raw report in strict chunk-ID order before later workers use its findings for deduplication
- [x] Create one non-partial `GQC-*` record per completed chunk, including explicit clean results
- [x] Reconcile observations, finding and investigation IDs, duplicate links, queue counts, and remediation candidates after Chunk 0030
- [x] Verify all 30 queue rows are terminal, no survey worker remains active, reports are ASCII/no-BOM, and campaign files pass `git diff --check`

## Next 100 deterministic coverage tranche

- [x] Migrate the three audits, three preflights, and `GQ1-CHUNK-0001` through `GQ1-CHUNK-0039` from ignored `temp/` reports into one tracked SHA-256-provenance evidence ledger
- [x] Change the worker process so new reports enter a tracked inbox and become terminal only after single-writer import into the durable evidence ledger
- [x] Process `GQ1-CHUNK-0031` through `GQ1-CHUNK-0130` with one fresh read-only `gpt-5.6-sol` medium worker per chunk, up to three disjoint workers concurrently
- [x] Normalize every raw report in strict chunk-ID order before later workers use its findings for deduplication
- [x] Create one non-partial `GQC-*` record per completed chunk, including explicit clean results
- [x] Reconcile observations, finding and investigation IDs, duplicate links, queue counts, and remediation candidates after Chunk 0130
- [x] Verify all 100 queue rows are terminal, no survey worker remains active, reports are ASCII/no-BOM, and campaign files pass `git diff --check`

## Diff-minimization-weighted 30-chunk tranche

- [x] Record the user correction that diff minimization, not general finding discovery, is 80 percent of the effort goal beginning with `GQ1-CHUNK-0108`
- [x] Amend the worker protocol so every chunk begins with frozen diff attribution and emits a measured `CANDIDATE`, `RETAIN`, `NO_INHERITED_EFFECT`, or `DEFER` assessment
- [x] Process `GQ1-CHUNK-0108` through `GQ1-CHUNK-0137` with one fresh read-only `gpt-5.6-sol` medium worker per chunk, up to three disjoint workers concurrently
- [x] Normalize one durable `GQD-*` diff-minimization decision and one `GQC-*` coverage record per chunk, plus only deduplicated secondary findings
- [x] Reconcile every actionable minimization candidate against DMR1 and existing GQ/adversarial owners before creating implementation work
- [x] After Chunk 0137, report candidate count, retained/deferred/no-effect count, named inherited paths and estimated inherited-line/hunk reduction, separately from general-quality findings
- [x] Verify all 30 reports are imported into the tracked evidence ledger, no inbox fragment or worker remains active, and campaign files pass `git diff --check`

## Diff-minimization-weighted second 30-chunk tranche

- [x] Continue the 80 percent diff-minimization and 20 percent general-quality protocol for `GQ1-CHUNK-0138` through `GQ1-CHUNK-0167`
- [x] Process all 30 chunks with one fresh read-only `gpt-5.6-sol` medium worker per chunk, up to three disjoint workers concurrently
- [x] Normalize one durable `GQD-*` decision and one `GQC-*` coverage record per chunk, plus only deduplicated secondary findings
- [x] Reconcile actionable minimization candidates against DMR1, the first weighted tranche, and existing GQ/adversarial owners
- [x] Report candidate, retained, deferred, and no-effect counts with named inherited paths and estimated inherited-line/hunk reduction
- [x] Verify all 30 reports are imported exactly once, no inbox or weighted temp report remains, no worker is active, and campaign files pass tracked ASCII/BOM and `git diff --check` audits

## Impact scoring and remediation ordering

- [x] Define a reproducible 0-100 impact rubric that prioritizes user/security/data-loss risk, original-file merge-pressure reduction, breadth, confidence, and implementation readiness
- [x] Score every terminal `GQ1-CHUNK-*` coverage unit by its highest-impact canonical fix owner, assigning explicit low scores to clean, duplicate-only, retained, or no-effect units
- [x] Add a durable descending ranking with score components, canonical owner, and concise rationale without duplicating remediation ownership
- [x] Require future survey workers to recommend a provisional score and the canonical writer to confirm or replace it during normalization
- [x] Reconcile score counts and ties, verify every terminal chunk appears exactly once, and run ASCII/BOM plus `git diff --check` audits

## GQR-0162 ranked remediation

- [x] Freeze the live overlap boundary and claim `GQR-0162` / `GQF-0175` for one fresh `gpt-5.6-sol` medium worker
- [x] Extract shared headless target construction into a branch-added CMake module while preserving explicit D1/D2 differences
- [x] Measure inherited-file paths, hunks, and lines before and after, rejecting any ownership inversion or non-material result
- [x] Validate CMake formatting/lint, exact target properties, option combinations, Windows D1/D2 headless builds, and focused metadata/replay fixtures where available
- [x] Review the worker patch, correct any scope or parity defect with the same worker, and close the ledger items only after final acceptance

## GQR-0143 ranked remediation

- [x] Freeze the eight-path live accessor boundary and claim `GQR-0143` / `GQF-0156` for one fresh `gpt-5.6-sol` medium worker
- [x] Move only the identical introspection accessor definitions and declarations into narrow branch-added owners, retaining private layouts and APIs
- [x] Measure the isolated inherited paths, hunks, and lines before and after and reject any callback table, public-layout expansion, or cosmetic movement
- [x] Validate source contracts, D1/D2 Windows builds, Android `INTROSPECT_ON` builds, and focused menu introspection/automation behavior
- [x] Review and correct the worker patch with the same worker, then close the ledger items only after final acceptance

Completion: after the regular-source follow-up, the eight inherited paths contain narrow opaque-layout snapshot adapters and public-header includes. The isolated inherited delta is `+30/-112`, a net reduction of 82 lines; the eight-path upstream view falls from `+2686/-262` to `+2604/-262`, also reducing branch-attributed additions by 82. The ordinary shared `.c` files own the nine public accessors, and no private layout is moved, mirrored, or exported

## GQR-0143 regular source follow-up

- [x] Audit why the initial extraction required type-complete `.inc` fragments and compare ordinary `.h`/`.c` alternatives
- [x] Replace every introspection `.inc` with regular shared headers and sources without moving or mirroring private engine layouts
- [x] Keep narrow opaque-type snapshot adapters in the paired owning translation units and preserve the existing public accessor API
- [x] Register shared sources only in branch-added Android build ownership and update focused contracts
- [x] Re-run focused tests, Windows D1/D2 builds, Android ABIs, scoped quality, and exact inherited-diff accounting

Completion: removed all three `.inc` files and replaced them with `game_menu_introspect_accessors.h/.c` and `game_window_introspect_accessors.h/.c`. The paired private-layout owners now populate small snapshots; the ordinary shared sources implement the unchanged public accessor API. Focused contracts, Windows D1/D2, both headless targets, and all three Android ABIs passed. The final 82-line inherited reduction is smaller than the fragment-based result by design, because private layouts remain solely in their original translation units

## GQR-0142 ranked remediation

- [x] Select rank 3 from the durable impact order and verify its paired PhysFS scope is disjoint from active rank-2 product edits
- [x] Claim `GQR-0142` / `GQF-0155` for one fresh `gpt-5.6-sol` medium worker and freeze the live inherited/shared boundary
- [x] Extend the existing branch-added PhysFS shared owner while preserving per-game directory setup and desktop behavior
- [x] Validate exact behavior, paired Windows builds, Android ABIs, focused tests, scoped quality, and inherited diff reduction
- [x] Root-review metrics, scope, validation, and terminal ledger state

Completion: the paired inherited Android PhysFS initialization surface fell from 40 lines across six hunks to 14 lines across four hunks, exactly removing 26 inherited lines and two hunks. The shared owner now preserves checked PhysFS initialization, symbolic-link setup, search-path diagnostics, Android argument initialization, and order; D1/D2 retain only their game-directory calls. Focused source and runtime contracts, paired Windows builds, all configured Android ABIs, scoped quality, whitespace, and encoding audits passed

## GQR-0156 ranked remediation

- [x] Select rank 4 from the durable impact order and verify the paired Redbook headers are clean and disjoint from active work
- [x] Claim `GQR-0156` / `GQF-0169` for one fresh `gpt-5.6-sol` medium worker and freeze exact declarations and consumers
- [x] Move the identical multi-source declaration block into one conventional branch-added shared header while retaining unconditional visibility
- [x] Replace each inherited block with one include while preserving platform guards, C linkage, types, and API parity
- [x] Validate focused contracts, Windows D1/D2, Android ABIs, scoped quality, exact metrics, and terminal records

Completion: `rbaudio_android.h` is the sole conventional declaration owner, and each inherited game header retains one include at the original declaration position. Against the campaign merge base, the paired inherited headers fall from 22 additions to four additions, an exact 18-line reduction; the isolated edit is `+2/-20`. Focused C/C++ contracts, D1/D2 Windows builds, all three Android ABIs, scoped quality and final audits passed. The maintained SAF Redbook automation stopped before playback at its pilot-menu navigation step, so it did not add runtime evidence beyond the successful compile/link boundary

## Initial hypotheses to verify

- The first survey searched primarily for paired D1/D2 duplicate bodies large enough to extract, rather than applying the reusable general cleanup rubric
- It ranked only likely implementation chunks and omitted review findings that needed investigation, local cleanup, test changes, warning fixes, or no-code dispositions
- It collapsed hundreds of paths into broad retained-policy and below-threshold categories without path-level records
- It reused diff-minimization stopping thresholds as global stopping thresholds
- It surveyed branch-added sinks only as extraction destinations instead of reviewing their internal quality
- It did not partition the repository into review domains or assign independent coverage passes comparable to the adversarial review
- It treated completed historical campaigns as categorical exclusions without checking for regrowth, adjacent defects, or new changes

## Required artifacts

- Expanded process: `general_code_quality_worker_process.md`
- Expanded ledger: `general_code_quality_ledger_20260811.md`
- Done archive: `general_code_quality_ledger.done.md`
- Survey reports and generated inventories: `temp/general_code_quality_20260811/`
- Linked diff-minimization subtrack: `d1d2_diff_minimization_ledger_20260811.md`

## Completion standard

- The ledger contains atomic, independently actionable chunks or explicit no-change coverage entries, rather than a small list of broad themes
- Every branch-modified path is mapped to at least one survey domain and coverage record
- High-risk and inherited paths receive findings sized around one defect, one ownership seam, one warning family, or one validation boundary
- Branch-added files are reviewed internally rather than merely counted as sinks
- Prior findings are linked by stable identifiers and rechecked for completion or regrowth
- The first implementation batch is selected from the expanded ranked queue only after live overlap and prerequisite checks
