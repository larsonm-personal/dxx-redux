# Dot stack review and Windows verification

Review date: 2026-10-11

Reviewed PRs [#5](https://github.com/larsonm-personal/dxx-redux/pull/5) through [#14](https://github.com/larsonm-personal/dxx-redux/pull/14), cumulatively from `062a4c0f467fedf956ba12f497a2e7842833846b` to `fd3fb408de946f799b36048ffca4191bd4ec0227`

The stack is checked out in `C:/local/dxx-redux-dot-review` on `review/dot-stack-windows`. The original dirty `C:/local/dxx-redux` checkout was preserved. Review fixes are local and uncommitted; nothing was pushed, merged, or posted to GitHub

Outcome: one confirmed defect introduced by PR #14 was fixed locally, along with two pre-existing Windows defects exposed by real engine tests. The Windows engine, public save, compact replay and ASan checks pass with these fixes. Android completed all 51 variants: 49 pass, and both variants of one inherited graphics interruption scenario fail reproducibly. One tooling cleanup check also remains blocked by an unrelated malformed AVD configuration

## Confirmed PR finding

### P2: Active compact replay artifacts are not protected from Linux retention

- PR: #14
- Location at reviewed tip: `android/tests/run_compact_public_replays.py:317`
- Opening `producer.lock` does not take an advisory lock on POSIX. `clean-old-artifacts.ps1` tests exclusive access to identify live producers, so the newly added compact runner looks inactive to another retention sweep
- A long-running capture can become older than three newer generations and have its output removed while the engine is writing evidence. This produces lost diagnostics or spurious fidelity failures
- Reproduction: the added `test_active_capture_holds_retention_lease` attempts a competing `flock` while `_run_cases` is active. It failed against the PR implementation and passes after acquiring `LOCK_EX | LOCK_NB`, following the existing paired replay runner
- Evidence: `temp/dot-review/retention-before.log`, `retention-after.log`
- Local fix: acquire the POSIX lock for the entire capture scope; retain the existing Windows open-handle behavior

## Windows defects exposed by this verification

These defects also exist before this PR stack. They are not attributed to dot's changes

### P1: Metadata cache structure has different layouts across MSVC translation units

- Location: `android/app/src/main/cpp/shared/route_analysis_cache.h`
- Engine headers leave MSVC packing at one byte. The shared summary structure was 556 bytes in the metadata caller and 560 bytes in the implementation; its 64-bit fields and filename had different offsets
- Running a real Debug metadata worker caused MSVC's runtime check to report stack corruption around `cache_summary`, leaving the harness waiting on a CRT error dialog
- Local fix: bracket the shared structures with MSVC `pack(push, 8)` and `pack(pop)`; extend the existing C test to include the header under packing one and check both ABI offsets and restoration of caller packing
- Evidence: `temp/dot-review/packing.log`, `worker-d1.log`, `worker-d1-fixed.log`, `headers-fixed.log`
- After the fix: real D1, D2, and imported D1 worker/recovery tests, malformed-header tests, and both public secret-area baselines passed

### P1: Switching Windows build configuration leaves incompatible cached SDL libraries

- Locations: `run-windows-build.ps1`, `android/helpers/test_host_platform.ps1`
- Reproduction: build Debug, then build RelWithDebInfo in the same tree. CMake retained Debug SDL/SDL_mixer library paths. Independently, vcpkg's timestamp-only DLL deployment retained newer Debug DLLs, even after correcting the link paths. This left a mixture of Debug/release libraries and runtime DLLs. Both the replay smoke and compact flight process immediately exited with heap corruption (`-1073740940`)
- Local fix: refresh CMake when the build type changes, detect an already-switched cache whose SDL_mixer path belongs to the old configuration, and synchronize deployed DLL contents with the selected configuration after building
- The existing host-build guard test now exercises both switch directions, stale library paths after a previous switch, and replacement of newer DLLs from the wrong configuration in both executable output directories
- Evidence: `temp/dot-review/smoke-d1-release.log`, `flight-d1-release.log`, `build-guard-final.log`, `build-release-fixed.log`, `build-runtime-fixed.log`

## Stack coverage

| PR | Area reviewed |
| --- | --- |
| #5 | Linux build/test entry points, failure propagation, timeouts, packed GCC scalar conversion |
| #6 | Pinned public installers, extraction and asset admission, shareware profile resolution |
| #7 | Existing Android scripts' public profiles, catalog membership and variant enumeration |
| #8 | Emulator acceleration/renderer policy, startup ownership, readiness and cleanup |
| #9 | Public metadata fixtures, worker recovery, malformed-header handling |
| #10 | D1 briefing dispatch and compile-time automation variants |
| #11 | Bounded software-emulator navigation and failure reporting |
| #12 | Public ASan runner, instrumentation checks, diagnostic propagation and stated limits |
| #13 | Public save/endian/import integrity, corruption rejection and artifact handling |
| #14 | Compact corpus admission, immutable oracles, action evidence, repeatability and paired qualification boundaries |

## Unresolved Android integration finding

`test_graphics_native_interruption` fails in both D1 and D2 at step 34/53: after sending Tab during the active graphics confirmation challenge, `automap_active` remains false and the 5-second wait expires

The existing pause policy in `d1/main/gamecntl.c:1464` and `d2/main/gamecntl.c:2080` rejects gameplay keys while paused, permitting only Escape/Pause. `android_pause_reasons()` includes `ANDROID_PAUSE_GRAPHICS`. That guard came from pre-stack commit `260f702107`; the stack only marks this existing scenario as shareware-compatible. PR #10's D1-specific briefing dispatch is not the cause: D2 fails identically, and the logged Tab dispatch reaches the game handler

This is an inherited conflict between engine input policy and the scenario's expectations. Resolving it requires reconciling the intended pause behavior with the test contract. It remains unfixed, and the expanded Android suite must not be represented as fully passing on this checkout

Evidence: `temp/dot-review/test_graphics_native_interruption-device.log`; both failures include durable automation step results and final introspection. A separate rerun after the full sweep reproduced both failures at the same step: `temp/dot-review/native-interruption-isolated.log`

Reproduce on an installed debug APK: `pwsh -NoProfile -File android/helpers/run_shareware_tests.ps1 -Serial emulator-5590 -Filter test_graphics_native_interruption`

## Validation

Windows host: Windows build 26200, PowerShell 7.6.6, Python 3.11, VS 2022/MSVC 19.44, x86 Windows engines, CMake 3.31.6/Ninja. Android: JDK 21, freshly built x86_64 debug APK, dedicated API 34 emulator `emulator-5590` with SwiftShader and Windows acceleration. WSL Ubuntu supplied POSIX-only harness tests

| Check | Result and evidence |
| --- | --- |
| Debug engine builds | Both passed; `temp/dot-review/build-packing-fix.log` |
| RelWithDebInfo engine builds | Both passed after repairing cached link paths and deployed DLLs; `build-runtime-fixed.log` |
| Debug native CTest | D1 69/69 and D2 78/78; D1 recorder rerun with a longer timeout after concurrent build load; `ctest-d1.log`, `ctest-d1-rerun.log`, `ctest-d2.log` |
| Windows ASan public profile | All 17 stages passed, including builds, D1 69/69 and D2 78/78 CTests, both replay smokes, metadata/worker/header recovery, imported D1 and three config-read variants; no sanitizer diagnostics; `android/temp/public-sanitizers-windows-review/summary.json` and `temp/dot-review/sanitizers.log` |
| Pinned public asset installation | Both original installers downloaded, digest-checked and extracted successfully |
| Public save integrity | 15 cases and 1,260 corruption checks passed; `save-debug.log` |
| Public metadata | D1, D2, imported D1/profile switch, malformed headers and recovery, D1/D2 baselines passed after packing fix; `worker-*-fixed.log`, `worker-d2.log`, `worker-imported.log`, `headers-fixed.log`, `secret-d1.log`, `secret-d2.log` |
| Linux CLI regression tests | 15 passed under WSL; `linux-cli-wsl.log` |
| D1 briefing dispatch compile fixtures | All three configurations passed with MSVC; `briefing-msvc.log` |
| Compact public runtime replays | Passed: all six recordings replayed twice with exact Linux-oracle fidelity and action checks, plus nine paired captures with strict native repeatability and the required imported D1 flight gameplay-state/SIM-RNG comparison; `temp/dot-review/compact-runtime-fixed/report.json`, `paired-d1/scoped-gameplay-report.json` |
| Replay fixture/comparison unit tests | Windows: 20 passed and one POSIX-only test skipped; WSL: all 11 runner tests passed, including the new lease regression; `compact-fixture-final.log`, `compact-unit-final.log`, `retention-after.log` |
| Host policy and profile tests | Shareware catalog/profile, software emulator startup policy, save profiles, preflight and managed process-wait checks passed |
| Android build and public suite | Fresh x86_64 APK built successfully. All 27 scenario families / 51 game variants executed serially: 49 pass, 2 fail in the inherited automap interruption scenario, reproduced independently; `android-build.log`, `android-summary.json`, `device-results.json`, `android-death.log`, individual `*-device.log` files |
| Scoped mixed-language quality | Passed for all seven changed code/test files; `quality-final.log` |
| Release tooling integration | All 79 fixture scenarios passed on an isolated rerun; `github-release-rerun.log` |
| Tooling smoke | 23 of 26 checks passed initially. Catalog and release checks passed isolated reruns after load-related timeouts. SDK cleanup still fails on the pre-existing duplicate AVD config key described below; `android/temp/tooling_smoke/run_dot_review/summary.json`, `temp/dot-review/catalog-rerun.log` |

All planned validation is complete. The original phone and pre-existing emulators were not used for these tests; the dedicated review emulator was stopped after verification, with its AVD retained for inspection

## Reproduce the main Windows checks

From `C:/local/dxx-redux-dot-review`, after provisioning the normal Windows dependencies:

```powershell
pwsh -NoProfile -File run-windows-build.ps1 -Target both -BuildType Debug -MaxParallel 6
# Use CTest from the CMake installation selected by the build script
ctest --test-dir buildd1 --output-on-failure --parallel 1 --timeout 600
ctest --test-dir buildd2 --output-on-failure --parallel 1 --timeout 600
pwsh -NoProfile -File android/helpers/setup_shareware_test_data.ps1 -Game both -Download
pwsh -NoProfile -File android/tests/test_d1_wall_blast_save.ps1 -NativeExe ./buildd1/main/test_upstream_compat.exe -ImportedExe ./buildd2/main/test_upstream_compat.exe -TimeoutSeconds 300
pwsh -NoProfile -File run-windows-build.ps1 -Target both -BuildType RelWithDebInfo -MaxParallel 6
pwsh -NoProfile -File android/tests/test_compact_public_replays.ps1 -D1Exe ./buildd1/main/d1x-redux.exe -D2Exe ./buildd2/main/d2x-redux.exe
pwsh -NoProfile -File android/tests/run_engine_sanitizers.ps1 -Category Public -MaxParallel 4
```

For Android, set `JAVA_HOME=C:/local/jdk-21`, build `:app:assembleDebug`, target only the dedicated emulator using `ANDROID_SERIAL`, and run `android/helpers/run_shareware_tests.ps1` serially. This review's IDE-style x86_64-only Gradle build produced `android/app/build/intermediates/apk/debug/app-debug.apk`; `DXX_TEST_APK` selected that APK explicitly

The dedicated AVD is retained at `temp/dot-review/avds/DotStackReview.avd`. To restart it for inspection:

```powershell
$env:ANDROID_AVD_HOME = (Resolve-Path temp/dot-review/avds).Path
$env:ANDROID_SERIAL = 'emulator-5590'
$env:DXX_TEST_APK = (Resolve-Path android/app/build/intermediates/apk/debug/app-debug.apk).Path
. ./android/helpers/test_helpers.ps1
Start-ManagedEmulator -AvdName DotStackReview -Serial emulator-5590 -Headless -GpuRenderer swiftshader -BootTimeoutSeconds 240
```

## Qualification limits

- The compact gate explicitly qualifies native fidelity/repeatability and imported D1 flight gameplay-state/SIM-RNG only. Imported pickup/door and full diagnostic/object/world parity remain unqualified. A green wrapper does not mean full D1-in-D2 simulation parity
- The ASan graphical smoke intentionally terminates after replay evidence is written. Its documentation excludes graceful shutdown/leak coverage for that process; this is a stated coverage limit, not a newly discovered defect
- Windows ASan uses RelWithDebInfo and defines `NDEBUG`; assertion-based test coverage was established separately by the complete Debug CTest runs
- GitHub's green Windows tooling job does not exercise these real Windows engine workloads. Linux engine results were inspected in CI; a full Linux rebuild was not repeated locally
- Android testing uses a dedicated accelerated Windows emulator with software graphics. It does not establish no-KVM Linux software-CPU performance or physical-device behavior
- Full tooling smoke encountered an unrelated existing AVD configuration with duplicate `hw.cpu.ncore` in the original checkout. That user's configuration was left untouched
