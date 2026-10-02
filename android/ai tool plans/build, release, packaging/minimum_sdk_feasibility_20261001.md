# Android minimum SDK feasibility study

Implementation follow-up: the accepted first step is a third API-23 legacy
GitHub APK using the shared current UI. Current and legacy minimum/target
SDK pairs now live in `android/distribution_versions.conf`; the original
dependency-config paths discussed below describe the investigation baseline.
See `three_android_distributions_20261001.md` for implementation validation

Requested: investigate how far the GitHub APK can lower its minimum Android
version compared with Play distribution, without implementing a compatibility
port yet

## Investigation plan

- [x] Verify current Play policy and distinguish target SDK from minimum SDK
- [x] Inspect current resolved JVM dependencies and native platform requirements
- [x] Identify runtime API barriers below API 24, including API 23 and API 21
- [x] Recommend a support floor, build arrangement, and validation scope

## Findings

## Recommendation

Aim first for Android 6.0 (API 23) in the GitHub distribution, keeping Play at
Android 7.0 (API 24). This is a bounded compatibility project using the same
Gradle/CMake toolchain and the same engine and launcher sources. It requires
more than changing one integer, but does not require two build systems

Android 5.0/5.1 (API 21/22) is a plausible second project if there are actual
devices to support. It needs a coordinated older AndroidX/Compose dependency
set and additional framework fallbacks. API 21 is the lowest sensible candidate
with the current native toolchain; it is not yet a proven runtime floor

Android 4.4 (API 19) and earlier would be a separate legacy port, involving an
older NDK and replacement or substantial reworking of the Compose launcher.
Do not make that the default scope of this work

| Distribution / candidate | Install minimum | Assessment |
| --- | --- | --- |
| Current Play and GitHub | API 24 / Android 7.0 | Current declared floor; some features still have unguarded newer API usage |
| Recommended GitHub first step | API 23 / Android 6.0 | Current UI dependencies can stay; remove Play SDKs and repair API compatibility |
| Extended GitHub legacy support | API 21 / Android 5.0, also covers API 22 | Native toolchain supports it; needs older UI dependencies and more fallbacks |
| API 19 / Android 4.4 or lower | Legacy port | Current NDK and Compose dependency stack cannot support it directly |

## What Play actually requires

As of 2026-10-01, new mobile apps and updates submitted to Play must target
API 36 (Android 16), following the 2026-08-31 deadline. That is `targetSdk`,
not the oldest Android version allowed to install the app. The official policy
explicitly permits running on older versions down to `minSdkVersion`

