# GQ2 chunk0359: Setup import dialogs, 2026-10-10

Diagnosis and implementation plans only, serially. Preserve concurrent changes; no product/test/helper edits, builds/tests/devices/network/probes/generators/formatters/staging/commits.

- [x] Review assigned frozen hunks1-22 and enclosing current source, including any overlapping current deltas
- [x] Trace dialog/import/publication consumers, existing owners and maintained fixtures
- [x] Reconcile actionable simplification and inherited boundaries
- [x] Publish canonical evidence and independently audit before DONE

Starting state:0358 publication audit PASS6149 SHA91a4c1a48c57b8335a0dd7ed4cf7d809dbf8ec6c48f181b27f5faa80922a1843. GQ2 358DONE/269TODO/627;1173 terminal;277 fixes;12 investigations;291 findings. Assigned SetupDialogs.kt frozen diff hunks1-22,newL20-L1275; later hunks23-30,new1372-1920 belong0360.

Current and frozen SetupDialogs.kt both1994lines SHA9926c51cf540bf50f096c2611f763f19d400fa185ae02fd7cd081c1570a43a29. Whole30-hunk branch diff SHA5bcc10360c01fdcac44bacb5566cf72fee3463a8d367b16f0a114696c595e35a. Current delta empty. Read enclosing1-1371 and all assigned added/removed payloads; later0360 implementation remains its own assigned scope. Prior0358 selected helpers are reusable only through exact current identity and audit provenance, without whole-file credit.

Preserve engine-owned archive/disc parsing and existing shared publication/copy/temporary ownership repairs. The assigned Kotlin is absent from1996, but final minimization disposition needs actual owner/consumer synthesis. Consumer/publication gates stay open until reviewed.

## Assigned source checkpoint

- Reviewed enclosing current/frozen lines1-1371 and all assigned branch hunks1-22 (+305/-263). The current file equals frozen; no current delta. The previously truncated hunk19 output was recovered by reading the entire 243-line removal again. Remaining hunks23-30 belong0360; no whole-file coverage claim.

- Set management now passes context to clear/delete, retaining permission cleanup. Mutating admission exists but switch/close remain separate; IO failure/cancellation and the captured destination need existing storage/registry owner reconciliation before any new finding.

- setupDownloadFile reads nullable Content-Length, uses the exact copy helper, posts progress and invokes callbacks from an IO/catch boundary. Preserve current exact-copy repairs; existing0358 suspend-result and attempt-owned progress plan is relevant.

- publishStagedArchiveFiles backs up prior files, tries same-filesystem rename before exact copy, and rolls back published members on exceptions. Rollback uses short-circuit all over backup renames and ignores deletion results. Preserve the disk-space optimization; ordinary rollback is not crash-safe multi-reader publication proof. Reconcile existing content-state owners.

- GOG analysis uses direct descriptor or exact staged copy and native file listing. Dialog state is remembered without source/set keys; copyingInstaller does not cover all native analysis. Progress posts and global temporary cleanup require attempt ownership and actual native return tracing. Preserve descriptor use lifetimes, native format authority and exact output checks.

- The new local extractInstaller synchronously admits processing before launch and finally clears it. It stages in a unique sibling directory, verifies requested count and publishes through FileSetContentManager. Audio registry creation uses forActiveSet while publication retains captured setDir; trace destination identity before admitting a duplicate finding.

- GOG staged extraction supplies expectedFiles; descriptor extraction reopens the source and does not receive that prior list. Analyze actual bridge validation and source identity contract. A negative extractedCount renders No files extracted but disables controls requiring count==0; trace existing outcome/collision owners and maintained fixtures.

- The primary GOG action intentionally imports game and available CD audio together; the secondary action preserves the checkbox choice. Both use the same local operation. Avoid removing distinct UI semantics or folding format-specific GOG and SOW results into a generic handler.

- SOW retains its separate exact staged copy, native progress and rollback publisher. Processing admission occurs inside launch, count is coerced nonnegative, and clearing processing follows the IO block rather than an outer finally. Global Close/Done cleanup and native retirement require consumer ownership evidence.

- Disc dialog initial analysis uses bounded exact CUE copy, shared ordered entry mapping, known BIN sizes and native parsing. New focus routing handles combined pending data/audio with keyboard mode and a frame wait. Later actions remain0360; selected consumer reads will not count as its assigned completion.

