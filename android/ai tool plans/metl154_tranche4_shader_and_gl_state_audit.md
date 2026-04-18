# Metl154 tranche 4: shader and GL-state audit

## Scope

This tranche implements Part C.3 from
`metl154_postfix_cleanup_and_debug_harness.md`:

- audit the merged-wall shader variants that survived the cleanup tranches
- audit Android-only GL state changes that were added during the metl154 investigation
- remove or merge any shader/state path that is no longer needed after the cached-premerge fix and shared extraction

Out of scope for this tranche:

- launcher debug option UI from Part C.1
- in-game debug panel work from Part C.2
- crosshair debug-trigger capture from Part D
- new regression harness file format from Part E

## Required validation

1. `android\run-code-quality.ps1 -Fix`
2. `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `android\run_test.ps1 -ScriptName test_merged_wall_snapshot_regression.json5 -Game d2 -Install`

## Work items

- [x] Audit every surviving merged-wall shader route and record whether it should stay, merge, or be deleted
- [x] Audit investigation-era GL state changes (`glPolygonOffset`, cull/depth toggles, wrap restores) and classify each as keep, move, or delete
- [x] Remove or simplify any shader/state path proven unnecessary after tranche 3
- [x] Re-run the merged-wall regression and confirm no route regression on Counterstrike L1
- [x] Update this file with findings and final validation

## Research notes

- The shared extraction in tranche 3 left the route-level snapshot and regression in place, so this tranche can validate behavioral equivalence with the existing `merged_wall_snapshot` automation.
- The current stable regression invariant is wall-level, not split-triangle-level: `seg=83`, `side=3`, `route=merge_cached`, `merge_impl=gpu_cached_single`.
- The audit target is specifically Android-only merged-wall rendering state, not unrelated desktop GL paths.
- The GLES3 shim audit showed only the fixed-function compatibility path plus the external-program override hooks used by the retained merged-wall shaders. The metl154-era shader sprawl was no longer in the shim.
- The still-live route split after tranche 3 is small and intentional: cached premerge stays for the fixed wall path, the GPU two-pass path stays as the generic fallback, and the old texmerge path stays only behind the surfaced legacy experiment toggle.
- The dead investigation-only experiment branches were still present in D1 and D2 OGL even though the Android-facing control surface already only exposed `default` and `force_legacy_texmerge`.

## Findings

### Shader route audit

- Keep `merge_cached` with `gpu_cached_single` as the validated fix path.
- Keep the GPU two-pass merged-wall fallback path for non-cached cases.
- Keep the legacy texmerge route only behind `MERGED_WALL_EXPERIMENT_FORCE_LEGACY_TEXMERGE`.
- Delete the dead investigation-only experiment branches that no longer had any live Android control path.

### GL-state audit

- `glPolygonOffset`, temporary cull disable, and temporary depth disable in the two-pass fallback path were audited but left in place for now.
- Those state changes are still part of the only surviving behavioral fallback path, so deleting them in this tranche would have been speculative rather than evidence-driven.
- Texture-wrap restore logic was audited and left unchanged because this tranche did not find proof that it was dead or wrong.

## Implementation notes

- Removed dead merged-wall experiment constants from the shared Android header and collapsed the surfaced experiment control to `default` and `force_legacy_texmerge` only.
- Simplified JNI debug-flag naming and experiment-mode naming to the surviving merged-wall surface.
- Removed dead `clip_all`, `overlay_only`, `alpha_raw`, forced-RGBA, no-mip, and forced-stock experiment branches from both D1 and D2 OGL texture and draw paths.
- Removed the obsolete per-mode texture invalidation path on experiment apply. The experiment apply log now records `texture_reload=0` instead of pretending to reload deleted experiment paths.
- Left the fallback two-pass GL-state block intact aside from deleting dead experiment gating around it.

## Validation result

- `android\run-code-quality.ps1 -Fix`: passed
- `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`: passed
- `adb logcat -c; .\android\run_test.ps1 -ScriptName test_merged_wall_snapshot_regression.json5 -Game d2 -Install 2>&1 | Out-File temp\test_output.txt -Encoding utf8; Write-Output "EXIT: $LASTEXITCODE"`: passed

## Status

- [x] Audit complete
- [x] Cleanup complete
- [x] Validation complete