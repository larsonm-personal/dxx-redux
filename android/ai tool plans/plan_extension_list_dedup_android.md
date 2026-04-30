# Android file extension list dedup

## Goal
Centralize the duplicated Android-side game file extension list into:

- one shared C definition under `android/app/src/main/cpp/`
- one shared Kotlin definition under `android/app/src/main/java/com/dxxredux/app/`

Keep one copy per language, annotate why the list exists, and switch the
current Android callers to consume the shared definitions instead of carrying
local copies.

## Known starting points
- C duplicates currently exist in `extract_gog.c`, `pkg_reader.c`,
  `jni_gog_import.c`, and `test_gog_fd.c`
- `jni_disc_import.c` has a related but separate extension list used for disc
  image extraction and a Mac-specific variant that likely should reference the
  shared base list rather than keep a second full copy
- Kotlin import scanning currently mixes exact filename sets with ad hoc
  extension checks in `SetupActivity.kt`

## Plan

### Phase 1: Map current Android callers
- [x] Confirm every Android C caller that uses the shared game-extension list
- [x] Confirm every Kotlin caller that needs the shared extension list
- [x] Separate true extension filters from exact filename sets so they are not
      accidentally collapsed into one concept

### Phase 2: Centralize the C list
- [x] Add a shared C header/source pair in the Android native extract area for
      the common game extension list and helper predicates
- [x] Annotate the list with what it includes and what it intentionally does
      not include
- [x] Update the current C callers to use the shared definition

Completed C callers:
- `extract_gog.c`
- `pkg_reader.c`
- `jni_gog_import.c`
- `test_gog_fd.c`
- related disc-image lists in `jni_disc_import.c` and `extract_cd.c`

### Phase 3: Centralize the Kotlin list
- [x] Add a shared Kotlin file for the Android launcher import extensions
- [x] Annotate how the extension list relates to `ALL_GAME_FILENAMES`
- [x] Update the current Kotlin import paths to reuse the shared list/helper

Completed Kotlin callers:
- `SetupActivity.kt` directory-import candidate filtering
- `GogImportBridge.kt` GOG audio subset helper
- `AndroidGameFileExtensionsTest.kt` for the shared Kotlin copy

### Phase 4: Validation
- [x] Run focused Android Gradle validation for the touched Kotlin/native glue
- [x] Run `android\run-code-quality.ps1 -Fix`
- [x] Run the relevant native/host build or test command for the touched C code

Validation run:
- `cmake -S android/app/src/main/cpp/extract -B android/tests/build`
- `cmake --build android/tests/build --config Release --target extract_cd extract_gog test_gog_fd`
- `android\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.AndroidGameFileExtensionsTest --tests com.dxxredux.app.ImportChooserConfigTest`
- `android\gradlew.bat :app:externalNativeBuildDebug --no-daemon`
- `run-windows-build.ps1 -Target both`

Note:
- The first `run-code-quality.ps1 -Fix -Paths ...` call used the wrong relative prefixes and fell back to a broader pass. The follow-up validation steps above were rerun against the formatted tree.
