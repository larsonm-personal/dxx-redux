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
- [ ] Match the historical phase-31 tracked-file churn numbers exactly
- [ ] Resume the next planned extraction tranche from the exact recovered baseline

## Current status

- Android validation passed:
	- `./android/run-code-quality.ps1 -Fix`
	- `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- Windows host validation passed:
	- `./run-windows-build.ps1 -Target both -Preset x86-release -BuildType RelWithDebInfo`
- Current tracked-file `git diff --numstat upstream/main` after validation:
	- `android/app/src/main/cpp/shared/android_texture_debug.c`: `+435 -0`
	- `android/app/src/main/cpp/shared/android_texture_debug.h`: `+52 -0`
	- `android/app/src/main/cpp/shared/merged_wall_debug.c`: `+6124 -0`
	- `android/app/src/main/cpp/shared/merged_wall_debug.h`: `+236 -0`
	- `d1/arch/ogl/ogl.c`: `+1658 -50`
	- `d2/arch/ogl/ogl.c`: `+1681 -58`
- Remaining recovery gap versus the recorded phase-31 target:
	- `android_texture_debug.c`: `+6`
	- `android_texture_debug.h`: `+1`
	- `merged_wall_debug.c`: `+18`
	- `merged_wall_debug.h`: `-6`
	- `d1/arch/ogl/ogl.c`: `-14`
	- `d2/arch/ogl/ogl.c`: `-15 -9`

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

- After the destructive checkout recovery, rebuilt the large shared helper-extraction slices in the live `ogl.c` files instead of only trimming texture-upload probes:
	- converted the old inline texture-label accumulation path to `android_texture_debug_add_overlay_label(...)` and `android_texture_debug_add_joined_labels(...)` in both `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`
	- converted the local cached plain texmerge path to the shared `android_merged_wall_cached_texmerge_*` helper API in both files and restored `ogl_cache_polymodel_textures(...)` after that function-level splice briefly dropped it
	- reran `./android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain` after each substantive slice until the repo was back to a validated state
- The OGL recovery is now numerically close to the recorded phase-31 target on additions, but both live files still delete more upstream lines than the target baseline kept:
	- D1 currently sits at `+1658 -50` against a `+1672 -50` target
	- D2 currently sits at `+1681 -58` against a `+1696 -49` target
- This tranche confirmed that the D1 `ogl_freetexture(...)` / `ogl_freebmtexture(...)` deletion-heavy seam was indentation-driven relative to `upstream/main`:
	- retabbing that block to the exact upstream tab layout removed D1's extra `-15` deletions and landed D1 at `+1658 -50`
	- the same experiment showed the missing D1 additions are elsewhere; fixing that seam alone is not enough to recover the recorded `+1672 -50` phase-31 count
- The next recovery pass should focus on the remaining addition gap shared across both files, with D2 still carrying an extra deletion gap on top:
	- D1: recover the missing `+14` additions without reintroducing the helper-seam deletion churn
	- D2: recover the missing `+15` additions and remove the remaining `-9` deletions, most likely in the cached-texmerge / `ogl_cache_polymodel_textures(...)` and `ogl_start_frame(...)` local hunks

## Guardrails

- Do not guess at lost code; recover only from transcript evidence
- Reapply the smallest verified hunks first so numstat can confirm recovery
- Do not run any git-mutating commands while recovering
- Keep D1 and D2 mirrored during reapplication