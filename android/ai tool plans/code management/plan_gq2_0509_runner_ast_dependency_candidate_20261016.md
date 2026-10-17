# Chunk 0509 runner AST dependency candidate, 2026-10-16

Diagnosis and conditional plan only. Unadmitted candidate UNADMITTED-RUNNER-AST-DEPENDENCIES; no GQF/GQR ID, finding total, status or priority changed. Source-confirmed call/import mismatch; no test or child process executed. Existing-owner reconciliation remains required before admission

## Concrete trigger and current behavior

Run android/tests/test_test_runner_result.ps1 in its normal clean PowerShell process. The fixture parses run_all_tests.ps1 and imports only the functions explicitly listed in its Names array. It imports Save-SuiteTestEvidence, Invoke-SingleTest and the recovery functions, but omits Write-SuiteEvidenceJournal and Install-SuiteTarget. Neither omitted name occurs anywhere in the fixture

Current Invoke-SingleTest calls Save-SuiteTestEvidence before launching a synthetic child and again after retaining its result. Save-SuiteTestEvidence calls Write-SuiteEvidenceJournal before Write-TestExecutionEvidence. Without that imported helper, command resolution fails inside its try block, catch sets script:evidenceFailed, and the fixture's explicit evidence-failure assertion cannot pass. The later mocked recovery coverage also calls functions that now delegate to the absent Install-SuiteTarget helper

This is a fixture binding problem, not evidence that the full aggregate runner lacks the helpers. The real runner declares both and initializes its journal, budget and standard-data state. Existing PASS/FAIL/TIMEOUT and provisioning assertions should be preserved

## Conditional local repair

After diagnosis restrictions are lifted:

1. Add Write-SuiteEvidenceJournal and Install-SuiteTarget to the existing selective production-function import list. Keep whole-runner execution out of the fixture so no real suite provisioning starts
2. Initialize the imported functions' new state explicitly: disabled test budget, zero completed test seconds, deliberate journal path or disabled journal, and suiteRequiresStandardGameData=true for the existing Install-AppAndData failure matrix
3. Keep the actual new helper implementations under test. Do not replace them, Save-SuiteTestEvidence or the provisioning functions with unconditional-success stubs
4. Preserve the existing three exact result/counter/log/run-ID observations, failed-provisioning serial preservation and emulator-only APK downgrade assertions. If adding the media-independent branch, drive the real Install-SuiteTarget with explicit state and a controlled existing installer response
5. Verify the standalone fixture completes and produces the expected observations. Keep the existing synthetic failure/timeout controls meaningful and confirm the ordinary aggregate still selects this test. No device, full-suite initialization or speculative helper framework is needed for the focused check

## Ownership and evidence limits

Bounded searches found no exact helper names or test-specific finding in the active/archived BR and quality ledgers, but that is not a complete ownership reconciliation. Review whole relevant infrastructure/publication owners and the existing source-integration fixture roots before admitting a separate local cause. Preserve current runner and timeout-mock repairs

Current HEAD af62c4bd356c624d4007a81a7b654224e1ac1124. Exact current fixture, production call ranges and helper source identities are bound in gq2-consumer-ingress-0509-20261016.json. No runtime reproduction, product edit, process probe or inherited saving is claimed

Progress SHA256 cb47986a65470614ef180478bf301ce5dfe41413a6c6883994d6df4fc7dff524

Independent exact identity/binding audit PASS 70; audit SHA256 e07f1374e6dac47208932cde45ac57d0a25183990113855c259044be00976fbb
