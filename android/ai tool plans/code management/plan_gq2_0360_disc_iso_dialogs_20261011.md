# GQ2 chunk0360: Disc and ISO import dialogs, 2026-10-11

Diagnosis and implementation plans only, serially. Preserve concurrent work. No product/test/helper edits, builds/tests/devices/network/probes/generators/formatters/staging/commits.

- [x] Review frozen hunks23-30, enclosing current source and overlapping current deltas
- [x] Trace native extraction, audio/publication, cleanup, UI consumers and maintained fixtures
- [x] Reconcile existing owners, concrete simplification and inherited boundaries
- [x] Publish canonical evidence and independently audit before DONE

Starting state:0359 publication audit PASS10978 SHAee6c9a02e02a462336c1f18f39667b49462dd020c2b55baba62e6fdc72358b0e. GQ2 359DONE/268TODO/627;1174 terminal/278 fixes/12 investigations/292 findings. Assigned SetupDialogs.kt hunks23-30,new1372-1920. Review the operation tail and whole ISO dialog through current1994; earlier declarations/analysis and selected0359 contexts may be reused only with exact audit provenance.

Live HEAD062a4c0f467fedf956ba12f497a2e7842833846b. Current SetupDialogs.kt1994lines SHA9926c51cf540bf50f096c2611f763f19d400fa185ae02fd7cd081c1570a43a29 matches prior frozen identity. Verify current diff and all assigned added/removed payloads before source checkpoint. Preserve native format authority, exact staged copies, combined and individual import semantics, captured destination and publication repairs.

GQR0278 admitted by0359 applies to GOG failure/action/focus state; it does not automatically prove an ISO or CUE defect. Reconcile actual transitions and existing BR0456/0472/0021/0419 and source-generation owners before admitting another root. No duplicate credit for0359 assigned hunks1-22.

## Assigned source checkpoint

- Assigned frozen hunks23-30 reviewed in full (+293/-265), including moved/removed extraction and audio bodies. Three prior directly read enclosing current ranges1372-1621,1622-1871,1872-1994 retain exact current/frozen byte identities;623 lines total. No whole-file claim or duplicate0359 source credit. Current delta is empty.

- The shared local importDisc admits processing synchronously and clears it in finally. It preserves combined data-plus-audio and separate data/audio actions. Combined audio runs only when data extraction reports positive publication; data success and audio failure remain distinct outcomes.

- prepareDiscImages returns held descriptors across coroutine context boundaries before preparedImages.use. Its failure cleanup and the free-space catch exist, but acquisition-to-use cancellation and actual writer retirement need existing attempt/resource-owner reconciliation; do not assume every cancellation is cleaned.

- Audio registration persists grants, derives optional hash/fingerprint metadata, copies a bounded CUE, adds the source and enables Redbook. AudioSourceManager.forActiveSet is resolved after IO while data publication retains captured setDir. Preserve registry atomic-add repairs while tracing admitted target identity and failed-attempt artifact/grant ownership.

- CUE backdrop dismissal deletes the temporary CUE when idle; the Close action still calls onDismiss directly. Existing BR0456 owns this cleanup discrepancy. Posted progress and optional metadata callbacks also need attempt lifetime ownership.

- Standalone ISO now calls extractIsoDiscContent with a staging destination. It keeps data-only semantics, coerces negative extraction counts to zero for retry visibility, and reports negative SOW separately. GQR0278's GOG zero-only-controls defect therefore does not automatically apply.

- ISO processing admission remains inside launch after free-space checking and lacks an outer finally. Its progress callback always returns zero. Trace native cancellation, staging publication and worker retirement before deciding whether existing BR0021/0456 already own the remaining state/resource gaps.

- Native disc/archive formats stay with their engine/native owners; the assigned Android UI file is absent from the1996 tree. Final inherited disposition awaits consumer/minimization synthesis; no original-line reduction or runtime acceptance claimed.

Source checkpoint: temp/general_cleanup_20261006/gq2-0360-source-checkpoint-20261011.json. Source integrity audit pending; consumer, simplification and publication gates remain open. No canonical queue credit or runtime acceptance.

## Partial consumer checkpoint

- Native ISO extraction now returns distinct DXX_EXTRACT_CANCELLED, aborts the current physical output on mid-file cancellation/error, and stops on member failure. JNI checks Java callback exceptions and returns a nonzero cancel signal. Preserve these current repairs; BR0021's older claim that ISO returns a successful partial count is stale. The JNI exit still calls store_attempt_budget after extraction, including a pending callback exception, so do not claim all JNI-after-exception acceptance is satisfied.

