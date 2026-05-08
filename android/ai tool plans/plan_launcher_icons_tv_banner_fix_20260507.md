# Android TV banner reload fix 2026-05-07

## scope

- Remove the conflicting placeholder `tv_banner` drawable from source resources
- Generate the TV banner into plain `drawable/` so the packaged resource matches Android TV launcher expectations
- Revalidate resource packaging with a narrow Gradle build

## steps

1. Move generated TV banner output to `drawable/tv_banner.png`
Status: completed

Result:
- `generateLauncherArt` now writes the TV banner to `build/generated/icon-res/main/drawable/tv_banner.png`

2. Remove the old `src/main/res/drawable/tv_banner.xml` placeholder
Status: completed

Result:
- The placeholder XML banner is deleted, so `@drawable/tv_banner` no longer resolves to mixed resource types

3. Run `:app:processDebugResources` and confirm only the PNG banner remains packaged
Status: completed

Result:
- `gradlew.bat :app:processDebugResources` passed
- `gradlew.bat :app:clean :app:processDebugResources` passed
- The generated icon resource directory now contains `drawable/tv_banner.png` and no source `tv_banner` file remains under `src/main/res`