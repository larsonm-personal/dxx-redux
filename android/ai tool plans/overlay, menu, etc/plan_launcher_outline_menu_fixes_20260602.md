# Launcher outline and menu fixes plan

## Scope
- Advanced settings file rows for crash reports and demos should match debug log green d-pad outlining.
- Storage inspector file info pop-open should default to Close.
- Weapon autoselect edit mode should let B unselect and cancel the active move.
- Resume recent save controls, screenshot, and choose-save pop-open items should have green d-pad outlining.
- Choose-save pop-open should show scroll indicator icons.

## Checklist
- [x] Locate existing launcher focus outline and scroll indicator patterns.
- [x] Patch crash report, demo, resume-save, and choose-save UI rows.
- [x] Patch storage inspector file-info default selection.
- [x] Patch weapon autoselect B-button behavior.
- [x] Run focused validation and formatter if practical.

## Validation
- `android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt','android/app/src/main/java/com/dxxredux/app/SetupResumePanel.kt','android/app/src/main/java/com/dxxredux/app/AutoselectEditorPage.kt','android/app/src/test/java/com/dxxredux/app/AutoselectReorderScrollTest.kt')`
- `cd android; .\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.AutoselectReorderScrollTest --tests com.dxxredux.app.ResumeSavePanelTest`
