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
- [-] Process all 30 chunks with one fresh read-only `gpt-5.6-sol` medium worker per chunk, up to three disjoint workers concurrently
- [ ] Normalize one durable `GQD-*` decision and one `GQC-*` coverage record per chunk, plus only deduplicated secondary findings
- [ ] Reconcile actionable minimization candidates against DMR1, the first weighted tranche, and existing GQ/adversarial owners
- [ ] Report candidate, retained, deferred, and no-effect counts with named inherited paths and estimated inherited-line/hunk reduction
- [ ] Verify all 30 reports are imported exactly once, no inbox or weighted temp report remains, no worker is active, and campaign files pass tracked ASCII/BOM and `git diff --check` audits

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
