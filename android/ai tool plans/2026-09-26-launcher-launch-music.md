# Launcher launch buttons and music sources

- [x] Replace game selection with two launch buttons and precise readiness dialogs
- [x] Support D2 startup with supported D1-only assets and retain content-specific checks
- [x] Reorder file sections and rename music actions
- [x] Keep inaccessible registered music sources visible with diagnostics
- [x] Run scoped quality checks, JVM coverage, Android build and launcher integration

Initial finding: MusicInfoSection lists the registry while MusicPickerPage filters out sources whose original CUE or BIN access probe fails. Only emulators are attached; phone-specific access failures still need launcher diagnostics.


Validation:
- Scoped mixed-language quality checks passed
- 29 JVM tests passed (launch readiness, CD source diagnostics/visibility, registry persistence)
- Android x86_64 debug APK built with both native engines, no new compiler warnings
- Final isolated emulator regression passed all 40 steps: missing-file dialogs for both engines, enabled and deselected inaccessible music sources, D2 button with D1-only data, First Strike loading, movement and firing
- GuideBot runner discovery/sampling checks passed after removing the obsolete selector tap; keyboard manual script updated too
- Evidence: `temp/d1-launch-runtime-20260926-123116/automation_result.json` and `native-logcat.txt`

The initial broader 86-step gameplay run passed launcher, music, loading, movement, firing, door and reactor checks, then timed out at the fixture-specific exit sequence (step 78). No native gameplay changes were made for this task. Evidence: `temp/d1-launch-runtime-20260926-122706`. The new LauncherButtons mode deliberately stops after playable startup; the original full gameplay mode remains intact.

The phone was not connected. Settings now retain registered sources, use the local playback CUE instead of requiring the original import CUE, and report/log unavailable BIN/CUE paths. Phone logs after opening Music Settings on the updated build can identify any remaining access failure.
