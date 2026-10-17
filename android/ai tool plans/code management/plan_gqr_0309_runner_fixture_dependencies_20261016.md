# Proposed GQR-0309 runner fixture dependency repair, 2026-10-16

Diagnosis and conditional plan only. Proposed GQF-0323 / GQR-0309, not yet admitted to canonical ledgers. P3/high, test-infrastructure/source-integration; PRIMARY 34 LOW, H/M/B/C/R 12/0/2/10/10. No product execution or code changes

Problem: test_test_runner_result.ps1 extracts production functions from run_all_tests.ps1 without starting the suite. Current extracted Save-SuiteTestEvidence calls Write-SuiteEvidenceJournal, and extracted recovery calls Install-SuiteTarget. The fixture imports neither helper. The journal command-resolution error is caught and sets evidenceFailed before Write-TestExecutionEvidence, and the fixture explicitly throws on that flag. Recovery has a separate missing installer binding. The full aggregate defines both helpers

Both helpers are absent from frozen b4997ac4115a3b2ac6d6cf0b8e1459675a7744ef. The current fixture's only later delta adds an infrastructure timeout mock, so the changed production dependencies were not integrated into this fixture. This is source-confirmed current branch integration failure, not runtime reproduction or undefined globals in the full runner

Conditional implementation:
1. Add Write-SuiteEvidenceJournal and Install-SuiteTarget to the existing selective AST import list. Retain actual production function bodies and the focused fixture boundary
2. Initialize TestBudgetSeconds=0, completedTestSeconds=0, an explicit disabled or fixture-owned EvidenceJournalPath, and suiteRequiresStandardGameData=true for the existing Install-AppAndData failure matrix
3. Keep external installer/device responses controlled. Do not replace the new dispatch helpers, Save-SuiteTestEvidence or journal with unconditional-success functions, and do not source the full aggregate
4. Preserve the existing PASS/FAIL/TIMEOUT result object, counters, exact RunId observations, timeout log, provisioning failure and serial-preservation checks, and emulator-only APK downgrade allowance
5. Consider a focused media-independent installer-dispatch case only if it adds a distinct real branch assertion without device provisioning

Future verification after diagnosis restrictions lift: run the standalone fixture in a clean host PowerShell process and require its final success plus all retained failure/timeout controls. Exercise an enabled fixture-owned journal if included and confirm equivalent evidence observations. Confirm maintained aggregate discovery still selects the test. No device or full-suite provisioning is needed for this local repair; apply repository scoped quality to actual changed files

Deduplication: BR-0543 resource retirement and BR-0169 aggregate false-success have different mechanisms. GQF-0158 bounded Python discovery, GQF-0162 extraction lexical false-pass, GQF-0313 FM generated-source transforms and GQF-0314 selected renderer fixture are separate causes. Exact path/symbol searches in prior code-management plans, active/done BR, quality done and DMR1 yielded no earlier owner outside this chunk. The 322-title census is mechanical support, not a claim of fresh complete review of every finding

No inherited-file edit or saving. Production runner helper redesign and generic dependency extraction are unnecessary for the concrete repair. Canonical admission remains pending exact publication; no IDs or totals changed by this plan
