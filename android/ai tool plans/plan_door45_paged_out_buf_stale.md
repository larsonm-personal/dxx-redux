# plan_door45_paged_out_buf_stale

Status: in progress. Phase 1 probes and the Phase 2 `buf` refresh are landed in D1/D2 OGL, Android validation passed, the on-device door45 repro confirms the fix, the emulator regression passes, and the Phase 5 cleanup/consolidation plus runtime-selectable target follow-up are now landed. Remaining work is the optional manual visual confirmation.

## New evidence

From the log, for the bad closed cover `door45#0`:
- `[mwall_mip_upload] name=door45#0 path=stock ... compressed=0 ... bytes=8192 ... bm_flags=0x0 real_flags=0x0` fires at frame-0 level load.
- No `[mwall_upload_src]` and no `[mwall_upload_cpu]` for `door45#0`. The filters in those probes early-out on `!data`.
- `[mwall_cover_gpu]` at first tap: `src_hash=0x272e5021 src_idx254=0 src_idx255=0` (CPU source bytes exist NOW, during the tap) but `gpu_hash=0x72b3c670 gpu_avg=3/3/3/255 gpu_black=3971 p0=104/80/156/255`. GPU content is near-black garbage with a stray non-black pixel at p0.

For animated frames `door45#1..#9`:
- All three probes fire in order: `[mwall_upload_src]` (source `decodebuf`, 4096 palette bytes), `[mwall_upload_cpu]` (expanded RGBA 16384 bytes), `[mwall_mip_upload]`.
- Second tap caught frame `door45#4`: `gpu_hash=0xdb186899` exactly matches its `[mwall_upload_cpu] hash=0xdb186899`, and `src_hash=0x727c9cb4` exactly matches its `[mwall_upload_src] hash=0x727c9cb4`. Round-trip is clean on the RLE/transparent path.
- The same is true in level 2 for the second "similarly broken" door.

## Root-cause synthesis

In `ogl_loadbmtexture_f` (d2/arch/ogl/ogl.c line 4866 and d1/arch/ogl/ogl.c line 4833):

```
buf = bm->bm_data;           // line 4866 in d2  -- captured EARLY
... PNG / KTX2 attempts ...
#ifdef ANDROID                // line ~5173 in d2
if (bm->bm_flags & BM_FLAG_PAGED_OUT) {
    piggy_bitmap_page_in(bi); // bm->bm_data and bm->bm_flags updated HERE
}
#endif
... ogl_init_texture ...
if (bm->bm_flags & BM_FLAG_RLE) {
    ... gr_rle_decode into decodebuf ...
    buf = decodebuf;          // RLE branch REFRESHES buf
}
ogl_log_door45_upload_source(..., buf, ...); // logs if buf != NULL
ogl_loadtexture(buf, ..., bm->bm_flags, ...); // uploads whatever buf points to
```

For `door45#0`:
1. At entry `bm->bm_flags == BM_FLAG_PAGED_OUT`, `bm->bm_data == NULL` (set by `gr_init_bitmap(..., NULL)` in piggy.c line 486).
2. `buf = bm->bm_data` captures NULL.
3. No hi-res replacement, so no `return` from the PNG/KTX2 blocks.
4. ANDROID paged-out fallback calls `piggy_bitmap_page_in(bi)`; this populates `bm->bm_data` and updates `bm->bm_flags` (picks up real flags from `GameBitmapFlags[]`, which for `door45#0` is `0x0` -- no RLE).
5. `bm->bm_flags & BM_FLAG_RLE == 0`, so the RLE branch is skipped. `buf` is never refreshed; it remains NULL.
6. `ogl_log_door45_upload_source(..., NULL, ...)` hits the `!data` guard and silently drops the log. (Matches the missing lines in the new log.)
7. `ogl_loadtexture(NULL, ..., bm_flags=0, ...)` enters, sees `if (data)` is false (line 4611), skips both the `ogl_filltexbuf` and the `bufP = data` branches. The static `texbuf` allocated at line 4317 is never populated for this call. It still contains stale bytes from the previous upload.
8. `glTexImage2D(..., texbuf)` uploads the stale buffer. GPU now holds unrelated residue (near-black with a stray colored pixel -- exactly `gpu_avg=3/3/3/255 p0=104/80/156 center=0/0/0/0`).

For `door45#1..#9` this bug does NOT fire because the RLE branch refreshes `buf = decodebuf` after page-in, so the fresh palette bytes make it through.

This explains all four observations:
- closed cover: near-black garbage with a single stray pixel.
- animated frames: render correctly.
- the "other broken door" in level 2: any animated-door family whose `#0` frame is non-RLE and only drawn as a merged-wall cover will hit the same path.
- desktop builds are fine because desktop callers run `PIGGY_PAGE_IN` before reaching `ogl_loadbmtexture_f`, so `bm->bm_flags & BM_FLAG_PAGED_OUT == 0` and `bm->bm_data` is already valid at line 4866.

