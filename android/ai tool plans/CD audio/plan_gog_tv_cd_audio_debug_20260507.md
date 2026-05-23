# GOG TV CD Audio Import Debug Plan

## Goal

Find why Android TV GOG D2 imports appear to skip CD audio extraction, add launcher-side debug logging to confirm or deny the leading hypothesis, and validate the logging build.

## Steps

- [x] Trace the launcher GOG import path to identify where `.gog` extraction can be skipped while `.inst` still extracts
- [x] Add focused launcher debug logging around the deciding branch and any filename filtering involved
- [x] Run a narrow launcher test/build validation and update this plan with the outcome

## Outcome

- Current evidence points to the failure happening before Redbook registration: the exported launcher logs show `DESCENT_II.inst` present in the set but no matching `.gog`, so `findGogPair()` cannot succeed.
- Latest launcher breadcrumbs showed `launcher-gog-native-start file=DESCENT_II.gog ... gog_galaxy=0`, which corrected the diagnosis again: this installer's `.gog` file is not taking the GOG Galaxy inner-inflate path at all, so the remaining Android failure is in the generic first-phase extraction branch.
- Fixed the generic first-phase extraction branch in `inno_reader.c` for large files by streaming the requested file range directly from the installer chunk to disk when the compressed chunk or file size exceeds 64 MB. This keeps large non-`gog_galaxy` files like `DESCENT_II.gog` off the old full-chunk allocation path on 32-bit Android.
- Added native logging around the streamed GOG Galaxy path so a future failure reports the inner compressed size, expected output size, and inflate/write failure details.
- Added launcher log entries around GOG installer analysis, extraction start, extraction result, and extraction exceptions so the next TV import can distinguish:
	- whether analysis saw both `.gog` and `.inst` entries in the installer;
	- whether `includeAudio` was still true when extraction started;
	- whether extraction produced only `.inst`, only `.gog`, both, or neither in the destination set.
- Added follow-up launcher-side audio progress logging in `SetupActivity.kt` after behavior remained unchanged on device:
	- audio file progress now logs when `.gog` or `.inst` first becomes the active extraction target;
	- each log includes the current output file presence, current on-disk byte size, and current free bytes in the destination volume;
	- extraction start/result logs now include expected audio sizes plus expected audio file states in the destination set before registration.
- Restored build metadata in the first line of newly created debug log files by writing the header directly in `DebugLog.kt` instead of routing it through the NETWORK category gate; the header now includes app version, version code, git build number, git hash, build type, build time, primary ABI, and `os.arch`.
- Split the launcher GOG analysis/start/result logs into shorter standalone lines so audio-specific fields are still visible even if the viewing/export path truncates long lines.
- Added native-to-launcher breadcrumbs in `jni_gog_import.c` so the exported launcher debug log now captures `launcher-gog-native-start`, `launcher-gog-native-done`, and `launcher-gog-native-fail` for audio files without requiring logcat.
- Fixed the native breadcrumb bridge by making `LauncherDebugLog.log()` callable as a JVM static method from JNI; earlier builds had the breadcrumb lines compiled in but unreachable because Kotlin `object` methods are instance methods by default.
- Normalized launcher progress filenames in `SetupActivity.kt` so exported `launcher-gog-audio-progress` lines now resolve `{app}\DESCENT_II.gog` to the real output filename and can report actual on-disk size/existence.
- Validation passed with JDK 21:
	- `android\\gradlew.bat :app:compileDebugKotlin :app:testDebugUnitTest --tests com.dxxredux.app.AndroidGameFileExtensionsTest --tests com.dxxredux.app.ImportTreeScannerTest`
	- direct `ktlint` check on `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
- Additional host extractor validation passed after the streaming fix:
	- built `extract_gog` and `test_gog_fd` in `android/tests/build`
	- `extract_gog.exe` against `setup_descent_2_1.1_(16596).exe` completed with `Done: 21 files extracted, 0 errors`
	- `ctest --test-dir android/tests/build -C MinSizeRel -R gog_fd_tests --output-on-failure`
- Android native validation passed after the fix:
	- `android\\gradlew.bat :app:buildCMakeDebug[arm64-v8a]-2`
- Follow-up launcher logging validation passed:
	- `android\\gradlew.bat :app:compileDebugKotlin`
- Additional validation passed after the debug log header and native launcher-log breadcrumb changes:
	- `android\\gradlew.bat :app:compileDebugKotlin`
	- `android\\gradlew.bat :app:buildCMakeDebug[arm64-v8a]-2`
- Streamed first-phase `.gog` extraction validation passed:
	- rebuilt `extract_gog` and `test_gog_fd` in `android/tests/build`
	- `extract_gog.exe` against `setup_descent_2_1.1_(16596).exe` again completed with `Done: 21 files extracted, 0 errors`
	- `ctest --test-dir android/tests/build -C MinSizeRel -R gog_fd_tests --output-on-failure`
	- `android\\gradlew.bat :app:compileDebugKotlin`
	- `android\\gradlew.bat :app:buildCMakeDebug[arm64-v8a]-2`
- Final fully streamed first-phase `.gog` extraction validation passed:
	- rebuilt `extract_gog` and `test_gog_fd` in `android/tests/build`
	- `extract_gog.exe` against `setup_descent_2_1.1_(16596).exe` again completed with `Done: 21 files extracted, 0 errors`
	- `ctest --test-dir android/tests/build -C MinSizeRel -R gog_fd_tests --output-on-failure`
	- `android\\gradlew.bat :app:buildCMakeDebug[arm64-v8a]-2`
	- `android\\gradlew.bat :app:compileDebugKotlin`
- Final large non-`gog_galaxy` fallback validation passed:
	- rebuilt `extract_gog` and `test_gog_fd` in `android/tests/build`
	- `extract_gog.exe` against `setup_descent_2_1.1_(16596).exe` completed with exit code 0 after routing `DESCENT_II.gog` through the streamed large-file fallback
	- `ctest --test-dir android/tests/build -C MinSizeRel -R gog_fd_tests --output-on-failure`
	- `android\\gradlew.bat :app:buildCMakeDebug[arm64-v8a]-2`