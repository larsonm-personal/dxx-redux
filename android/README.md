# android build dependencies
* see auto-download scripts in `android/get_deps/`
* for a fresh Ubuntu VM, see `android/get_deps/README-ubuntu.md`
* create `dependency_base.txt` in the repo root with the path to your dependency directory (e.g. `C:\local`)
* android NDK (gets automatically found under `<dependency_base>/android-ndk.../`)
* android SDK command line tools (`<dependency_base>/android-sdk.../`)
* JDK (`<dependency_base>/jdk.../`)
* wsl (for bash)

# VS code extensions
* extension pack for java (ms)

# google play console setup
For direct APK downloads without Play Store installation, see the usage and setup comments in [release-github.ps1](release-github.ps1).

Three distributions share the same launcher UI and native engines:

| Distribution | Minimum Android | Gradle selection | Application ID |
| --- | --- | --- | --- |
| Google Play | API 24 (7.0) | Default | `com.dxxredux.app` |
| GitHub matching | API 24 (7.0) | `-PgithubRelease=true` | `com.dxxredux.app.github` |
| GitHub legacy | API 23 (6.0) | `-PlegacyRelease=true` | `com.dxxredux.app.github.legacy` |

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

Build/publish the matching APK with `./android/release-github.ps1 -Version 1.2.0`
or the legacy APK with the additional `-Legacy` switch. Add `-BuildOnly` to
prepare local files. The legacy tag is `android-legacy-vVERSION`; the matching
tag remains `android-vVERSION`. Release titles and generated notes include
`minsdk: api M (android VERSION), targetsdk: api N (android VERSION)`, inspected
from the APK. Upload recovery also requires `-Legacy` for a saved legacy build.

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
* https://support.google.com/googleplay/android-developer/answer/6112435
* note on programmatic auth https://stackoverflow.com/questions/76541480/how-does-fully-create-an-internal-release-on-google-play-console-via-api

## gpgs
* https://developer.android.com/games/pgs/start
1. Create a Play Games project in Google Play Console
2. Configure OAuth consent screen
3. Generate OAuth 2.0 client IDs:
   - Android client ID (linked to app signing key SHA-1)
   - Web/server client ID (for server-side token exchange)
4. Add the Games project to the app's Play Console listing

## crash reports
* Crash reports are exportable from the launcher's Advanced page
* If xCrash only shows the child exit stub or another incomplete native report, inspect the exported tombstone's `dxx-redux breadcrumbs` section before digging into gameplay logs
* If the tombstone is missing custom sections or the crash callback only had emergency data, also inspect any exported `crash_error_*.txt` fallback report from the same Advanced page export
