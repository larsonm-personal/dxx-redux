Abort Autosave Slot Plan

Goal: make in-game Abort Game create a separate Android autosave type and show it in the launcher recent-save resume panel without overwriting exit, launcher-return, or minimize saves.

1. [completed] Trace existing autosave slot constants, save triggers, save metadata, and recent-save panel ordering.
2. [completed] Add an Android-only abort autosave slot/type in both D1 and D2, triggered only by the in-game Abort Game path.
3. [completed] Update launcher-side save classification and recent-save rows so abort saves are shown as a distinct option.
4. [completed] Add or extend focused tests for the new abort autosave type.
5. [completed] Run focused validation and mark completed plan items.

Validation:
- `android\run-code-quality.ps1 -Fix -Paths android\app\src\main\cpp\shared\android_save_meta.h ...` passed after formatting the shared metadata header.
- `android\gradlew.bat -p android testDebugUnitTest --tests com.dxxredux.app.ResumeSavePanelTest` passed with JDK 21.
- `buildd1\maths\test_android_save_meta.exe` passed after rebuilding the target through the Visual Studio x86 developer environment.
- `buildd2\maths\test_android_save_meta.exe` passed after rebuilding the target through the Visual Studio x86 developer environment.
