# Store asset existing-owner continuation, 2026-10-13

Diagnosis and implementation plan only for GQ2-CHUNK-0391. No product, helper or fixture edits, execution, new finding admission, rating or owner closure. Assigned generator scope remains L1-L750; CLI751-1040 and named media/native consumers are context. Generator absent1996; inherited saving0.

- [x] Inspect complete assigned/current generator and named entry wrappers/fixture
- [x] Inspect media composition/audio, boss and combined consumers and recipe history
- [x] Reconcile complete current OPEN BR0173/0233/0543 definitions
- [x] Trace launcher native graphics read/stage interface and bounded native audio producer
- [x] Finish candidate acceptance/root reconciliation and directly applicable rating
- [x] Consolidate qualified consumer evidence and independently audit
- [x] Publish canonical dispositions and queue completion after publication audit

## Owned capture lifecycle: BR-0173 and BR-0543

Capture.start_recording159-248 starts audio before acquiring a recorder/log. Guest startup can throw during log open, Popen, file polling, PID read, signal or wait; host startup can return KO after audio starts. Opening changes rotation and starts recording430-434 before entering try444. Flyout starts recording495 before try497. Therefore startup failures can bypass each caller's finally. Guest stop250-263 signals/waits/closes/pulls before stopping audio; an earlier error skips later release. Host stop264-271 can reject KO/missing media before audio stop. Opening stop/write478-479 can prevent rotation reset480-481. Featured finally889-892 and boss finally70-74 perform restoration sequentially; one error skips remaining restore steps.

Implement one narrowly scoped capture-session owner in the existing Capture helper. Record each successfully acquired audio, recorder, log and temporary state resource immediately. Enter cleanup protection before the first mutation. Attempt every applicable retirement/restoration independently in reverse order, retaining the primary failure and reporting cleanup failures as secondary diagnostics. Use finite graceful-stop and escalation/wait bounds; close the acquired host log even if guest stop or ADB pull fails. Do not signal a stale shared guest PID without generation identity. Keep partial logs/media clearly marked as failed evidence.

The dedicated emulator is deliberately kept running for inspection by documented policy. Preserve that policy; the defect is lost recording/audio/display ownership, not merely a surviving emulator. Acquire exclusive use of the dedicated serial/AVD before shared package/data/remote-stage mutation, and reject another invocation rather than clearing its sandbox or recorder. Use an invocation token and private local generation, not seconds-only output uniqueness or AVName alone. Keep offline editing outside device ownership.

Future actual-entry acceptance through generate-store-assets.ps1, generate-store-boss-preview.ps1 and test_store_asset_pipeline.ps1 must cover startup failure at every resource acquisition, ADB/recorder early exit and timeout, signal/pull/log/media-write failure, Ctrl-C, and multiple cleanup failures. Assert stopped owned recording/audio, every restoration attempted, preserved primary error, exact unowned sentinel preservation and deterministic retained failure diagnostics. Exercise guest/host, opening/flyout/replay/featured/boss and reuse/offline routes. None executed in this diagnosis.

## Complete generation publication: BR-0233 and BR-0173

General compose, combined compose and boss compose all write final store-preview-30s.mp4 directly through ffmpeg -y before downstream validation. JSON evidence, recipes, review pages and many images are also written directly; featured uses a fixed sibling .tmp.png. A failed encode/validation or concurrent repeat can replace the previously accepted local review with partial media or mismatched evidence.

Build a private output generation with collision-free acquired ownership. Run each mode's own validation against that generation before publishing the delivered media plus its corresponding recipe, provenance, validation and gallery as one coherent generation. Preserve prior valid bytes on encode, read, validation, write, close or replacement failure. Remove only acquired temporary files. On regeneration with identical normalized content, preserve existing bytes, modification times and timestamps per repository instructions. This is existing publication ownership, not a new store-specific root.

Future actual wrapper controls preseed a valid delivered generation and fail early/late encode, audio/stem read, replay/source check, motion validation, PNG export, manifest/gallery write and final replacement. Barrier-run same output/device and distinct outputs. Require no partial public generation, no mixed evidence, a deterministic winner, exact prior preservation and owned failure residue. Preserve the differing primary/supplementary/boss result policies.

## Native graphics snapshot and restoration boundary

SetupActivity1091-1106 sends protected updates through updateAllConfigFiles. SetupConfigFiles121-161 uses GraphicsConfigSerialization's nativeStage inside the native transaction; NativeGraphicsSafety.install binds the reader/stager/lock. JNI nativeRead reads graphics_safety_read_staged(root,nullptr). The underlying native API already accepts a game and merges pending fields under the store lock. The Python featured/boss callers instead parse four numeric keys from one game descent.cfg, then broadcast one saved set to all configs on restoration. This does not snapshot pending/native global and per-game semantics.

