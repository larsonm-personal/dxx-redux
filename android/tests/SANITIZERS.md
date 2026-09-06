# Engine sanitizer runs

From the repository root, using PowerShell 7 and Visual Studio's C++ AddressSanitizer component:

```powershell
# Incrementally rebuild both instrumented games, then all engine CTests,
# config/loader fixtures, committed input demos, D1 demos under D2, and mission routes
./android/tests/run_engine_sanitizers.ps1

# A smaller routing pass over First Strike, Counterstrike, Castaway, and Obsidian
./android/tests/run_engine_sanitizers.ps1 -RoutingDevelopmentSet

# Individual categories and filters
./android/tests/run_engine_sanitizers.ps1 -Category EngineTests
./android/tests/run_engine_sanitizers.ps1 -Category LoaderBounds -Game d2
./android/tests/run_engine_sanitizers.ps1 -Category Demos -Game d2
./android/tests/run_engine_sanitizers.ps1 -Category Routes -MissionJson kcxf2.json -Level 4
```

`-Category` accepts an array. `-DemoFileName`, `-CTestFilter`, `-MissionJson`, and `-Level` restrict their respective categories. `-MaxParallel` defaults to four, limiting compiler, CTest, and route-worker concurrency. Demos are serial because their replay sandboxes are not a parallel corpus runner.

The runner always invokes the incremental build before testing. MSVC AddressSanitizer builds use `buildd1-asan` and `buildd2-asan`, with their own freshness stamps, runtime DLLs, and symbols. Normal builds and checked-in regression JSON/demo expectations are not changed. Run one sanitizer suite at a time; build directories and demo sandboxes are shared between sanitizer invocations.

Results go to a fresh `android/temp/engine_sanitizers/<timestamp>` directory, or `-OutputRoot`. Each stage has a log and a summary entry written as it completes. Later test categories still run after a test failure; a build failure stops testing. A nonzero exit means a build/test/asset failure or a sanitizer diagnostic, not necessarily a navigation regression. Route `timeout`/`partial` results remain route outcomes; native process errors fail the stage. Inspect route results for navigation changes separately.

Demo expected-state comparisons remain enabled. An existing deterministic replay mismatch still fails; sanitizer mode does not bless or rewrite it. Native diagnostics are captured even when no result JSON was written, including SDL's redirected log files. Instrumented replay sandboxes are separate from normal replay sandboxes and retained for diagnosis.

The loader fixture pass needs the local Levels of the World CD mission data and KCXF2 archive. It checks that `comet.rdl` rejects overlay texture 14924 explicitly without a memory error. In `kcxf204.rl2`, ignoring serialized section offsets formerly produced bogus wall animation indices; the test now requires successful loading and collecting the blue key. This does not assert that KCXF2's entire route is confirmed. Missing fixtures are failures, not silent skips.

`ConfigBounds` covers a config without a final newline, a config being rewritten concurrently, and private worker settings isolation, using Counterstrike L1. This retains coverage for the earlier config-reader heap overread. Diagnostic files and results stay under the report directory.

For individual manual replays or builds:

```powershell
./run-windows-build.ps1 -Target both -Sanitizer address
./android/tests/run_input_demo_replay.ps1 -Sanitizer address -DemoPath <demo.dximdemo> -Mode realtime
./android/tests/run_input_demo_regressions.ps1 -Sanitizer address -RunMode headless
```

The CMake option is shared by both engines and all their registered CTests. On a supported Clang/GCC host, configure a separate build directory with `-DDXX_SANITIZERS=address,undefined` and run its CTest suite. This combination is not offered by MSVC or the Windows wrapper. Source-built dependencies inherit instrumentation; prebuilt vcpkg libraries, graphics drivers, and operating-system DLLs do not. AddressSanitizer does not detect every undefined behavior or data race. This suite does not run Android/emulator tests, and does not yet drive the Kotlin/JNI metadata corpus through instrumented workers.
