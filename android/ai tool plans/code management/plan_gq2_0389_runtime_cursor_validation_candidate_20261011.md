# GQ2 chunk 0389 runtime cursor validation candidate, 2026-10-11

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. Candidate only: no canonical finding/remediation admission, implementation or runtime proof yet.

## Concrete failure and source evidence

Get-RuntimeSampleState in android/helpers/runtime_targeted_sampling.ps1:65-79 catches JSON read/parse failures and initializes a false-valued rings member, but accepts any truthy rings value without requiring an IDictionary. ConvertFrom-CompatibleJsonHashtable in android/helpers/powershell_compat.ps1:78-89 preserves JSON value types and does not validate the sample-state schema. A readable document such as {"schema":"dxx-runtime-sample-v1","rings":1} therefore reaches Select-RuntimeHashRingItems:108 with rings as a number. The numeric value has no dictionary Contains method. That call is outside the loader's catch, so the intended unreadable/unrecoverable-state fallback does not cover this readable wrong-shaped document. Set-RuntimeSampleCursor:86 separately indexes the same unchecked value for mutation.

The master runner actually reads ReportDir/runtime_sample_state.json at android/run_all_tests.ps1:1182-1184 before test execution. Regeneration forwards a persistent state path to four wrappers at android/regenerate_all_regression_data.ps1:219-246,518-519. This is an actual retained input contract, not an unused generic helper. No fixture or runtime probe has been executed in this tranche; the failure route is a static type/control-flow conclusion.

The original test_failures_and_hash_ring_sampling_20260818.md requires a reported random fallback when prior state cannot be recovered. It explicitly resumes after the last selected target. Retain selection-time advancement, budget crossing, grouping, explicit seed reproducibility and valid cursor recovery.

## Proposed focused implementation

Validate the deserialized root and rings container before returning state to either reader or writer. Use the current dxx-runtime-sample-v1 contract directly. Validate selected ring records and last_target before using them as cursor identity. Invalid or unsupported state should produce a precise warning and the documented seeded/random fallback rather than an uncaught method/property/index exception. Preserve other valid rings according to an explicit policy instead of resetting arbitrary valid state silently.

Keep engine files untouched. Put schema admission in the shared sampler, so production and maintained callers use one implementation. Preserve PowerShell 5.1 and modern PowerShell compatible dictionaries and existing ordinal JSON-key handling. Do not add old-state migrations or compatibility readers.

## Actual-entry future acceptance

Extend android/tests/test_runtime_targeted_sampling.ps1 against the production helper with valid state, missing file, malformed JSON, root null/scalar/array, rings number/string/array, malformed ring record, invalid last_target, wrong schema, vanished target, and a valid cursor with unrelated rings. Assert reported fallback and deterministic seeded selection for unrecoverable states, exact next-target recovery for valid state, no pre-selection test-runner failure and preservation of independent valid rings under the selected validation policy.

Exercise the actual master/regeneration selected entry paths with isolated caller-supplied ReportDir/state paths. Keep fixtures private and owned; the current sampling fixture's fixed state path and regeneration fixture's fixed tree remain existing BR-0173 ownership acceptance. Do not infer broad integration correctness from an AST-extracted function alone. No tests or source edits are authorized in this diagnosis tranche.

## Ownership separation and remaining admission work

BR-0622 already owns non-mutating dry-run behavior, including sampling-state changes recorded by GQ1-CHUNK-0613. BR-0233 owns checked atomic publication/prior-generation preservation. BR-0173 owns unique temporary/session resources and cleanup. State publication using PID.tmp without exclusive read-modify-write ownership and guaranteed temporary cleanup should be reconciled with those owners rather than bundled into this candidate's readable-state validation root.

BR-0159 owns atomic validated tool installation, BR-0569 host/tool lookup identity, BR-0596 formatter failure propagation, BR-0597 source scope coverage, BR-0600 physical containment, BR-0538 exclusive formatter admission and BR-0603 verified process termination. Preserve completed BR-0157 dependency-authentication repair; new formatter version policy must not reopen its specifically completed source/oracle acquisition locations.

Before canonical admission, finish deduplication against historical chunk0613/GQR-0230 and any retained current-state review, record exact frozen/current scope and qualification, and derive the applicable rating from the ledger rubric. This candidate is distinct only if readable malformed cursor recovery was not already assigned to an existing root. Final complete consumer assessment and canonical publication remain pending.
