# GQR-0277: Propagate completed Setup command failures, 2026-10-10

Diagnosis and implementation plan only. GQF-0291 OPEN / GQR-0277 TODO; P2/high, score44 (H/M/B/C/R=23/0/4/10/7). No product/test changes or runtime reproduction

## Concrete defect and boundary

A valid LauncherScriptExecutor setup_command/import_iso with an absent iso_path calls importIsoImageFromPath, which returns -1. SetupImportTracker.complete records failed, but the command returns normally from runIo. Its ordered PendingResult retains initial result0. The executor accepts that result, increments currentStep and can write PASS when the script ends. The same acknowledgement boundary loses CD negative results and caught CD/ISO exceptions; missing preview source/data and false playback starts also return normally.

GOG's negative-count check and current import_mod's nonnull/enabled check already reach runIo's explicit failure result. Preserve those counterexamples. This finding does not claim the executor ignores explicit failure codes, all imports falsely pass, or maintained extraction tests lack status assertions.

Prior GQ1-0505 explicitly preserved exception-to-result propagation without certifying every command. BR0224 owns invalid/unknown script schema; BR0284 owns script replacement/run identity; BR0482 owns game-start acknowledgement; BR0190 owns host ADB failure; BR0456/0472 own bytes/registries. Archived BR0020 owns native partial-success and complete extraction publication, whose current negative failure is already correct in this witness. None needs reopening to fix this command boundary.

## Implementation

- Define the result at the existing command-operation boundary: completed, valid empty/no-op when that command permits it, failed with reason, and cancelled. Keep native extractors and game-format semantics in their current owners.
- Carry the actual outcome from each recognized asynchronous command to one ordered completion path. Convert negative extraction counts and caught ordinary failures into failure; never infer success merely because a lambda returned normally.
- For imports, use one private asynchronous operation to own begin/terminal tracking, the captured file set, actual work lifetime, ordered completion and one final refresh. Format-specific extraction and audio-registration callbacks remain explicit. Resolve zero-file/cancellation policy per command rather than treating every zero as failure.
- Give the existing tracker an attempt identity so an older job cannot replace a newer terminal snapshot. Preserve the actual writer until it settles; coroutine cancellation is not proof of native IO retirement. Coordinate with existing storage/preview owners, not a new general command framework.
- Keep SETUP_COMMAND_RESULT_FAILED propagation and launcher step failure intact. Where an operation intentionally acknowledges admission before long-running completion, use an explicit correlated terminal outcome instead of reusing success; do not introduce an unbounded wait or assume Android ordered receivers may live indefinitely. Existing deadline/request identity plans remain BR0287/0284.
- Audit the assigned CD/ISO/SOW and MIDI/CD preview routes against this result policy, retaining preview generation reservation and serialized native controls. Pilot batch failure remains BR0236; game handoff remains BR0482.

## Acceptance before closure

Use a maintained Android integration runner through the real Setup receiver and LauncherScriptExecutor, not just a result-mapping unit test. Add the missing-ISO witness with a following observable sentinel action: require failure at the import step, failed tracker for that attempt, no sentinel execution and durable FAIL. Repeat negative CD/SOW results, caught ordinary import exception, missing preview data and rejected playback start through the actual command routes. Preserve successful ISO/CD/GOG/mod controls and documented empty/no-op semantics.

Exercise cancellation, overlapping attempts, active-set switch and Activity recreation through their existing owners; require no stale tracker overwrite, wrong-set audio registration, stranded completion or old-writer cleanup. Keep this work to ordinary missing-source/controlled-failure cases; deferred malformed-media/security/resource-pressure probes remain separate.

Existing test_extract.ps1 uses asynchronous host dispatch and independently rejects import_state.failed; retain that behavior. test_anniversary_content_import.jsonc additionally waits for complete and asserts content/readiness. The workflow source fixture checks asynchronous dispatch and run identity; it does not prove ordered command failure. Extend rather than replace these useful checks. Register any new top-level runner using the repository automation catalog conventions, then perform scoped quality, applicable JVM/native builds and the actual Android integration. No acceptance is credited until executed in an implementation tranche.

## Inherited boundary

SetupActivity and this result policy are Android-owned and absent from the 1996 tree. Preserve native failure values, extraction publication and preview control authority. No D1/D2 original-file edits or inherited savings are needed.