Source checkpoint: temp/general_cleanup_20261006/gq2-0359-source-checkpoint-20261010.json. Source integrity audit PASS171; consumer, simplification and publication gates remain open. No canonical queue credit or runtime acceptance.

## Partial consumer review

- Fresh consumer evidence: whole GogImportBridge142, NavigationDialogs89, JNI GOG488 in two ranges, parsing32/audio-registration59/disc-content244 fixtures and D1 unified148 script; bounded registration, caller, cleanup/config, audio registry, content publication, descriptor lifetime, runner and seven existing-owner sections. These are source reviews, not executions. Remaining SOW/disc, focus integration and source-identity owner reconciliation stay open.

- Current GogImportBridge parses nonnegative sizes, bounded CRC and duplicate portable names. Path extraction relists and compares expectedFiles before JNI; PKG JNI also compares name/size/CRC against its opened archive. Inno JNI path and reopened-descriptor extraction do not compare that earlier manifest. Requested-count equality alone cannot establish source identity when names/bytes change with equal count. Pin or validate the analyzed native generation; do not add an independent Kotlin archive parser. Reconcile the existing source-admission owner before a new finding.

- Both current native Inno paths reject colliding output names and stop on first member error. JNI latches nonzero progress or Java exceptions and returns DXX_EXTRACT_CANCELLED for Inno. inno_open_fd duplicates the caller descriptor; inno_close closes that owned duplicate. Kotlin descriptor use remains correct for each individual native call. Preserve these BR0020/current cancellation repairs; no claim that every decoder cancellation loop was freshly reviewed.

- BR0020 is FIXED, and BR0511 is FIXED collision rejection, not retry ownership. Preserve their current native error, count, staging and collision repairs. GOG UI negative extractedCount hiding the zero-state action is a separate state/outcome candidate; exact finding ownership remains unresolved. Do not reopen fixed native failure roots merely because a later UI path has inadequate retry.

- BR0021 remains OPEN historically, but its old claim that JNI ignores callback cancellation is stale for the current GOG bridge. The current dialog callback returns0 unconditionally; coroutine cancellation alone does not tell native extraction to stop. Plan one attempt cancellation signal and preserve native distinct cancellation without fallback, progress after owner retirement or cleanup before actual return. Native format interpretation remains authoritative.

- BR0456 explicitly owns overlapping global temporary cleanup. Current cleanupTmpDir still recursively deletes filesDir/tmp, and GOG/SOW dismissal/completion call it. Exact-copy and UUID repairs do not make that global deletion owned. A dialog-owned staging subtree must survive analysis/extraction until actual worker retirement; dismiss only the associated attempt, then clean its subtree and report leftovers. Preserve setupDownloadFile exact-copy repair and use one result/progress owner rather than nested catch/callback completion.

- BR0472 owns registry/byte generation consistency and truthful delete/clear outcomes. Current AudioSourceManager.addSource takes AtomicFilePublication.transaction and reloads before saving, so old unlocked-snapshot wording is not current evidence. forActiveSet resolves active selection at invocation; sourcesFile is scoped to that set's .content/audio. GOG publishes to captured setDir but registers through this late active selection. Plan an immutable admitted target passed to both content and audio owners; recheck existing0356/0336 captured-target plan before duplicate admission.

- Current synthetic known GOG CUE uses AtomicFilePublication.writeUtf8, disproving the old BR0456 direct-write description. buildGogAudioSource uses the supplied setDir for root audio and constructs actual relative paths. Do not move audio into the managed extras bundle or remove the native/known-format ownership boundary.

- FileSetContentManager.publishDiscImport holds CONTENT_LOCK while hoisting, preparing/staging managed extras and publishing files. DiscContentImportTest actually invokes this production path and verifies preserved mission directories/companions, stable disabled reimport, base content, separate audio ownership and same-file-key audio rename. The optional Anniversary corpus test uses assumeTrue and is not unconditional corpus coverage. No concurrent registry, interruption or dialog lifecycle acceptance is established.

- NavigationAlertDialog delegates to Material with repeatDpadKeys and LocalNavigationRepeatRoot. It does not itself implement initial focus seeding. BR0473 specifically owns delayed repeated focus; BR0475 owns a picker release reaching the underlying controller page. Neither proves a defect in this dialog's new one-frame focus seed by name alone. Trace actual requester attachment, pending-action conditions and maintained Activity/Compose controller tests before disposition.

