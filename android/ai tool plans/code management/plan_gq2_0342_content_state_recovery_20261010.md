# GQ2 chunk0342: File-set content state recovery, 2026-10-10

Diagnosis and implementation plan only. Extend existing BR-0472 launcher registry recovery and coherent ownership. No new finding/remediation ID, status or rating. No product/test changes or runtime fault execution.

## Source-confirmed boundary

FileSetContentManager.kt672-680 maps both an absent state file and every read/JSON failure to emptyMap. repairState654-669 then constructs an entry for every stored owner, defaults missing enabled flags to true and order to sorted owner IDs, and writes the replacement whenever it differs. Both listEntries93-98 and reconcile53-91 call this repair. Therefore, given existing disabled/reordered owners and a malformed content_state.json, an ordinary listing or reconciliation discards the prior state document and publishes default enabled/order state without an error or recovery decision. The condition follows directly from the source; no corruption was injected or runtime behavior measured.

SetupSections.kt244-250 and SetupActivity.kt3543-3564 reconcile during ordinary refresh. Launch preflight725-770 calls reconcile and rejects exceptions/conflicts, but the swallowed state error supplies neither; buildLaunchPaths then selects enabled owners. The current exception boundary at SetupActivity.kt790-842 is a repair to preserve, not evidence that ordinary write failures universally escape launch.

Existing BR-0472 requires distinguishing invalid registry state from intentional empty state, retaining recoverable ownership and avoiding silent replacement. Prior GQ1-CHUNK-0552 already assigns content state/projection/audio coherence to BR-0472/0186/0103. This plan adds precise state-load evidence and acceptance for that existing owner.

## Smallest complete change

- Make state loading distinguish valid state, genuinely absent state, invalid content and unavailable storage. Validate the current disposable schema as a whole; reject duplicate or malformed owner IDs and invalid enabled/order values instead of silently coercing a partial document.
- Keep the current first-use missing-state behavior and defaults for genuinely new owners after a valid load. Do not treat a read/parse failure, inaccessible path or wrong file type as first use. Preserve bytes and stored ownership on failure, and return a typed recovery-required/unavailable outcome before repair can publish replacement state.
- Preserve CONTENT_LOCK and existing atomic, synced state writes. Under that same owner, recover a verified last-valid current-schema generation if available; otherwise expose an explicit recovery action. A recovery action may rebuild current state after preserving the invalid artifact, but ordinary listing must not silently enable previously disabled content.
- Keep entry-manifest/payload recovery distinct from enabled/order recovery. A recoverable payload does not prove its prior enable policy. Do not infer state from alphabetical order, stale UI data or a different active set.
- Propagate recovery status through reconcile/listing consumers, launch preparation and visible setup controls. Retain prior coherent UI data with a clear unavailable/recovery indication, block launch that lacks a valid policy, and avoid pruning audio or changing projections from an invalid authoritative inventory.
- Keep the schema disposable: replace current structures directly as necessary; no legacy compatibility readers or format migrations.

Expected implementation surface is FileSetContentManager.kt plus the smallest existing result/caller handling in SetupActivity.kt and SetupSections.kt, and other callers that currently consume a bare list. Reuse AtomicFilePublication only to the extent it actually preserves the chosen recovery generation; its temporary backup is normally retired after replacement and is not by itself a durable last-valid-state store. Keep engine-format parsing and d1/d2 files out of this launcher fix.

## Focused acceptance

Extend the production-backed FileSetContentManagerTest.kt cases instead of duplicating state repair in a test helper. Retain the existing genuinely missing-state case383-408, valid enable/order changes, new-owner default, movie-merge identity/state preservation and two-manager serialization. Add disabled/reordered multiple-owner setups with truncated JSON, invalid types/IDs/order, duplicate IDs, unreadable/non-file state paths and unavailable set storage. Ordinary list, reconcile, UI refresh and launch preflight must preserve invalid bytes and prior payloads, avoid default-enabled publication and report recovery accurately. Verify explicit recovery or a verified last-valid generation separately, then verify a successful retry persists the selected state across manager recreation.

Inject state-write/replace and recovery-publication failures during authorized implementation validation; require one complete prior or new policy, preserved source/retry identity and no unrelated owner changes. Exercise actual setup and launch result handling after scoped Kotlin checks and relevant Android build. Current tests were read only: the missing-state fixture does not establish corruption recovery, and JVM Android stubs do not validate native hard-link behavior.

## Separate follow-ups

Delete retirement, projection publication, audio pruning, source generation and captured set identity remain coordinated BR-0472/BR-0103/BR-0186/BR-0506 acceptance. This plan does not claim to repair every transaction by changing state loading. Complete inventory/JSON/hash admission remains GQR-0212. Chunk0342 consumer/native/minimization review and final canonical publication are still pending.
