# GPU timer and MSAA diagnosis continuation, 2026-10-09

Diagnosis only. GQ2-CHUNK-0085 remains TODO until fixture/owner reconciliation and immutable terminal import/verification. No product/test/script changes or execution acceptance. Frozen base7877ad30d05887b8e19869ed4c50075e41e2f88e to headb4997ac4115a3b2ac6d6cf0b8e1459675a7744ef; current HEADb661b6eb24e91adf5f3e566bc28eb73d4b679fda.

- [x] Read all assigned timer hunks1-4/new35-77, MSAA hunks1-35/new10-465 and header hunks1-6/new15-63
- [x] Read complete frozen timer77/MSAA482/header90 plus current physical deltas; recover MSAA349-388 after truncated combined output
- [x] Trace paired context-name reset, start/flip timer lifetime, MSAA readback/presentation, runtime option destruction and drawable-size callers
- [x] Read complete timer stub fixture and shared GPU policy fixture; trace actual capability shared sample/format admission and report publication
- [x] Trace graphics safety failure callback and named EGL replacement-context forget ordering plus actual automation fault arm
- [x] Finish maintained MSAA allocation-failure/context-loss/smoke assertion review and diagnostics interpretation; preserve archived FIXED BR0647, do not reopen from stale earlier ledgers
- [x] Reconcile BR0260 current source repair without automatic closure, OPEN BR0251/GQR0174 EGL/shader admission, and producer allocation/failure-publication ownership
- [x] Verify frozen/base/original attribution, exact hunk/range identities and current bindings; publish/import report and terminal records, verify counts/ranks/hash

## Findings to retain during continuation

Timer checks active query before starting another view; it reads oldest result only after availability and skips a sample when the ring is full. Paired renderer ends once before resolve/swap. Context-loss callers zero query IDs/cursors/in-flight/sample instead of deleting retired IDs in the new context. Stub fixture asserts delayed ring saturation without result read, repeated begin guard, wraparound/recycling and disjoint sample suppression. It was inspected, not executed. BR0260 remains OPEN in the active bootstrap ledger; current-source repair is already recorded historically, not fresh runtime closure.

MSAA policy uses actual supported window channel tuples and shared color/depth count, selecting the smallest common count at least requested. Global max alone is deliberately ignored. Format mismatch refreshes current capabilities, then unsupported format/size/sample, creation, bind and resolve failures latch and notify graphics safety. Valid color/depth sample counts must match. Validated generation increments only after actual creation checks. Context forget zeros names before destroy helper, avoiding deletion of retired-context objects in new context. Runtime option pending application destroys old FBO to allow requested count changes even when dimensions match.

Nested frame depth gates resolve; later same-frame views rebind while color clear is only first bind. Resolve captures error, unbinds, updates success serial or failure latch and reports CPU resolve duration. Diagnostic create_complete/last_frame_resolved have explicit object/serial checks, but they are not whole-frame visual success or proof of rollback. Trace is log-enabled and bounded by presented flip count; scene errors are recorded separately. Earlier broad ledger calls BR0647 open, but authoritative archive marks it FIXED: preserve its accepted oracle repair and inspect current tests before making a new claim.

Graphics safety renderer_failed takes a matching active persisted trial ID, captures failure state once outside restoration, logs evidence outside mutex and calls reject decision after releasing lock. No trial ID means no decision. JSON/string allocations in this error path are not visibly contained by that callback; allocation/failure-publication ownership still needs reconciliation, not an unsafe injected experiment or generic duplicate finding. Existing accepted backup/descriptor repairs GQR0128/0129 remain DONE.

Actual EGL named paths invoke smash under discarding_lost_context_resources only after replacement context is made current and before shim initialization; shader initialization return/admission remains separate GQR0174/BR0251 acceptance. No whole EGL source coverage from those ranges.

No builds/configure, formatter/generator, driver/device/runtime/deferred malformed/security/allocation/resource probes, staging or commits. Concurrent multiplayer work preserved. No terminal credit or new finding/status change at this checkpoint.

Fixture follow-up checkpoint is recorded in plan_general_diagnosis_resume_20261009.md with exact partial ranges and current hashes. Remaining failure-callback ownership requires actual enclosing-boundary reconciliation before terminal import; no allocation execution or closure inferred.

Existing BR0251 acceptance extended by terminal report and canonical observation: optional diagnostic allocation/publication must not bypass required rejection, exported callback failures contained, durable outcome truthful. No whole-stack termination claim or new root; GQR0173 introspection scope remains distinct. Terminal imported as GQC1054/GQD0934; final verification gate remains pending until checks pass.

Final verification passed: immutable report/import body/SHA, ordered scope fingerprint, all45 frozen hunks/diff hashes,17 current bindings,226DONE/419TODO main queue,269 findings/255 remediations unchanged statuses,1041 unique sorted terminal ranks/255 remediation ranks,HEAD and scoped tracked diagnosis whitespace check. All gates complete; next0086, no product acceptance.
