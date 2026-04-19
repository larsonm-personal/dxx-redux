# Plan: door45 hidden single-draw state reset

## Goal

Use the latest phone logs plus a renderer audit to close out the Android-only
single-draw state-reset trial, then pivot the next tranche to GPU readback and
lighting-state probes on the bad hidden single-cover path.

## Required validation

1. `android\run-code-quality.ps1 -Fix`
2. `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`

## Findings From debuglog_20260419_094557.txt And Code Audit

- [x] Experiment `11` now reaches the parent merged face on the bad level-1 view and logs `[mwall_texexp]` on `seg=80 side=0 tmap2=0x125`, so the new parent-face gate is live
- [x] The actual `door45#0` hidden cover draw still logs `[mwall_texexp_skip] reason=face_tmap2_zero`, which confirms it remains an ordinary single-texture cover draw rather than a merged two-texture draw
- [x] The phone-side source data still looks sane on the bad draw: `[mwall_cover_src] name=door45#0 hash=0x272e5021`, `64x64`, base texture only, with the expected raw rows dumped in `[mwall_cover_src_row]`
- [x] The live bad draw logs `program=3 tex0=91 tex1=0 tex2=0`, but that by itself does not prove a tex1 contamination bug
- [x] The earlier tex1-based Trial A theory is falsified by the renderer audit: Android `g3_draw_tmap()` single-texture draws use the GLES3 shim in `android/app/src/main/cpp/shared/gles3_shim.c`, and that fragment shader samples only `uTex` from texture unit 0
- [x] All existing `gles3_shim_use_external()` call sites in the merged-wall two-texture paths are paired with `gles3_shim_use_external(0)`, but the follow-on single-texture path does not explicitly force the shim state back to normal before binding the hidden cover texture
- [x] The lowest-risk remaining state-leak trial is therefore not alpha cutoff on `ogl_prog_tex2`; it is an explicit Android-only reset to `gles3_shim_use_external(0)` plus `glActiveTexture(GL_TEXTURE0)` before binding a single-texture draw that inherited merged-face context by parent `tmap2`

## Findings From debuglog_20260419_102600.txt

- [x] The newest phone build still shows the same failing centered `door45#0` bbox cover on the level-1 repro: `[mwall_snapshot_focus_cover] focus=center_cover ... cover_bot=door45#0 ... overlap=8878.1`
- [x] The live bad draw is materially unchanged from the earlier failed capture: `[mwall_cover_live] ... shader=single ... expected=91 program=3 active_tex=0x84c0 tex0=91 tex1=0 tex2=0 ...`
- [x] The phone-side source payload is still identical on the bad draw: `[mwall_cover_src] ... name=door45#0 ... hash=0x272e5021` with the same dumped rows, so the state-reset trial did not uncover a CPU-side source-data issue
- [x] The parent merged face still clears secondary units under experiment `11`: `[mwall_texexp] ... kind=single_clear_units ... before=310/309/0 after=310/0/0`, while the hidden cover still logs `[mwall_texexp_skip] reason=face_tmap2_zero`, so the parent/child control flow is unchanged
- [x] The snapshot still lands in the same partial-face state: `[mwall_snapshot] ... partial=1 center_hits=0 cover_events=3` plus `[mwall_snapshot] no_projected_faces ... partial_candidates=1`, which means the state-reset trial did not change the bad on-device behavior in any visible way
- [x] The stock upload path still reports ordinary generated mipmaps for both affected textures: `[mwall_mip_upload] name=door45#0 ... uploaded_levels=1 generated=1` and `[mwall_mip_upload] name=door45#9 ... uploaded_levels=1 generated=1`
- [x] Trial fix B and Trial fix C are no longer good primary explanations for this exact hidden single-cover path because the renderer audit still says the failing draw goes through the shim's single-sampler `uTex` path rather than a two-sampler merged-wall shader

## Findings From debuglog_20260419_110639.txt

- [x] The repro is still visually unchanged on the centered level-1 hidden cover, and the snapshot again focuses the bad `door45#0` bbox cover: `[mwall_snapshot_focus_cover] focus=center_cover ... cover_bot=door45#0 ... overlap=3054.8`
- [x] The new lighting probe rules out lighting collapse on the bad draw: `[mwall_cover_light] ... rgb_min=0.255/0.255/0.255 rgb_max=1.000/1.000/1.000 rgb_avg=0.636/0.636/0.636 ... black=0 dark=0 white=2`ok
- [x] The new GPU readback probe shows the bound level-0 texture content is already mostly black before shading: `[mwall_cover_gpu] ... handle=91 ... gpu_avg=3/3/3/255 gpu_black=3971 center=0/0/0/255`, while the same draw still reports the expected source hash `0x272e5021` and a nonzero first texel sample `p0=104/80/156/255`
- [x] `[mwall_snapshot_overdraw] ... later_face_hits=0 later_cover_hits=0` plus the unchanged live binding state `[mwall_cover_live] ... expected=91 tex0=91 tex1=0 tex2=0` rule out later overdraw and make the remaining draw-state theories substantially weaker
- [x] This capture does not yet provide a paired GPU-readback control for a good hidden cover: the snapshot only read back `door45#0`, while the earlier `door45#9` draws happened before any snapshot request and emitted only `[mwall_coverbox]`

## Findings From The Closed-Versus-Animated Door Split

