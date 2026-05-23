# Touch editor floating stick region edges - work plan

Status legend: `[ ]` not started, `[~]` in progress, `[x]` done.

## Scope

- Let touch sticks in floating mode expose the same tap-selectable, draggable region borders that mouse-mode sticks already expose in the touch editor.

## Local hypothesis

- Floating sticks already render their floating-zone overlay and already reuse the shared zone-resize path once an edge selection exists.
- The missing behavior is local to `TouchEditorPage.kt`:
  - `hitTestAll(...)` only adds stick floating-zone edge hits when `stick.mouseMode` is true.
  - `drawAllControls(...)` only renders selected stick-zone edges in the mouse-mode branch.

## Cheap discriminating check

- Add a focused unit test that hit-tests a floating, non-mouse stick at its floating-zone edge and expects a `SELECTED_TYPE_STICK_ZONE_EDGE` hit.

## Plan

- [x] Patch `TouchEditorPage.kt` so editable stick-zone edges are available for `stick.floating || stick.mouseMode`.
- [x] Keep existing whole-stick move behavior intact for non-mouse floating sticks while adding edge highlight rendering for the selected region edge.
- [x] Add focused unit coverage for floating-stick edge hit testing.
- [x] Run Android Kotlin compile and unit tests for the touched slice.

## Result

- Floating, non-mouse sticks now contribute floating-zone edge hits in `hitTestAll(...)`, so the existing edge-selection and drag-resize path can be entered by tapping a floating-zone border.
- Selected floating-stick zone edges now render their highlighted border line in the editor, matching the mouse-mode affordance without changing whole-stick movement behavior.
- Added focused unit coverage for both the new positive case and the existing negative case.

## Validation

- `android\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.TouchEditorZoneEdgeTest`
- `android\stop-stale-formatters.ps1`
- `android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt','android/app/src/test/java/com/dxxredux/app/TouchEditorZoneEdgeTest.kt')`
- `android\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.TouchEditorZoneEdgeTest`
- file diagnostics: no errors in `TouchEditorPage.kt` or `TouchEditorZoneEdgeTest.kt`
