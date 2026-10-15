# Chunk0423 conditional demo guide continuation, 2026-10-14

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. This document proposes later work; it does not authorize implementation or report runtime acceptance.

## Current source conclusions

The assigned README diff preserves all package/output table cells and changes the two known_versions filename references from json5 to jsonc. Retain that diff. Current guide additions describe a separate public native SOW fixture route; preserve its pinned package and output identities and offline admission policy.

The current Android extractZipContents function (SetupFileImport.kt:213-394) stages selected ZIP entries, rejects colliding direct outputs and duplicate SOW volume basenames, accounts streamed bytes against the extraction budget, then passes the complete SOW path collection to DiscImportBridge.extractSowArchives with one native extraction attempt. It checks the native result, accepts the attempt budget, rejects collisions between direct and SOW outputs, and requires the named package's expected file set. Cancellation cleans the attempt and propagates. This is source inspection, not executed resource, malformed-archive or transactional-publication proof.

installDemoArchive (789-825) dispatches Mac versus ZIP extraction, rejects extraction errors, empty results and missing named-package files, stages copied outputs and manifest entries, then delegates publication. Its source does not prove the delegated publication helper's durability or concurrency contract. Existing BR0008/BR0233 remain open.

The StuffIt CMake oracle consumer explicitly rejects a nonzero native extractor result and checks each present expected file's exact length and digest. It still returns success for an absent oracle, CMake before 3.19, or an entirely missing archive corpus. BR0158 already owns this false-pass distinction. Do not treat a green invocation without a checked-count receipt as extraction coverage.

BR0586 concerns a different generator, generate-stuffit-corpus-manifests.ps1 and its reference-tool result handling. The checked child result in test_stuffit_demo_oracles.cmake is not evidence that BR0586 has been repaired or closed.

The Mac helper's missing-package branch records OracleFailures for required oracle archives but continues without incrementing failures. The final oracle guard preserves the previous oracle on missing/failed required archives, while the final exit depends on failures. This remaining requested-refresh result gap belongs to BR0169; preserve the oracle-preservation repair. The complete-success oracle write still delegates no atomic generation transaction, retaining BR0233's publication obligations. Do not revive the historical claim that partial archive results always overwrite the oracle.

## Conditional implementation

1. Correct the Android model's SOW step to describe one whole-volume assembly call with the shared extraction attempt, rather than sorted per-volume extractSowFiles append calls. Preserve preamble handling, continuation semantics, cancellation, budget accounting, collision admission and required outputs.
2. Distinguish the existing test_stuffit_direct build target from the registered stuffit_demo_oracle_tests CTest name. Replace the already-completed small-CLI proposal with its actual maintained entry point. Keep proposed -UseInTree switching explicitly future work until implemented and verified.
3. Keep public direct SOW fixture setup, DOSBox-generated reference trees and Mac external-tool oracle refresh as separate workflows. Preserve their distinct D1 PIG sizes and hashes; do not normalize one output into another's reference.
4. Under existing BR0158, require explicit required/optional fixture profiles, exact pass/skip/fail accounting, and at least one actual checked fixture before a passing coverage claim. Exercise absent oracle, old CMake, zero/partial/complete archives, failed extraction, missing output, wrong size and wrong digest. No checks have been run here.
5. Under BR0169, make an explicitly requested incomplete oracle refresh observable as failure while leaving the prior oracle unchanged. Cover each required archive absent independently, all absent, complete success and extractor failure. Keep ordinary optional-package behavior separate.
6. Under BR0233, require complete generation and publication with rollback, unchanged-byte/time preservation and appropriate durability/concurrency policy. Validate with controlled failure injection only in a later authorized implementation tranche.

## Remaining diagnosis gates

Finish current package/known_versions/hash_assets/reference-JSON reconciliation, upstream archive/resource/tool ownership and prior-plan deduplication before terminal disposition. Preserve the unchanged current DOS/Mac helper repairs already noted in the primary plan. No new root, remediation, rating, investigation, inherited saving or canonical DONE status is admitted by this continuation.
