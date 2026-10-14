# Headed GuideBot recovery projection candidate, 2026-10-13

Diagnosis and proposed implementation plan only under GQ2-CHUNK-0393. No code/test edits, execution, canonical finding/ID admission, rating or inherited saving. Candidate needs final consolidated owner/dedup and publication gates.

## Concrete producer/consumer mismatch

route_confirmation.cpp record_objective_and_replan1036-1089 records route_step_index, kind, activation_kind, is_switch_restorer, restored_wall_num and trigger_num from the live State.step. prepare_next_goal670-671 marks unfired-trigger access work as LEVEL_METADATA_ROUTE_RECOVERY_ACCESS. The result struct keeps these fields, and route_confirmation_result.cpp emits access_for_route_step for access recovery, restores_switch_wall for other restoration, and trigger for trigger objectives.

game_introspect.cpp2003-2059 instead constructs each objective with only route_step_index/kind/activation_kind/label/frame/seconds. It omits all three recovery/trigger fields. regenerate_all_guidebot_simulations.ps1's headed route reads introspection.route_confirmation and feeds it into the same compact converter as headless file output. Test-GuidebotObjectiveProjectionMatch122-178 requires those fields to distinguish valid additional recovery work from completion or replacement of the authored objective.

Source-static witness: the maintained schema fixture's access case has authored step1 kind exit/activation enter_exit, then actual trigger recovery index1/shoot_switch/trigger6/access_for_route_step1, followed by the required exit. The full native serializer preserves access_for_route_step and trigger; the converter recognizes the additional work and accepts the required exit. Introspection removes those fields, so the converter takes the ordinary objective branch and compares actual trigger against expected exit, returning false and publishing route_mismatch even for a native confirmed result. The switch-restoration fixture has the analogous restores_switch_wall omission and mismatched activation. This is source/control-flow evidence, not an executed game or fixture result.

## Proposed implementation and simplification

Preserve the conditional native recovery/trigger projection at both producer paths. Prefer extracting this small field projection into the existing native route result component and reusing it from introspection, rather than duplicating another serialization policy in PowerShell/Kotlin or merging complete result documents. Keep native objective names, timing/rounding, detailed/raw versus compact fields, ordinary objective shape, ordering and D1 guards distinct where their current contracts differ. Check the existing D2 target registration and Android build path before exposing a small C++ helper; do not add a new subsystem or mirror engine layouts.

The current D2 main source list includes route_confirmation_result.cpp. That bounded build context alone is not a complete Android target/link review. If common projection cannot be reused without broad coupling, explicitly preserve the same conditional fields in the current introspection producer with actual cross-producer parity coverage. Do not add no-op aliases or application-format migrations.

## Actual-entry future acceptance

- Feed a production native route summary with access recovery and a required exit through the real file/introspection producers; require exact access_for_route_step/trigger and compact status parity
- Cover switch restoration with its planned shoot switch, wrong/missing restored wall, access for another step, invalid/missing trigger, recovery-only output, out-of-order work, repeated completed prerequisites and ordinary objectives
- Preserve existing converter rejection of saved-world/objective-only results and existing confirmed/prefix/route_mismatch semantics; no loosening of objective validation to hide lost metadata
- Extend maintained schema/parity coverage to call the production producer boundary, not only hand-built JSON or regex checks, then exercise actual Headed and Headless runner paths on a native recovery case with fresh correlated result capture
- Run appropriate D1/D2 Android and native host builds, scoped quality/catalog checks and existing result/schema/runner/publication/parity fixtures during implementation; no such commands run in diagnosis

## Ownership reconciliation and limits

GQF0186/GQR0173 own producer exceptions, acquired buffers and truthful failed publication; BR0233 owns durable output; BR0173/0543 own temporary/process/device lifetime. None describes this valid native recovery-field omission and downstream route_mismatch. Existing GQF0301/GQR0287 owns selected target correlation; BR0668 owns duplicate corpus identities. Preserve those statuses and do not close them from this candidate. Existing schema fixture explicitly accepts recovery work while requiring the original authored objective, so the planned correction restores that current contract.

Source-only candidate, no broad gameplay failure claim and no runtime reproduction. Authored Android/native helper paths only; no inherited engine diff or saving proposed. Final root admission and controlling local rating remain pending complete consumer qualification.


## Final admission proposal

Propose NEW GQF-0306 OPEN / GQR-0292 TODO, P2/high,47MEDIUM (H23/M0/B4/C10/R10), after consolidated consumer and canonical publication audit. H23: a valid confirmed headed recovery result can be relabelled route_mismatch. M0: authored native/script paths only. B4: focused headed/native result contract. C10: actual recorder, both serializers, shared converter and maintained schema witness are explicit. R10: local native field projection and existing producer/schema/parity coverage, no prerequisite. This is a source-static finding proposal, not runtime reproduction or current canonical admission.

No current GQF recovery/introspection/route record covers this valid-field omission; GQF0186/GQR0173 covers exceptions/publication, GQF0301/GQR0287 selection and BR0668 duplicate corpus. Complete current root descriptions, source chain and existing owners preserve their statuses. Keep the candidate's47 independent from unit controlling GQR0114REFERENCE56, reached by the assigned RAR dispatcher. No score summation or inherited saving.

Required result: native headed/file objective payloads preserve trigger and conditional access/restoration fields and compact classification agrees for valid recovery. Retain strict wrong/missing/out-of-order objective rejection; do not repair parity by suppressing route_mismatch. Actual production producer and headed/headless future acceptance remains as listed above. Canonical admission/fix rank and chunk publication pending.


## Chunk 0393 published and independently audited, 2026-10-13

GQC1222ISSUES/GQD1102NO_INHERITED_EFFECT;newOPENGQF0306/TODOGQR0292 headed recovery projection47MEDIUM source-static finding and actionable native producer/parity plan. No prior finding/remediation/investigation status changes. All750assignedRETAIN/1003wholecurrent/253extra separated/raw=frozen/wholeadded1003/baseoriginalabsent/saving0. Controlling same-owner GQR0114REFERENCE56 retained; no rating sum.

Report SHA4e543d9b52542430cce5b9cf7105d137a796f866f77e7ce8c34c8e40ad6e84af; immutable snapshot SHAa49eb80ad307443273054a5aa82fd6326fe407a68e74f0fe714ad510898a7470; independent publication PASS4442 SHA354a3eab9cb575d6bdd48f8dc4d7e61b97f588ce11266b0c7fdc9ef909009799 proves nine-operation/five-ledger replay, all prior entire root/queue bytes and rank suffix/sorts, exact report/snapshot/154qualification artifacts/current mapped contexts/full definitions/assigned source/frozen/baseoriginal/manifest/pre-finalplans/HEADproduct. No runtime execution. Final primary gate checked only afterPASS and five current canonical identities reverified. Published snapshot/mutation immutable.

GQ2now393DONE234TODO627;1209terminal ranks292fix ranks306findings292remediations12numbered investigations. Supplemental investigation resume rows are preserved separately, not additional numbered investigations. NextGQ2-CHUNK-0394 regenerate_all_guidebot_simulations.ps1L751-L1003/253assigned tail. Broader remaining queue/sweeps/investigations/current-head supplements/finalclosure pending;goalactive.
