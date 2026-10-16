# Chunk 0470 GOG and GLES consumer trace, 2026-10-15

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. This is a partial consumer checkpoint, not chunk completion, new finding admission, runtime evidence or implementation authorization.

Current HEAD and product diff are revalidated in the accompanying exact-source package. Prior turn completed source acceptance; its unpersisted consumer reads are captured here at explicit semantic bounds, supplemented by fresh bridge, renderer-selection, publication and catalog reads. Source-only conclusions follow.

## Installer admission and completion

The wrappers call Push-VerifiedDeviceFile. Its helper compares exact device size and SHA-256 against the selected host file before reuse and after pushing. SkipPush fails on mismatch rather than accepting a basename alone. This source inspection does not read, hash, stage or execute any installer payload.

SetupActivity import_gog captures the active set, imports on its IO receiver, and checks count >= 0. Exceptions become ordered-broadcast failure through runIo; LauncherScriptExecutor awaits that ordered result. Preserve this stronger failure path when reconciling existing setup-outcome findings. Local completion does not establish general cancellation, process-death or end-to-end timeout guarantees.

SetupDiscImport creates private installer staging, lists a manifest, passes it to extraction, checks the filtered expected count, publishes positive results, and deletes staging in finally. GogImportBridge rejects malformed size/CRC records and duplicate case-insensitive names. Extraction lists again and requires equality with expectedFiles, then passes name, size and CRC entries to native extraction. The native parser, native manifest enforcement, path safety, installer replacement races and publication rollback still require their actual implementations; this bridge alone cannot prove them.

FileSetContentManager.publishDiscImport serializes hoisting, preparation and staged publication with CONTENT_LOCK. The called publication functions remain pending; lock ownership alone is not evidence of atomic disk durability. clear_audio_sources closes local descriptors and clears registry/playlist under its transaction; ignored filesystem delete results and full playback-owner retirement require existing-owner reconciliation before any new finding.

Both wrappers are catalogued under storage_import and listed as single-emulator extract tests. Explicit master timeout entries are 420 seconds. Whole scheduler budget composition, variant execution, helper cleanup failure and final result admission remain pending.

## GLES probe and renderer eligibility

The automation action stops with failure if the native probe returns false. The GL probe uses a private 8x8 target, VBO zero/nonzero draw offsets, a center pixel/error check, array-binding cache checks and the 2D batch probe. The latter compares whole immediate/batched pixel buffers across empty, lines, rectangles, mixed and overflow scenarios. These are meaningful native pixel assertions; menu state and a PASS log alone are not their oracle.

The probe saves selected internal and GL state and restores it on its ordinary path. Single-center-pixel coverage, complete state restoration, exceptional cleanup and presentation remain separate limits requiring whole existing owners. No runtime, hardware, screen presentation or general renderer acceptance follows from source inspection.

Both D1 and D2 CMake branches select gles3_shim.c only under OPENGL and software_renderer_debug.c under NOT OPENGL. The software implementation of this probe returns 0. Thus invoking the action on software deliberately fails rather than silently passing. Whether the harness excludes software for this GLES-specific script remains pending; this is not yet a new defect.

## Continuation and limits

The combined read output clipped an unrelated reconcile fragment of FileSetContentManager. This checkpoint credits only its complete publishDiscImport boundary, not the whole file or reconcile. Catalog ranges are registrations, not proof that any scenario ran.

Next trace native installer manifest/extraction and staged publication, full wrapper/master variant and cleanup/result flow, renderer eligibility, menu preset/reset/identity ownership, automap and Guidebot consumers, then whole material finding definitions and prior plans. Reconcile the stale Mac-audio comment against actual variant flags. Only afterward synthesize a direct unpooled disposition and publish canonical evidence with terminal mounted-Drive verification.

Canonical chunk 0470 remains TODO, with only its source gate closed. No new finding, remediation, rating, status, inherited saving or ledger append. Broad numbered queue, preflights, sweeps, investigations, older recovery, global TSV, 0327 snapshot, current-head/dirty/untracked supplement and closure remain required.