- The complete D1 GOG script invokes setup_command import_gog, asserts D1 readiness/root filenames/managed extras, then launches and checks level1. The bounded runner requires a real fixture, uses verified push, delegates to run_test and returns its exit code. This bypasses GogImportDialog and cannot validate dialog retry, focus, provider replacement, overlapping attempt cleanup or cancellation.

- Concrete next work: inspect SOW actual native attempt/cancellation and publication fixtures; trace remaining set-management and disc-analysis consumers through audited prior bindings; inspect D2 GOG audio integration and actual dialog/controller coverage; locate exact source-generation and negative-outcome owners; synthesize minimal shared attempt/result policy, then publish/audit0359. Hunks23-30 remain0360. No new canonical root/status/rating or inherited saving admitted yet.

Consumer checkpoint: temp/general_cleanup_20261006/gq2-0359-consumer-checkpoint-20261010.json. Source audit hash binds the plan before these consumer notes; consumer audit will bind this updated plan. All remaining gates stay open.

## Chunk0359 source and partial consumer checkpoints independently audited

Assigned source complete: all22 frozen hunks1-22 (+305/-263), including the entire removed hunk19 recovered after truncated output, and enclosing current/frozenL1-L1371. SetupDialogs.kt current bytes equal frozen; no current delta. Later hunks23-30 remain0360. Source checkpoint SHA0e4b950ccab72403eed817809f4f78cf0e8be85756f5960436ebbbbbdf2ce1e1; scopeb8c4d7917ec8fcf9e11331173a83ff18984c41866d22753dbbb9a42a5922d0ea. Independent audit PASS171 SHAf6e888621242a51b00a1999e2add407e0436f1e391cab2efde04fe866fad5467 verifies exact source/diff unions, every assigned added/removed payload against Git blobs, manifest, original absence, unchanged canonical queue and product diff.

Partial consumer checkpoint binds26 fresh contexts plus6 assigned source ranges. Whole Kotlin GOG bridge, navigation wrapper, JNI GOG bridge in two ranges, parser/audio/disc-content fixtures and D1 unified script read; bounded registration, actual callers, cleanup/config, audio registry, content publisher, native descriptor lifetime, runner and seven owner sections reviewed. Checkpoint SHAba42dbd1d3e11f2803295a331ac0d387e186209b07fa7acbfeb004b3e8386fd4; scope8d53db73ec82731eb293cc0ab3bf84e23795b1ce2f7063505d2e4edcb2250fc7. Independent audit PASS154 SHA8ac88b37528805a9d8e87a55e9c31b579a9ec538f60bf05d15b944853de978e9 verifies all current full-file/range identities, exact source chain/unions, unchanged five canonical files and627 queue, honest gates and selected production/test distinctions. Product diff unchanged at both audits. Checkpoint plan hashes precede subsequent notes/handoff.

Preserve GOG synchronous admission/finally, native member-failure and cancellation propagation, descriptor duplication/closure, strict listing/collision validation, exact atomic copy and generated CUE, content-locked publication and rename-first audio. BR0020 and BR0511 are FIXED; BR0511 is collision rejection, not retry. BR0021 historical ignored-JNI-cancellation wording is stale for current GOG, but the dialog still never signals cancellation. BR0456 owns remaining global temp cleanup; BR0472 owns registry generation while current atomic add/reload repairs must remain. Captured content destination versus late forActiveSet audio registry needs immutable admitted target. Reopened Inno analysis/extraction lacks earlier-manifest comparison; exact source-generation owner dedup remains pending. Negative GOG count versus zero-only retry is an outcome-state candidate, not a newly admitted root.

Disc-content fixtures exercise actual publication, managed mission ownership, audio preservation and rename file identity; optional Anniversary corpus is conditional. D1 integration invokes import_gog automation, not GogImportDialog. No executed runtime acceptance. Navigation wrapper only supplies repeat handling; BR0473 delayed retry and BR0475 picker-release owners do not alone prove a defect in this dialog's one-frame focus seed.

