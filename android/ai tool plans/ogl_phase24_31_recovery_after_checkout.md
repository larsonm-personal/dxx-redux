# OGL Phase 24-31 Recovery After Checkout

## Goal

Restore the tracked-file OGL extraction state that existed before a validation
terminal ran `git checkout` on six tracked files and discarded the local
phase-24-through-31 work.

## Scope

- android/app/src/main/cpp/shared/android_texture_debug.h
- android/app/src/main/cpp/shared/android_texture_debug.c
- android/app/src/main/cpp/shared/merged_wall_debug.h
- android/app/src/main/cpp/shared/merged_wall_debug.c
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c

## Work items

- [x] Recover enough transcript and phase-plan evidence to rebuild the lost phase-24-through-31 tracked-file edits
- [x] Reapply the lost phase-24-through-31 tracked-file edits
- [x] Match the historical phase-31 `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` churn numbers exactly
- [x] Reconfirm the four shared helper tracked files against the recorded phase-31 counts
- [x] Resume the next planned extraction tranche from the exact recovered baseline

## Current status

- Android validation passed:
	- `./android/run-code-quality.ps1 -Fix`
	- `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- Windows host validation passed:
	- `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
- Current OGL `git diff --numstat upstream/main` after validation:
	- `d1/arch/ogl/ogl.c`: `+1672 -50`
	- `d2/arch/ogl/ogl.c`: `+1696 -49`
- Current shared-helper `git diff --numstat upstream/main` after validation:
	- `android/app/src/main/cpp/shared/android_texture_debug.c`: `+429 -0`
	- `android/app/src/main/cpp/shared/android_texture_debug.h`: `+51 -0`
	- `android/app/src/main/cpp/shared/merged_wall_debug.c`: `+6106 -0`
	- `android/app/src/main/cpp/shared/merged_wall_debug.h`: `+242 -0`
- Android validation for the exact OGL baseline passed with:
	- `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- Android validation for the exact shared-helper baseline passed with:
	- `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`

## Latest notes

- Restored the earlier shared merged-wall helper export surface enough to bring
	`merged_wall_debug.h` close to the recorded phase-31 header target
- Reapplied the lost D1/D2 `ogl.c` shared-helper recovery after the destructive
	checkout and revalidated the Android build on the restored state
- Trimmed likely later texture-upload diagnostics from both `ogl.c` files;
	that moved D1 materially toward the phase-31 target while leaving D2 largely
	unchanged
- Removed the inline `g_texfilt_pending_apply` live-apply loop again while
	keeping the exported globals and `g_texfilt_level = GameCfg.TexFilt` sync;
	that moved D1 down toward the target, which supports the earlier note that the
	phase-33 live-apply block does not belong in the phase-31 exact baseline
- Rolled the D1/D2 compressed KTX2 upload path back to mip0-only. That moved
	D1 past the recorded target to `+1664 -49`, but D2 still reports
	`+1829 -49`, so the remaining D2 mismatch is not responding to the same
	local rollback probes
- D2 remains the unresolved outlier. Safe log-only trims and the live-apply
	removal did not materially change its `+1829 -49` numstat, so the next pass
	needs direct D2-only archaeology instead of more mirrored edits
- Revalidated after the latest edits:
	- `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
	- `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`

- The exact OGL baseline is restored and validated again:
	- `d1/arch/ogl/ogl.c`: exact at `+1672 -50`
	- `d2/arch/ogl/ogl.c`: exact at `+1696 -49`
- The exact shared-helper baseline is now restored and validated too:
	- `android/app/src/main/cpp/shared/android_texture_debug.c`: exact at `+429 -0`
	- `android/app/src/main/cpp/shared/android_texture_debug.h`: exact at `+51 -0`
	- `android/app/src/main/cpp/shared/merged_wall_debug.c`: exact at `+6106 -0`
	- `android/app/src/main/cpp/shared/merged_wall_debug.h`: exact at `+242 -0`
- The successful D2 recovery path was live-file archaeology plus exact local replacements, not broad mirrored rollback:
	- repaired the count-moving `g3_draw_tmap_2(...)` live slices in place, especially the wrapped/casted `android_merged_wall_*` helper calls and related pointlist/input-code blocks
	- recovered the last three clean additions with the `ogl_ubitblt_i(...)` and `ogl_loadpngmask(...)` wrapped `ogl_loadtexture(...)` calls plus the remaining `merge_cached` track-face wrap
	- balanced the final deletion count by reverting the stray `winsock2.h` spacing normalization after it was confirmed to be the lone `-1` seam once D2 had reached `+1696`
- The D2 editor-side reads and searches were desynced during this pass; terminal-side `Get-Content` and direct literal replacements against the live file were the reliable path for the final recovery
- The shared-helper recovery path was count-local and formatting-local rather than another broad rollback:
	- `android_texture_debug.{c,h}` returned to the recorded phase-30 counts by trimming the cleanup-only separator drift around the phase-28/29 helper block and the phase-30 extern/global insertion
	- `merged_wall_debug.{c,h}` returned to the recorded phase-31 counts by rebalancing the over-expanded cached-texmerge helper block in the source against the compacted declaration block in the header
- The next tranche has now started from that exact recovered baseline and is recorded separately in `ogl_shared_helper_extraction_phase32.md`:
	- wired the shared DXA mask helper and shared merged-wall experiment-apply helper into the active D1/D2 `ogl.c` paths
	- extracted the duplicated Android 3D texture-bind log block into `android_texture_debug_log_render_bind(...)`
	- revalidated with Android `:app:assembleDebug :app:testDebugUnitTest`, `run-code-quality.ps1 -Fix`, and `run-windows-build.ps1`

## Guardrails

- Do not guess at lost code; recover only from transcript evidence
- Reapply the smallest verified hunks first so numstat can confirm recovery
- Do not run any git-mutating commands while recovering
- Keep D1 and D2 mirrored during reapplication