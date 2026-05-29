# Multiple Controller And Touch Config Slots

## Goal
- Add named slot sets for controller configs and touch layouts
- Keep slot 0 as a protected default slot
- Share the slot editing UI between controller and touch editors while keeping backing data independent
- Update import and export paths so all-config export includes every slot, while editor-level export keeps the current slot only

## Phases
- [x] Map existing controller and touch config persistence, presets, editor UI, and import/export paths
- [x] Add a shared slot model and repository helpers with 25 character name limits and protected default slot behavior
- [x] Wire controller config slots into load, save, preset apply, editor display, and current-slot import/export
- [x] Wire touch layout slots into load, save, preset apply, editor display, and current-slot import/export
- [x] Update all-config import/export schema to include arrays of controller and touch slots
- [x] Add or update focused integration/unit tests for slot persistence and import/export
- [x] Run formatting, build, and relevant tests, then mark completed phases

## Validation
- android/run-code-quality.ps1 -Fix
- android/run-code-quality.ps1
- android/gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ConfigSlotRepositoryTest --tests com.dxxredux.app.ControllerConfigSerializationTest
- android/gradlew.bat :app:assembleDebug

## Follow-up Refinements
- [x] Make the shared slot dialog use a dropdown for active slot selection
- [x] Reduce slot dialog button text and padding so labels fit
- [x] Add scroll indicators to the slot dialog body
- [x] Rename duplicate prompt heading to copy to new slot and editable slot field to edit slot name
- [x] Hide manual controller-page action highlights on touch devices until controller navigation is active
- [x] Re-run focused tests, formatting, and Android assemble

## Follow-up Validation
- android/run-code-quality.ps1 -Fix
- android/run-code-quality.ps1
- android/gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ConfigSlotRepositoryTest --tests com.dxxredux.app.LauncherFocusPolicyTest --tests com.dxxredux.app.ControllerConfigSerializationTest
- android/gradlew.bat :app:assembleDebug
- git diff --check

## Slot Ui Follow-up
- [x] Auto-save edited slot names on keyboard done or focus loss and remove Apply Name
- [x] Show current editor slot labels as slot: [name]
- [x] Anchor the touch editor slot label to the screen bottom-left corner
- [x] Move controller Save and Cancel actions to the bottom row with Export and Import in the middle row
- [x] Re-run focused tests, formatting, and Android assemble

## Slot Ui Follow-up Validation
- android/run-code-quality.ps1 -Fix
- android/run-code-quality.ps1
- android/gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ConfigSlotRepositoryTest --tests com.dxxredux.app.LauncherFocusPolicyTest --tests com.dxxredux.app.ControllerConfigSerializationTest
- android/gradlew.bat :app:assembleDebug

## Touch Toolbar Wrap Follow-up
- [x] Let the portrait touch editor bottom-sheet toolbar wrap onto multiple rows
- [x] Size the touch editor bottom-sheet peek height to the wrapped toolbar height
- [x] Re-run formatting and Android validation

## Touch Toolbar Wrap Validation
- android/run-code-quality.ps1 -Fix
- android/run-code-quality.ps1
- android/gradlew.bat :app:assembleDebug

## Notes
- Do not edit android/outstanding_bugs.md
- Keep existing single-config files readable as default slot inputs
- Bump config version only if an existing persisted format needs forced regeneration