Next: SOW native attempt/cancellation and publication fixtures; remaining set-management/disc-analysis consumers using exact audited prior bindings; D2 GOG audio integration and actual dialog/controller coverage; exact source-generation/outcome owners; concrete simplification/inherited synthesis and final0359 publication/audit. See plan_gq2_0359_setup_import_dialogs_20261010.md for detailed notes.0359 remainsTODO; GQ2 stays358DONE/269TODO/627,1173 terminal/277 fixes/12 investigations/291 findings. No canonical finding/status/rating, inherited saving, product/test/helper edit or execution. Remaining supplements/sweeps/investigations, historical0116 audit debt and final live-head reconciliation required. Goal active.

## Final review and implementation synthesis

Assigned hunks1-22 and enclosing1371 lines are fully reviewed; the source and partial-consumer checkpoints remain exact. Final additional context closes SOW bridge/outer-loop cancellation, rollback/cancellation fixtures, CUE ordering/native parse projection, the combined-button focus attachment, D2 GOG command integration and existing source-generation ownership. Thirteen exact audited0358 contexts provide set/content/audio/copy owners and maintained launcher focus coverage. Later dialog implementation hunks23-30 remain0360 despite the selected attachment context.

### Simplification and inherited boundary

Keep the already extracted local GOG operation. Its synchronous admission and finally-owned reset remove a real repeated handler and improve correctness. Share one private attempt/result policy only where the dialogs repeat admission, progress, terminal presentation and owned cleanup; keep GOG expected-file validation, SOW continuation semantics and CUE native parsing in their current format owners. Consolidate caller-owned cleanup through the existing BR0456 attempt lifetime plan rather than another generic publisher or provider wrapper. Use one captured file-set destination for content and audio registration under BR0472/0356's immutable admitted request. Preserve exact copy, rename-first large audio publication, content lock order and atomic CUE/registry repairs.

GQD1067 NO_INHERITED_EFFECT, zero inherited savings: the assigned Kotlin path is absent from1996. UI state and import lifetime policy belongs in authored Android orchestration. Native JNI/archive readers already own formats and physical output; no move of format rules into Kotlin and no new d1/d2 hook is needed. Further extracting GOG/SOW/native internals into an engine adapter would increase coupling without reducing inherited changes.

### Deduplicated observations

- GQF0053/GQR0040 exactly owns the Inno analyzed-source/reopened path or provider generation gap. GQF0091/GQR0078 separately owns collision-weak PKG source identity. Current PKG typed manifest and Inno collision/count checks remain necessary but do not close those existing roots. Reference47 (23/0/7/10/7); no duplicate source finding.
- BR0456 owns global filesDir/tmp cleanup while another analysis/import is alive. Give each attempt a subtree and cancellation signal, await actual writer retirement, publish only for its admitted destination, clean only its own subtree and preserve truthful cleanup results. Exact-copy and generated-CUE repairs remain intact.
- BR0472 retains captured registry/bytes identity and truthful set delete/clear. Reused current FileSetManager and AudioSourceManager evidence preserves atomic/reload repairs; no stale unlocked-snapshot allegation. The late forActiveSet registry must use the same destination already captured by publication.
- BR0021 remains the cancellation owner. Current JNI and SOW outer assembly propagate cancellation and shared budget state; the UI callbacks still return0. DiscExtractionAttempt is a native cumulative state carrier, not a live cross-thread coroutine cancellation token. A cancellation request must reach the callback and suppress stale progress/results until actual native return; keep per-format outcomes and no-fallback semantics. BR0044 JNI exception discipline remains distinct; no full decoder audit or cancellation runtime claim.
- NEW GQF0292/GQR0278 priority44: a negative GOG result is stored in extractedCount, hiding actions guarded by zero while the focus effect still selects the absent primary action. Failed native extraction is presented as No files extracted and the ordinary error UI is gated away. Introduce one private outcome controlling truthful status, retry/close and attached focus, retaining the two intentional audio choices. See plan_gqr_0278_gog_dialog_outcomes_20261010.md.
- The cached Compose1.12.1 class and current generated build-model identity reject an assumed unattached-requester crash: empty nodes print a warning and return false. Only the relevant method dispatch/empty-node branch was inspected. No runtime reproduction or fresh dependency resolution. The confirmed new root is inconsistent failure/action state, not a crash.
- CUE focus predicates agree with the actual combined button's data-and-audio predicate. NavigationAlertDialog supplies repeat handling, not initial focus policy. Keep new one-frame combined action focus; BR0473 delayed retry and BR0475 picker-release ownership are not blanket reasons to remove it.