## Proposed fix (one-line, mirrored in d1 and d2)

Refresh `buf` after the Android paged-out fallback, before the RLE branch:

```c
#ifdef ANDROID
    if (bm->bm_flags & BM_FLAG_PAGED_OUT) {
        ... piggy_bitmap_page_in ...
    }
    buf = bm->bm_data;   /* refresh after page-in: bm_data was NULL at line 4866 */
#endif
```

Equivalent alternative: move the original `buf = bm->bm_data;` assignment to just after the ANDROID page-in block. Minimal diff, keeps upstreaming simple.

## Validation plan

- [x] Phase 1: add the targeted bm_data/page-in probes.
  - [x] Add `[mwall_loadbmtex_entry]` at the top of `ogl_loadbmtexture_f` (Android-only, gated by `ogl_is_door45_mip_diag_bitmap`) logging `name`, `bm_flags`, `bm->bm_data == NULL`, `bm->bm_w/h`, `bm->gltexture`.
  - [x] Add `[mwall_loadbmtex_post_pagein]` just after the paged-out page-in block with the same fields.
  - [x] Build, reinstall, repeat the level-1 pose repro, pull debuglog.
  - [x] Confirmed: in `debuglog_20260419_124745.txt`, `door45#0` logs `entry: bm_flags=0x10 data_null=1`, `post_pagein: bm_flags=0x0 data_null=0`; `door45#1..9` log `post_pagein: bm_flags=0x9 data_null=0`.
- [x] Phase 2: land the one-line fix in both d1 and d2 ogl.c.
  - [x] Refresh `buf = bm->bm_data;` after the Android paged-out fallback, before the RLE branch.
  - [x] Android validation: `android/run-code-quality.ps1 -Fix` and `android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest` passed after the mirrored D1/D2 edits.
  - [x] Re-run the repro. `door45#0` now logs `[mwall_upload_src]` and `[mwall_upload_cpu]`, and `[mwall_cover_gpu]` shows sane RGB instead of near-black garbage.
  - [x] GPU-readback confirmation: `door45#0` now reports `upload_src hash=0x272e5021`, `upload_cpu hash=0x09787999`, `gpu_avg=42/62/59/255`, `gpu_black=0`, `p0=32/64/64/255`, `center=32/56/48/255`. The prior failure signature (`gpu_avg=3/3/3/255 gpu_black=3971 p0=104/80/156`) is gone.
  - [ ] Optional manual visual confirmation on-device: closed door cover should render as the normal opaque door panel instead of near-black.
- [ ] Phase 3: sweep.
  - [x] Grep for other `buf = bm->bm_data;` or equivalent captures before PAGED_OUT handling in d1/d2 ogl paths that might have the same stale-capture pattern. Result: only the two `ogl_loadbmtexture_f` copies had this exact bug shape.
  - [x] Re-evaluate whether `ogl_loadtexture` should `memset(texbuf, 0, ...)` when `data == NULL`. Current recommendation: do not add it for now. There are no intentional `ogl_loadtexture(NULL, ...)` call sites in d1/d2 OGL, and a zero-fill fallback would mask future logic bugs instead of surfacing them.
- [ ] Phase 4: regression test.
  - [x] Land `android/game_scripts/test_door45_cover_gpu_regression.json5` to drive to the known seg-80 pose, request `merged_wall_snapshot`, and assert the focused target-cover GPU stats for the closed `door45#0` frame.
  - [x] Extend `merged_wall_snapshot` with a shared-Android `target_cover_gpu` object populated from the existing `[mwall_cover_gpu]` readback path so the script can assert the real fixed condition without scraping log files.
  - [x] Carry the target choice into the committed regression script via `set_debug texture_target=door45#0` and assert `debug_flags.texture_target == door45#0` so the selected texture is explicit at the automation layer instead of buried in C code.
  - [x] Note for the final script: the closed-door pose can legitimately yield `merged_wall_snapshot.status=no_projected_faces` and `selected_count=0` while still producing a valid `target_cover_gpu` object. The regression keys off `cover_event_count` and `target_cover_gpu`, not the older face-selection count.
  - [x] Validate with `android/run_test.ps1 -ScriptName test_door45_cover_gpu_regression.json5 -Game d2 -Install` on the working emulator, keeping the output in `temp/` per the harness guidance.
  - [x] Emulator result on 2026-04-19: PASS (file-based, 29/28 steps, 9115ms, TEST_EXIT=0). Final introspection still reported `merged_wall_snapshot.status=no_projected_faces` and `selected_count=0`, but `target_cover_gpu.valid=true` for `door45#0` with `src_hash_hex=272e5021`, `gpu_hash_hex=5aa172e1`, `gpu_avg_r/g/b=42/62/59`, `gpu_black=0`, and overlap about `189.33`.
