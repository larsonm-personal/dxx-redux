# Plan: Version Code with Rev Suffix

## Summary
Change version code from `[commit_count]` to `[commit_count * 10 + rev]` where rev is 0-9.
Add auto-increment logic to the upload script so re-deploys without new commits bump the rev.

## Version code scheme
- Old: `git rev-list --count HEAD` -> e.g. `1234`
- New: `git rev-list --count HEAD` * 10 + rev -> e.g. `12340` (rev 0), `12341` (rev 1)
- Rev range: 0-9. If deployed version has rev 9, fail

## Files to modify

### 1. android/app/build.gradle (gitVersionCode)
- Multiply by 10 (rev always 0 for IDE/local builds)

### 2. android/1_build-aab.ps1
- Multiply commit count by 10 for versionCode (rev 0 default)
- Display shows base commit count and effective version code

### 3. android/1_build-aab.sh
- Same as ps1

### 4. android/0_upload_to_test.ps1
- Before building, query the target track's current version code
- Parse deployed version into (commit_count, rev) = (code / 10, code % 10)
- Our commit count = git rev-list --count HEAD
- If deployed commit count matches ours:
  - Set rev = deployed_rev + 1
  - If rev > 9, fail
- Else rev = 0
- Pass the computed version code to the build step somehow
  - Option: pass a -VersionCode parameter to 1_build-aab.ps1
  - The build script uses this override instead of computing its own

### 5. android/2_deploy-playstore.ps1
- No fundamental changes; it already gets versionCode from the upload response
- The release name "v$versionCode" will naturally show the new scheme

## Phases
- [x] Phase 1: Create plan
- [x] Phase 2: Modify build.gradle (gitVersionCode * 10, with versionCodeOverride support)
- [x] Phase 3: Modify 1_build-aab.ps1 to accept -VersionCode override, default to commit*10
- [x] Phase 4: Modify 1_build-aab.sh similarly (uses VERSION_CODE_OVERRIDE env var)
- [x] Phase 5: Modify 0_upload_to_test.ps1 to query track, compute rev, pass override
- [x] Phase 6: Extract auth helpers to playstore-auth.ps1, refactor 2_deploy-playstore.ps1
- [x] Phase 7: Lint checks pass
