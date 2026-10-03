# Combined AAB and universal APK build

## Work

- [x] Rename 1_build-aab.ps1 to 1_build_aab_apk.ps1 and update active callers/documentation
- [x] Build both artifacts in a single Gradle task graph, preserve existing AAB output overrides, and publish a matching signed universal APK
- [x] Run scoped code quality and a real signed build; verify both engines/all ABIs and shared task execution
- [x] Record validation and output locations

## Design

The existing Gradle application has no APK ABI splits and includes armeabi-v7a, arm64-v8a, and x86_64. Request bundle and assemble for the same variant in one invocation so compilation, resources, dexing, and native builds are shared. Keep OutputPath as the AAB destination and derive a sibling with the same basename plus -universal.apk. Verify APK signing and both engine libraries for each ABI before copying either output.

## Validation

- Scoped code quality and PowerShell parsing passed for the build script and its updated callers
- No active PowerShell/workflow references to the old script remain
- A real Release Gradle build succeeded for both output tasks in 7m 22s; its log has one Kotlin compilation and one Ninja invocation per ABI
- The corrected Windows apksigner.bat lookup passed the full script rerun in 1m 15s, reusing native compilation
- Saved APK verified with APK Signature Scheme v2; saved AAB verified with jarsigner
- Both engine libraries have identical SHA-256 hashes between the AAB and APK for all three ABIs
- APK metadata reports com.dxxredux.app, versionCode 23730, and all three ABIs
- Custom AAB output mapping also checked with a path containing spaces
- git diff --check passed

## Outputs

- android/build-outputs/dxx-redux-release-20261002-151017-v23730.aab (84.2 MB)
- android/build-outputs/dxx-redux-release-20261002-151017-v23730-universal.apk (54.2 MB)

Build evidence: android/temp/build_aab_apk_release.log and android/temp/build_aab_apk_release_verified.log
