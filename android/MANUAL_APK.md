# Manual universal APK

In GitHub Actions, select **Package - Android universal APK**, then **Run workflow**.
The workflow file must be present on the repository's default branch for GitHub to
offer the manual run button. Choose a branch and the signing mode.

To dispatch a branch containing the workflow with the GitHub CLI, run:

```powershell
gh workflow run package-android.yml --repo larsonm-personal/dxx-redux --ref cmake -f signing=unsigned
```

Before this change is merged, use `--ref ci/android-apk-manual-20261002` instead.

- `unsigned` builds without signing secrets and produces an APK to sign later
- `test` signs with a private, stable test key and is restricted to the default branch

The artifact contains the universal APK, its SHA-256 checksum and inspected APK
metadata. It expires after three days. No automatic push, PR or scheduled builds
are configured. Both D1 and D2 engines are included for ARM32, ARM64 and x86_64.
The app ID is `com.dxxredux.app.ci`, with the launcher label `DXX-Redux (CI)`.
Game data is supplied by the player. Play Games and Play update services are omitted.

## Private test signing

Create a dedicated keystore once outside the repository with alias `ci-test`, and
keep a private backup. Add these repository Actions secrets:

- `ANDROID_TEST_KEYSTORE_BASE64`: Base64 encoding of the keystore
- `ANDROID_TEST_STORE_PASSWORD`: keystore password
- `ANDROID_TEST_KEY_PASSWORD`: private-key password

The workflow decodes the keystore into the runner's temporary directory, reads
passwords through environment variables, and removes the temporary keystore after
signing. The key and passwords are never uploaded as artifacts. Use the same key
for subsequent builds so existing CI installations can be updated.

For local unsigned builds, use JDK 21 and the configured Android SDK/NDK, then run:

```powershell
.\android\gradlew.bat -p android :app:assembleRelease -PciApk=true
```

The output is `android/app/build/outputs/apk/release/app-release-unsigned.apk`.
CI mode always bypasses local release keystore settings. Production GitHub and
Play releases continue to use their existing signing configuration.
