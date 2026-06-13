## Save music overlay preferences to player file

Goal: persist the expanded Android music choices in the player save file,
using the existing playsave-owned file format helpers as the source of truth.

- [x] Find current Android/player-file music preference hooks
- [x] Add persisted fields for source, mission soundtrack preference, play order, and volume
- [x] Load those fields into runtime music state when the player is selected
- [x] Save those fields when overlay music settings change
- [x] Keep launcher/shared prefs aligned where Android needs them
- [x] Run scoped code quality and focused verification

Verification:
- `.\android\run-code-quality.ps1 -Fix -Paths @(...)`
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.AdminTrayUiTest --tests com.dxxredux.app.SettingsChildOverlayControllerTest --tests com.dxxredux.app.MusicLaunchPolicyTest`
- `.\gradlew.bat :app:externalNativeBuildDebug`