- [x] The new log already shows that the animated states are not all on the same upload branch as the bad closed state: `door45#0` uploads with `bm_flags=0x0 real_flags=0x0`, while `door45#1` through `door45#9` all log `bm_flags=0x9 real_flags=0x9`
- [x] In this codebase, `0x9` means `BM_FLAG_TRANSPARENT | BM_FLAG_RLE`, so the animated frames go through the RLE decode path and upload as alpha textures, while the closed `door45#0` frame bypasses RLE decode and uploads as an opaque stock texture
- [x] The portal state also differs between the good animated frames and the bad quiescent frame in the same log: the animated capture logs `wall_state=2 wall_flags=0x12 wid=7`, while the bad closed frame logs `wall_state=0 wall_flags=0x10 wid=2`; that makes the animated frames a useful same-door control, but not proof that the exact closed-state render path is healthy once animation starts
- [x] Because the nonzero frames already look normal by eye and all share the same transparent-RLE flag pattern, the control target no longer needs to be specifically `door45#9`; any visible animated `door45#1` through `door45#9` frame should be sufficient for the next instrumentation pass

## Work items

- [x] Add an Android helper in D2 that detects a single-texture draw inheriting merged-wall face context and forces the GLES3 shim back to non-external mode before `ogl_bindbmtex()`
- [x] Mirror the same helper and call site in D1
- [x] Re-run the Android validation pair after the state-reset trial lands
- [x] Confirm from a fresh phone log that the state-reset trial does not change the bad `door45#0` draw state or visible defect
- [x] Add an Android-only GPU readback probe for named bad stock textures so the log can compare GPU-sampled texels against the existing CPU-side source hash on the failing draw
- [x] Add hidden-cover lighting/color instrumentation so the log captures per-vertex or post-submit color ranges for the bad single draw and can rule in or rule out lighting collapse
- [x] Re-run the phone repro with the new GPU readback and lighting probes
- [ ] Add a paired control capture that snapshots any visible animated `door45#1` through `door45#9` frame, or another known-good hidden cover using the same transparent-RLE path
- [x] Log CPU-expanded upload-buffer stats or hashes for `door45#0` and at least one animated `door45#1` through `door45#9` frame immediately after `ogl_filltexbuf()` and compare that CPU upload payload against `[mwall_cover_gpu]`
- [x] Log the pre-`ogl_filltexbuf()` source branch for named door45 frames so the log explicitly distinguishes direct `bm->bm_data` upload from the RLE-decoded `decodebuf` path

## Validation

- [x] `android\run-code-quality.ps1 -Fix`
- [x] `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
- [x] Current workspace state builds cleanly after the upload-path diagnostics in D1 and D2 plus the widened animated-frame target filter
- [x] On-device `test_door45_pose_repro.json5` rerun with the new probes
- [ ] No paired animated-frame control snapshot yet for the new GPU readback probe

## Implemented In This Tranche

- [x] Thread the final per-vertex color array from D1 and D2 single-cover draw paths into the shared Android merged-wall logger
- [x] Add `[mwall_cover_light]` logging in `android/app/src/main/cpp/shared/merged_wall_debug.c` so bad hidden-cover draws now emit min/max/avg RGB and alpha ranges plus the first four vertex colors
- [x] Add one-shot per-snapshot `[mwall_cover_gpu]` readback logging for named target textures `door45#0` and any animated `door45#1` through `door45#9`, using a temporary FBO attached to the texture handle and hashing the returned RGBA payload
- [x] Add `[mwall_upload_src]` logging in D1 and D2 so named door45 uploads record whether the stock path used direct `bm->bm_data` or the RLE-decoded `decodebuf` source, plus source hashes and 254/255 counts before `ogl_filltexbuf()`
- [x] Add `[mwall_upload_cpu]` logging in D1 and D2 immediately after `ogl_filltexbuf()` so named door45 uploads record the expanded CPU payload hash, average RGBA, black-pixel count, alpha buckets, and sample texels before the GL upload completes
- [x] Thread `bitmapname` and a simple upload-path tag through `ogl_loadtexture()` so stock and PNG uploads can emit named diagnostics without changing the normal render logic
- [x] Keep Trial B and Trial C deferred until the new GPU and lighting logs say the current shim-path model is incomplete
- [x] Validation on this workstation currently stops at Android code-quality plus Android assemble/unit-test success; a fresh paired on-device animated-frame capture is still pending

## Next branches if this trial fails

- Primary next tranche: instrument the stock upload path in `ogl_filltexbuf()` / `ogl_loadtexture()` for named door45 frames so the log records the exact CPU-expanded bytes, averages, and hashes passed to GL for both the bad closed opaque frame and a good animated transparent-RLE frame
- Primary control: capture a deliberate paired snapshot for any visible animated `door45#1` through `door45#9` frame so the new GPU readback compares the closed opaque path against a same-door animated control, not just against unrelated hidden covers
- Secondary control: if the CPU-expanded upload bytes look sane for `door45#0` but `[mwall_cover_gpu]` still comes back black, add a named-texture RGBA8888 upload control to test whether the corruption appears during GL upload or driver storage rather than during palette expansion
- Deferred control from old Trial B: keep the dedicated single-sampler external-program control deferred unless the new upload-path evidence says the GPU-resident texture content is actually correct and only the final draw is wrong
- Deferred control from old Trial C: keep the dummy secondary-unit bind deferred unless later evidence reintroduces an actual secondary-unit dependency on the hidden single-cover path
