# Game Preferences launcher section plan

- [x] Locate the Game Preferences section definitions and existing launcher-offer preferences
- [x] Move the Launcher section to the top with resume offer first and demo installer offer second
- [x] Run focused formatting or tests for the touched launcher preference code if practical

Validation:
- [x] `.\android\run-code-quality.ps1 -Fix -Paths android\app\src\main\java\com\dxxredux\app\EnginePreferencesPage.kt "android\ai tool plans\launcher admin\plan_game_preferences_launcher_section_20260613.md"`
- [x] `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.DemoInstallerOfferTest`
