# Launcher GPGS APP_ID Manifest Type Fix

## Plan

- [x] Reproduce launcher startup on emulator and capture fresh logcat
- [x] Identify launcher-local startup fault candidate
- [x] Switch GPGS APP_ID manifest metadata to a string resource type
- [x] Re-run cold launcher start and confirm the Bundle ClassCast warning is gone

## Notes

- Live repro showed `SetupActivity` stays alive, but Play Games v2 logs `Key com.google.android.gms.games.APP_ID expected String but value was a java.lang.Integer`
- The current manifest injects `${gamesAppId}` directly into `android:value`, which becomes a numeric manifest literal
- Fixed by generating `@string/games_app_id` via Gradle `resValue` and pointing the manifest meta-data at that string resource
- Validation: `:app:assembleDebug` passed, `adb install -r` passed, cold `SetupActivity` start no longer logs the `APP_ID expected String` / `ClassCastException` warning