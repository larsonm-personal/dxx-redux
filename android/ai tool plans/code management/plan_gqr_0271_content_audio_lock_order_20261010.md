# GQR-0271 proposal: Content and audio lock order, 2026-10-10

Diagnosis and implementation plan only. Proposed NEW GQF-0285 / GQR-0271 from GQ2-CHUNK-0342; canonical admission remains pending. P1/high, correctness/concurrency. Provisional impact56 (H/M/B/C/R=32/0/7/10/7). No product/test/helper edit or deadlock/stress execution.

## Concrete source witness

Two distinct process-wide monitors are acquired in opposite orders:

- FileSetContentManager.setEnabled current194-203 holds CONTENT_LOCK and calls writeState682-688, which calls AtomicFilePublication.writeUtf8. reconcile, entry publication and projection updates use the same content-before-publication order. AtomicFilePublication.kt9-17 implements the second monitor with synchronized(lock).
- AudioSourceManager.pruneMissingSources369-397 starts AtomicFilePublication.transaction, then checks local sources through resolveExistingFile530-545. A source absent from the ordinary root but present in managed content reaches FileSetContentManager.resolveFile292-316, which acquires CONTENT_LOCK while the publication monitor is still held.

Concrete admitted input is an ordinary managed CUE/BIN bundle registered after adoption. AudioSourceManagerPersistenceTest.kt already builds that input and exercises pruning while the content is disabled, but only sequentially. No malformed input, provider failure or invented wrapper is needed.

If content mutation holds CONTENT_LOCK, audio pruning acquires the publication monitor, and each then requests the other monitor, both threads remain blocked. SetupSections.kt400-407 dispatches content enable changes to IO; SetupActivity.kt3543-3568 performs startup/refresh reconciliation on IO and then invokes pruning from the composition effect. These operations have separate scheduling and can overlap. SetupSections244-250 also reconciles independently. The locks are process-wide, so operations on distinct sets still contend. Blocking monitor acquisition has no timeout or cancellation escape. When the refresh thread participates, the launcher can hang/ANR; other registry/content operations can also stop behind the same monitors. This is a source-derived conditional interleaving, not a reproduced device hang.

Branch causation is explicit. FileSetContentManager is absent at the GQ2 base. AudioSourceManager frozen hunk17 introduces the publication transaction around pruning; hunk36 adds the managed-content fallback beneath it. Base pruning and local lookup do not acquire this pair in opposite order. Current AudioSourceManager bytes equal the frozen blob.

## Smallest complete implementation

Use the existing FileSetContentManager.withContentLock boundary outside the existing AtomicFilePublication.transaction in pruneMissingSources, preserving the established content-before-publication order. Keep reload, resolution, row selection and save inside the coherent mutation boundary. Do not remove either lock, perform an unlocked stale snapshot/save, add a second transaction framework or use timing sleeps/timeouts as the fix.

Census all current callers and any other publication-locked path reaching a content API before landing. Current production prune callers are SetupActivity startup/delete-content and SetupSections confirmed deletion; the reviewed calls do not already hold the publication monitor. Preserve FileSetManager/ModManager's content-before-publication order and content manager reentrant calls. Document the shared order next to the small existing boundary so later callers do not reintroduce inversion.

Allowed product surface: AudioSourceManager.kt; a concise invariant comment in FileSetContentManager.kt or AtomicFilePublication.kt only if needed to make the shared order explicit. No d1/d2 changes, provider/format migrations, general lock replacement or unrelated UI/state-recovery rewrite.

## Acceptance

Extend the maintained AudioSourceManagerPersistenceTest.kt/FileSetContentManagerTest.kt coverage with a production-backed concurrent case. Seed a registered managed CUE/BIN owner, preserve disabled-content retention, and overlap actual pruning with actual content enable/reorder or reconciliation publication. Use explicit barriers and bounded child-JVM supervision so a regression cannot deadlock the entire test worker or retain poisoned global monitors for later tests.

The fixture must control an admitted production interleaving, not implement a second model of the lock graph. One possible existing seam is withContentLock: hold it in the content worker, start real prune on another worker, establish that pruning is blocked attempting the content monitor, then perform the real content state write and release. The corrected order lets the write acquire publication and both operations finish; the old order blocks publication. Verify monitor/thread state or a narrow test seam rather than relying on an arbitrary sleep. Do not wrap pruning externally in the publication transaction: that would manufacture the forbidden order even after the product fix.

Require both operations to finish within the deadline, exact persisted enable/order and audio rows after recreation, no unexpected prune for disabled managed files, truthful removal for genuinely missing files, and continued unrelated registry writes. Cover both arrival orders, separate manager instances, same and distinct sets, and ordinary root-only/SAF-retained controls. A test that merely observes no hang under a short random loop is insufficient.

Run scoped Kotlin quality, focused JVM tests and the relevant Android build after implementation. Exercise actual setup refresh while toggling managed content on the emulator with a deadline and visible responsiveness checks. Register any new top-level integration runner and its master timeout. No such execution is authorized or claimed in this diagnosis tranche.

## Deduplication and separate owners

Canonical active/done quality and adversarial ledgers, inherited minimization ledger, imported evidence and prior code-management plans were searched for the two monitor names and content/audio lock-order/deadlock roots. Existing BR-0472 owns persistence/corruption/ownership outcomes, BR-0103 extraction generation, BR-0186 replacement, and BR-0506 launch generation; none identifies this opposite monitor acquisition. They remain separate acceptance work. GQR-0212 resource bounds do not repair a blocking lock cycle. Keep plan_gq2_0342_content_state_recovery_20261010.md separate.

No inherited-file reduction is claimed. The fix remains in authored launcher ownership code and preserves shared publication helpers and native authority.
