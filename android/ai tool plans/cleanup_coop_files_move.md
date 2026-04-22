# Phase 3 Cleanup: Coop File Move

## Goal

Start phase 3 by relocating the identical coop warp implementation into shared
native code, while keeping the larger `coop_save` relocation in the same tranche
plan for a follow-up step after the first build-validated extraction.

## Scope

- android/app/src/main/cpp/shared/coop/coop_warp.c
- android/app/src/main/cpp/shared/coop/coop_warp.h
- d1/main/coop_warp.c
- d1/main/coop_warp.h
- d2/main/coop_warp.c
- d2/main/coop_warp.h

## Work items

- [x] Verify `d1/main/coop_warp.c` and `d2/main/coop_warp.c` are identical
- [x] Move the shared `coop_warp` source and header to `android/.../shared/coop`
- [x] Replace the D1 and D2 `coop_warp` copies with thin wrappers that preserve include paths
- [x] Run validation and record the result
- [x] Survey `coop_save` divergence and decide whether the follow-up move can stay single-copy or must split per game

## Guardrails

- Keep existing call sites unchanged in D1 and D2
- Keep non-Android builds intact by preserving the existing `coop_warp.h` include path in each game tree
- Do not widen this first phase-3 step into `coop_save` logic changes before the extraction path is validated

## Result

- phase 3 is now started with the low-risk half of the coop-file move: `coop_warp`
	lives in `android/app/src/main/cpp/shared/coop/` and the D1/D2 source and
	header paths were reduced to thin wrappers so existing include sites stay unchanged
- Android validation passed before and after the code-quality pass:
	- `android\\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- repo code-quality passed with:
	- `android\\run-code-quality.ps1 -Fix`
- Windows host validation also passed for both games with:
	- `run-windows-build.ps1 -Target both`
- the quick `coop_save` survey indicates the follow-up should not assume a clean
	single-copy move: `d2/main/coop_save.h` already adds escort/buddy metadata
	fields beyond D1, and the current `coop_save.c` file diff is large enough to
	prefer split shared copies or a shared core with per-game hooks