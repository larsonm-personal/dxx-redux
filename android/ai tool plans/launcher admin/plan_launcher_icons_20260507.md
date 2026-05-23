# Launcher icon wiring 2026-05-07

## scope

- Route the phone launcher icon through `android/app_images/icon_512.png`
- Route the Android TV banner through `android/app_images/tv_banner.png` when present, otherwise derive a simple fallback banner from `icon_512.png`
- Keep the existing dual launcher categories on `SetupActivity`
- Validate with a narrow Android app build

## steps

1. Add a generated-resource Gradle task and source-set hook
Status: completed

Result:
- `android/app/build.gradle` now generates launcher resources from `android/app_images`
- `icon_512.png` is required and feeds the phone launcher icons
- `tv_banner.png` is optional and overrides the fallback Android TV banner when present

2. Point the manifest at generated launcher icon resources
Status: completed

Result:
- `AndroidManifest.xml` now sets `android:icon` and `android:roundIcon`
- The existing `android:banner` wiring remains in place for Android TV

3. Run a narrow Android build to confirm the resources package cleanly
Status: completed

Result:
- `gradlew.bat :app:processDebugResources` passed after fixing the banner centering math in the Gradle task

4. Mark the plan complete and capture the future icon-edit workflow
Status: completed

Workflow:
- Replace `android/app_images/icon_512.png` with a square PNG to refresh the phone launcher icons on the next Gradle build
- Add or replace `android/app_images/tv_banner.png` with any 16:9 PNG to override the auto-generated Android TV banner on the next Gradle build