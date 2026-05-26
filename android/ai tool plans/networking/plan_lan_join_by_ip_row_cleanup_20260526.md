# Plan: LAN Join by IP Row Cleanup

## Scope

- Launcher-only Compose layout cleanup for the LAN Games screen
- Remove the portrait `Your IP` text from the title row
- Move `Your IP` and `Join by IP` together into a compact portrait row
- Keep `Host LAN Game` and the scan toggle sharing the main action row

## Steps

- [x] Read project instructions and locate the LAN Games Compose layout
- [x] Patch the LAN header/action layout with the smallest practical change
- [x] Run Kotlin formatting or targeted quality checks if available
- [x] Run a targeted compile check if practical

## Notes

- Existing unrelated local changes are present and should not be touched
- This should stay in `android/app/src/main/java/com/dxxredux/app/multiplayer/`
- Initial pass applied this to landscape; the follow-up corrected the scope to portrait and restored the landscape layout
- Follow-up: keep the portrait IP visible on the same row as the compact `Join by IP` button, with IP text on the left

## Validation

- `android\run-code-quality.ps1 --fix` was attempted, but this PowerShell script expects `-Fix`; that run checked Kotlin successfully and failed on existing unrelated PowerShell/shfmt issues
- `android\run-code-quality.ps1 -Fix -Paths android\app\src\main\java\com\dxxredux\app\multiplayer\LanDiscoveryTab.kt`
- `android\run-code-quality.ps1 -Fix -Paths android\app\src\main\java\com\dxxredux\app\multiplayer\MultiplayerScreen.kt`
- `android\gradlew.bat :app:compileDebugKotlin` from `android\`
