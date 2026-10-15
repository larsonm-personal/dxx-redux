# Nightly tooling sweep

## Deployment and trust

This change does not activate a schedule until the workflow is on the repository's default branch. The default was `main` when designed; the intended simple deployment is to make `cmake` default and merge the reviewed implementation there. Neither action is performed by these scripts. If `main` remains default, a separately reviewed default-branch deployment is required; copying only a cron stanza without its trusted controller/reusable workflows is insufficient.

The coordinator resolves `refs/heads/cmake` once and passes that immutable commit to every executor. Controller scripts and the committed admission policy come from `github.workflow_sha`, separately checked out from the target. PR validation instead pins the PR head and runs a bounded representative host/public-emulator batch. PR jobs have read-only repository permission and cannot publish progress.

Only the isolated publish job has contents-write permission. It runs no target checkout or target script. It validates receipts against the exact plan, policy, target commit, catalog hash, batch ID and lane, then uses normal fast-forward Git Data API updates to `ci/nightly-tooling-state`. It never pushes `cmake`, changes the default branch, merges, or rewrites history. A publication conflict is refetched/reconciled once; otherwise the run reports failure and leaves work safely repeatable.

## What “all” means

The master `android/run_all_tests.ps1 -ListTests -ExtendedGraphics` remains authoritative discovery. A committed admission policy adds reviewed hosted environments and estimates to exact IDs. Unadmitted catalog owners are explicitly listed with their requirements; they are not silently counted as covered.

Execution reuses existing machinery:
- Master PowerShell owners use exact-name selection, full child-corpus switches and existing infrastructure/reporting.
- Existing audited public-shareware scenarios use `run_shareware_tests.ps1` with fixed exact scenario and game. Its assertions and integration owners are unchanged. All 29 public scenario owners are represented as 55 game-specific selections.
- Only the two audited no-media instrumentation owners can omit standard retail provisioning. Their exact identity, type, infrastructure and empty arguments are checked. Published-document and future variants stay media-required.
- Host adapters invoke existing unittest modules, source scripts, native CTest and the original full/light extraction CMake entry point. They do not invent replacement product tests.
- JVM setup runs the existing Gradle owner including metadata-core and historical proxy comparison; Android uses x86_64 debug/instrumentation APKs and KVM-backed headless emulators.

Retail campaigns/media, physical GPU/audio/touch behavior and physical multi-device topology are explicit exclusions. Software-rendered emulator success does not establish physical-device behavior. Full extractor and light extractor retain separate IDs and omitted/media-dependent leaves remain visible.

## Budget and progress

A nightly batch has at most 2700 estimated test seconds across lanes. Setup, downloads, native/Gradle builds and cleanup are reported separately and are not included in this test estimate. Each hosted lane has a finite 90-minute ceiling. A running owner may finish beyond its estimate; before another owner starts, the executor/master checks consumed test time. This is an approximate 45-minute test budget, not a promise of 45-minute total wall time.

Planning never advances coverage. A per-test JSONL journal records running/terminal observations. The always-run collector recovers completed observations after a failed/interrupted batch. Missing, running and cancellation records remain pending. Actual test assertion failures and test timeouts are retained as failed attempts and do not rerun indefinitely; unresolved failures remain in subsequent summaries.

Transient setup/download/emulator problems remain retryable. Consecutive observations are retained with run links, and never-attempted owners sort ahead of repeatedly blocked work so one bad environment cannot starve other lanes. Stable missing capabilities of the immutable target are reported as exclusions, not coverage. A changed target or admitted catalog starts a fresh sweep.

When no eligible work remains, subsequent nights skip executor jobs until the revision/catalog changes. The compact state branch keeps owner outcomes, bounded reasons/exclusion summaries, leaf counts and run links, not tracebacks or full artifacts. Identical state is not recommitted. Detailed logs, JSONL journals, leaf results and Gradle XML/HTML remain workflow artifacts. Summary distinguishes terminal owner attempts, verified leaf counts, unknown/incomplete leaf coverage, retained failures, retryable infrastructure, explicit exclusions and overall sweep outcome. A passing aggregate owner does not prove skipped child checks ran.

## Qualification

Local PowerShell execution remains unavailable by prior instruction. Portable Python state-machine tests cover unchanged selection, partial interruption, failed attempts, actual timeout, retryable setup, exhausted sweep, changed SHA/catalog, malformed ledger, wrong provenance, unknown/duplicate entries and command injection. Workflow YAML/actionlint and shell syntax are checked locally. Existing hosted tooling checks run the portable controls and real catalog/helper tests. The new same-repository PR workflow exercises authoritative catalog export, representative host owners and real Android build/emulator/public-shareware owners without publishing state. Hosted results are required before claiming PowerShell or emulator runtime qualification.

## Admission status and first hosted validation

The initial 158 selections are a first runnable slice, not all repository tests. [Source reconciliation](../tests/NIGHTLY_COVERAGE_RECONCILIATION.md) distinguishes admitted owners, their children, explicit manual exclusions, and the still-unreviewed backlog. Pending audit does not imply unavailable.

Read-only PR validation selects seven existing owners (2,610 estimated test seconds): catalog contracts, controller UI, D1 public death scenario, full JVM owner, full extractor, strict audio fingerprint, and the lazy D1 native CTest adapter. Cold setup/build time is additional and bounded by job timeouts. These hosted paths remain unqualified until their actual jobs pass.
