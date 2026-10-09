# Missing mission movies in the metadata viewer

- [x] Trace the mission overview, native metadata worker, and installed resource paths
- [x] Scan authored D2 movie dependencies in native code using the selected mission's resources
- [x] Expose a refreshable check on the mission overview and show missing media in red
- [x] Verify missing/present media, resolution fallback, and D1 behavior with focused tests and relevant builds
- [x] Run scoped code quality and record results

## Implementation

The mission metadata dialog scans on opening and displays missing movie filenames in the theme's red error color above the level report. Its Scan for missing movies button reruns the check without rebuilding the level report. Missing movie archives are listed to help locate the optional media.

The existing metadata worker accepts movies_only requests that load mission resources but bypass level/route analysis. These requests bypass the whole-mission result cache, so importing, removing, or disabling movie media is visible on the next scan. Enabled global media projections are included alongside the selected mission resources.

The native scanner reads plaintext or engine-decoded TXB briefing/ending references, plus built-in campaign intro/fly-out/ending selections from native tables. D1 robot model references are not interpreted as movies. It checks loose movies and high/low-resolution MVL archives using PhysFS case resolution. Temporary movie mounts are released after each scan; the metadata worker now also releases its mission movie library before releasing its mission context.

This is an availability check, not a movie decoder integrity test. A scan without a loadable mission reports unavailable rather than claiming that no movies are missing. Optional absent briefing text does not itself become a missing-movie warning.

## Validation

- Windows CMake builds passed for D1 and D2, including both metadata workers
- CTest test_mission_movies passed for both games: movie selectors, comment handling, deduplication, loose media, both resolutions, mount cleanup, built-in campaign references, and D1 exclusion
- Original Vertigo HOG/TXB/MVL scan passed through test_upstream_compat --mission-movies with the local retail fixture
- Reusable android/tests/test_mission_movies.ps1 -NoBuild passed all five cases through one real metadata-worker process: HOG/MN2 only (10 missing), low resolution (0), removed (10), globally installed high resolution (0), disabled global media (10). Level 1's rb3.mve and rb9.mve are explicitly checked
- Android Kotlin compilation and 10 focused JVM tests passed: MissionMovieScanTest and LevelMetadataResultCacheTest
- Scoped mixed-language code quality passed
- Automation catalog and master test-runner catalog checks passed; the integration runner is registered as a host test with a build allowance
- git diff --check passed

No APK was installed on the user's phone, and the Compose screen was not visually reviewed on-device. The UI compiled; the scanner and real worker request/response flow were tested on the host. Existing unrelated work was preserved, including edits in the shared test catalogs.
