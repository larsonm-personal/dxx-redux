## Touch overlay music menu refactor implementation

Goal: make the in-game Android music overlay control source, play/pause,
volume, one-track-per-level behavior, and tracks live without relaunch.

- [x] Add native Android music runtime state/apply APIs shared by D1 and D2
- [x] Add Android-only builtin music source policy so base MIDI can bypass mission song lists
- [x] Add single-player overlay pause that does not pause music
- [x] Replace the track-only music panel with source, play/pause, volume, play-order, and scroll controls
- [x] Add Music to the normal game settings tray, not just gamepad-only mode
- [x] Persist launcher-backed music prefs when overlay changes source
- [x] Add/update focused JVM tests for tray policy and panel navigation helpers
- [x] Run scoped code quality and focused build/test verification

Verification:
- `.\android\run-code-quality.ps1 -Fix -Paths @(...)`
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.AdminTrayUiTest --tests com.dxxredux.app.SettingsChildOverlayControllerTest`
- `.\gradlew.bat :app:externalNativeBuildDebug`