- [ ] Phase 5: cleanup and framework consolidation.
  - [x] Remove the temporary `[mwall_loadbmtex_entry]`/`[mwall_loadbmtex_post_pagein]` probes now that the regression is green. These were root-cause probes only and are no longer needed.
  - [x] Keep `[mwall_upload_src]`, `[mwall_upload_cpu]`, `[mwall_mip_upload]`, and `[mwall_cover_gpu]` as the permanent texture-integrity diagnostics. They still provide the durable source-bytes, expanded-upload, mip-policy, and GPU-storage proof used by the regression and follow-up debugging.
  - [x] Move the duplicated `door45#` upload-diagnostic helpers out of `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` into a shared Android-side helper (`android_texture_debug.c/.h`), following the `merged_wall_debug.c` pattern, so future targeted texture probes do not require mirrored D1/D2 helper bodies.
  - [x] Replace the hard-coded `door45#` name gate with a runtime-selectable Android debug target in the shared helper. The framework now accepts `set_debug texture_target=<name>` for explicit regressions and defaults to crosshair-driven selection for tap/snapshot GPU readback when no explicit name is set.
  - [x] Expose the selected texture target through introspection as `debug_flags.texture_target` so automation can assert the configuration it requested.
  - [x] Leave the actual `buf = bm->bm_data;` refresh mirrored in D1/D2. That fix remains in the engine copies because it corrects real texture loading behavior, not just debug instrumentation.
  - [x] Post-cleanup validation on 2026-04-19: `android/run-code-quality.ps1 -Fix`, `android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`, and `android/run_test.ps1 -ScriptName test_door45_cover_gpu_regression.json5 -Game d2 -Install -TimeoutSeconds 120` all passed. Final introspection still reported `target_cover_gpu.valid=true` for `door45#0` with `src_hash_hex=272e5021`, `gpu_hash_hex=5aa172e1`, `gpu_avg_r/g/b=42/62/59`, `gpu_black=0`, `center=32/56/48/255`, `cover_event_count=3`, and `TEST_EXIT=0`.
  - [x] Runtime-target follow-up validation on 2026-04-19: `android/run-code-quality.ps1 -Fix`, `android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest`, and `android/run_test.ps1 -ScriptName test_door45_cover_gpu_regression.json5 -Game d2 -Install -TimeoutSeconds 120` all passed after adding the shared target setter/getter. Final introspection reported `debug_flags.texture_target=door45#0` and `merged_wall_snapshot.target_cover_gpu.cover_bot=door45#0` with `valid=true` and `TEST_EXIT=0`.

## Open questions / alternative theories still worth disproving

- Could `bm->bm_data` be non-NULL but point to an unmapped offset (`(ubyte*)(size_t)bmh.offset` per piggy.c line 1668)? If so, reading through it would SIGSEGV during the `!data` check on the upload probe (since it is non-NULL). The log shows no SIGSEGV and no `[mwall_upload_src]` line, so either `bm_data` is NULL at capture (most likely -- set by `gr_init_bitmap(..., NULL)` at line 486) or the filter skips for another reason. The Phase 1 probe will disambiguate.
- Could the residue in `texbuf` be from a different door45 frame? Check: `door45#9 upload_cpu p0=0/0/0/0` is very different from door45#0 gpu `p0=104/80/156`. That purple does not match any door45 upload in the log, so the residue is probably from a texture uploaded before the door45 burst (e.g. a level-load UI or background texture). If the fix does not resolve it, dump `texbuf[0..15]` at upload entry to characterize the residue and trace the prior upload.
- Could the broken level-2 door be on a different animation family with a similar non-RLE `#0` frame? Confirm in Phase 2 by checking the `[mwall_cover_gpu]` lines in that level for the new log: any family whose `#0` frame has `bm_flags=0x0` and no upload_src/upload_cpu pair is the same class of bug.

## Notes / memory hooks

- The stale-`buf` pattern is a direct consequence of adding the ANDROID paged-out fallback between the original `buf = bm->bm_data;` line and the RLE/upload code. Upstream dxx-redux does not have this fallback, so the bug is android-port-specific and safe to fix behind `#ifdef ANDROID` (or simply by moving the assignment).
- Because `door45#0` is only rendered as a merged-wall cover (not as a normal face), it never gets PIGGY_PAGE_IN'd through the regular visibility path in the android port, which is why this bug survived until the merged-wall cover work exposed it.