- CUE extraction validates sector geometry and image indices, keeps one attempt budget across tracks and nested SOW, stages all tracks, and publishes only after successful post-processing. finally removes its staging directory. Current tests exercise data order, one shared budget, earlier-track rollback, nested failure, blocked publication, cancellation and storage geometry. They inject extraction callbacks and do not prove dialog cancellation or Android descriptor lifetime.

- Standalone ISO helper stages output and publishes only for positive primary extraction and nonnegative nested extraction. Negative primary extraction reaches the dialog's No supported game files message, but extractedCount remains zero and the retry button remains attached. This differs from GQR0278 GOG retry disappearance. Exact truthful ISO error ownership and the separate default native/nested budgets still need final deduplication before any new admission.

- BR0419 explicitly already owns this CUE registration's warning-only persistable grant denial and failures between grant acquisition, CUE copy, registry add and config update. The current SafUriPermissions helper returns false for denied document/tree grants; it does not make warning-only admission durable. Retain shared-grant-aware cleanup, require durable access or an explicit owned copy, and keep optional track-name identification independent of required source admission.

- GQR0273/GQF0287 already owns early SafDescriptorStager setup before its source cleanup boundary. PreparedDiscImages removes the descriptor from its map before handing it off, so its later close cannot repair that exception. The prior implementation plan expressly names this disc caller. Avoid duplicating the finding or adding unconditional caller close that breaks successful pass-through.

- BR0456 explicitly covers CUE Close bypassing stagingCue/cleanup, cancellation between prepared acquisition and IO return, late progress publication and unrelated shared-temp deletion. Retain exact atomic copy, current staging directories, prepared resources and rollback publication. One admitted attempt must own every descriptor/stage and settle actual work before cleanup and final UI results.

- BR0472 retains registry/artifact/grant generation and captured target consistency. Current AudioSourceManager reload-under-lock and atomic add are preserved through exact audited0359 bindings. Disc data uses captured setDir while audio resolves forActiveSet after hashing; the admitted destination should be immutable. Content/audio lock ordering remains existing GQR0271; no new lock-order claim is introduced here.

- Provider fingerprinting opens BINs inside a try/finally and closes acquired descriptors, returning optional track-name matches. Path-backed registration and SAF registration have similar metadata/registry construction but different access and ownership contracts. A later shared metadata/result constructor may simplify them; do not convert paths and URIs through one lossy interface or require optional identification to succeed.

- Actual SetupActivity callers clear only dialog URI/name state on dismissal and refresh content; they do not own or await the native import worker. CUE focus uses pending-data/pending-audio predicates and waits a frame; ISO focus predicates retain a button after a negative result. Finish supported/empty-track and cancellation/outcome review before making final focus claims.

- The current test_extract.ps1 standalone ISO path invokes a setup command and explicitly rejects failed import_state and missing expected outputs. It is useful import/publication coverage but bypasses IsoImportDialog and cannot prove its retry, focus, dismissal or cancellation behavior. The prior launcher focus fixtures likewise are not full disc-dialog acceptance.

- Twenty fresh context ranges and27 exact selections from audited0359 are bound separately. All selected current full-file/range hashes match. Source-only audit PASS92 binds8 assigned hunks and623 enclosing lines; queue credit remains TODO. No builds/tests/device/network or product/test/helper changes were performed.

Remaining: finish source-generation, supported/empty-track, ISO outcome/budget deduplication and actual consumer/fixture boundaries; write the minimal existing-owner implementation synthesis; publish final report/snapshot and canonical rows, then independently audit. Source gate alone is complete.

## Owner reconciliation checkpoint

- Exact owner deduplication resolves standalone ISO primary/nested budget reset as existing GQF0061/GQR0048, priority56(32/0/10/10/4). The existing plan54-82 explicitly names both UI/path callers, one attempt through the production lambda/postprocess, pending-exception native unwind, sticky cancellation and actual JNI continuity acceptance. No new budget finding or resource-pressure execution.

- GQF0046/GQR0033 owns direct ISO publication completeness. Current extractIsoDiscContent stages output and guards managed publication; retain this caller repair while the broader raw API/terminal cleanup acceptance remains open. Do not mark the global remediation fixed from this caller alone.

