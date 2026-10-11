# GQ2 chunk0361: Setup sections and mod presentation, 2026-10-11

Diagnosis and implementation plans only, serially. Preserve concurrent work. No product/test/helper edits, builds/tests/devices/network/probes/generators/formatters/staging/commits.

- [x] Review frozen hunks1-60, enclosing current source and overlapping current deltas
- [ ] Trace mod/import/details/recommended UI, content owners and maintained fixtures
- [ ] Reconcile existing owners, concrete simplification and inherited boundaries
- [ ] Publish canonical evidence and independently audit before DONE

Starting state:0360 publication audit PASS10900 SHA58ac6d2f95998aa0236d2f78d4f4196e8bf259d9524839aee858e758a0d46c51. GQ2 360DONE/267TODO/627;1175 terminal/278 fixes/12 investigations/292 findings. Assigned SetupSections.kt hunks1-60,frozennew15-2496. Later hunks61-106 remain0362/0363 assigned scopes; bounded overlap context does not discharge them.

Live HEAD062a4c0f467fedf956ba12f497a2e7842833846b. Current source4851lines SHAe0a32cb2bd58190ee4936b1dde7a38053c95434957aad28f08350b6f1757c326; frozen4862 SHA499ce6c2ff035a3eadd0200d7f3a090e0a61720fab6425cc83461a4ed124456b; base3634 SHAffc180367654a7ac541e6fe5a03c309593a8ecf99d9e4ac5380c0db1860fab05. Whole106-hunk branch diff SHA794af5b2161166242a71c77184c4fcfec7b7ac95e8dcd1a3d8abdf58a7bd972a. Current11-hunk delta SHAabb1c40de3f34e880b1638788fcadedab86463cfa381c1ddcae2db59001f3a00; inspect overlapping bodies1-7 separately from frozen assigned hunks. Enclosing current range starts1 and reaches at least2500 after current mapping.

Current mod/metadata edits and new helpers belong to concurrent work. Preserve their state and use exact source/consumer hashes; final-head supplemental reconciliation remains required. Keep native game-format ownership and desktop parity, and avoid broad inherited deduplication.

## Assigned source checkpoint

- Reviewed every assigned branch hunk1-60 (+565/-106) and enclosing current lines1-2500, across10 complete250-line reads. Current delta bodies1-7 (+57/-53) separately reviewed; remaining current bodies8-11 and branch hunks61-106 stay later assigned/supplement scopes.

- ModsSection captures managers by setDir but remembers several mod/detail/download/scan states without that key. Refresh/reconcile and metadata jobs run asynchronously; source/target identity, stale retained results and mutation outcomes need actual manager/lifecycle consumer evidence before findings. Preserve current scoped managers and reconcile owner.

- Regular mods and level mods duplicate row action wiring; managed content and demos repeat IO mutation/list refresh. A small shared action/row renderer may remove duplication, but preserve independent managers, ordered group neighbors, captured target and rejection/status semantics. Do not collapse these ownership contracts into a generic storage API.

- The current recommended-mod popup replaces inline rows and sets downloadProgress=0 before launch. Keep that synchronous admission repair and preserve current concurrent new popup/usage helpers; trace setupDownloadFile callback dispatch, cancellation and importCompleted result before deduplication.

- Delete mod/custom audio execute synchronously while managed content deletion launches IO, then prunes audio. None of these UI actions by itself proves durable bytes/registry/grant cleanup. Existing BR0472 and GQR0271 content/audio ordering, deletion and cleanup plans likely apply; exact current consumer trace remains pending.

- Mod details supports multiple readmes, variant display and duplicate basename disambiguation while preserving actual constituent paths. readExtractedMissionZipTextFile now delegates to MissionZip.readExtractedDocument, removing its duplicated byte/read/decode loop. Preserve this simplification and the FileProviderGrantStore external-document boundary; actual store/caller exception and grant ownership remain consumer work.

- MissionZipMusicDialog retains separate stagingTrackId, progress, previewTarget, midiPreviewTarget, previewAutoPlay and previewSkipJob. Skip cancels its predecessor and checks currentCoroutineContext.ensureActive before publishing the next track, and key(track.id) isolates preview composition. Preserve these repairs; Close and analysis/lookup/ordinary preview worker retirement, cleanupOldFiles and shared stages require actual owner evidence.

- Local analysis invalidates nonblank chromaprints on fingerprint database identity changes; web lookup requires explicit opt-in, available config and nonblank chromaprint. Cached missing fingerprints/embedded names and retryable lookup failure have distinct semantics. Preserve them while tracing source-generation/cache identity and real fixtures.

- MIDI metadata prefers direct events and optionally inherits a peer MIDI only when an HMP lacks text; format parsing remains in MidiMetadataBridge. The five song-slot labels expressly mirror native songs.h. Later simplification must preserve peer provenance, ordered song-list references and metadata budget/encoding policy.

- Constituent metadata now retains a loaded result across refresh while restarting async metadata lookup. Errors via runCatching become null metadata; current-state loading/outcome, stable source/set identity and stale callbacks need exact existing-owner reconciliation.

- Classic demos still enumerate on refreshTrigger and delete captured files without checking delete return values. This is a source-derived candidate for existing mutation/result ownership, not a new admitted root. Retain onRefresh and actual native demo-format ownership.

- ModRow oversized textures now reports device adaptation rather than enable rejection; scanResult.canEnable still gates unsafe archive enablement. Trace current DxaTextureScanner/native texture-cap consumer and tests to avoid treating superseded rejection policy as the current contract.

- No inherited saving or runtime acceptance claimed from source review. SetupSections.kt is authored Android and absent1996. Consumer, existing-owner/minimization synthesis and canonical publication remain open.

Source integrity audit pending. Only source gate is complete; no canonical DONE credit.
