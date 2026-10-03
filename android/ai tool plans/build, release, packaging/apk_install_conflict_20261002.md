# APK installation identity conflict

- [x] Inspect package IDs, local APK signature, and installed package metadata
- [x] Build the Play AAB and direct-install APK with their existing distinct distribution IDs
- [x] Check the exported APK identity before publication and document signing/update behavior
- [x] Run scoped code quality, a real combined build, and package identity/signature validation

The reported filename matches the new combined builder's release APK. Its package
is com.dxxredux.app and its local certificate matches the GitHub release certificate
in the downloaded build-info.json. Play installs use a separate app signing key,
so the local upload-key APK cannot update them. The phone now has this locally
installed release; its previous install was removed, so its signature cannot be
checked retrospectively.

For Release/Internal, build the Play AAB first, then assemble the corresponding
GitHub distribution APK with the same version and signing configuration. Reuse
native outputs while rebuilding the distribution-specific JVM sources and manifest.
Keep the existing GitHub package ID to allow updates of earlier direct APKs signed
with the same stable key. Retain Debug's development identity and existing tooling.
Never uninstall a device app to validate this change.

## Validation

- Existing deployment contract tests: all five passed, after updating the builder's renamed path
- The new actual-artifact integration check rejects the old conflicting APK/AAB pair
- Gradle configuration passed for the direct-install distribution
- Play bundle tasks passed in 4m 24s; both distributions ran the relevant CMake tasks
- Actual artifact check passed: Play AAB com.dxxredux.app, APK com.dxxredux.app.github,
  shared version 1.1 (23750), valid APK signature, identical D1/D2 libraries for all
  three ABIs, and no Google Play runtime SDKs in the direct APK
- New APK certificate SHA-256 matches downloaded GitHub release metadata:
  c4edcb365a4ecb9a4b8ba5a51073e4fad3b0820d65f9cbb65c77fb49b7221987
- No device apps were uninstalled or modified
- Scoped code quality and git diff --check passed
- Combined builder completed both Gradle invocations and exported the checked pair:
  android/build-outputs/dxx-redux-release-20261002-155255-v23750.aab and
  android/build-outputs/dxx-redux-release-20261002-155255-v23750-universal.apk

The initial full build was stopped by the native-retention guard while another
build was active. Retried after the other build finished; no guard was bypassed.