Sources: [Play target API policy](https://support.google.com/googleplay/android-developer/answer/11926878?hl=en),
[SDK manifest semantics](https://developer.android.com/guide/topics/manifest/uses-sdk-element)

The separate automatic protection feature lists a minimum API level of 24
among its prerequisites. The repository's `MIN_SDK=24` comment attributes its
floor to that feature. Automatic protection is configurable in Play Console;
this study did not inspect the account to establish whether it is enabled

Source: [Automatic protection prerequisites](https://support.google.com/googleplay/android-developer/answer/10183279?hl=en-GB)

There is also a concrete dependency floor independent of that policy:
`play-services-games-v2:22.1.0` declares minimum API 24. Therefore merely turning
off protection would not make the current Play app compatible with API 23

Keep `targetSdk=36` and `compileSdk=37` for both distributions. Neither needs to
be lowered to support old devices; runtime API availability must be guarded or
provided through supported backports

## Current repository evidence

- `android/get_deps/tool_versions.conf`: `MIN_SDK=24`, `TARGET_SDK=36`,
  `COMPILE_SDK=37`, NDK r30 / `30.0.16248370`
- `android/app/build.gradle`: both distributions read the same minimum SDK.
  `-PgithubRelease=true` already changes the application ID and disables Play
  behavior, but does not change the dependency list or minimum SDK
- `android/app/src/main/AndroidManifest.xml`: the Play Games initialization
  provider is disabled using a placeholder on GitHub builds, but its AAR still
  participates in manifest merging and enforces API 24
- `PlayGamesAuth.kt` and `UpdateChecker.kt`: GitHub builds skip sign-in and
  Play updates at runtime, but their Google imports are still compiled
- `android/release-github.ps1`: builds the same release variant with that
  property, includes ARM32, ARM64 and x86_64, and hard-codes Android 7.0 in its
  help and generated release notes. It currently checks identity, signatures
  and ABIs, but does not verify the APK's actual minimum/target SDK

### Resolved dependencies

Inspected the 74 AAR manifests referenced by the existing release lint model,
rather than assuming a library's current documentation matches its pinned
version. Representative results:

| Dependency | Resolved version | Manifest minimum |
| --- | --- | --- |
| Play Games v2 | 22.1.0 | 24 |
| AndroidX Core / Core KTX | 1.19.1 | 23 |
| AppCompat | 1.8.0 | 23 |
| Activity / Activity Compose | 1.13.0 | 23 |
| Compose UI / Foundation / Runtime | 1.12.1 | 23 |
| Lifecycle | 2.10.0 | 23 |
| SavedState | 1.4.0 | 23 |
| Window | 1.5.0 | 23 |
| Play Services Basement | 18.9.0 | 23 |
| Material3 | 1.4.0 | 21 |
| OkHttp Android | 5.5.0 | 21 |
| 7-Zip-JBinding Android | 16.02-2.03.1 | 21 |

Material3's own lower manifest minimum does not lower its transitive Compose
floor. Manifest compatibility also does not prove every code path works on
the declared minimum

The fresh diagnostic API-23 dependency graph merged successfully after forcing
Play Games 21.0.0 solely for analysis; its highest AAR minimum was 23. That
older Games version is not the proposed shipping solution: GitHub does not use
Games, so the proper solution is to exclude it from the GitHub runtime entirely

The AndroidX policy page now states that its default minimum for new releases
is API 24. Existing artifacts are unaffected. Supporting API 23 therefore
requires dependency updates to check compatibility rather than blindly accept
the newest release. Supporting API 21 would require a substantially older,
coordinated set of Core, Activity, Compose, Lifecycle, SavedState and transitive
dependencies, with compile checks for newer APIs used by our launcher

Source: [AndroidX minimum SDK policy](https://developer.android.com/jetpack/androidx/versions)

ZXing Embedded 4.3.0 resolves `com.google.zxing:core:3.4.1`, a plain JAR with
no AAR minimum to merge. Upstream explicitly requires API 24 by default and
documents core library desugaring as a way to support API 21+. This is another
reason the manifest number alone is insufficient

Source: [ZXing older SDK configuration](https://github.com/journeyapps/zxing-android-embedded#older-sdk-versions)

### Native and hardware requirements

The installed r30 `meta/platforms.json` declares minimum API 21. Its
`meta/abis.json` also declares minimum API 21 for all three packaged ABIs.
There is no native metadata reason to retain API 24 or change NDK for API 23
or API 21. A future implementation must rebuild all engine/dependency native
libraries against the selected minimum and validate packaged third-party
libraries, including 7-Zip and xCrash; changing only APK metadata is unsafe

The engine uses OpenSL ES audio, not an AAudio-only path. The default graphics
build links GLESv3, creates an ES 3 context and uses GLSL ES 3 shaders. Old
Android versions do not guarantee suitable GPUs. A lower SDK alone does not
add support for ES 2-only hardware. There is an existing software-rendering
build option, but it needs separate feature and performance validation before
being offered as a fallback

Current APK ABIs include `armeabi-v7a`, so this is not an ARM64-only release.
The modern NDK assumes NEON for ARMv7; very old non-NEON devices would need
additional legacy-toolchain work. The APK does not include 32-bit x86, so old
Intel devices also need an ABI decision rather than just a lower SDK

Sources: [NDK API 21 baseline](https://github.com/android/ndk/wiki/Changelog-r26),
[NDK non-NEON retirement](https://github.com/android/ndk/wiki/Changelog-r23)

## Runtime audit and diagnostic lint

Ran release Kotlin/Java compilation, manifest merge and focused Android lint
with a temporary Gradle init script. The script sets API 23, forces Games
21.0.0 to get past its otherwise unrelated manifest rejection, and enables
`NewApi` and `InlinedApi` with `abortOnError=false` so the complete report can
be collected. Maintained Gradle, dependency pins and app source were not edited

The API-23 report contains **85 errors and 2 warnings across 21 files**:
23 errors require API 24, 43 require API 26, and 19 require API 30. These are
lint findings, not 85 independently established runtime bugs. Some calls are
unreachable on older devices or restricted to development helpers

The 23 API-24 errors are the incremental findings relevant to API 23:

- `List.replaceAll` in `AudioSourceManager.kt` and `CustomAudioSetManager.kt`
- `ConcurrentHashMap.newKeySet` / `KeySetView` in `LobbyService.kt` and
  `MissionTransferService.kt`, and `computeIfAbsent` in
  `MissionZipExtractionStore.kt`
- `Map` / `HashMap.putIfAbsent` in `SetupFileImport.kt` and
  `MultiplayerCallsigns.kt`
- `stopForeground(int)` in `MultiplayerForegroundService.kt`, plus two
  warnings about its `STOP_FOREGROUND_REMOVE` constant. Use the pre-24 boolean
  overload on API 23
- `URLConnection.getContentLengthLong` in `SetupDialogs.kt`. Use a compatible
  header-reading fallback while preserving large download sizes

Much of the Java collection work can use core library desugaring or small
source replacements. Preserve concurrency semantics for shared maps and sets;
do not replace atomic operations with unsynchronized check-then-insert code

Existing API-26/30 findings include:

- `File.toPath`, `Path.startsWith` and `Path.relativize` throughout file-set,
  mission, archive and transfer code
- `Files.move` / `StandardCopyOption` in `BinHexDecoder.kt`
- `Instant.now` in `LogAssetSnapshot.kt`
- New process redirection/timed-wait/forced-destroy APIs in the host-tar RAR
  fallback. Review whether these paths can be reached on Android when the
  native RAR backend is unavailable; Java library desugaring does not solve
  every process API
- Unguarded `WindowManager.currentWindowMetrics` in `TouchEditorPage.kt`
- `NetworkRequest.Builder.clearCapabilities` in `LanHostAddresses.kt`
- A debug presentation probe in `LevelPreviewActivity.kt` checks API 24 but
  its Window overload of `PixelCopy.request` needs API 26. This is a test-tool
  compatibility issue, not a release feature requirement
- `ApplicationExitInfo` accessors in `GameProcessExitDiagnostics.kt` produce
  15 findings, but their input list is empty below API 30. These need clearer
  API-boundary annotations/structure, rather than raising the app minimum

The project sets Java language/bytecode compatibility to 11 but does not
enable `coreLibraryDesugaring` or include `desugar_jdk_libs`. Language
desugaring alone does not provide `java.time` and `java.nio.file`. Select a
pinned desugaring library configuration that includes the needed NIO APIs, or
replace those calls with compatible equivalents. Validate dependency code
paths as well, especially Commons Compress/Commons IO and QR scanning

`Math.addExact` and `Math.multiplyExact` also appear in source and are platform
API 24, but D8 has method backports for these and this lint run did not flag
them. Do not count them as additional confirmed blockers merely from the SDK
database

Sources: [Java API desugaring](https://developer.android.com/studio/write/java8-support),
[D8 method backports](https://r8.googlesource.com/r8/+/master/src/main/java/com/android/tools/r8/ir/desugar/BackportedMethodRewriter.java)

### Additional work below API 23

API 21/22 needs all the API-23 fixes plus an older UI dependency graph and
framework changes. Examples found directly in source:

- Numerous `Context.getSystemService(Class)` calls require API 23, including
  launcher media, network address tracking, DNS-SD and foreground services.
  Use `ContextCompat.getSystemService` or the older string-based overload
- `AlarmManager.setAndAllowWhileIdle` in the multiplayer service requires
  API 23; use a compatible pre-Doze alarm path below it
- Check runtime permission behavior, SAF provider operations and URI grants,
  crash dumping, networking/TLS trust, and all bundled decoder/extractor
  native libraries on the actual oldest devices

Large LZMA dictionaries and soundfonts can exhaust older devices even if the
SDK and GPU are compatible. The manifest already requests a large heap for
192 MiB dictionaries and the archive policy allows a 256 MiB decoder ceiling.
Include a low-memory device in validation; package compatibility alone does
not establish practical playability

## Proposed implementation scope

1. Keep one Gradle project, one CMake project and shared source. For the first
   implementation the existing `githubRelease` property can select a separate
   `GITHUB_MIN_SDK=23` while `MIN_SDK=24` remains the Play floor. Alternatively
   use `play` / `github` product flavors if building both in one checkout needs
   distinct outputs and dependency configurations
2. Put real Play Games authentication and Play updates in Play-specific
   sources, and matching no-op implementations in GitHub-specific sources.
   Conditionally package their dependencies and provider/metadata. A runtime
   boolean or disabled provider does not remove the dependency minimum
3. Keep current API-23-compatible UI pins shared initially. Add library
   desugaring and/or compatible source replacements, plus framework version
   guards. Fix the existing unguarded API-26/30 paths as part of this work
4. Let Gradle pass the selected minimum to CMake and rebuild native libraries
   for all packaged ABIs. Keep compile and target SDK current for both builds
5. Update `release-github.ps1` help/release notes to reflect the verified floor;
   inspect `aapt2 dump badging` minimum and target SDK and record them in
   `build-info.json`. Extend its release test for these checks
6. Prevent future dependency updates from silently raising the GitHub floor.
   Verify the resolved manifest graph and compile/lint both distributions
7. Test API 23, API 24 and a current Android release. For an API-21 extension,
   add API 21 and API 22, older UI pins and their updater policy before calling
   that floor supported

Use a real source split rather than a `tools:overrideLibrary` waiver for Play
Games. Such a waiver suppresses manifest enforcement without establishing
runtime compatibility

API 23 has moderate initial implementation/test effort and modest ongoing
maintenance if dependencies remain compatible. API 21 has substantially more
dependency maintenance, even with one build system. Only below API 21 does
this proposal clearly need an older native toolchain as well

## Acceptance checks for implementation

- Build both D1 and D2 for all three ABIs; full APK/AAB assembly must succeed
- Confirm final APK min/target SDK, signatures, ABIs and distribution ID
- Run API compatibility lint, reviewing guarded/development-only findings
  explicitly, and exercise library paths that app-source lint cannot see
- Install and launch both games on the oldest supported OS, and exercise
  menus, touch editor, controllers/gyro, suspend/resume and surface recreation
- Exercise ZIP/7z/RAR/BinHex/disc import, mission downloads, SAF grants, saves,
  log export, MIDI/MP3/CD preview and media-button routing
- Host/join LAN games, transfer missions, scan/emit QR codes and follow join
  links; test the service while backgrounded and during network changes
- Verify Play sign-in/updates still work in Play builds, and no Play SDK
  initialization remains in GitHub builds

This study establishes policy and dependency feasibility, not an installable
legacy release. No APK was built, installed, published or claimed to work on
API 23/21. Available emulator images are API 34 and 36, so old-device runtime
validation remains part of implementation

Diagnostic evidence is under ignored `android/temp/minsdk-study/`:
`api23.init.gradle`, `gradle-lint.log`, `lint-api23.txt` and `lint-api23.xml`.
The diagnostic task returned exit 0 because report collection disabled abort
on lint errors; that is not a clean lint result

The API-24 baseline with the maintained Games 22.1.0 dependency also completed
Kotlin/Java compilation, manifest merge and lint report collection. It contains
**62 errors and no warnings**. Comparing issue IDs, locations and messages
confirms these are precisely the shared findings; API 23 adds 23 errors and
2 warnings. Files: `api24.init.gradle`, `gradle-lint-api24.log`,
`lint-api24.txt` and `lint-api24.xml`. This second run restored the ordinary
API-24/Games-22.1.0 generated release compilation/manifest model

No old-OS adoption percentage is claimed. The AndroidX policy's 99% coverage
goal concerns Play check-in data, which may not represent people repurposing
old devices for sideloaded Descent. API 23 adds Android 6.0 specifically; API 21
adds Android 5.0 and 5.1 as well. Actual requested hardware should determine
whether the larger API-21 maintenance commitment is worthwhile

## Follow-up: a Google-free legacy edition

Removing Play Games, Play In-App Updates and their transitive Google Play
Services runtime dependencies is feasible. Single-player and LAN gameplay do
not need Google account authentication. This means an app that does not require
Google Play Services on the device; it does not imply removing every
Google-authored open-source library or the Android build tools

Removing those SDKs alone leaves the current Compose/AndroidX floor at API 23
and the native toolchain floor at API 21. A special legacy edition can go lower
only by changing those additional constraints:

- API 21: keep a fuller launcher using older, compatible AndroidX/Compose
  artifacts, or build a simpler platform-Views launcher to avoid maintaining
  that older Compose graph. Retain the current NDK and share the game engine
- API 19: a plausible experimental target using NDK r25, a platform-Views
  launcher and replacements for dependencies/framework calls requiring API 21
  or higher. Native third-party libraries must be rebuilt or replaced; the
  current 7-Zip Android AAR alone declares API 21. File imports and staging
  need redesign where they depend on API-21 SAF tree access or `android.system.Os`
- Below API 19: increasingly speculative and not established by this study.
  Below API 18, the current ES 3 renderer also loses its platform baseline,
  requiring an older graphics path or a verified software build

The existing game `MainActivity` extends the platform `Activity`, uses a
`SurfaceView`, and has no direct Compose imports. That makes sharing the game
activity, native engine and much of the custom touch UI a credible design.
It still imports AndroidX Core and uses an AppCompat text widget, and shares
launcher/storage services, so it is not already independent of modern libraries

A lean legacy launcher could initially cover game-data selection/import,
game selection and launch, while the existing native game menus supply most
game settings. Mission catalogs, rich import tools, previews and the Compose
touch editor would need individual decisions about porting or omission.
These are proposed scope choices, not a demonstrated feature-complete port

Use one repository with a separate legacy app module when the launcher and
native toolchain diverge this much. Share engine sources, compatible Android
services and build helpers; keep the old dependency/NDK pins local to the
legacy module. This is a second maintained app configuration and launcher,
without duplicating the engine or inventing an unrelated build system

Recommend API 21 as the initial Google-free legacy target, with API 19 only
after a small launch/render/audio/input/storage prototype succeeds on suitable
hardware. ES 3 support remains a device requirement even on API 19. Android
4.4 runtime compatibility has not been tested or claimed

Sources: [NDK API 19/20 retirement in r26](https://github.com/android/ndk/wiki/Changelog-r26),
[OpenGL ES 3 platform and hardware requirements](https://developer.android.com/develop/ui/views/graphics/opengl/about-opengl)
