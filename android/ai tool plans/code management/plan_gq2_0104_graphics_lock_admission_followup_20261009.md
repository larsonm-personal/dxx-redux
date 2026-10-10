# Graphics option lock admission and ownership follow-up, 2026-10-09

Diagnosis only. Continuation under existing OPEN BR-0029 settings/configuration ownership and serialization acceptance; no duplicate GQF/GQR admission. Preserve FIXED BR-0200 complete transaction publication and DONE GQR-0128/GQR-0129 original retention and descriptor cleanup. No implementation or runtime evidence

## Current source proof and limits

android_graphics_options.c persist_config_if_needed holds a process-local config mutex, attempts graphics_safety_lock, then calls mirror_config_key even if the lock is null. Mirror uses the existing graphics_config_patch_files/patch_batch transaction, which stages full files and backups but acquires no cross-process lock itself. graphics_safety_lock returns null for invalid root, different nested root, failed OS file acquisition/lock or caught allocation. Store guard rejects null before requested/journal operations; the native option mirror does not

Only five fields enter safety field/queue admission: texture filtering, anisotropy, MSAA, menu and HUD filtering. Direct protected setters call note_option before runtime mutation, and initialized note_option requires begin_attempt with its own checked store guard. That earlier journal lock is released before the later mirror lock, so it is not a retained lock across the complete update. Initialized protected edits are not claimed to bypass a persistent journal lock failure. Previews/uninitialized and applying paths have different admission; do not infer ordinary reachability without their callers

Gamma, main-view FOV, corner inset, classic depth and D2 movie filtering have no note_option gate. NativeSetGraphicsOption tries queue_option first; UNKNOWN falls through to the direct setter with persistence enabled for these options. Paired native menus also call direct setters. A lock acquisition failure does not imply config replacement must fail: for example an inaccessible existing .graphics_safety.lock can coexist with writable config paths and parent directory. This is a source-derived future fault case, not an executed permission probe. Do not claim observed disk corruption or an actual concurrent loser from the conditional path alone

BR-0029 already owns settings mutation off the engine thread and uncoordinated whole-file read/modify/replace updates, including cross-process writers. Treat checked lock admission as part of that serialization implementation, rather than reopening the completed single-transaction data-integrity repairs. Engine-thread dispatch alone cannot serialize a second process; likewise a file lock alone does not provide engine-thread affinity or coherent runtime request acknowledgement

## Proposed implementation boundary

- Keep persistence in the existing shared owner. If requested persistence cannot resolve/acquire the safety lock, return PERSIST_FAILED before mirroring; release the process-local mutex exactly once. Preserve persist0 as a runtime-only path
- Hold the admitted cross-process lock across the full mirrored transaction and route every cooperating launcher/game writer through the same root/lock contract. Verify lock ordering with state mutexes and config_write_begin/end; do not hold file/state locks across GL recovery
- Define truthful runtime-versus-persistence failure semantics for unprotected fields whose runtime mutation currently precedes disk admission. Either stage/acknowledge one engine request or explicitly retain a failed-persistence status for a retry; do not invent acceptance of an uncommitted request
- Preserve five-field safety journaling, first-run preview, paired config targets, D2-only movie scope, parser/default/unknown keys, existing 16 MiB limit and retained rollback backups. No migration layer, new general transaction framework or inherited extraction

## Future acceptance

Use actual persisted-option JNI/native callers and production transaction, not a stub that merely counts a lock attempt. Cover successful acquire, root resolution failure, lock-file open/lock failure and supported nested-root behavior, with config paths independently writable. Assert failed admission does not invoke public replacements or erase prior bytes, returns PERSIST_FAILED to the actual UI/native caller, and does not advertise committed requested state. Check persistent failure before protected journal admission separately from failure after successful journal publication

Barrier-synchronize game and launcher disjoint key updates under the same root across root/D1/D2, including D2 MovieTexFilt and nonprotected gamma/FOV edits. Require both updates survive and every mirror has one coherent committed generation, unique temporary ownership, no foreign cleanup, deterministic retry and unchanged backups. Exercise persist0 launcher application and original safety rollback/restart fixtures as controls; preserve engine-thread command/snapshot acceptance from BR-0029 and pending JNI/exception owner GQR-0170

Run meaningful production native lock/transaction tests, paired relevant desktop/Android builds and ordinary emulator integration when implementation is authorized. Malformed, allocation, resource, security and race probes remain deferred in this diagnosis tranche; no tests created or executed here
