# Chunk 0502 MIDI multiplayer and callback consumer ingress, 2026-10-16

Diagnosis and plans only, serial work under plan_general_diagnosis_resume_20261009.md

Reviewed10explicit consumer contexts1573overlapping lines, including whole native MIDI preview and Kotlin bridge, plus5whole existing owners. Exact file/scoped identities are retained; all12assigned current source identities and all5canonical ledgers remain unchanged

Progress: temp/general_cleanup_20261006/gq2-consumer-ingress-0502-20261016.json
SHA271fd95c3fa8691384d4ee374764e82e35f59781b541d8b8c2f47041bc5a1ba8
Independent byte/scope/source/canonical/whole-owner replay: PASS53
Audit: temp/general_cleanup_20261006/gq2-consumer-ingress-audit-0502-20261016.json
SHA6fbfcaf66214d4cc786a86f7475f2752787e85336414a322808029f7f69a9dd4

## Actual consumers and owner boundaries

- Native renderer applies latest queued seek under playback mutex. Approximate seek reconstructs controller/program/pitch/note/sustain state without intervening PCM, restarting sounding envelopes by explicit preview policy. Source-string checks and Python lock model do not execute this native concurrency
- Native UI seek queues with control lock/atomics; snapshot reads separate atomics rather than a coherent generation tuple. Whole Kotlin bridge serializes seek/getState/start/stop/effects through lifecycleLock, so native nonblocking alone does not prove end-to-end UI responsiveness while synth load/start owns the bridge lock
- Bounded JNI preview calls and D2-only CMake source placement match the library loaded by the bridge, keeping one state owner. Other JNI metadata functions are not given whole-file credit. Initial clipped bridge/JNI read was recovered with bridge tail and JNI1-130
- Actual producer sets playing/output_enabled false before final ring write and callback then refuses ring reads while reenqueuing silence. Whole OPEN BR0276 already owns final tail, producer-position and quiescent completion. No new root
- Actual checked OpenSL graph publication and reverse failure cleanup are retained. Whole archived BR0248 remains FIXED with historical SDL-specific validation limits; do not reopen that repair or infer complete MIDI EOF acceptance from checked initialization
- Stop releases playback mutex before render join/OpenSL shutdown, then frees MIDI/reset state. Pause/resume use control lock and atomics. Callback enqueue failure is logged; complete existing audio/lifecycle/raw quality reconciliation remains pending
- Actual generator completion callback creates failed records for missing summary/start error/timeout, copies declared metadata atomically, and retains returned records. A nonempty summary is not locally rejected solely for nonzero process exit. The synthetic six-case fixture does not exercise conflicting exit/summary, malformed/mixed summaries or real scheduling. Whole result owners must be reconciled before admitting a defect
- Actual ADB helper returns output without native exit status and lacks guaranteed process disposal. Send-MpCommand discards result. Wait-ForCondition checks time outside the callback; nested ADB can exceed remaining outer budget. Get-GameIntrospection offers Fresh, but multiplayer calls omit it and clear snapshot before launch only. Correlation/lifetime owners remain pending
- Aggregate currently gives multiplayer240seconds plus explicit0/90second soak and passes SoakSeconds. Preserve that repair and qualify historical BR0658 no-override wording. Child total TimeoutSeconds remains unused and phase budgets are independent; whole current deadline reconciliation pending
- Whole OPEN BR0656 already owns unqualified send_sync fallback and skipped paired/soak checks. Whole OPEN BR0657 already owns nonfatal chat with unconditional successful exchange banner. Whole OPEN BR0667 owns missing travel fields and checked0 PASS. No duplicate findings

Only source gate is closed; consumer/owner/disposition/publication remain open. Next metadata normalization/intent/names/statistics/flyout/provenance/trigger-cycle producers and dispatch, removed corpus/current replacement, remaining whole correlation/process/deadline/result/audio owners and raw quality/prior plans. Canonical501DONE126TODO627 unchanged. No product/test/helper edits or execution, build, formatter/generator/device/network/resource/security probe, new finding/status/rating/runtime/original-saving claim, canonical evidence append, Drive copy, staging or commit. Broader goal active
