# C5: Google Play Games Sign-In -- Implementation Plan

## Goal

Replace the per-process dev token (`dev-UUID`) with real Google Play Games
Services (GPGS) authentication. When GPGS is unavailable (no Play Services,
emulator), fall back gracefully to the existing dev token path.

## Architecture

### Auth Config

A `auth_config.json5` template file holds the Google OAuth2 web client ID
needed by `requestServerSideAccess()`. This is the **web** client ID from
Google API Console (type "Web application"), NOT the Android client ID.

- Template: `android/auth_config.json5.template` (committed)
- Actual:   `android/auth_config.json5` (gitignored, user fills in)
- The Gradle build reads the client ID at compile time and injects it
  into `BuildConfig` (or a generated Kotlin constant)

### Sign-In Flow

1. `SetupActivity.onCreate()` calls `PlayGamesSdk.initialize(this)`
2. When user taps "Connect" on the Online tab, `PlayGamesAuth` checks
   if already authenticated via `isAuthenticated`
3. If authenticated, requests server auth code via
   `requestServerSideAccess(webClientId, forceRefresh=false)`
4. Auth code sent as `play_games_token` in AUTHENTICATE message
5. Server exchanges with Google OAuth2, gets stable GPGS player ID
6. On failure or non-GPGS device, falls back to dev token

### Files to Create

- `android/auth_config.json5.template` -- template with instructions
- `android/app/src/main/java/com/dxxredux/app/multiplayer/PlayGamesAuth.kt`

### Files to Modify

- `android/get_deps/tool_versions.conf` -- add PLAY_GAMES_VERSION
- `android/app/build.gradle` -- add GPGS dependency, read auth_config
- `android/app/src/main/AndroidManifest.xml` -- add GPGS app_id metadata
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` -- init SDK
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingService.kt` -- use PlayGamesAuth
- `android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkProtocol.kt` -- auth_method field
- `.gitignore` -- add auth_config.json5

## Detailed Steps

1. Pin GPGS SDK version in tool_versions.conf
2. Add dependency in build.gradle
3. Create auth_config.json5.template and gitignore auth_config.json5
4. Add Gradle task to read auth_config and expose client ID as buildConfigField
5. Add GPGS app_id metadata to AndroidManifest (read from auth_config)
6. Create PlayGamesAuth.kt: init, isAvailable, getAuthToken(activity) -> String?
7. Update MatchmakingService.sendAuthenticate to use PlayGamesAuth token
8. Initialize PlayGamesSdk in SetupActivity.onCreate
9. Build + code quality
10. Update plan file