Proposed simplification: expose a correlated debug snapshot/restore operation backed by the existing native store, with explicit global/game/pending ownership. Reuse the maintained native reader; if per-game snapshots are required, thread the game selector through the thin JNI interface. Do not replace the script regex with another Kotlin parser or assume the current global JNI reader is a complete per-game baseline. Snapshot before any temporary mutation and wait for operation acknowledgement and matching persisted/native state before capture or completion. Preserve settings that were not changed by the capture and restore every owned baseline even after replay/export failure. Root reconciliation for the existing graphics transaction/read domains remains pending; do not admit or close a separate finding from this proposal alone.

Future acceptance uses different initial global/D1/D2 resolution/aspect values and pending settings; run featured and boss capture failure/success routes and verify exact prior policy restoration without native game-format duplication in scripts/JVM. Include deferred native staging and reject malformed or uncorrelated acknowledgements. Existing graphics store and integration fixtures are relevant but historical passes do not prove these new controls.

## Native audio producer evidence and limits

game_automate.cpp675-756 maintains two bounded180-second PCM buffers. Stop first detaches the callback; SDL_androidaudio.c39-43 synchronizes callback replacement with SDL_LockAudio, and the OpenSL callback198-210 invokes capture under the mixer lock. This removes a speculative unsynchronized-buffer-stop root from this observed path. Native WAV header writes and close statuses and metadata fclose are not fully checked; direct final writes retain BR0233's verified-write acceptance. No whole native automation/audio-driver review, allocation/runtime probe or new native audio finding is claimed.

Preserve isolated native MIDI, the repaired explicit signal inversion before effects retiming, real sample comparison and final delivered-container audio checks. Do not merge the primary exact replay policy with intentionally reported supplementary differences or the boss-specific accepted difference set. Shared FFmpeg supervision should bound render/decode lifetime and memory while retaining each caller's required format/rate/stream options and diagnostics.

## Authored simplification disposition

A single resource owner can replace repeated partial finally sequences and shared fixed recorder bookkeeping while tightening behavior. Native-backed snapshot/restore can remove script-level config parsing and duplicate baseline logic. Route equivalent process/decode mechanics through one bounded helper only where the call's output contract remains explicit; general media, combined stream-copy and boss rearrangement retain distinct recipes and checks. Do not broadly consolidate capture timing, primary/supplementary acceptance or all editors merely because syntax resembles another path.

No line-removal estimate or implemented saving yet. Assigned generator is authored-only; extra native/context review does not enlarge its canonical scope or earn inherited diff reduction. Boss non-finite audio correlation and source/provenance candidate acceptance still need recipe-specific trigger and ownership reconciliation before deciding whether a new finding is warranted.


## Final candidate disposition and rating

The non-finite boss audio correlation observation remains a bounded media-validator follow-up for the separately queued boss helper0396. np.corrcoef can produce NaN for degenerate PCM and comparison with0.97 does not reject NaN, but this review has not established that the maintained recipe and native capture path produce that input or that a specific changed delivered file passes every other acceptance gate. Malformed-media probes remain deferred. Do not admit a demonstrated runtime failure, duplicate root or canonical investigation from this source observation alone. A later actual-entry validator study must use supported decoded audio and all picture/source/music/motion gates, require finite metrics and exact expected decoded sample extent, and preserve valid accepted output.

Source hashes in combined and boss preparation archive the selected input identities; composition rechecks archive hashes. General full validation enforces recorded primary replay equality while supplementary capture records differences. The offline compose command has a narrower documented editing contract. No fresh-source equality guarantee is inferred for every offline route and no standalone Capture optional-result defect is admitted. Preserve distinct policy evidence rather than strengthening every route by assumption.

Archived FIXED BR0199 detached graphics snapshot/runtime application and BR0200 complete mirrored config publication remain FIXED. Their complete definitions/resolutions were read. Current native stage/read delegation uses the retained transaction policy; proposed snapshot restoration extends the capture owner boundary and does not reopen either completed repair. FIXED GQF0141/0142 retained-original and descriptor cleanup likewise remain unchanged; current candidate acquisition failure ownership is already BR0233.

Controlling directly applicable BR0543 REFERENCE47 MEDIUM (H23/M0/B4/C10/R10), same-owner GQ1-CHUNK-0688 and0390. Shared capture-helper use is a focused Android build-script boundary, not broad paired engine/core impact; inherited minimization M0 because the assigned path is absent1996. Preserve BR0173/0233 as additional current owners without summing scores or importing unrelated P1 acquisition references.

Assigned generator L1-L750 disposition RETAIN. Source is needed for real dedicated-emulator capture and native automation/provenance. Proposed capture-session cleanup and native-backed snapshot simplification preserve current timing and media policy. No proven dead assigned block or mechanical inherited extraction justifies deletion. Extra751-1040/290CLI context and other media/native paths remain context, not additional terminal scope. Newfinding/remediation/investigation0; inherited saving0. All failure acceptance remains future work, not implementation or runtime closure.