- JNI nativeParseCueRows rejects n<=0, and cue_parse rejects unsupported track types; successful parsed tracks are data or audio. The proposed empty/unknown-track Done-focus counterexample is not admitted. No crash or runtime focus result is claimed.

- The exact audited canonical definition rows are bound as contexts, not priority ranking rows. Final canonical publication must relocate their unchanged text with full prior-file identity when inserted rows shift line numbers.

- Remaining work: settle ISO negative-result message ownership without importing the GOG absent-retry claim; decide whether any source-generation contract requires a separate follow-up beyond existing import/SAF owners; finish minimal implementation and inherited synthesis, final canonical publication and independent audit.

## Chunk0360 assigned source and owner checkpoints independently audited, 2026-10-11

All8 assigned SetupDialogs hunks23-30 (+293/-265) and enclosing current/frozen1372-1994 (623lines) reviewed; current file equals frozen and has no current delta. Source checkpoint SHA10c45759a764946676bfa8f44b42448f45af4b4be4e2cef553c99193ae65976f; source audit PASS92 SHA88926b5c668d36bcfde647e681966821ca41ef1997d558b889ece7fc78dd33a4. Only the source plan gate is complete; no duplicate0359 source credit or canonical DONE credit.

Consumer checkpoint SHA255c159302b78b77f9a13eaf3e298940bba773179e7bb48b3b89c25ec5518166 independently audited PASS258 SHAa18735651577f7347243bc9acc65bc161af50026cc018f3beb4c94d9e1f74973. Later owner checkpoint SHAc0e8d3e14303913e1b4f83d9965e29994ee1b4ddc68dbc578fbed333a10865a0 adds8 contexts to20 fresh plus27 exact audited0359 selections; independent owner audit PASS293 SHA2a344b736af320a6abf459ee6bb8a449a7bf65927db3f85df5dd42c2efbf9f54. It verifies all58 source/consumer context identities, exact prior selections, checkpoint chains,338-line CUE fixture union, unchanged five canonical files and627 queue, and unchanged product diff since source checkpoint. Plan hashes precede this handoff append.

Existing GQF0061/GQR0048 exactly owns standalone ISO primary/nested default-attempt reset; priority56(32/0/10/10/4), production UI/path/JNI continuity and exception-unwind acceptance already planned. GQR0033 retains raw API/publication acceptance while current ISO caller stages and guards publication. Preserve native distinct cancellation/current-output abort, CUE shared attempt, exact atomic copying, registry reload/locking and managed publication. BR0419 already owns denied persistent grants and audio artifact/registry/config partial registration; BR0456 owns CUE Close/cancel/late-progress cleanup; GQR0273 already names PreparedDiscImages early stager failure. No duplicate root/status/rating changes.

Native CUE parsing rejects zero tracks and unsupported types, so an empty/unknown-track absent Done focus witness was rejected. ISO negative count leaves retry attached but reports No supported game files; do not import GQR0278's GOG absent-retry claim. Actual test_extract ISO command route rejects failed status and missing outputs but bypasses the dialog. Remaining: exact ISO error-message/source-generation disposition, minimal existing-owner implementation and inherited synthesis, final report/snapshot, canonical publication and independent audit. Next numbered scope after0360 is0361 SetupSections hunks1-60.

GQ2 remains359DONE/268TODO/627;1174 terminal/278 fixes/12 investigations/292 findings. Product baseline changed from the0359 run through concurrent work before this resumed review; all0360 checkpoints bind the current f8348177ae34f546d8ff674b2ee97cd9c82167585377609c4dc456dbbfc62343 product diff. No product/test/helper edits, builds/tests/devices/network/probes/generators/formatters/staging/commits by this work. Remaining numbered chunks, supplements/sweeps/investigations, historical0116 audit debt and final live-head reconciliation are required. Goal active.

## Final implementation and inherited synthesis

