# Touch More Overlay Fixes - 2026-05-11

## Goal
Finish the Android touch overlay "More" button so it is tappable when open, has an editable location in the touch overlay editor, and lists actions that are not already exposed by the current touch layout.

## Plan
1. [x] Create this plan file
2. [x] Research the original "More" / remaining-actions design in code and git history
3. [x] Fix open More popup hit handling so it wins over underlying stick or axis regions
4. [x] Add configurable More button placement to the touch layout model and editor
5. [x] Expand remaining-action selection so the popup includes the expected missing touch actions, with D1/D2 filtering
6. [x] Run focused tests and code quality checks, then update this plan with results

## Notes
- Keep the change in Android Kotlin code unless the research shows a C-side action is missing.
- Preserve existing touch layout files where possible; add defaults for older layouts.
- Original design was introduced around commit 52da9ba as a remaining-actions overflow for actions not already exposed by the current layout.
- The open popup tap issue was caused by event short-circuiting while the popup was open, not by a native z-order problem. Open-state events now still flow through the More handler before any underlying control handling.
- The More button is now stored as `TouchLayout.moreActions`, serialized in normal and human-readable JSON, and editable in the touch editor for position, size, and opacity.
- Remaining-action selection now checks direct buttons and radial segments, filters already-bound actions, omits D2-only actions in D1, and includes weapon, guidebot, utility, pause, menu, quick save/load, gyro, and launcher-return actions where applicable.
- Validation: scoped ktlint passed for `TouchControl.kt`, `TouchOverlayView.kt`, `TouchEditorPage.kt`, and `HumanReadableConfig.kt`.
- Validation: focused unit tests passed with `:app:testDebugUnitTest --tests com.dxxredux.app.RemainingKeyTouchActionsTest --tests com.dxxredux.app.TouchEditorZoneEdgeTest`.
- Full `android\\run-code-quality.ps1 -Fix` was attempted, but it failed on unrelated pre-existing `SetupActivity.kt` ktlint max-line-length issues and PSScriptAnalyzer output. Incidental formatter diffs outside this task were cleaned back out.