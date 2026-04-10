# Play Store Update Check

## Goal
Show an update banner at the top of the launcher when a newer version is available
on the Play Store. "Update" opens the Play Store page; "Dismiss" persists the
dismissed build number so the banner won't reappear for that version (or older).

## Design

### Version comparison
- The app's versionCode is the git commit count (an integer)
- Play Core's AppUpdateInfo provides `availableVersionCode()` which is the
  Play Store version's versionCode
- Compare: if `availableVersionCode > installedVersionCode`, update is available
- The dismissed version is stored as an int in SharedPreferences

### Persistence
- SharedPreferences key: `dismissed_update_version` (Int, default 0)
- On dismiss: store `availableVersionCode`
- Show banner when: `availableVersionCode > max(installedVersionCode, dismissedVersion)`

### Files to change
1. `android/get_deps/tool_versions.conf` -- add APP_UPDATE_VERSION
2. `android/app/build.gradle` -- add play app-update dependency
3. `android/app/src/main/java/com/dxxredux/app/UpdateChecker.kt` -- new file,
   encapsulates Play Core query + dismiss logic + banner composable
4. `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` -- call
   UpdateBanner() in the main Column, right after the title row

### UI
- A Card with a Row: text "Update available" + two buttons "Update" / "Dismiss"
- Placed between the title row and the file-detail popup section
- Uses Material3 theming consistent with existing UI

## Status
- [ ] Add dependency
- [ ] Create UpdateChecker.kt
- [ ] Wire into SetupActivity
- [ ] Build + verify
- [ ] Lint
