# GQR-0291 planned runtime cursor validation, 2026-10-12

Proposed GQF-0305/GQR-0291 admission for GQ2-CHUNK-0389. Diagnosis and plans only; canonical admission/publication pending. P2/high, score47 MEDIUM (H/M/B/C/R23/0/4/10/10). No inherited saving.

## Problem and actual input path

A persisted sample state with {"schema":"dxx-runtime-sample-v1","rings":1} parses successfully. Get-RuntimeSampleState65-79 checks only truthiness of rings and returns this numeric member. Select-RuntimeHashRingItems108 calls rings.Contains outside the loader's recovery catch; a number has no dictionary Contains method. A readable wrong-shaped state therefore aborts selection instead of warning and using the documented fallback. Set-RuntimeSampleCursor86 also indexes the unchecked container. This is a static type/control-flow conclusion, not an executed runtime result.

The production master selects from ReportDir/runtime_sample_state.json at current android/run_all_tests.ps1:1200-1202. Regeneration forwards its persistent state path to maintained CD/fingerprint/metadata/simulation wrappers. ConvertFrom-CompatibleJsonHashtable78-89 preserves JSON scalar/array/dictionary types; it does not provide the sampler's application schema admission.

## Focused implementation

Validate the root, current schema, rings dictionary, selected ring record and string last_target before any consumer method/index operation. Invalid state must yield a precise warning and the documented reproducible seeded fallback. Preserve valid unrelated rings under an explicit policy, reject unsupported schemas directly and retain ordinal JSON key handling across PowerShell5.1 and modern PowerShell. Do not add migrations for obsolete application state.

Keep valid prior-target recovery, disappeared-target fallback, budget crossing, group interleaving, empty input behavior, seed semantics and selection-time cursor advancement. The August18 maintained plan explicitly advances after the last selected target; do not change this to completed work.

## Future acceptance

Extend the maintained production-helper fixture for valid and missing state, malformed JSON, root null/scalar/array, rings numeric/string/array, malformed selected ring record, invalid last_target, wrong schema, vanished target and valid unrelated rings. Assert deterministic seeded fallback with a warning, valid next-target recovery and preservation of unrelated valid state. Exercise the actual master/regeneration entry paths with private caller-supplied ReportDir/state paths; no AST-only fixture may stand in for production entry acceptance. Run on Windows PowerShell5.1 and supported modern hosts.

No execution or product/test edit in this tranche. Publication faults and concurrency probes remain separately deferred under their existing owners.

## Deduplication and boundaries

BR-0622 and historical GQ1-CHUNK-0613 own preview/unselected-family cursor mutation. GQR-0230 exact generation selection is DONE and does not validate cursor schema. BR-0233 owns durable atomic publication and prior generation preservation; BR-0173 owns private temporary/fixture ownership; BR-0543 owns guaranteed runner resource cleanup. The new readable-state admission root does not duplicate those mechanisms. Their status and completed repairs remain unchanged.

The obsolete targeted/proportional/fraction policy cleanup is a separate authored simplification candidate in plan_gq2_0389_existing_owner_continuation_20261011.md. None of the eight assigned helper paths exists in the 1996 tree. Do not move any game-format knowledge or change inherited D1/D2 files.

## Readiness and completion

H23: a corrupted readable persisted state prevents the focused production sampling workflow. M0: authored paths only. B4: focused sampling workflow across maintained callers. C10: concrete numeric input and exact escaping consumer call. R10: local validation with an existing maintained helper/entry fixture to extend, no prerequisite. This rating is proposed until the final consumer and canonical publication audits.

Completion requires actual type/state controls and production entry verification, not only changing a parser guard or observing a green unrelated formatter test. Queue remains TODO until implementation acceptance; diagnosis chunk completion does not imply remediation completion.