- GQF0061/GQR0048 remains the highest-impact owner, REFERENCE56(32/0/10/10/4): make extractIsoDiscContent own one DiscExtractionAttempt and pass it into both primary extraction and nested post-processing. Keep the existing staged publication contract, native cancellation, CUE geometry and CUE shared-instance routing. Actual UI/path/JNI byte/entry/cancel continuity acceptance remains required in the later implementation phase.
- Extend existing GQF0292/GQR0278 outcome policy with the ISO failure-versus-empty status projection. ISO retry remains attached, so retain the original GOG absent-retry witness and score44. Interpret native and nested outcomes once before deriving displayed count/status/available actions. Keep format-specific helpers and combined/individual CUE choices; do not introduce a general import framework.
- Capture set/content destination and attempt identity before admission. BR0472/0456 already own matching artifact/registry/grant publication and joined cleanup. Persistable audio grant denial is a required source-admission failure under BR0419 unless owned bytes are copied; optional disc labels and track-name matches remain optional. Keep current atomic add/reload and retained-aware grant cleanup.
- GQR0273 already owns transferred source cleanup before stager setup. Put prepared-disc acquisition and use in one deterministic owner boundary, including cancellation during dispatcher return and before use; preserve successful pass-through ownership. Avoid independent unconditional closes of the same descriptor.
- Source analysis is a view/estimate, not permission to publish a prior generation's selected bytes. Standalone ISO lists again during actual native extraction and imports the complete supported set, with no member-selection promise tied to the earlier list. Retain authoritative native validation of the opened generation and actual extraction budgets. CUE uses remembered track geometry, selected URI order and sizes; the admitted attempt must bind those to its retained CUE/BIN snapshots under existing BR0456 and source-stability policy. Revalidate mutable provider sources or stage one immutable admitted generation; do not implement duplicate Kotlin disc-format readers. No distinct source-generation finding survives this ownership comparison.
- Current publication/copy and CUE fixtures remain useful controls, including338-line CUE fixture union and actual managed publication. They do not prove composed-dialog retry/cancel/focus or Android provider lifetime. Later acceptance must cover successful data followed by audio failure as two truthful outcomes, all individual and combined actions, denied/shared grants, actual worker retirement and isolated cleanup.
- GQD1068 NO_INHERITED_EFFECT, saving0: the assigned Android SetupDialogs.kt path is absent from1996, and all proposed attempt/result/registration changes stay in authored Android owners. Preserve native format knowledge and paired desktop parity. Relocating parser policy or broad D1/D2 deduplication would not improve this chunk's boundary.

All assigned source and consumer/minimization diagnosis is complete. Final canonical publication and independent reconstruction audit remain open; no runtime acceptance or product/test/helper edit.

## Chunk0360 canonical publication and independent audit complete, 2026-10-11

GQC1188 ISSUES/GQD1068 NO_INHERITED_EFFECT; RETAIN/GQR0048 REFERENCE56. All8 assigned frozen hunks23-30 (+293/-265),623 enclosing current/frozen lines, empty current delta and59contexts reviewed:3 source/29 fresh/27 exact audited0359 selections. Native cancellation, CUE shared attempt, exact copying, atomic registration and staged content publication retained. No inherited saving or runtime acceptance.

Existing GQF0292/GQR0278 extended to ISO terminal failure/cancellation versus empty-result status. ISO retains retry; GOG absent-action focus does not transfer. Original P2/high44 rating and all priority payloads unchanged. Existing GQR0048 primary/nested budget, GQR0273 descriptor transfer and BR0419/0456/0472 grant/lifetime/target owners reused; no new finding/fix/investigation.

Independent publication audit PASS10900 SHA58ac6d2f95998aa0236d2f78d4f4196e8bf259d9524839aee858e758a0d46c51 reconstructs all five canonical files exactly, preserves prior ranking payloads and all definitions except two explicit same-root outcome extensions, verifies all other queue rows,8 hunk/base/frozen payloads, manifest/original/current identities, checkpoint/prior-reuse chains and59contexts. Four unchanged canonical definition ranges relocated exactly. Initial audit drafts treated a remediation title as a status field, retained an earlier hunk-union range, and required CRLF in an unchanged historical LF plan; corrected only those audit expressions before all checks passed. Product diff unchanged. Snapshot/audit plan hashes precede this closure.

Snapshot SHA8ba86261e33820c4a746267f3a5f2e0ae586e09dc6bf7b4d1c6703024422f74c; report SHAf1ecc4349eeafe5b7ac84a91e7ed4dbed49c924ed92eef4541abd25e89f87672; scopeb438ee0c8c677cc4e1407d098bb4ea84e8599f826ceef738860851fabae5d44f.

GQ2 now360DONE/267TODO/627;1175 terminal/278 fixes/12 investigations/292 findings. Next0361 SetupSections hunks1-60,new15-2496. All remaining numbered chunks, supplements/sweeps/investigations, historical0116 audit debt and final live-head reconciliation required. Diagnosis/docs only; no product/test/helper edits or builds/tests/devices/network/probes/generators/formatters/staging/commits. Goal active.