### Validation boundaries and later acceptance

The D1 and D2 GOG scripts execute import_gog automation, then assert readiness/managed content or exact audio identity and preview/game playback state. Both bypass the dialog. D2's real assertions cover both variants despite an outdated comment claiming the PKG path skips audio checks. Correct that comment during the relevant test maintenance; do not weaken assertions.

DiscContentImportTest invokes actual content publication and verifies mixed mission companions, stable disabled reimport, base/audio ownership and rename file identity; its optional Anniversary corpus uses assumeTrue. CueDataTrackExtractionTest checks rollback after a blocked later destination and cancellation before a next track using injected extraction, not native decoder cancellation or recovery after process death. LauncherFocusPolicyTest covers predicates and safe unattached requests; the maintained D-pad runner covers launcher navigation but does not provide the negative GOG dialog transition oracle.

GQR0278 acceptance must drive the actual dialog through native failure, mismatch, cancellation, zero and success, advance Compose frames, retry once, verify attached/reachable actions and no false onImported. Existing BR0456/0472 acceptance adds overlapping attempts, old-result dismissal, set replacement, recreation and failed cleanup. GQR0040/0078 supply same-count/mutable-source identity tests. Native archive authority, counts, collision rejection, same-volume rename and D1/D2 ordinary imports remain controls. All tests are planned, none executed here.

No source/test/helper changes, network/device/probe/build/generator/formatter/staging/commit activity. The read-only javap inspection did not run app code. Final publication and independent audit remain the last gate.

## Chunk0359 canonical publication and independent audit complete, 2026-10-11

GQC1187 ISSUES/GQD1067 NO_INHERITED_EFFECT; RETAIN/GQR0040 REFERENCE47. All22 assigned hunks1-22 (+305/-263),1371 enclosing current/frozen lines, empty current delta and66 bound contexts reviewed:6 assigned source,47 fresh consumers and13 exact audited0358 selections. Native format/output ownership and current shared GOG, cancellation, exact-copy, generated-CUE, registry and content-publication repairs retained. No inherited saving or runtime acceptance.

NEW GQF0292 OPEN/GQR0278 TODO P2/high44(23/0/4/10/7): negative GOG result removes zero-only retry/error controls while nonempty fileList still selects the absent primary focus target. One private dialog outcome must determine truthful failure, retry/close and attached action focus. Cached Compose1.12.1 bytecode warns/returns false for empty requester nodes; an assumed crash was rejected, not reported. Existing generated model identifies this dependency, with no fresh resolution or app execution. Actual dialog/frame/retry acceptance is planned separately from command-based D1/D2 GOG integrations. Existing GQR0040/0078 source identities and BR0456/0472/0021 owners are reused without duplicate findings; BR0020/0511 remain FIXED.

Independent publication audit PASS10978 SHAee6c9a02e02a462336c1f18f39667b49462dd020c2b55baba62e6fdc72358b0e reconstructs all five canonical files exactly, preserves every prior ranking payload and definition and every other queue row, verifies assigned hunk/base/frozen payloads, manifest/original/current identities, checkpoint/prior-reuse chains,66 contexts and cached dependency/model/disassembly identities. Four unchanged canonical definitions relocated exactly. Product diff unchanged through source, consumer, final review, publication and audit. Snapshot/audit plan hashes precede this closure.

Snapshot SHAc07cd034cdd814a03ca4c14eb817418d97ae03340efcaff3f1b498f35d4ae83d; report SHAddaa190541ffa5c3899a9efc13c069cf716770712573a96e7e419c5dc6ea79b8; scopec1816f8fc8d1b26710d93a20c53df27260031f3ce1f83e8e90609371444d2f41. Artifacts retain chunk-start20261010 filename; completion is2026-10-11.

GQ2 now359DONE/268TODO/627 numbered;1174 terminal/278 fixes/12 investigations/292 findings. Next0360 SetupDialogs hunks23-30,new1372-1920. Selected0359 focus-attachment context does not discharge0360 assigned review. Remaining numbered chunks, supplements/sweeps/investigations, historical0116 audit debt and final live-head reconciliation required. Diagnosis/docs only; no product/test/helper edits or builds/tests/devices/network/probes/generators/formatters/staging/commits. Goal active.
