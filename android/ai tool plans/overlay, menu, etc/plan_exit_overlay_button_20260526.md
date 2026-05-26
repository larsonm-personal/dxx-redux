# Exit Overlay Button Fix

## Goal
Fix Android's always-visible exit overlay button when the game is in non-standard screens such as multiplayer waits and cutscenes.

## Plan
- [x] Trace the Kotlin exit button path and native meta-action/JNI handlers.
- [x] Identify screens that do not process the current injected input or quit request.
- [x] Add a narrow shared native hook so blocking/waiting screens can observe the exit request.
- [x] Validate with focused build or tests where practical.

## Result
- `META_RETURN_TO_LAUNCHER` now queues the exit autosave only when the current front window can consume it: gameplay or the Android-aware pause window in single-player.
- Other screens push `SDL_QUIT` immediately with `android_force_quit` set, covering multiplayer waits, movie/cutscene windows, and menus that do not run the game-control autosave consumer.
- Validation passed:
  - `android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/cpp/shared/android_meta_actions.c','android/app/src/main/java/com/dxxredux/app/ExitButtonView.kt')`
  - `.\android\gradlew.bat -p android assembleDebug` with `JAVA_HOME=C:\local\jdk-21`
