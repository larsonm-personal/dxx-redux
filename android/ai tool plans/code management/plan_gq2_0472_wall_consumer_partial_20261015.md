# Chunk 0472 merged-wall consumer partial checkpoint, 2026-10-15

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. Source replay PASS108, audit SHA a9da254f92702fbda21077191ec7deee2a093f0d551286abc33c953b58dae759. Previous goal turn addressed a separate merge-conflict request and verified that file already clean; this continuation makes concrete diagnosis progress. No implementation edit or runtime execution.

## Accepted boundaries

Exact current contexts are packed in gq2-wall-consumer-partial-0472-20261015.json with full-file and body hashes. These are selected boundaries, not whole subsystem credit. Locator searches were partially clipped and earn no body credit. Accepted reads: merged_wall_debug.c4230-4600,7681-7899,2950-2967; game_automate.cpp6766-6810; jni_main.c996-1030; game_introspect.cpp1636-1765; d1/arch/ogl/ogl.c2410-2455; d2/arch/ogl/ogl.c2432-2477.

Actual automation and JNI snapshot requests converge on android_merged_wall_request_snapshot. It clears prior result and target-cover/readback tracking, writes pending with current request frame, then publishes pending after a barrier. finish_snapshot refuses the request frame itself, copies current frame/tracked/cover data and clears pending before completing the selected-face result. This is a fresh request reset and later-frame check; full concurrency, frame tracking and completion/serialization need remaining trace.

The target-cover GPU producer attaches cover_bitmap->gltexture->handle to a temporary FBO. It checks FBO completeness and glReadPixels error, restores prior FBO, deletes temporary FBO, frees readback storage on relevant paths. It reads the texture dimensions, limited to256 each, computes average/black count/center from that texture, and source hash from the source bitmap. Thus test texture64x64 and average/center thresholds concern GPU texture contents, not projected screen dimensions or proof of a correctly composed/displayed wall. The earlier source-plan wording projected64x64 must be corrected to texture64x64. No removal of existing GL/VBO repair is proposed.

Target selection prefers ordered covers, then greater overlap with0.01tolerance, then later cover draw order. It stores face/cover identities and texture/source data. Introspection serializes those fields under target_cover_gpu separately from global sample_rgba/avg_rgba.

Both engine gr_flip boundaries prepare framebuffer readback, then conditionally sample for pending snapshot or introspection request. Shared sampler reads screen center and4x4grid into global sample/averages, then invokes finish_snapshot. These composed-frame samples are distinct from target texture center/averages. Current sampler boundary has no explicit GLerror gate; broader existing owner reconciliation remains required before allocating any finding. No fresh runtime, visual equivalence or successful presentation proof follows from source alone.

## Continuation

Source gate alone may close after PASS108. Four later gates remain open. Next complete face-selection/cover overlap/route and last-draw production, tracking/readback reset and lifecycle, shader/VBO repair owners, whole existing BR/quality/prior plans. Then Mac demo graphics admission, cache/mod generation and manifest/texture census, parameter/dependency/native cursor/harness cleanup, unpooled disposition and immutable publication/Drive verification.

Canonical0472 remains TODO;471DONE156TODO627/1287terminalranks321findings307remediations307fixranks12investigations unchanged. Ledger825840473bytes/SHA71a61d6f4d2800ba6aa979ba600badfd9f14b305bd0524c257aec0ce80d62669 revalidated locally by source audit. Prior mounted0471receipt remains historical; no new append/copy or cloudsync claim. Broad queue and closure requirements remain. No product/test/helper edit/execution, build/test/probe/formatter/generator/network/device/provider/media/payload/staging/commit.
