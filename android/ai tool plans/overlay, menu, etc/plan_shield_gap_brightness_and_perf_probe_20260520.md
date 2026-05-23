# Shield gap, loading bar, and perf probe tranche

## Status

- [x] Reconfirm local anchors for the keyboard-gap draw path, loading overlay colors, and Android perf stats plumbing
- [x] Add Android-only bottom-strip fill for the keyboard-shifted menu viewport in D1 and D2
- [x] Brighten the loading bar border and fill colors
- [x] Add instrumentation-only GL and texture-load timing probes
- [x] Run a focused Android build
- [x] Update this file with results

## Scope

1. Fix the extra stale draws at the bottom of the level-select screen on Shield with a low-risk solid fill
2. Brighten the loading bar border and fill by about 2x without changing layout
3. Add instrumentation only for performance, with emphasis on low-level or general costs that could affect both loading and gameplay

## Current hypothesis

- The bottom artifact is caused by the keyboard-shift viewport exposing a strip that is never repainted before swap
- The user-facing loading bar issue is limited to two fixed Kotlin paint colors
- The performance issue may have both load-specific and frame-specific causes, so the first instrumentation pass should capture:
  - texture-cache stage totals for KTX2 read, PNG read, upload, and mask work
  - per-frame GL cost for MSAA resolve, swap, and GL error drain

## Validation target

- `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:externalNativeBuildDebug --console=plain`

## Notes

- Keep the bottom-fill fix Android-only and mirrored in D1 and D2
- Keep perf work instrumentation-only in this tranche
- Prefer existing overlay/debug-log plumbing over new UI surfaces

## Result

- The bottom-strip fix is implemented as a narrow Android-only scissored clear in `gr_flip()` after the keyboard offset is applied and before swap
- The loading bar border and fill are brightened without changing layout or text treatment
- Per-frame instrumentation now records:
  - swap time
  - MSAA resolve time
  - end-frame GL error drain time
- Texture-cache instrumentation now records:
  - KTX2 read time
  - PNG or JPG or TGA read time
  - upload time
  - mask time
- The video overlay shows the new flip and cache-stage rows, and the cache pass also emits one summary line to the texture debug log

## Validation

- `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug --console=plain`: passed