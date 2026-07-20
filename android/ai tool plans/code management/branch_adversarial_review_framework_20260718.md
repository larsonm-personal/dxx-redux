# Plan: Branch Adversarial Review Framework 2026-07-18

## Goal
- Define a deterministic, resumable review process for the complete `cmake` branch diff
- Make each review unit suitable for one independent `sol-5.6-medium` call
- Store actionable findings, dispositions, evidence, and progress in one canonical review ledger
- Give the campaign an explicit completion condition instead of stopping after a convenient sample

## Existing work to preserve
- Preserve the user's uncommitted change in `android/outstanding_bugs.md`
- Treat `upstream/main` as the intended PR target, while pinning an exact merge-base commit for each campaign generation
- Do not modify product code while designing the review framework
- Follow `.github/copilot-instructions.md`, including Android-port boundaries and paired D1/D2 review requirements

## Framework phases
- [x] Read repository instructions and record the dirty-worktree boundary
- [x] Identify the local branch, likely PR target, merge base, and aggregate diff size
- [x] Research current AI-assisted PR review guidance and extract applicable checks
- [x] Classify changed files by reviewability, provenance, risk, and generated/vendor status
- [x] Define deterministic chunk generation and stable chunk identifiers
- [x] Define per-call input, context expansion, review method, and no-finding output requirements
- [x] Define the single-file finding schema, lifecycle, deduplication, and checkpoint rules
- [x] Define cross-file sweeps, validation passes, and a measurable campaign completion gate
- [x] Create the canonical review process and initial ledger artifacts
- [x] Dry-run the standards against representative files from the branch
- [x] Record final framework decisions and exact startup instructions

## Initial baseline
- Branch: `cmake` at `c01d8fe4686c63d931b1e543a6305bbafaa944a9`
- Intended upstream remote: `https://github.com/dxx-redux/dxx-redux.git`
- Current `HEAD` merge base with both `origin/main` and `upstream/main`: `fb555eec75e1ed12c8348805ab335afb4c721b06`
- Current three-dot diff: 2,638 files, 672,291 additions, 4,047 deletions
- Current upstream divergence: `upstream/main` has 23 commits after the merge base; the branch has 1,076 commits after it
- Uncommitted user-owned file at framework start: `android/outstanding_bugs.md`

## Design constraints already established
- Review the pinned merge-base-to-branch diff so later upstream movement cannot silently change completed chunks
- Inventory every changed path, but distinguish authored source from generated data, binaries, fixtures, plans, and vendored material
- Keep review comments in one canonical Markdown file with stable IDs and explicit status values
- Require every chunk to end in a recorded reviewed-with-findings or reviewed-no-findings state
- Add separate integration sweeps so per-file review does not miss duplicated logic, ownership boundaries, build wiring, or end-to-end failures

## Validation
- [x] Confirm all generated chunk IDs are deterministic across repeated runs at the same baseline and head
- [x] Confirm every changed path is either assigned to at least one review chunk or explicitly classified with a recorded reason
- [x] Confirm no chunk exceeds the context budget without a documented split
- [x] Confirm the ledger can resume from interruption without relying on chat history
- [x] Confirm the dry run produces enough source and line evidence for another agent to validate each finding

## Framework outcome
- Canonical process: `branch_adversarial_review_process.md`
- Canonical single-file ledger: `branch_adversarial_review_ledger.md`
- Deterministic generator: `android/helpers/new_adversarial_review_ledger.ps1`
- Initial source and mechanical chunks: 599
- Initial queue including preflight, sweeps, and closure: 617 calls
- Changed-path coverage check: all 2,638 paths represented
- Repeated-generation comparison: identical after normalizing the generation timestamp
- PowerShell quality result: scoped `android/run-code-quality.ps1 -Fix` passed
- Encoding result: ASCII content, UTF-8 without BOM, LF endings, and final newlines
