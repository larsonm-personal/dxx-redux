# android build dependencies

- see auto-download scripts in `android/get_deps/`
- for a fresh Ubuntu VM, see `android/get_deps/README-ubuntu.md`
- create `dependency_base.txt` in the repo root with the path to your dependency directory (e.g. `C:\local`)
- android NDK (gets automatically found under `<dependency_base>/android-ndk.../`)
- android SDK command line tools (`<dependency_base>/android-sdk.../`)
- JDK (`<dependency_base>/jdk.../`)
- wsl (for bash)

# VS code extensions

- extension pack for java (ms)

# google play console setup

Run `./android/0_upload_to_test.ps1` for the publishing menu:

1. Build and upload to Play Store (default; Internal build, internal track)
2. Build and release both Android APKs on GitHub
3. Do both, with Play Store first and GitHub second

Options 2 and 3 ask for the GitHub release version before work starts. Use
`-Action 3 -ReleaseVersion 1.2.0` to run both without prompts. `-BuildOnly` builds
the selected destinations locally; existing `-BuildType`, `-TrackName` and
`-BuildOnly` invocations keep the Play Store path unless `-Action` is supplied.
GitHub publishing uses the release helper's clean, pushed source requirement.

Run `./android/1_build_aab_apk.ps1 -BuildType 2` to build a release AAB and a
signed universal direct-install APK together. Release/Internal build the Play
AAB (`com.dxxredux.app`), then the GitHub APK (`com.dxxredux.app.github`), reusing
native compilation. Both files appear in `android/build-outputs/` with a
matching timestamp/version name; the APK ends in `-universal.apk` and contains
all three supported ABIs. Release and Internal (`-BuildType 3`) require
`android/keystore.properties`; Debug (`-BuildType 1`) uses the debug signing key.
`-OutputPath` overrides the AAB destination and puts the matching APK beside it.

The Release/Internal APK can coexist with a Play installation, with separate app
data. Keep using the same `keystore.properties` and keystore for direct APK
updates. Android rejects an update with the same package ID but a different
signing certificate; Play's app signing key differs from the local upload key.
Older combined release APKs used the Play package ID and could conflict for this
reason. Install the new direct APK alongside the old app; export/import your
configuration if needed, and keep the older installation to retain its saves. Debug
keeps the development package ID (`com.dxxredux.app`) and debug key, so use an
emulator or dedicated test device for those APKs.

Check a Release/Internal pair without installing it:
`./android/tests/test_combined_package_identity.ps1 -Aab PATH -Apk PATH`.

For direct APK downloads without Play Store installation, see the usage and setup comments in [release-github.ps1](release-github.ps1).

Three distributions share the same launcher UI and native engines:

| Distribution    | Minimum Android | Gradle selection       | Application ID                   |
| --------------- | --------------- | ---------------------- | -------------------------------- |
| Google Play     | API 24 (7.0)    | Default                | `com.dxxredux.app`               |
| GitHub matching | API 24 (7.0)    | `-PgithubRelease=true` | `com.dxxredux.app.github`        |
| GitHub legacy   | API 23 (6.0)    | `-PlegacyRelease=true` | `com.dxxredux.app.github.legacy` |

All three target API 36 (Android 16). Both GitHub distributions exclude Google
Play Games and Play update runtime SDKs. Separate application IDs allow them to
coexist; each installation has its own app data.

Edit `android/distribution_versions.conf` to change the current or legacy
minimum/target SDK pair. Gradle, About, diagnostic headers and release checks
use that file; dependency/tool pins remain in `get_deps/tool_versions.conf`.

On Android 6, the legacy APK requests VM safe mode to avoid an observed old ART
compiler crash in Kotlin coroutine code. This can slow the launcher and other
Java/Kotlin work; the native game engines remain compiled. Android 7 and newer
use normal VM compilation, including when running the legacy APK.

Build/publish both APKs in one `android-vVERSION` release with
`./android/release-github.ps1 -Version 1.2.0`. Add `-BuildOnly` to prepare both
builds locally, or `-UploadOnly` to verify and publish both saved builds without
rebuilding. `-Legacy -BuildOnly` prepares only the legacy APK locally.
Public filenames use `android-1-recommended-universal.apk` for Android 7.0 or
newer and `android-2-only-for-android-6.0-universal.apk` for the legacy APK, after
the shared product/version prefix. The numbers sort the recommended download
first. Use the legacy APK only on devices unable to install the recommended APK.
Generated notes include each APK's inspected minimum/target SDK and SHA-256.
Both builds must have the same source commit, versionCode and signing certificate.

Only the two signed universal APKs are uploaded. Build metadata and checksums stay
in the local output directory for `-UploadOnly` verification. Reusing a version
rebuilds both APKs, moves the tag to the built commit, refreshes generated notes
and removes obsolete metadata/checksum attachments. Custom notes and the
release's draft/published status are preserved. Once both uploaded APK hashes
are verified, the helper retires an old separate legacy release/tag for that
version. A draft keeps the previous legacy release until publication.
GitHub automatically adds the source ZIP/tarball links; these cannot be disabled
or deleted as release assets.

Run `./android/tests/test_android_distributions.ps1` to build and inspect all
three debug APKs, including both native engines for every supported ABI. To
run existing device tests against a direct-install APK, set `DXX_TEST_PACKAGE`
to its application ID and `ANDROID_SERIAL` to the device serial first.
`tests/test_distribution_build_info.ps1 -Serial SERIAL -Apk APK` checks the
installed About text and log header against that APK's actual identity/SDKs.
`tests/test_xcrash_native_report.ps1 -NoBuild -Serial SERIAL` verifies a native
crash and its distribution/version/date/SDK header on an emulator.
Use `helpers/run_test.ps1 -ScriptName test_distribution_launch.jsonc` for the
short D1/D2 gameplay/automap check after setting the package/device variables.

## basic, and programmatic access

- https://support.google.com/googleplay/android-developer/answer/6112435
- note on programmatic auth https://stackoverflow.com/questions/76541480/how-does-fully-create-an-internal-release-on-google-play-console-via-api

## gpgs

- https://developer.android.com/games/pgs/start

1. Create a Play Games project in Google Play Console
2. Configure OAuth consent screen
3. Generate OAuth 2.0 client IDs:
   - Android client ID (linked to app signing key SHA-1)
   - Web/server client ID (for server-side token exchange)
4. Add the Games project to the app's Play Console listing

## crash reports

- Crash reports are exportable from the launcher's Advanced page
- If xCrash only shows the child exit stub or another incomplete native report, inspect the exported tombstone's `dxx-redux breadcrumbs` section before digging into gameplay logs
- If the tombstone is missing custom sections or the crash callback only had emergency data, also inspect any exported `crash_error_*.txt` fallback report from the same Advanced page export

## code quality

See [CODE_QUALITY.md](CODE_QUALITY.md) for the mixed-language check/fix runner, installation, preview and scoped reruns. Its default scope excludes inherited upstream files and all of `d1/` and `d2/`.
