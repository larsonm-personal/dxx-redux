# Windows/Linux Android tooling and storage scope

Date: 2026-09-26
Status: implementation in progress; see execution record below

## Current verification status

The gap inventory below records the starting state. The execution record documents repairs and their limits

- Linux bootstrap, APK/AAB builds, JVM tests, normal and sanitizer native builds have execution evidence below. Latest normal native run passed all 54 D1 and 63 D2 CTest cases
- Shared tooling CI passed on Windows 2022 and Ubuntu 24.04 after the cleanup-fixture correction. New uncommitted work extends that workflow with pinned-runtime/extraction checks; this extension has local Linux evidence only so far
- Latest extended Linux tooling smoke passed 17 tests. Pinned Python 3.12.8 now supports bounded extraction on Linux x86-64, with archive/tree admission and no PATH fallback. Both bounded test entries passed through the master runner; all four available demo packages passed extraction
- Native retail route runs now discover and stage mixed D1/D2 data on either host. All 60 FirstStrike/Counterstrike levels passed two repeats on Linux; temporary base-data copies were removed on success and injected engine failure
- Managed version retirement covers DOSBox and the Linux Python runtime and protects sanitizer build-cache references. Linux directory, in-place cmakelang and soundfont installers recover from forced termination; Windows transaction parity and SDK cleanup remain unfinished
- SDK bootstrap now registers current versioned packages and invokes managed retirement through sdkmanager. The cleanup integration passes on Linux; real registration owns five current packages and preserves four unowned legacy packages. Verified partial-uninstall recovery now uses directory identity and payload manifests; Windows writer locking and legacy SDK adoption remain unfinished
- Remaining work includes a complete test/capability ledger, remaining asset-dependent wrappers, replay/benchmark content differences, broader build CI, and device validation. This host has no KVM and its two bounded software-emulator attempts did not boot; those tests require another usable device/host
- No native, test, or formatter process from the latest completed tranche remains running. The protected outstanding_bugs.md file has not been changed

## Outcome and boundaries

Support the same Android dependency bootstrap, APK/AAB builds, host tools, quality checks, and automated test entry points on Windows and Linux. Keep OS details in shared helpers. Build in bounded storage use and cleanup on success, failure, timeout, and the next invocation after interruption

Initial acceptance platforms: Windows x64 and Ubuntu Linux x64 with PowerShell 7 and Bash. Preserve existing Windows PowerShell 5.1 entry points where currently supported. Other Linux distributions remain best effort until exercised. Distinguish the host platform from Android ABI; do not equate every non-Windows host with Linux. Leave explicit extension points for macOS and host architectures, without promising macOS support in this work

This covers Android tooling plus the desktop/native tools, game-data preparation, server, and emulator infrastructure needed by its tests. It does not port the native macOS game target, change game behavior, publish releases, or require proprietary media in public CI

## What already exists

This is completion and regression repair, not a new Linux port

- May 24-26 plans cover Linux dependency installation, desktop builds, regression helpers, and data recovery. `testing/plan_run_all_tests_linux_20260602.md` records a later Linux test-runner/quality pass, including an emulator test
- `get_deps/get_all.sh`, `get_deps/helpers/platform.sh`, and `Get-DepPlatform.ps1` already resolve several OS-specific downloads and paths. `get_deps/README-ubuntu.md` documents Ubuntu bootstrap
- `helpers/test_host_platform.ps1` already resolves SDK tools, host binaries, Gradle wrappers, PowerShell, ports, and host build invocation. Reuse this layer and the dependency platform helpers rather than creating parallel implementations for every test
- `run-linux-build.sh` builds D1/D2 host binaries. Native CTest, Gradle unit tests, replay runners, and portions of the main test harness already use portable entry points
- `helpers/retain-recent-artifacts.ps1` already retains three prior producer generations and checks a 4 GiB reserve. `CLEANUP.md` also documents payload pruning, byte budgets, locks, and replay failure cleanup

## Concrete gaps found in this checkout

| Area                    | Evidence                                                                                                                                           | Required work                                                                                                     |
| ----------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| Workspace cleanup       | `clean-workspace.ps1` explicitly rejects deletion outside Windows; its process inventory uses CIM                                                  | Implement Linux activity/ownership checks and portable deletion safeguards                                        |
| Native Gradle retention | `build.gradle` invokes `powershell.exe` only on Windows                                                                                            | Run the shared retention policy on both hosts before new `.cxx` generations                                       |
| Cleanup tests           | `tests/test_clean_workspace.ps1` mocks CIM and creates a Windows junction                                                                          | Exercise real Linux symlinks, process ownership, and lock semantics alongside Windows cases                       |
| Dependency lifecycle    | `get_ndk.sh` deletes its download only after successful extraction, extracts directly into the install root, and leaves superseded NDK directories | Staged, validated installation; failure cleanup; bounded obsolete-version retention                               |
| JDK reproducibility     | `get_jdk.sh` derives a `latest` URL while validating against configured `JDK_VERSION`                                                              | Download the exact configured release and verify its archive; retain existing rollback behavior                   |
| Environment selection   | `helpers/set_vars.sh` chooses the last glob match for JDK/NDK                                                                                      | Resolve configured versions explicitly so pruning and version selection agree                                     |
| Build entry points      | `1_build-aab.ps1` calls `gradlew.bat`; `helpers/build.sh` always waits for a key                                                                   | Shared wrapper resolution and noninteractive operation with reliable exit codes                                   |
| Test/helper drift       | Direct `gradlew.bat` calls in dual-emulator tests, crash-report tests, mission archive helpers, warning collection, and metadata regeneration      | Route callers through shared Gradle/tool helpers; inspect branches before classifying each occurrence as a defect |
| Host regeneration/tests | Direct Windows build calls in guidebot tests/helpers and metadata runners                                                                          | Shared host build dispatch, executable lookup, and capability reporting                                           |
| Process lifetime        | `helpers/process_lifetime.ps1` enrolls Windows jobs but initialization is a no-op on Linux                                                         | Test and implement owned child/process-group cleanup, including parent termination and grandchildren              |
| Sanitizer parity        | Linux build CLI lacks the Windows sanitizer option; sanitizer/replay guards reference Windows builds                                               | Provide Linux configuration or an explicit tracked capability gap, never silent success                           |
| Documentation           | `.github/copilot-instructions.md` refers to absent `android/run_emulator.sh`                                                                       | Document `pwsh android/Run-Emulator.ps1` and remove stale paths                                                   |
| Continuous validation   | Existing `.github/workflows/package-linux.yml` packages desktop games; no Android test workflow in the tracked workflow inventory                  | Add Windows/Linux checks for the Android tooling and selected integration paths                                   |

The inventory includes 238 top-level `android/tests/test_*.ps1` files. This is a file count, not 238 demonstrated failures or independent suites. Python tests, native tests, JSONC game scripts, and support-owned tests also need accounting. Existing source-pattern tests are useful but cannot prove runtime portability

## Implementation sequence

### 1. Establish the baseline and shared host contract (2-3 engineering days)

- Inventory runnable tests and artifact producers, including `game_data/`, packaging, quality tools, dependency updates, and nested runners
- Record each test's host, tools, media, device, display, Docker/network, and manual prerequisites. Separate passed, failed, unavailable, intentionally unsupported, and not run
- Consolidate host/tool/path discovery into the existing helpers. Keep thin Bash and PowerShell interfaces where needed, with the same version/config source and precedence rules
- Abstract process inspection, owned child termination, disk free-space lookup, path containment, and links. Keep Windows and Linux implementations behind these contracts
- Explicitly support paths containing spaces and Linux case sensitivity; audit native command argument quoting, executable permissions, line endings, and exit-code propagation

Deliverable: an executable capability inventory and a small Windows/Linux smoke profile, with an initial failure ledger

### 2. Make storage lifecycle safe and bounded (3-5 days)

Do this before large bootstrap/build/test validation runs

- Extend workspace and native-build cleanup to Linux, keeping preview mode and current protections for tracked/untracked work, fixtures, original media, nested repositories, links, and active runs
- Add producer ownership/locks with stale-owner recovery. Linux permits unlinking open files, so inability to delete or an open-file probe is not sufficient protection. Account for concurrent runs, PID reuse, symlinks, and changes between discovery and deletion
- Define output families for every producer. Each must reuse a fixed owned directory or declare retention and cleanup; new timestamped outputs must not escape policy through custom names
- Keep three prior report/package generations by default, plus the planned/current output. Preserve existing stricter family budgets such as paired replay captures. Add byte limits for large payload families; generation limits alone do not bound disk use
- Clean per-run download/extraction/build staging in `finally`/traps. Recover abandoned owned staging on the next run after a forced kill. Preserve small failure diagnostics; avoid archiving copied binaries and full payloads by default
- Check the actual output volume before large operations, including the dependency volume. Include unpacked size and replacement overlap in space estimates; periodically check long-running trace/extraction jobs. Stop with an incomplete result if the reserve is exhausted
- Keep explicit full scratch cleanup separate from producer startup retention. Do not let a producer invoke a global sweep that deletes unrelated diagnostic history

Deliverable: Linux deletion support, shared producer lifecycle helpers, policy documentation, and synthetic integration tests on both hosts

### 3. Complete bootstrap, updates, and build/package parity (3-5 days)

- Audit every downloader and `check-updates.ps1` installation path, including optional fpcalc, archive, Rust/server, formatter, and audio/oracle tools needed by selected tests
- Use exact version/architecture/OS selection and verified downloads. Install to staging, validate, publish, then prune superseded owned installations. A failed update must leave the last usable version intact
- Share resolution between installation and build/test invocation. Handle foreign-host `dependency_base.txt` values explicitly rather than manufacturing a Windows path on Linux
- Enable debug APK, local release AAB, JVM tests, native host tools, and quality checks on both hosts. Preserve signing/version behavior; do not upload artifacts during validation
- Route Gradle/native cleanup through the shared policy. Preserve useful incremental caches while removing obsolete generations

Deliverable: fresh and incremental builds on both platforms, plus upgrade/failure/retry demonstrations

### 4. Bring tests over in dependency order (4-7 days)

1. No-device script/Python checks, dependency/cleanup integration tests, and quality tools
2. Native D1/D2 CTest, extraction/metadata host tools, JVM tests, headless replays, guidebot and data-regeneration tests
3. One-emulator launch, automation, launcher, import/SAF, audio lifecycle, and crash-report tests
4. Dual-emulator/server/multiplayer and Docker NAT tests, run serially where device state is shared
5. Graphics/audio capture, sanitizer, manual/hardware tests, and proprietary-media corpus runs on suitable hosts

Fix shared chokepoints first, then individual scripts. Audit Windows-only names as well as code: a `test_windows_*` file may exercise portable behavior and need portable naming/dispatch. Do not blanket-skip tests because they historically ran on Windows

Tests needing unavailable media, hardware, acceleration, or a display must report an explicit reason. Windows-specific contracts such as PowerShell 5.1 or Job Objects remain Windows tests with corresponding Linux behavior tests where applicable. Full corpus runs belong on equipped machines; public CI can use redistributable fixtures and synthetic inputs

Deliverable: a per-test parity report with remaining exceptions, not merely a green sampled suite

### 5. Prevent renewed drift and finish documentation (2-3 days)

- Add Windows/Linux CI smoke coverage on script/build changes: helper integration, cleanup, native/JVM checks, and a bounded Android build where practical
- Add scheduled or manually dispatched emulator/full profiles with explicit capability requirements and retained artifact limits. Keep default quick profiles useful, but require full accounting for acceptance
- Add a focused check that flags new platform-specific invocations outside approved helper implementations, with narrow exceptions for genuine platform tests
- Document bootstrap, builds, test profiles, cleanup, disk budgets, and recovery on both hosts. Retire stale shell paths and reconcile prior Linux plans

Planning estimate: 14-23 engineering days for the full scope, plus large-corpus/device execution time. This is an estimate from inspection, not a measured backlog. Re-estimate after phase 1; cleanup race handling and multimedia/multiplayer infrastructure are the main uncertainties. Phases 1-3 are a useful build-and-storage milestone, but do not complete test parity

## Storage policy by owner

| Owner                                     | Default lifecycle                                                                                                                                           |
| ----------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Dependency downloads and extraction       | Unique staging; delete archives/staging after success or failure; next-run recovery for interrupted owned stages                                            |
| Managed JDK/NDK/tool versions             | Keep configured/referenced versions and active users; prune superseded owned versions after validated replacement; keep rollback until publication succeeds |
| SDK packages and emulator images          | Remove obsolete project-owned packages through SDK tooling; preserve packages/AVDs referenced by supported configurations                                   |
| Native `.cxx` configurations and packages | Existing producer retention of three prior generations per family; protect active configuration/ABI users; explicit cleanup may retain fewer                |
| Fixed CMake/Gradle/Cargo build trees      | Reuse compatible trees; remove stale owned variants by policy, not unconditional clean builds                                                               |
| Test reports and large payloads           | Bounded generations plus family byte budgets; trim bulky reproducible payloads separately from compact failure evidence                                     |
| Temporary emulator/device data            | Clean owned temporary AVDs, snapshots, uploads, and run data on completion/failure; preserve intentional persistent test AVDs and source assets             |
| Global/shared caches                      | Use tool-supported pruning only within declared ownership; do not recursively purge user-wide Gradle, Cargo, SDK, or dependency directories                 |

Because dependencies may live outside the repository and serve other projects, track managed installations and references explicitly. Report protected old versions and why they remain; never assume every old directory under `dependency_base.txt` is disposable. Allow an explicit managed-root policy for dedicated installations

## Acceptance criteria

- Fresh supported Windows/Linux machines can bootstrap pinned tools, build APK and local AAB, build D1/D2 host tools, and run the documented quick profile
- Repeated bootstrap/build/test runs do not accumulate unbounded superseded versions or output generations; record bytes before/after and exceptions
- Failed downloads, checksum failures, full disks, extraction errors, build failures, test timeouts, and interrupted parents preserve valid inputs/last good installations and leave only bounded diagnostics or recoverable staging
- Concurrent builds/tests survive cleanup. Tests cover active ownership, links, case-sensitive paths, tracked files, nested repositories, and a dependency root on a different filesystem
- Full-suite accounting exists for both hosts. Every unrun/unsupported test has a concrete reason and follow-up; representative emulator tests pass for both games, and advanced profiles run where infrastructure permits
- macOS is an explicit future capability rather than an accidental Linux fallback

## Validation performed during scoping

- Passed `pwsh -NoProfile -File android/tests/test_dep_platform.ps1`
- Passed `bash android/tests/test_dependency_temp_files.sh`; its synthetic files clean up on exit
- Passed `bash -n` for tracked shell scripts in dependency bootstrap/helpers, Android helpers, and the root Linux build script
- Inspected existing code and prior Linux/cleanup plans. No dependency bootstrap, full builds, emulator runs, large corpus tests, or workspace deletion were performed
- The checkout's filesystem had approximately 24 GiB available when inspected. This is a local observation, not a supported minimum for the full pipeline
- `git diff --check` passed. The required scoped quality runner attempted to install its missing PSScriptAnalyzer dependency, then stopped because `shfmt` is absent; no applicable code files were selected for these Markdown-only changes

## Execution record

Implementation started 2026-09-26 following explicit authorization to complete dependency fetch, local builds, and practicable tests. `android/outstanding_bugs.md` is read-only and has no changes

Completed and observed so far:

- Real Linux bootstrap completed with `get_all.sh --skip-host-prereqs --no-prompt`; the prerequisite helper separately reports all host prerequisites present
- Fixed JDK latest-versus-pin drift: exact 21.0.12+8 archives and per-host SHA-256 pins, with updater support for patch releases and pin validation before publication
- Added shared staged directory publication for SDK/NDK/CMake, rollback on publication failure, and staging/download cleanup on failure
- Dependency integration fixtures exercise the real installers with local archives; failed download, failed publication, checksum rejection, retry, and repeat-run cases pass
- Workspace cleanup now uses a shared Windows/Linux process inventory. Synthetic cleanup tests and real child-process protection pass on Linux
- Gradle invokes native retention on Windows and Linux; Unix internal file symlinks in owned CMake trees can be cleaned without traversal
- Free-space checks select the actual mounted output filesystem; verified on `/dev/shm`
- The three-ABI debug APK build passed (`bash android/helpers/build.sh assembleDebug --max-workers=2 --console=plain`, 7m 7s)
- Dependency tests passed through `run_all_tests.ps1 -Filter 'test_dep*'` with two passes and no skips in `temp/test_reports/report_20260926_162435.md`
- The 58 Python `test_*contracts.py` checks pass after sourcing `set_vars.sh`; stale source assertions were updated to the current initialization, restore, and JNI conversion code
- A scoped quality pass passed for the first script tranche; later changes need the next scoped pass

Still in progress, not completion evidence:

- Native host builds exposed Android-only declarations used on all hosts, missing `size_t` header ownership, GCC packed-field JSON reference errors, and a fixture callsign overflow. Both games compile and their suites passed; the combined runner also passed after the latest shared test fixture edits
- Broader host/regeneration/test portability, obsolete dependency-version retention, interrupted-process recovery, and CI parity remain outstanding
- Emulator acceleration check exits 3: `KVM requires a CPU that supports vmx or svm`. There is no `/dev/kvm`; emulator test feasibility remains to be assessed, and unavailable infrastructure must be reported rather than counted as passing
- No Windows execution has been performed in this Linux workspace

Current reusable logs are under `android/temp/`: `linux-bootstrap.log`, `linux-assemble-debug.log`, `linux-native-tests.log`, `linux-packed-fields.log`, `linux-python-contracts.log`, `linux-cleanup-test.log`, and `linux-dependency-updates-test.log`. They are disposable logs, not permanent fixtures

### Further Linux verification and portability work

- D2 compiled successfully and passed all 63 CTest tests (`linux-native-d2-ctest.log`, 78 seconds); D1 previously passed all 54 tests. The combined native runner also passed after the final shared fixture changes
- The Android JVM suite completed successfully: 1,082 test cases reported, 1,078 passed and four fixture-dependent skips, zero failures/errors. XML evidence is under `android/app/build/test-results/testDebugUnitTest`; log is `android/temp/linux-jvm-tests.log`
- `pwsh android/1_build-aab.ps1 -BuildType 1` produced a 61.8 MB local debug AAB in 1m 20s. Inspected its ZIP entries and confirmed native libraries for armeabi-v7a, arm64-v8a, and x86_64. No upload/publishing was performed
- Added `Initialize-RegressionJavaEnvironment` to the existing shared host helper. It selects the configured JDK major, preserves an explicit JAVA_HOME, and manages PATH using the host separator. Missing configured Java no longer silently selects an unrelated newer directory. Real helper fixtures cover spaces, override, repeated setup, and missing-install behavior
- Build-AAB, emulator setup, warning collection, mission metadata CLI, and metadata comparison/regeneration now use shared Java/Gradle selection. Public metadata regeneration defaults to `Host`; `Windows` remains an accepted alias
- The real metadata CLI built and its mission archive variant integration test passed on Linux (`linux-mission-archive-variants.log`). Test temporary directories now clean up in finally
- Formatter installers (shfmt, ShellCheck, clang-format, ktlint) now download and validate in a shared owned workspace on the destination filesystem before publication. Their integration fixtures cover low space, partial downloads, corrupt input, publication rollback, termination, repeat runs, and temporary cleanup, alongside SDK/NDK/CMake tests
- Fixed download helpers to propagate transport failure when called from a conditional; previously their explicit success return could hide curl/wget errors when Bash errexit was suppressed
- Scoped mixed-language formatting/lint passed after these changes (`linux-scoped-quality.log`)
- Metadata worker/workspace/pool tests are now classified as host tests rather than requiring an emulator. The two metadata tests passed through the master runner with no skips (`report_20260926_170234.md`); the pool and runner-wiring tests also passed

Additional concrete blockers found, to resolve or account for in the remaining test work:

- Implemented pinned Linux x64 7-Zip 26.02 installation from the [official release](https://github.com/ip7z/7zip/releases/tag/26.02), with archive and executable SHA-256 verification. The real install/extraction/corrupt-replacement recovery test passed. Bootstrap now includes verified 7-Zip, and the updater no longer labels it manual-only on Linux. Other extraction tools/runtime still need porting
- The bounded Python extraction runtime is pinned to a Windows embedded Python distribution. Linux runtime admission/packaging and associated tests still need implementation
- The two real metadata workspace integration archives (`game_data/mission_files/chromium.zip` and `d1secret.zip`) are absent from this checkout. Do not count their integration profile as executed
- Linux parent-death cleanup in `process_lifetime.ps1` remains unimplemented. Ordinary child timeout/error cleanup is a separate, narrower capability
- Existing formatter/download staging improvements do not yet reclaim superseded dependency version directories or recover workspaces left by SIGKILL; both remain required work

The filesystem currently has approximately 15 GiB free. Keep using incremental fixed build trees and bounded reusable logs; avoid unnecessary duplicate full builds

Latest completed checks after adding Linux 7-Zip:

- Repeated real `get_all.sh --skip-host-prereqs --no-prompt` completed all 13 steps, including the verified Linux 7-Zip installation
- Current `assembleDebug --max-workers=2 --console=plain` passed in 25 seconds after the latest production-source changes
- Combined `test_native_host_unit_tests.ps1` rebuilt both games and passed 54 D1 plus 63 D2 tests
- `test_7zip_install` passed through the master runner with no skips (`report_20260926_170738.md`)
- Final scoped mixed-language formatter/linter checks and `git diff --check` passed

No completion claim: the remaining storage ownership/pruning/recovery, Linux runtime and process lifetime, full host test inventory, emulator feasibility, CI, and Windows verification work above is still open

### Managed dependency retention

Added `get_deps/clean-dependencies.ps1`, invoked by successful bootstrap and dependency update runs. An ownership registry tracks configured managed installations and sharing checkouts. Preview/apply reports distinguish managed, referenced, active, and unmanaged versions. Cleanup checks current config, process command/executable/cwd, environment overrides, retained CMake caches, installation identity, tree changes, and links. Existing unowned versions are not silently adopted.

The integration fixture uses two checkouts sharing a dependency root containing spaces. Shared-version protection, active users, cache references, held locks, unavailable configuration, external links, preview, same-version replacement, and repeated cleanup are covered. A subprocess is forcibly killed after moving an old installation to quarantine and deleting its marker; a subsequent real cleaner invocation completes the retirement using the on-disk journal. This passed on Linux. Windows behavior is not yet executed.

Remaining storage work includes direct helper invocation integration, per-installer operation locks and interrupted staging recovery, SDK package/image pruning, and coverage of remaining output producers. The managed version retirement journal covers interrupted cleanup, not interrupted downloads/installations.

Observed after enabling retention:

- Real Linux bootstrap passed all 14 steps. The ownership registry contains eight configured tool installations; the original NDK r27d and shfmt 3.13.1 directories are explicitly reported as unmanaged and preserved
- Workspace cleanup, real process protection, and dependency-update regression tests passed (`linux-retention-regressions.log`)
- Added PATH override and missing-replacement protection, case-sensitive checkout registration, and explicit retirement of checkout references through `-UnregisterRepository`
- Scoped mixed-language quality checks passed. Managed retention passed through the master harness in six seconds with no skips (`report_20260926_172752.md`)

### Guidebot runner portability and snapshot retention

Replaced hardcoded Windows guidebot executable names, native build command, Gradle wrapper, JDK, and ADB locations with the shared host helpers. Runtime snapshot discovery includes Unix shared libraries and ignores dangling optional Unix ABI links. The native Linux executables are already built; the runner now resolves their actual names (`dxx-redux-d2-headless-route` and `d2x-redux`).

Snapshot cleanup now reads the existing file/hash manifest, so extensionless Linux binaries and versioned `.so` files are released along with Windows binaries. Modified snapshots and metadata are preserved, and manifest path traversal is rejected. Older Windows snapshots without a manifest retain their existing cleanup behavior.

The guidebot schema, publication batching, reporting, runner discovery/sampling/failure-publication checks, and metadata workspace tests passed locally. The runner's intentionally failing engine fixture uses an empty test HOG directory instead of depending on unavailable retail media; this is not evidence of a real route-corpus execution. The runner fixture has unique, locked, retained temporary output and finally cleanup. Host-only suite classification now includes these guidebot tests. Master-harness and scoped-quality checks are pending for the latest changes.

### Host profile and further device-script portability

- Added an explicit `run_all_tests.ps1 -HostOnly` profile for tests catalogued as requiring no device infrastructure. Excluded tests remain visible as skips with their declared infrastructure; this does not prove that the legacy classifications are correct. Browser and timeout-policy guidebot tests still need classification/fixture review
- Guidebot master-harness verification passed three host tests, with three explicitly excluded tests, in `temp/test_reports/report_20260926_174150.md`
- Reviewed eight additional host tests and classified them accordingly. Standard game-data resolution, metadata archive sources, intent schema, targeted sampling, parallel metadata results, and CD metadata sources passed. JSONC/tracklist validation now scans checked-in JSON sidecars without requiring optional archive payloads; it passed against seven current tracklists
- Ordinary JSON normalization now resolves Python through the shared host helper, including Linux `python3`. Numeric-form normalization passed. This does not change or satisfy the separately verified bounded extraction-runtime requirement
- Removed Windows-only Gradle/JDK selection from extraction, Mac-media SAF, xCrash, and both dual-emulator setup tests. xCrash selects the host aapt2 filename. All five scripts parse; actual Linux Java/Gradle selection and aapt2 execution passed. Device behavior has not been exercised
- Scoped mixed-language formatting/lint and `git diff --check` passed after these changes. The dependency platform and pinned Java integration fixtures passed again
- `android/outstanding_bugs.md` has no staged or unstaged changes and remains read-only

The full goal remains open: installation interruption/concurrency cleanup, remaining host/runtime portability, complete test accounting, emulator feasibility, and Windows execution evidence are not yet satisfied.

### Guidebot browser and timeout host coverage

- Corrected the browser and timeout-policy tests' emulator classification. Both are host tests with optional retail fixture portions
- Browser list/search/preview and timeout calculation/generated-script assertions ran successfully on Linux. The complete tests report SKIP with concrete missing archive/D2 asset reasons, using the harness's required exit code 2. No missing-fixture test is counted as a pass
- Timeout test now resolves native executables/builds through shared helpers, accepts a HOG directory override, and uses a unique retained/locked workspace with finally cleanup. Verified that no timeout fixture directory remained after the skipped native portion
- Interactive browser now invokes the shared host build and only applies Start-Process WindowStyle on Windows. Actual visible gameplay remains unverified without the game fixtures/display
- Final guidebot master report: `temp/test_reports/report_20260926_175028.md`, three passes, two fixture skips, one emulator-profile skip, no failures/timeouts. Scoped formatter/linter and diff checks passed
- Further inventory found `test_input_demo_replay_failures.ps1` still requires Windows Framework csc despite its host classification; port its controlled child-process fixture before claiming full host-suite support

### Linux replay failure integration

- Ported `test_input_demo_replay_failures.ps1` to compile its controlled child with the Linux host C compiler while retaining the existing Windows Framework fixture
- Linux fixture deliberately links a versioned `.so.1` using an origin-relative runtime path. Replay staging now uses shared runtime-library discovery, preserving the existing address-sanitizer PDB handling
- All six real-wrapper outcomes passed: prelaunch validation failure, early exit, forced timeout, result followed by error, comparison mismatch, and success. Checks retain native diagnostics/identity and verify the child and staged sandbox are gone
- Failure archives reject staged native executable/library payloads. Fixture source and compiled outputs are removed in finally; a producer lock protects its bounded retained diagnostics during execution
- Standalone integration passed, then the formatted final version passed through the master runner in nine seconds with no skips: `temp/test_reports/report_20260926_175416.md`. Scoped formatting/lint and `git diff --check` passed
- Windows execution remains unverified; this fixture test does not prove cleanup after forcible termination of the PowerShell parent

### Broader host-suite audit and extraction build

Ran the default host-only profile through completion: `temp/test_reports/report_20260926_175500.md`, 8m24s, 45 passes, six failures, 182 skips (including excluded device/rotating scenarios), zero timeouts. This is sampled host coverage, not exhaustive coverage. Its failures are preserved as observed, even where subsequently fixed:

- `test_cue_iso`: fixed the extraction build's Unix FluidSynth/TagLib uninstall-target conflict by ordering dependency setup so TagLib reuses the target. Added the missing limits header in fingerprint_match. Fixed an unsigned subtraction bound in MIDI display-name formatting and added a long-composer regression. Fixed the test fixture's undersized realpath buffer by allocating the resolved path before checking/copying it
- After these fixes and scoped formatting, extraction CTest completed: 51 passes, one explicit real-media skip, zero failures. Log: `android/temp/linux-extraction-native.log`. Existing GCC truncation warnings elsewhere in extraction sources/fixtures remain visible; they were not suppressed
- `test_input_demo_comparison_policy`: resolved ordinary Python through the shared helper; standalone rerun passed all 64 Python cases. Log: `linux-replay-policy.log`
- `test_guidebot_route_regressions`: 23/24 selected cases failed. Individual evidence is in `android/temp/route_regression_cases/run_20260926_175621_894/`; missing mission archives are among the causes. Classify each cause before changing verdicts
- `test_input_demo_regressions_d2`: actual recorded-result mismatches (including position/energy), not merely absent fixtures. Preserve the expected results and investigate causes within the repo's determinism guidance
- `test_test_process_output_capture`: child holder PID has already exited when Get-Process is called. Linux process/output lifecycle still needs investigation
- `test_vertigo_metadata_checkpoints`: metadata worker launch has an empty FileName in New-MetadataKotlinWorker; this is an unresolved Linux launcher bug, not a fixture skip

The host sweep also passed the JVM runner, server integration, replay runtime smoke, and both native CTest suites (54 D1, 63 D2). The graphics replay canary passed. The original audio fingerprint test counted unavailable MP3 data as PASS; it now emits a concrete skip marker and exit code 2 (verified directly), pending master rerun.

Latest incremental three-ABI APK build passed after the MIDI source change in 1m19s (`linux-assemble-debug.log`). Scoped formatting/lint and diff checks passed. All processes launched for this audit are terminal. Approximately 14 GiB remained free. The protected outstanding_bugs file still has no changes.

### Persistent metadata worker and output-pipe lifecycle

- Fixed New-MetadataKotlinWorker to execute the Unix CLI directly; Windows retains its command-shell launch. The previous Linux FileName was empty because ComSpec is Windows-specific
- The real persistent Kotlin protocol now has integration coverage in the host runner test: two descriptor requests using a path containing spaces, persistent process identity, and verified shutdown. Passed standalone and through the master runner (`report_20260926_180747.md`, two seconds, no skips)
- All native/Kotlin worker creation and processing is enclosed in finally cleanup, including partial startup. Ran the actual metadata runner in a surviving PowerShell process, caught the missing pinned D1-data setup error, and verified all three real worker handles had been disposed
- The Vertigo test now reaches the missing pinned D1-data requirement. Its full metadata/checkpoint assertions remain unverified; the launcher defect is fixed but this test is not reported as passed
- Fixed the output-capture fixture's parameterless WaitForExit call, which waited for the descendant's inherited output pipes to close on Linux. The test now reads/merges logs while that descendant remains alive, then terminates it and cleans its locked temporary directory. Master regression passed in two seconds (`report_20260926_180717.md`)
- Scoped formatting/lint and diff checks passed. No processes from this tranche remain running. The outstanding_bugs file remains untouched

### Cmakelang installation rollback and temporary storage

- Cmakelang installs Python console launchers with absolute interpreter paths. Its installer now retains the previous tree in an owned workspace while constructing/validating the final-path installation, restoring that tree on ordinary failure or termination
- Embedded Python, get-pip, and virtualenv fallback downloads are all contained in the same trapped workspace. Pip cache writes are disabled. Both format/lint modules are validated before retiring the previous tree. Linked destinations are rejected, and disk reserve is checked before mutation
- Added deterministic integration coverage for failed installation, failed validation, TERM rollback, successful publication, repeated use, fresh-install failure, and workspace cleanup. It runs through the existing dependency-install test owner
- Performed a real Linux install and repeat run under an isolated dependency root containing spaces. Both executables worked and the entire temporary fixture was removed. Log: `android/temp/linux-cmakelang-install.log`
- Scoped formatting/lint passed; dependency-install master test passed in four seconds without skips (`report_20260926_181116.md`)
- This does not yet serialize concurrent installers or recover this final-path transaction after SIGKILL. Those remain required storage work. Existing cmakelang transitive package pins also still need review

### Ordinary Python utility callers

- Replaced direct `python` calls in D1 replay parity, DOS MIDI reference parity, and LAN ending/flyout fixture preparation with shared host resolution and launcher-prefix arguments. This does not change bounded extraction runtime admission
- Extended platform integration to invoke the real Python ending/movie generator against a synthetic movie library and output paths containing spaces, checking extracted content and mission output with finally cleanup
- Platform integration passed through the master runner (`report_20260926_181424.md`, one second, no skips). DOS MIDI synthetic mode passed its three native/Python CTest cases using the existing extraction build tree
- Scoped formatting/lint and diff checks passed. Device LAN behavior, actual D1 recordings, and DOS reference captures remain unavailable/unverified. Separate DOS capture and FM experiment wrappers still contain Windows-oriented launch/build choices for later inventory

### Software emulator feasibility probe

- Confirmed no `/dev/kvm` and no attached device. Launched the existing API 34 x86_64 AVD with `-read-only -no-snapshot -no-window -no-audio -gpu swiftshader -accel off` on dedicated port 5580
- The first bounded attempt reached ADB device state but did not set sys.boot_completed within 185.5 seconds. It was shut down normally (emulator exit 0). This does not establish that software emulation is impossible
- Started one longer supervised attempt with a 600-second boot limit, then automatic APK installation and launcher startup checks if boot succeeds. This is still pending; inspect `android/temp/linux-software-smoke.json` and its process handle before claiming results. Log: `linux-software-smoke.log`; initial emulator PID 152155, exec session 42965
- The supervisor terminates its emulator process group on success, timeout, or exception. This attempt uses fixed reusable logs and the existing read-only AVD; no extra system image or AVD copy was created. Free space remained about 13 GiB after the first attempt

### Cmakelang dependency pins

- Pinned the installed cmakelang utility set to cmakelang 0.6.13, PyYAML 6.0.3, and six 1.17.0, matching the real verified installation. Pip now uses explicit versions and --no-deps rather than resolving floating transitive packages
- Cached-install admission checks all three package versions plus executable/module startup; version drift enters the existing rollback-protected repair path
- Deterministic installer fixtures verify explicit pins and disabled dependency resolution. A fresh real installation and repeat run in a path containing spaces passed; the existing managed installation also passed validation without mutation
- Scoped quality checks and dependency-install master regression passed (`report_20260926_182250.md`, five seconds, no skips)
- The longer software emulator smoke probe remains active (session 42965, PID 152155); at approximately four minutes its status still had no boot completion. Continue polling the existing supervisor rather than starting a replacement

### Remaining archive installer temporary cleanup

- Soundfont replacement now stages on the destination filesystem, verifies the shared SHA-256 policy, and atomically replaces the asset only after validation. Failed downloads/checksums/publication and TERM preserve the existing asset and remove temporary files. Linked destinations are rejected
- Windows unar and DOSBox installers now use the shared owned workspace and directory publication/rollback helper instead of extracting/copying directly into their final directories. Their downloads/extraction trees are covered by exit/signal cleanup
- Expanded archive integration fixtures cover low space, transport failure, corrupt archives, publication failure, termination, successful installation, and repeated use for these installers. Windows packages are synthetic and are not executed on Linux; this is not Windows-host execution evidence
- Existing bundled soundfont passed its real hash check without modification. Scoped formatting/lint passed; master dependency-install test passed in six seconds (`report_20260926_182642.md`)
- Software emulator probe remains active in session 42965 (PID 152155). At 8m24s ADB, zygote, and boot animation were running, but boot completion was absent. A bounded 80-line diagnostic log is in `linux-software-boot-diagnostics.log`; it shows framework initialization and rendering activity, not proof of a fatal boot failure. The supervisor's ten-minute boot deadline and cleanup are still pending

### Software emulator feasibility result

The longer supervised software-only API 34 attempt ended after 600.8 seconds without sys.boot_completed. APK installation and launcher startup were not attempted. The supervisor terminated the emulator process group; the emulator returned exit 0. Verified that ADB reports no devices and no QEMU/netsim processes from the probe remain. Approximately 13 GiB remained free.

The host acceleration check still exits 3 with `KVM requires a CPU that supports vmx or svm`; `/dev/kvm` is unavailable. The two bounded probes show ADB/zygote/rendering activity but no completed boot in three and ten minutes respectively. This supports treating device execution as impracticable under this host's current configuration, not claiming software emulation can never work. Device parity remains unverified and requires an accelerated host or an attached device. No emulator-dependent test is counted as passing.

Evidence: `android/temp/linux-software-smoke.json`, `linux-software-smoke.log`, and `linux-software-boot-diagnostics.log`. Session 42965 is terminal. No probe remains pending; retain the existing host test/build work and proceed with the remaining installer recovery, runtime, and test inventory work.

### Native guidebot wrapper portability and coverage audit

- Confirmed test_android_sdk_lifecycle.jsonc exercises Android rendering and suspend/resume; it correctly remains device-dependent
- Inspected all 23 failures from the sampled route sweep: 22 reported mission/base-asset resolution blockers; original navigation instead failed on a hard-coded .exe path. Asset resolution errors still require case/path review before blanket missing-fixture classification
- Ported original/live navigation wrappers to shared host build and executable resolution. Both original-routing levels (1 and 11) passed twice; both available Counterstrike live-escort cases passed twice. Maximum Hostages remains explicitly skipped because its ZIP is absent
- Each wrapper now retains bounded, uniquely named, locked diagnostic generations and removes copied mission/player directories in finally. The route owner propagates explicit exit-2 skip markers without hiding failures or counting incomplete cases as passing
- Master filtered navigation report: report_20260926_183442.md, zero failures, owner marked skipped for the unavailable Maximum case; nested original case passed. Fixed aggregate log: android/temp/linux-guidebot-navigation.log
- Found and fixed a false missing-data skip in the timeout policy wrapper: retail CD files are uppercase on this Linux filesystem, which the native engine accepts. The real engine now verifies the one-second, 61-frame timeout. Master report_20260926_183513.md passed in two seconds without skips, including the incremental D2 build
- Scoped quality checks and git diff --check passed. New diagnostic generations contain no staged player/mission directories. All test and formatter processes from this tranche are terminal. outstanding_bugs.md remains unchanged

### Existing retail data discovery and native metadata tests

- Corrected the earlier conclusion that pinned D1 data was unavailable: the hashes match extracted Anniversary/Definitive Collection CD and GOG data already under game_data. Only the old mixed staging directory was absent
- Added shared candidate discovery below game_data, retaining the existing staging preference and exact multi-file hash admission. Host metadata regeneration now uses these candidates without copying datasets. Integration verifies discovery of uppercase retail filenames in paths containing spaces and rejects wrong hashes
- Ported metadata header and Vertigo worker wrappers to shared host executable/build resolution; catalogued both as host-only tests. Both now use unique locked retained scratch locations and finally remove their temporary payloads, including startup failure paths
- The real D1/D2 worker malformed-header cases pass. The Vertigo persistent worker fixture passes all five requests (valid data, missing HAM, unrelated HOG, repeated mount cleanup)
- Most significantly, the existing full Vertigo checkpoint regression now passes all 23 levels within the normal deadline using the actual host metadata runner. The master suite passed both Vertigo tests without skips or failures in 11 seconds: report_20260926_183951.md; fixed log android/temp/linux-vertigo-master.log
- Scoped formatting/lint and diff checks passed. The protected outstanding_bugs.md remains unchanged. About 13 GiB remains free
- Follow up: use the shared discovery in remaining metadata/flyout wrappers and investigate mixed D1-in-D2 staging separately. Missing staging is not evidence that the retail source assets are absent

### Flyout and provenance host wrappers

- Ported flyout metadata tests to shared host builds/executable resolution and hash-verified data discovery. Retail OTHER-H.MVL is selected case-insensitively from the admitted D2 directory, with an explicit override for other locations
- All seven real D1/D2 cases passed: in-engine timing, malformed exterior, missing level, retail movie duration, missing movie, 90-second synthetic movie, and malformed movie. Master report_20260926_184259.md passed in three seconds without skips; both incremental host builds completed
- Flyout and provenance wrappers use unique locked scratch generations with startup retention and finally removal. Verified flyout cleanup after both success and an intentional missing-movie-path failure; no fixture generations or metadata workers remain
- Provenance now uses shared Java/Gradle/native host tools and admitted game-data directories. Its required ROGUE.zip and chron10b.zip are absent, so standalone and master runs explicitly skip before creating a workspace. Full provenance assertions remain unverified; report_20260926_184314.md records the concrete missing archives
- Both tests are now correctly catalogued as host tests. Scoped formatting/lint and diff checks passed, and outstanding_bugs.md remains unchanged. All test and formatter processes from this tranche are terminal

### Linux AddressSanitizer entry points (validation running)

- Added --sanitizer none|address to run-linux-build.sh, isolated buildd1-asan/buildd2-asan directories and freshness stamps, explicit CMake instrumentation and BUILD_TESTING, plus a dependency-volume-style reserve/headroom check before instrumented builds
- Shared Invoke-RegressionHostBuild accepts sanitizer/concurrency options for both hosts. Replay freshness rebuilds and run_engine_sanitizers.ps1 now use that dispatch. Sanitizer runner resolves native executable/CTest names, records extensionless Linux binaries, and holds a producer lock on reports; final diagnostic scanning includes LeakSanitizer
- Existing build-guard regression passed. Invalid sanitizer and simulated insufficient-space requests were rejected before creating the D2 build tree. These checks do not prove an instrumented build/test pass
- Started the real runner with -Category EngineTests -Game both -MaxParallel 2. It is STILL ACTIVE in exec session 97709, initial Bash PID 171820. Report root android/temp/engine_sanitizers/20260926_184528. Poll this handle; do not restart or delete build trees merely because an observation expires. It builds D1 then D2, then runs both complete CTest suites
- At the latest observation D1 compilation had reached about 360/802 actions, with approximately 13 GiB free. No build/test pass is claimed yet. Native compiler output is in the report build-d1.log; the fixed outer runner log is android/temp/linux-sanitizers.log
- Ported the config-reader fixture to host executable/runtime-library resolution. It removes copied binaries/libraries in finally and cancels its background writer even after setup errors. The concurrent-rewrite and private-settings variants passed with the existing normal Linux engine; unterminated-config validation also completed successfully. These are NOT sanitizer execution results
- Documentation now describes Windows/Linux entry points. Scoped formatting/lint is PENDING until the active build finishes, to avoid rewriting the running Bash script. git diff --check passes; outstanding_bugs.md is unchanged
- Next: finish observing session 97709, diagnose any genuine compile/sanitizer findings, run ConfigBounds with the resulting instrumented D2 engine, then scoped quality checks and incremental verification as needed

### Tooling smoke profile and CI drift checks

- Added run_tooling_smoke.ps1 with eight shared Windows/Linux checks and the Linux synthetic installer owner. Each child has a 180-second deadline; failures, timeouts, and unexpected skips fail the profile. It records incremental JSON and per-test logs in unique locked report generations with retention
- All nine checks passed locally, including cleanup, managed dependency retirement, process lifecycle/output, platform selection, pinned data discovery, replay comparison, and installer rollback fixtures. Report: android/temp/tooling_smoke/run_5a4b9b2a3ace4024be30b1c932603ad2/summary.json. The local host identifies as Ubuntu 26.04.1 LTS
- Added .github/workflows/android-tooling.yml for Ubuntu 24.04 and Windows Server 2022, relevant push/PR paths and manual dispatch, read-only repository permission, cancellation of superseded jobs, and three-day diagnostic artifact retention. Checkout v4.2.2 and upload-artifact v4.6.2 commit pins were verified against their official GitHub release tags
- Added TOOLING_SMOKE.md with local prerequisites, coverage boundaries, and retention. This is a tooling gate, not evidence of remote Windows execution or full build/device parity. No workflow was published or dispatched from this workspace
- Workflow YAML parsed successfully and scoped quality checks for the new runner/workflow/docs passed. Full quality checks for the previous sanitizer changes remain pending until the running Bash build is terminal
- Existing sanitizer run remains LIVE in session 97709 (D1 Bash PID 171820 at 10m29s, approximately 721/802 actions). About 12 GiB remains free. Poll the same run and inspect its report root android/temp/engine_sanitizers/20260926_184528; do not launch a duplicate
- No tooling-smoke process remains running. outstanding_bugs.md is unchanged, and git diff --check passes

### Sanitizer build observation: D1 complete, D2 active

The existing sanitizer runner completed the full D1 instrumented build successfully in 714 seconds (802 build actions). Its summary records build-d1 exit 0. Verified that the built test_input_demo_controls links libasan.so.8 and ran that real instrumented binary with the runner's ASAN_OPTIONS; it returned PASS. This validates runtime startup, not the full D1 sanitizer suite, which the runner schedules after both builds.

Session 97709 remains LIVE. D1 Bash PID 171820 is terminal; the same runner is now building D2 through Bash PID 182539. At the latest check D2 was approximately 282/1557 actions, 2m27s into its build. D1's fixed instrumented tree occupies 1.4 GiB and D2's partial tree about 324 MiB; approximately 12 GiB remains free. No duplicate build was started.

Full instrumented CTest suites and ConfigBounds are still pending. Existing source warnings (const qualifiers and C++ string-literal conversions) are visible in the build log and have not been suppressed. Full scoped quality checks for sanitizer edits remain pending until the Bash build is terminal. No new source edits were made during this observation; git diff --check passes and outstanding_bugs.md remains untouched.

### Formatter cache and publication version admission

- shfmt, ShellCheck, ktlint, and clang-format installers now validate the current host's cached executable before reuse and validate staged version output before publication. Shared parsing retains prerelease suffixes, so an unpinned prerelease does not satisfy an exact stable version
- shfmt/ShellCheck/ktlint require their configured full version. clang-format preserves its existing major-version pin contract (20); this is version admission, not archive checksum verification
- Extended archive integration exercises a wrong-version cache, failed replacement download, rejected prerelease download with the previous bytes preserved, successful repair, and temporary workspace cleanup for all four tools
- Real installed formatters passed admission without downloads: ktlint 1.8.0, shfmt 3.14.1, ShellCheck 0.11.0, and clang-format 20.1.0 under the configured major-20 pin
- Scoped formatter-installer quality checks passed. Master test_dependency_install passed in nine seconds without skips: report_20260926_190405.md; fixed log android/temp/linux-dependency-version-master.log
- Sanitizer session 97709 remains LIVE, building D2 through PID 182539. Latest observation: roughly 582/1557 actions after 7m19s; approximately 11 GiB remains free. Do not restart the existing runner. D1 build is complete; both full instrumented CTest suites are still pending
- The installer test process is terminal. Full quality checks for the earlier sanitizer changes remain pending until the active Bash build is terminal. outstanding_bugs.md remains untouched

### Linux child supervision and sanitizer findings

- Added a Linux Python supervisor to the shared headless process pool. It watches the requesting PowerShell process through a pidfd, starts an owned process group, adopts orphaned descendants as a subreaper, and kills/reaps remaining descendants on worker or owner exit. Detached sessions are included through adopted-child discovery
- Integration tests exercise actual pool launches, preserved output and nonzero status, detached grandchildren, and SIGKILL of the PowerShell owner. Windows retains the existing Job Object path. Direct launches outside the pool remain an audit item
- Added the pool test to tooling smoke and documented Linux Python 3.9+/kernel 5.3+ requirements. The first full profile exposed a subreaper zombie lifecycle problem in the inherited-output fixture: the supervisor waited for the main worker while that worker waited for an adopted zombie. Fixed it by reaping exited adopted children while the main worker is active, without consuming the main worker's exit status. The rerun has passed that previously timed-out case; full profile completion is pending
- Full scoped mixed-language formatting/lint completed successfully after the original sanitizer build terminated. Fixed log: android/temp/linux-full-quality.log
- Original sanitizer session 97709 is terminal, exit 1. Both instrumented builds succeeded (D1 714 seconds, D2 1333 seconds); CTest passed 53/54 D1 and 62/63 D2. Both failures are the homing fixture calling track_track_goal with target 0 while its stored track_goal remains -1. Corrected the fixture to store the acquired player target before the cloaked-player assertion, matching the engine caller's consistent target input. No engine behavior or assertions were weakened
- Targeted instrumented test_upstream_compat rebuilds for both games are running; follow with full instrumented tests and ConfigBounds. Earlier results do not prove the correction yet. Protected outstanding_bugs.md remains untouched

- Corrected supervisor validation completed: all ten tooling smoke tests pass, no skips, exit 0. Report: android/temp/tooling_smoke/run_8dda35c23ecb4117a0189b914ef7300c/summary.json. Fixed aggregate log: android/temp/linux-tooling-smoke.log. The inherited-output test now completes, and the forced-owner-kill/detached-descendant integrations still pass

- Both corrected instrumented native suites now pass: 54/54 D1 in 22 seconds and 63/63 D2 in 40 seconds, report android/temp/engine_sanitizers/20260926_192321. The same run reached all three ConfigBounds variants and detected a real mission-HAM model reload leak (791044 bytes in 124 allocations), rather than a config-reader bounds error. Fixture copies were removed in finally despite the failures
- The model cleanup already existed behind **ANDROID** in d2/main/piggy.c. Removed only that guard so host HAM reloads release the previous polygon models too. D1 has no corresponding HAM reload path. This preserves Android behavior and extends its existing cleanup to desktop/headless builds; sanitizer detection remains enabled
- Scoped quality for the two-line core change passed. D2 EngineTests plus ConfigBounds are rebuilding/running to validate it; fixed log android/temp/linux-sanitizers-d2-reload.log. Do not claim leak validation before this run completes

- Correction to the preceding reload hypothesis: extending the Android guard did not change the ConfigBounds leak report, although all 63 instrumented D2 native tests still passed. Reverted that experiment completely; d2/main/piggy.c has no diff. The mission asset service already frees the prior models, and packed polymodel pointers complicate LeakSanitizer reachability, so the allocation stack alone did not establish a reload leak
- Found a concrete lifecycle omission in route_confirmation_headless_main.cpp: game data was initialized but gamedata_close was never called (normal d2/main/inferno.c does call it). Registered that existing cleanup immediately after successful initialization, covering normal and early exits. Scoped quality passed. ConfigBounds is rebuilding/running against this lifecycle fix; fixed log android/temp/linux-sanitizers-config.log

- Final headless lifecycle validation: gamedata_close alone exposed an audio callback shutdown race. The runner correctly rejected sanitizer diagnostics despite native exit 0. The final callback closes audio first with digi_close, then releases game data; the engine core remains unchanged
- All three instrumented ConfigBounds scenarios now pass with exit 0 and no AddressSanitizer/LeakSanitizer findings: unterminated config, concurrent rewrite, and isolated user directory. Report android/temp/engine_sanitizers/20260926_192905/summary.json; fixed log android/temp/linux-sanitizers-config.log. Copied executables/runtime payloads are absent after completion
- Native suites previously passed 54/54 D1 and 63/63 D2 after the homing fixture correction. CTest LastTest.log under each fixed instrumented build tree retains those results; automatic retention may prune earlier diagnostic report generations, as designed
- Full scoped quality passed before the lifecycle edit, and scoped quality for the final lifecycle edit passed afterward. git diff --check passes. All sessions started in this tranche are terminal, including original sanitizer session 97709. Approximately 9.3 GiB remains free. outstanding_bugs.md remains unchanged
- Remaining work includes installer concurrency/SIGKILL recovery, bounded extraction Python on Linux, remaining host wrappers and replay differences, Windows execution evidence and broader CI. Direct child launches outside the shared pool require selective lifetime integration; persistent services such as the ADB server must not be accidentally treated as disposable workers

### Linux dependency transaction recovery

- Added a shared Linux flock lock before cache admission in JDK, NDK, CMake, SDK command-line tools, shfmt, ShellCheck, clang-format, ktlint, and DOSBox shell installers. Lock scope is the destination parent, with a bounded 60-second default wait (override 0..3600 seconds). Linux host prerequisites now include flock/util-linux
- Participating installers reuse one owned .dxx-install-state/work directory per parent for downloads, extraction, and publication backup. Recovery runs under the lock before cache checks, including when a different cached sibling tool is invoked. It restores the previous directory if publication was interrupted before the new directory appeared, or removes the obsolete backup after completed publication. Normal failures preserve prior installs and remove staging
- Kernel lock descriptors remain inherited by download/extraction children, preventing recovery while those children survive a SIGKILL of the Bash owner. Stable lock inodes are not removed. Linked transaction state and unrecognized state are rejected instead of traversed or deleted
- Synthetic integration passes SIGKILL during download, after backup rename, and after new-directory publication; cached sibling recovery; competing installers; an orphaned download retaining the lock; and rejection/preservation of linked transaction state. Existing JDK/SDK/NDK/CMake/archive/cmakelang failure and rollback tests also pass
- All ten tooling smoke checks pass: android/temp/tooling_smoke/run_1a02905dcede41c2b06a860348f72637/summary.json. Fixed log android/temp/linux-tooling-smoke.log. Scoped mixed-language quality and git diff --check pass; fixed quality log android/temp/linux-installer-transactions-quality.log
- Real installer entry points succeeded for all nine participating families. CMake 3.31.6 and DOSBox-X 2026.01.02 were downloaded and validated; the other checked tools reused their installed caches. The DOSBox check validates its pinned Windows package, not native Linux execution. Fixed log android/temp/linux-real-dependency-reuse.log. No work/download/backup remains under the two real transaction roots; each retains only a 15-byte format marker and an empty lock
- Registered the newly installed managed CMake and ran dependency retirement. Configured versions remain protected, as do unowned legacy NDK r27d and shfmt 3.13.1; no unrelated installation was deleted. Fixed log android/temp/linux-dependency-retention.log. Approximately 9.0 GiB remains free
- Remaining transaction coverage: Windows locking/recovery, in-place cmakelang, single-file soundfont/bootstrap downloads, and legacy unowned staging. DOSBox version retirement is not yet part of the managed-family registry. These limits are explicit in CLEANUP.md; they do not narrow the overall goal
- Normal Linux D1/D2 builds are running through the public build entry point to validate the newly installed managed CMake. Follow with native CTest suites. outstanding_bugs.md remains unchanged

- Public normal D1/D2 builds completed successfully in session 46575 using their default system CMake (/usr/bin/cmake), not the newly downloaded managed CMake. Fixed log android/temp/linux-native-after-transactions.log. Existing string-literal warnings remain visible; no new core edits were made in this tranche
- Explicit managed-CMake validation is now LIVE in session 62041: bash run-linux-build.sh --target both --jobs 2 --cmake /home/user/local/cmake-3.31.6/bin/cmake. It reconfigured and is rebuilding affected targets in the existing fixed trees; latest observation D1 133/165 actions. Fixed log android/temp/linux-native-managed-cmake.log. Do not start another build while it runs. Run both native CTest suites after it finishes
- Made the orphan-download test PID handoff atomic to avoid a readiness-file race. Scoped test-file quality and the full archive integration rerun pass; fixed logs android/temp/linux-installer-test-quality.log and android/temp/linux-installer-transactions-final.log. All installer/smoke/formatter sessions are terminal; only managed-CMake session 62041 remains running. The protected bug list is unchanged

### Remaining host route wrappers and managed CMake validation

- Managed-CMake session 62041 completed the D1 build and is still building D2. D1 CTest with /home/user/local/cmake-3.31.6/bin/ctest passes all 54 cases in 4.88 seconds; fixed log android/temp/linux-ctest-d1-managed.log. No duplicate build was started
- Ported test_guidebot_saved_world.ps1 to shared Windows/Linux build/executable resolution and hash-verified retail data discovery. Its processes now use the shared supervised pool with a 300-second deadline. Unique retained run directories hold a producer lock; finally removes copied checkpoint/player payloads while preserving JSON/log diagnostics. Memory streams are disposed through the failure path too
- The real saved-world test passes on Linux with the already built engine: restored pose, blue key, difficulty, repeated-result identity, mismatched-level rejection, and unchanged input save. Report android/temp/test_guidebot_saved_world/run_1bc9e2dc550c4d408a7a0fd47c015d5c contains only logs, result JSON, and the empty lock after completion. Fixed log android/temp/linux-saved-world.log
- Ported test_counterstrike_level2_trigger21_route.ps1 to shared executable lookup and admitted retail data discovery. It uses the supervised pool, captures diagnostics, and locks its fixed output during generation/assertions. The real Counterstrike switch-access and key-progression assertions pass; fixed log android/temp/linux-counterstrike-route.log
- Both wrappers are now explicitly catalogued as host tests in run_all_tests.ps1. Scoped quality checks for both wrappers and the catalog pass. Master-runner validation and D2 native CTest follow after the current build completes. The protected bug list remains unchanged

- Managed-CMake session 62041 is now terminal, exit 0. Both D1/D2 builds succeeded with the explicitly selected pinned CMake 3.31.6. All 54 D1 and 63 D2 native tests pass with its CTest (4.88 and 32.72 seconds); fixed logs android/temp/linux-native-managed-cmake.log, linux-ctest-d1-managed.log, and linux-ctest-d2-managed.log
- Master runner confirms both new host classifications: test_guidebot_saved_world passed in three seconds (report_20260926_195200.md), and test_counterstrike_level2_trigger21_route passed in six seconds (report_20260926_195232.md). Both reports have zero failures, timeouts, skips, or unrun tests. The managed CMake directory was prepended to this validation process's PATH to avoid switching tool versions during the incremental saved-world build
- Normalized explicit executable/data arguments in the Counterstrike wrapper before its supervised process changes working directory. Verified from android/ using relative executable/data/output paths, including spaces in the retail data directory; assertions pass. Removed this extra probe's JSON/native log/lock afterward, retaining android/temp/linux-counterstrike-explicit.log as bounded evidence
- Saved-world copied checkpoint/player directories are removed after completion. Counterstrike's default fixed report occupies approximately 504 KiB plus a small log and an empty lock. Scoped quality for the final path correction passes, and git diff --check is clean. All build/test/formatter sessions from this tranche are terminal. Approximately 9.0 GiB remains free; outstanding_bugs.md remains untouched

### GitHub Linux tooling failure (run 36290040730)

- The Ubuntu 24.04 job failed only test_managed_dependencies; Windows 2022 passed all nine checks. Uploaded Linux diagnostics show retirement was conservatively blocked by an unreadable tool process (PID 1011), rather than an installer/build failure
- Kept production cleanup protections unchanged. The test snapshots pre-existing unreadable background processes and excludes those exact PID/name/command-line matches from its fixture inventory. Newly launched fixture children still use real process inspection. The forced-termination cleanup child receives the same isolation
- Added an explicit unreadable-tool case asserting both the protection reason and survival of the retired version. Existing real active-process, shared-checkout, cache, lock, link, crash-recovery and deletion checks remain
- Reproduced the original CI failure locally with a same-user Python process made non-inspectable using prctl(PR_SET_DUMPABLE, 0). Original test exits 1 with the same cannot-inspect reason; fixed log android/temp/ci-managed-original-repro.log. The reproduction process and temporary original-test script are removed after validation
- Scoped PowerShell formatting/lint passes (android/temp/ci-linux-fix-quality.log). Final full smoke validation runs with the non-inspectable Python process still alive. No native code changed; no native rebuild is needed for this test-only correction
- Full Linux smoke suite passes all ten checks with the unreadable background process present: android/temp/tooling_smoke/run_87d15579f86e496e872d7cbee4ed55bc/summary.json, fixed log android/temp/ci-linux-smoke.log. Reproduction process terminated successfully; temporary probe script removed. Changes remain local; the GitHub workflow has not been rerun with this fix. outstanding_bugs.md remains unchanged

### Remaining dependency retirement coverage

- Previous goal turn made progress: reproduced and corrected the Linux CI managed-cleanup fixture failure, preserving production fail-closed protection; ten Linux tooling checks passed
- Current work adds the omitted DOSBox-X managed installation family using its explicit DOSBOX_DIR_NAME rather than reconstructing a directory from DOSBOX_VERSION. The installed executable is at the package root after extraction. Unreadable DOSBox processes receive the same conservative protection as other tools
- Found a safety gap in retained build references: normal D1/D2 caches were inspected but their fixed sanitizer build trees were omitted. Include both sanitizer trees before retiring any managed tool
- Extend the existing cross-platform cleanup integration to cover all four host build caches, explicit DOSBox directory names, two-checkout pins, incomplete replacements, preview, deletion, and unmanaged/current preservation. Run scoped quality, full tooling smoke, then register the real previously verified DOSBox package and inspect cleanup results
- All ten local tooling smoke checks pass with the expanded retention cases: android/temp/tooling_smoke/run_01d2884d31984c3aa130495284921482/summary.json, fixed log linux-retention-coverage-smoke.log. Final directory-name validation uses the same safe suffix grammar as retirement records
- Real get_dosbox.sh cache admission verifies the installed executable hash without downloading; registration/apply now includes /home/user/local/dosbox-x-2026.01.02. All configured installations are protected, and legacy unowned NDK/shfmt remain unmanaged and untouched. Evidence: android/temp/linux-dosbox-cache-verification.log and linux-retention-coverage-real.json
- User committed the prior CI fixture correction as 6df32f2c during this work. GitHub run 36291204049 completed successfully for both Ubuntu 24.04 and Windows 2022. That remote evidence covers the fixture correction, not these new uncommitted retention changes

### Linux metadata benchmark runner

- Prior turn made progress with DOSBox retirement and sanitizer-cache protection; the final targeted cleanup integration and scoped quality passed
- Port the metadata benchmark to shared Windows/Linux build and executable helpers, shared hash-admitted retail data discovery, and supervised process execution. Remove hardcoded Visual Studio paths, Windows-only process launch options, and incorrect hardcoded x86 build identity
- Add explicit level IDs for running available retail cases while keeping the full manifest unchanged. Filtered runs must never update full-suite baseline history, and missing full-suite archives remain an explicit skip (exit 2) or failure with RequireAssets
- Bound unique diagnostic generations using retention and producer locks, serialize current-summary/history writes, and remove extracted mission payloads in finally on success or failure. Verify with real D1/D2 levels and preserve digest checks; missing archive coverage remains outstanding
- Real retail diagnostic execution completed all seven selected levels (one warmup plus three measured repeats each), with identical metadata hashes within every level and successful live GuideBot benchmark. This does NOT pass expected manifest digest validation: the first normal run failed D1 L1 before diagnostic mode was used
- Traced a concrete host difference: manifest hashes were recorded with CRLF text output. Removing only the later-added flyout field and converting the Linux output to CRLF reproduces the existing hashes exactly for D1 L1, D2 L1 and D2 secret L3. D1 L10/L21 and D2 L2/L12 have additional differences and remain unaudited. No expected hashes or native output fields were changed
- Canonicalize only metadata digest line endings to the existing UTF-8 CRLF convention on both platforms, without reserializing JSON or suppressing fields. The policy self-test checks line-ending equivalence and content sensitivity. Reports identify the digest convention, selected IDs, full-suite coverage, and whether expected digest validation was enabled
- Verified policy-only success, unknown-ID rejection, filtered AcceptBaseline rejection, absent-archive skip exit 2, and RequireAssets failure exit 1. Logs: android/temp/linux-benchmark-policy.log. Manifest/history and protected bug list remain unchanged
- Shared public build validation is running in session 11692 using managed CMake 3.31.6 and existing fixed trees; D1 reached 457/476 actions at last observation. Wait for this session; do not start another build. It will run the seven-level diagnostic again after both builds. Scoped quality passed after the latest digest handling edits
- D1 public build completed; all 54 D1 CTest cases pass (4.50 seconds), fixed log android/temp/linux-benchmark-build-ctest-d1.log. Its test session 97629 is terminal
- Build/diagnostic session 11692 remains LIVE, currently D2 176/1204 actions. Do not restart it. After completion, run D2 CTest, then rerun the selected seven retail benchmark levels with SkipBuild to validate the final CRLF digest/report edits (the already-running PowerShell process parsed the earlier revision). Keep normal digest failure visible and use SkipDigestValidation only as an explicitly labeled diagnostic; do not change manifest hashes without auditing content changes
- Final scoped quality session 23169 is terminal and passes; policy self-test passes after canonical digest changes. All other test/formatter sessions are terminal. Manifest/history/outstanding_bugs.md remain unchanged, git diff --check passes, free space remains approximately 9.0 GiB

### In-place Linux dependency transaction recovery

- Previous turn made progress on benchmark portability and verified D1 native build/tests. Resumed existing session 11692; D2 remains running, with no duplicate build started
- While native compilation runs, extend the existing shared Linux transaction journal for final-path Python environments. Record in-place mode before backup, record installation start before creating the destination, and commit only after validation. Recovery removes uncommitted partial environments and restores prior versions; committed versions survive while old backup/staging is removed
- Cover interruption during recovery itself: remove the installation-start marker before moving the backup back so a subsequent recovery cannot delete the restored good tree
- Integrate get_cmake_format.sh before cached-install admission. Windows ordinary rollback remains unchanged. Test SIGKILL after backup, during pip, after commit, during backup restoration, and during first installation, alongside existing normal failure/validation/TERM/cache cases
- Initial expanded cmakelang integration passes; fixed log android/temp/linux-cmakelang-recovery.log. Run scoped mixed-language quality and the full installer/tooling smoke suite before accepting shared helper changes. Then validate real cached cmakelang admission and absence of transaction workspace
- Added an atomic temporary v2 journal-format marker while in-place replacement is pending. Older archive-only helpers reject that state instead of treating a partial destination as committed. Current recovery restores v1 after work is fully reconciled; tests assert both pending and completed markers
- Final full tooling smoke passes all ten tests, including archive installers and all in-place crash cases: android/temp/tooling_smoke/run_88a4ad57f17b440d9601edec82c0cf78/summary.json, fixed log linux-cmakelang-tooling-smoke.log. Scoped mixed-language quality passes (linux-cmakelang-recovery-quality.log)
- Real cached cmakelang 0.6.13 passes package/executable admission with the shared lock and no download. /home/user/local/.dxx-install-state contains only format (15 bytes, v1) and empty lock; no work, backup, or pending format remains. Fixed log linux-cmakelang-cache.log

### Completed native build and benchmark validation

- Session 11692 completed successfully: both public native builds and its seven-level diagnostic benchmark. No build remains running. D2 CTest passes all 63 tests (34.95 seconds), complementing the 54 D1 passes; fixed log android/temp/linux-benchmark-build-ctest-d2.log
- Reran all seven retail levels with the final canonical-digest/report implementation, one warmup plus three measured repeats per level. Repeat determinism passes; the snapshot explicitly records complete_suite=false, expected_digest_validation=false and metadata_digest_format=utf8-crlf-sha256. Fixed log linux-benchmark-final-diagnostic.log and summary android/temp/level_metadata_analysis_filtered_current.json
- Separately verified that normal expected-digest validation still rejects the changed D1 L1 metadata (exit 1), rather than hiding the stale/content differences. Fixed log linux-benchmark-final-digest-check.log. No manifest expected hashes or benchmark history were modified
- Master runner initially classified the missing-archive exit 2 as FAIL because its protocol requires an anchored RESULT: SKIP marker. Corrected both benchmark asset-skip messages; master rerun now reports one SKIP, zero failures/timeouts, exit 0 (temp/test_reports/report_20260926_204338.md; fixed log linux-benchmark-master.log). Full custom-archive coverage remains unavailable
- Final scoped benchmark quality passes; git diff --check passes. All build, test and formatter sessions from this tranche are terminal. Approximately 9.0 GiB remains free. outstanding_bugs.md remains untouched
- Remaining goal work includes bounded extraction Python on Linux, remaining asset-dependent wrappers, replay/benchmark content differences, Windows installer crash recovery, single-file/bootstrap recovery, SDK package retention, and broader build/device CI. No completion claim is made for these gaps

### Pinned bounded-extraction Python on Linux

- Previous turn made progress: completed native builds/117 native tests, cmakelang recovery, final tooling smoke and benchmark runner validation. No prior sessions remained active at turn start
- Preserve the existing exact Python 3.12.8 policy, full repository-runtime tree admission, explicit digest overrides and prohibition on PATH fallback. Add a native Linux x86-64 package alongside the existing Windows embedded runtime, without claiming macOS or ARM support
- Upstream source: https://github.com/astral-sh/python-build-standalone/releases/tag/20241219. Selected cpython-3.12.8+20241219-x86_64-unknown-linux-gnu-install_only_stripped.tar.gz (21,296,625 bytes), published SHA-256 698e53b264a9bcd35cfa15cd680c4d78b0878fa529838844b5ffd0cd661d6bc2. Independently downloaded/checked the archive and computed the existing repository tree digest 808a5964f7b4fdf86c86e2870a64e5dcc563a73d165671c38d3b2b1bf004a2a1 (78 MiB extracted). Inspection staging was automatically removed by shared transaction cleanup
- New shell installer dispatches Windows to its existing verified oracle installer and uses shared Linux lock/staging/publication/recovery plus archive/tree validation for Linux. Add it to bootstrap and register Linux versions for managed retirement
- Resolve the Linux interpreter by fixed native path only after tree verification. Disable bytecode writes during identity probing and supervision to preserve the pinned tree
- Next: port runtime policy integration tests to both hosts, test hostile PATH, explicit admission, wrong version/digests, unsupported-host policy and spaced paths; run bounded extraction/supervisor tests and scoped quality, then check reuse and storage cleanup
- Runtime integration now covers hostile PATH entries on both OS families, full-tree tampering in an isolated checkout, explicit-path SHA/version rejection, unsupported-host policy, spaced paths and the resource-limited supervisor suite. Repository Python bytecode is disabled in probes/supervision and Python child fixtures so the admitted tree remains unchanged
- All 23 supervisor test cases ran on Linux: 19 passed, four platform/capability skips. All four available verified demo installer ZIP payloads passed bounded extraction. Fixed logs linux-bounded-supervisor-tests.log and linux-bounded-extraction-tests.log
- Extended local tooling smoke passes all 12 checks: android/temp/tooling_smoke/run_ac48574e7eca42759a788171e2ffe017/summary.json. New IncludeBoundedRuntime option adds the two bounded checks; CI now installs the verified runtime and enables this option on both runner OSes. Workflow YAML parses; remote execution of this extension is still pending a user push
- Full get_all.sh --skip-host-prereqs --skip-avd --no-prompt completed all 14 selected steps with no duplicate AVD. Runtime cache reuse succeeds and bootstrap registered python-bounded-3.12.8-20241219-linux-x64 in the ownership registry. Fixed log linux-bootstrap-bounded-python.log
- Master HostOnly filter test_bounded_* passes both entries (1 and 14 seconds), zero failures/timeouts/skips: temp/test_reports/report_20260926_205913.md, fixed log linux-bounded-python-master.log. Its Python sub-suite retains the four documented platform/capability skips
- Download/staging and copied runtime fixtures are gone. The real runtime occupies about 78 MiB, retains only its original three pyc files, and passes tree admission after all tests. The shared transaction state retains only format and empty lock. Approximately 8.9 GiB remains free
- Final mixed-language scoped quality passes (linux-bounded-python-final-quality.log), git diff --check passes, and outstanding_bugs.md remains unchanged. Every installer, smoke, master, supervisor, and formatter session in this tranche is terminal

### Mixed retail data for native route tests

- Previous goal turn completed pinned Linux extraction-runtime provisioning and verified all 12 extended smoke checks, bootstrap reuse, master classification and cleanup
- The GuideBot batch runner still assumes a fixed D2 CD path and a preassembled game_data_to_copy_to_emulator/temp directory for D1-in-D2. Replace implicit paths with existing hash-admitted discovery; preserve explicit caller overrides
- Add a shared mixed-source staging helper that selects exact pinned files, reserves space for copies, normalizes filenames, verifies copied content and removes a partial stage on failure. Reject existing destinations instead of overwriting caller data
- Hold a producer lock for the entire GuideBot batch and remove its automatically staged base data in finally, preserving result JSON and mission staging used by existing test handoffs. Staging remains under retained run roots if the whole producer is killed
- Validate synthetic mixed-source and cleanup cases, existing runner discovery/infrastructure fixtures, then real FirstStrike physical route regressions with the existing native build. Do not change their game-state assertions

- Shared mixed-source staging integration passes: exact hashes, case normalization, copy independence, existing-destination protection and partial-stage cleanup. Existing runner discovery/sampling/infrastructure fixtures also pass
- Both FirstStrike L5 long-path and L21 live-object regressions passed directly and through the route-test owner. The owner now uses the shared process pool, retains its producer lock through execution/reporting and preserves existing skip/failure handling. Owner log: android/temp/linux-firststrike-owner.log
- Full FirstStrike and Counterstrike batch passed all 60 levels twice (120 engine executions), with every level status ok, no repeat differences and no infrastructure errors. Report: android/temp/guidebot_simulation_regression/20260926_210928/summary.json; fixed log linux-retail-route-batch.log. Retained reports/logs occupy approximately 40 MiB; automatic base-data copies are gone
- Injected an invalid engine into a real FirstStrike L1 run: runner returned exit 1, preserved the infrastructure error report and removed automatic base-data. Evidence: android/temp/linux-guidebot-stage-failure.log and guidebot_stage_failure/run_20260926_211500/summary.json
- Latest GitHub Actions status remains run 36291204049, successful on Ubuntu and Windows; it does not cover the newer uncommitted changes. No checked-in mission data or protected bug-list edits were made
- Final extended tooling smoke passes all 12 checks, including shared staging, installer interruption recovery and bounded runtime/extraction: android/temp/tooling_smoke/run_be3f4ab91a554eb8af3097a2dda0f9bc/summary.json; fixed log linux-mixed-data-tooling-smoke.log. Final scoped quality passes (linux-mixed-data-final-quality.log), git diff --check passes, and all sessions from this tranche are terminal

### Single-file soundfont recovery

- Previous goal turn made progress: mixed retail staging, real route coverage, shared owner supervision and all 12 tooling checks passed
- Reuse the Linux installer lock and fixed workspace for the soundfont, with recovery before cached-file admission. Keep the journal outside packaged assets and enforce same-filesystem publication. Windows retains its existing same-directory temporary-file path
- Extend the archive integration fixture with forced termination before and after soundfont publication, failed retry preservation, cached recovery and absence of recovery files in assets
- Full dependency installer suite passes, including JDK, archive-tool and cmakelang forced-termination recovery (android/temp/linux-soundfont-installer-suite.log). Final archive suite additionally passes the Windows-host-shim soundfont fallback checks; this is Linux execution evidence, not a new Windows CI result (linux-soundfont-final-archive.log)
- Real soundfont cache admission passes without a download or asset modification. The sibling .dxx-install-state retains only its 15-byte format and empty lock, with no work directory or packaged recovery files; its exact path is ignored in android/.gitignore
- Non-regular soundfont destinations are rejected and preserved. Scoped quality and git diff --check pass, every test session from this tranche is terminal, and outstanding_bugs.md is unchanged. Bootstrap PowerShell recovery, Windows transaction parity, SDK package retention and the other goal gaps remain open

### PowerShell bootstrap recovery

- Previous goal turn made progress on soundfont interruption recovery and verified all installer suites
- Replace random bootstrap download directories with the shared locked workspace, recovering before cached PowerShell admission. Stage and validate user-local tarball installations before shared rollback-capable publication; preserve package-manager ownership for deb/rpm installations
- Validate the exact installed version and reject unsupported bootstrap architectures explicitly. Add isolated installer fixtures for download/package failure, SIGKILL recovery, tarball validation and replacement boundaries without invoking real package managers
- Full installer integration passes with the new PowerShell fixture included (android/temp/linux-powershell-installer-suite.log). Coverage includes deb/rpm download/package failures, exact installed-version rejection, SIGKILL before/after package installation, wrong tarball version, publication failure, kills during download/backup/publication, cached recovery and link repair without redownload
- Scoped mixed-language quality passes (linux-powershell-quality.log), git diff --check passes, all test sessions are terminal, and protected outstanding_bugs.md is unchanged. No actual package manager or PowerShell replacement was run: this host reports 7.7.0-preview.1 while the configured bootstrap pin is 7.6.6
- Cleanup documentation distinguishes download/local-tree recovery from OS package database recovery. Version retirement for user-local PowerShell installations, Windows transaction parity, SDK package retention, remaining test/capability coverage and other documented gaps still need work

### User-local PowerShell version retirement

- Previous goal turn made progress: bootstrap recovery and isolated installer validation passed
- Register versioned PowerShell trees in shared managed cleanup, preserving checkout pins, replacement availability, active processes and unmanaged installations. Protect bootstrap command links outside the version directory, and fail conservatively when PowerShell processes cannot be inspected
- Extend the managed-cleanup integration with two-checkout pins, missing replacement, external Linux command link, preview and old-version deletion
- Initial managed integration passed. Extended smoke then caught an initialization typo in the added unreadable-PowerShell fixture; corrected it and rerunning that test. All 11 other extended smoke checks passed, including bootstrap installer recovery and bounded extraction (android/temp/tooling_smoke/run_b1ff66fe3ee94f76b89f70f23f8a6cb4/summary.json)
- Real dependency preview preserved all 11 configured installs and both unmanaged legacy directories; no real deletion or registration was needed (linux-powershell-retention-preview.json). This host has system PowerShell rather than a user-local versioned tree
- SDK follow-up inventory: system images occupy about 7.7 GiB across API 23 and API 34. Both visible AVD config files reference API 34. Further ownership, registered-checkout and process checks are still required before retiring any package; no SDK content was deleted
- Corrected managed integration passes, including the injected unreadable pwsh process, external command link, shared pins and actual old-version deletion (android/temp/linux-powershell-retention-final.log). Scoped quality and git diff --check pass. All sessions from this tranche are terminal; outstanding_bugs.md remains unchanged. The full smoke report retains its original fixture failure, with the successful targeted rerun recorded separately

### SDK provisioning portability and partial-install handling

- Previous goal turn made progress: managed PowerShell retirement and safeguards passed, with the corrected targeted integration recorded separately from the earlier smoke fixture failure
- Fix SDK executable quoting and package argument arrays for paths with spaces. Share SDK environment and native/bat selection across platform finalization, emulator/image installation and AVD creation
- Hold the Linux command-line installer lock during SDK tool use, propagate license/list/install failures and stop treating empty image directories as complete packages. Apply disk reserve checks before package downloads
- Add isolated provisioning integration with spaced paths, host-specific executables, API aliases, partial caches, low-space rejection and failure propagation. SDK ownership-aware retirement remains a separate unfinished part of the goal
- New SDK provisioning fixture and full installer suite pass (android/temp/linux-sdk-installer-suite.log). Tests cover spaced SDK paths, package argument boundaries, API .0 alias selection, license/list/install failure propagation, disk reserve, partial cache repair, failed post-install admission, native-vs-Windows executable selection, lock contention and interrupted command-line publication recovery
- Real get_emulator.sh cache reuse passes with configured JDK 21 and API 34, without downloading or booting an emulator (linux-sdk-real-cache.log). Its command-line transaction directory retains only the 15-byte format and empty lock; no work directory remains
- Scoped quality passes (linux-sdk-provisioning-quality.log), git diff --check passes and all sessions from this tranche are terminal. outstanding_bugs.md remains unchanged. Windows wrapper selection is exercised by a Linux-hosted fixture; actual Windows CI for these newer changes remains pending
- SDK package ownership registration, safe old-package retirement and sdkmanager partial-download cleanup remain unfinished; this tranche repairs provisioning and does not claim those storage gaps are complete

### SDK package and reference inventory

- Previous goal turn made progress: shared SDK provisioning, partial-install admission, actual cache reuse and installer integration passed
- Build a reusable read-only inventory from local package.xml metadata, registered checkout pins and visible AVD image references, including moved AVD descriptors and environment-specific AVD roots. Distinguish unknown metadata from unreferenced packages; do not label either eligible for deletion
- Reference sources: https://developer.android.com/tools/variables documents AVD locations and overrides; https://developer.android.com/tools/sdkmanager documents package IDs and --uninstall. Keep the repository's pinned SDK tools; do not migrate tooling versions during this cleanup work
- Inventory is a prerequisite for package ownership registration and sdkmanager-based retirement. Active processes, retained build caches, cross-user AVD visibility, ownership and destructive-action revalidation still need integration before any deletion
- Added inspect-sdk-retention.ps1 and shared SDK inventory helper. Real snapshot contains 12 directories: seven referenced packages, four unreferenced packages (build-tools 36, platforms 23/36 and API 23 image), and directly extracted command-line tools with missing package.xml classified Unknown. Evidence: android/temp/linux-sdk-inventory.json. No packages were deleted or registered
- Integration passes for two sharing checkouts, integer/.0 platform aliases, command-line latest aliases, moved absolute/relative AVD descriptors, environment roots, malformed metadata, unavailable checkout/AVD data and linked-path rejection. The test is included in the shared tooling smoke profile and master no-infrastructure catalog
- Master HostOnly filter passes one test, zero failures/timeouts/skips (temp/test_reports/report_20260926_213936.md; linux-sdk-inventory-master.log). Scoped quality passes (linux-sdk-inventory-quality.log)
- Final extended tooling smoke passes all 13 checks (android/temp/tooling_smoke/run_30692a932d154f649a416ff05f43ab71/summary.json; linux-sdk-inventory-smoke.log), including corrected managed PowerShell retirement, all installer recovery fixtures and bounded extraction. Every session from this tranche is terminal; git diff --check passes and outstanding_bugs.md remains unchanged. Inventory does not authorize deletion; ownership registration, process/cache checks and SDK uninstall integration remain open

### Managed SDK package retirement

- Previous goal turn made progress: SDK/reference inventory, master classification and all 13 extended tooling checks passed
- Add explicit current-package ownership registration, persistent AVD search roots, process/cache guards, replacement admission and SDK-manager uninstall through the supervised process pool. Preserve unowned packages and unknown metadata
- Use the command-line installer lock for cleanup as well; verified FileShare.None blocks Linux flock on the same inode. Journal before uninstall, reconcile missing packages on retry and preserve incomplete or changed payloads rather than deleting blindly
- Validate in an isolated SDK fixture before any real registration or cleanup. Windows shell writers still need full lock parity; SDK partial-uninstall recovery with missing identity remains an explicit incomplete case
- Initial SDK cleanup integration passed. Expanded smoke caught a fixture-only hidden-marker removal issue; corrected the simulated uninstall to use Force. All 13 other smoke checks passed (android/temp/tooling_smoke/run_95c7972d018f4c54bfc5921a82d6d166/summary.json)
- Corrected master HostOnly SDK cleanup test passes (13 seconds, zero failures/timeouts/skips): temp/test_reports/report_20260926_215323.md and linux-sdk-cleanup-master.log. Coverage includes live SDK working-directory process protection, PATH override, unreadable Java process, retained sanitizer cache, uninstall failure/retry, lost success reconciliation, preserving partial payloads after marker loss, persistent AVD roots and actual old-image deletion in a fixture
- Scoped quality passes (linux-sdk-cleanup-quality.log). Bootstrap now includes SDK registration/apply after successful provisioning, with its step count updated to 16 before optional steps
- Real registration/apply succeeded: build-tools 37.0.0, CMake 3.31.6, platforms 34/37.0 and the API 34 image are registered and protected; four legacy unowned packages remain protected. No real SDK package was removed. The default AVD root is persisted in the SDK ownership registry (android/temp/linux-sdk-cleanup-registered.json)
- All sessions from this tranche are terminal. git diff --check passes and outstanding_bugs.md is unchanged. The full smoke report retains the original fixture failure; the corrected master rerun is the final cleanup validation, with the 13 other smoke checks already passing
- Remaining SDK work: fully automatic interrupted removal when identity metadata was deleted, Windows writer lock parity, and separately reviewed adoption of legacy SDK packages. Standalone provisioning helpers do not yet trigger retirement independently of the final bootstrap step

### Interrupted SDK uninstall recovery

- Previous goal turn made progress: owned SDK retirement, fixture uninstalls, persisted AVD roots, master validation and real registration of five current packages succeeded
- Record directory identity and a bounded SHA-256 manifest before sdkmanager starts uninstalling. Reconcile only surviving original files after interruption, retaining all reference/process/cache/replacement checks even when package.xml or the ownership marker disappeared
- Move verified partial payloads into a deterministic quarantine before deletion; journal that transition and reject older cleaners with a temporary schema version while quarantine recovery is pending
- A direct probe showed .NET CreationTime changes when directory contents change on this Linux host, so use native device/inode/birth identity on Linux and volume/file ID/creation identity on Windows in a shared helper
- Expanded SDK recovery integration passes changed-content/new-file/replaced-directory protection, references renewed after retirement began, missing package.xml/ownership marker recovery, and actual process termination after quarantine move and during quarantine deletion. Preview preserves quarantines; retry removes remaining known files and restores schema 1
- Final extended Linux smoke passes all 14 checks (android/temp/tooling_smoke/run_14f48b2708c2423d9bf0ac7aff5e9ba4/summary.json; linux-sdk-recovery-smoke.log). Scoped quality passes (linux-sdk-recovery-quality.log), git diff --check passes, and all sessions from this tranche are terminal
- Windows directory-identity interop source compiles on this host, but native Windows execution of this new recovery path remains pending. A fresh real SDK preview preserves five configured packages and four unowned legacy packages; no real payload was removed or hashed for retirement (linux-sdk-recovery-preview.json)
- User pushed commit 95f3c7fc during this work. GitHub run 36295068814 passes on both Ubuntu 24.04 and Windows 2022: https://github.com/larsonm-personal/dxx-redux/actions/runs/36295068814. This covers the prior committed tooling work, not the newer uncommitted SDK cleaner/recovery changes
- outstanding_bugs.md remains unchanged. Remaining goal work still includes Windows writer lock parity, legacy SDK adoption after reference review, standalone producer integration, the remaining test/capability ledger and other recorded build/device gaps

### DOS MIDI host test runner

- Reuse the shared extraction build tree, resolve host executable names and cached CMake, and normalize paths relative to the checkout
- Supervise configure/build/CTest/reference conversion with bounded child processes; retain per-run diagnostics under a producer lock and check disk reserve before building
- Verify all three synthetic tests are registered and execute them even without local DOS captures. Missing captures produce SKIP/exit 2; changed hashes and failed negative controls remain failures
- Catalogue this as a no-infrastructure host test and validate synthetic execution, missing-reference classification and failure behavior without changing reference fixtures
- Real Linux configure/build and all three synthetic CTest cases pass using the existing shared extraction tree (android/temp/linux-midi-synthetic.log). No duplicate extraction build tree was created
- Master HostOnly selects this as Tier 0 and reports SKIP after synthetic PASS because all three proprietary captures are unavailable; zero failures/timeouts (temp/test_reports/report_20260926_221943.md; linux-midi-master.log)
- Injected wrong reference hashes fail after the real synthetic tests pass; an empty CTest catalog fails explicitly. Spaced paths and invocation from /tmp work with checkout-relative arguments, and the temporary policy fixture was removed (linux-midi-policy-checks.log)
- Updated listening-command examples to select the shared build tools and the chosen retained run. The separate game08 listening helper still hardcodes Windows executable names and remains a portability gap; reference/audio parity cannot be validated here without the captures
- Scoped quality passes (android/temp/linux-midi-quality.log), git diff --check passes, and all processes from this tranche are terminal. outstanding_bugs.md remains unchanged. Current runner changes have Linux evidence; Windows execution and real capture parity remain unverified

### Original and live GuideBot navigation runners

- Previous goal turn made progress: DOS MIDI host runner uses shared builds and supervised processes; real synthetic, skip classification and failure-policy checks passed
- Replace fixed CD-layout assumptions in both navigation runners with shared pinned D2 discovery and temporary lowercase staging. Preserve all native assertions and repeated-result comparisons
- Share supervised native invocation with bounded timeouts and retained logs; remove staged base data and user/mission payloads through existing finally cleanup
- Validate both real retail runners using current binaries, route-owner classification, and cleanup after injected native failure/timeout. Missing Maximum media remains a reported coverage skip
- Both real retail runners pass their native assertions twice with identical JSON: Original D2 levels 1/11 and live Counterstrike grate/reactor cases (linux-original-navigation.log, linux-live-navigation.log). Live returns SKIP/2 for the unavailable Maximum archive after retail coverage succeeds
- Route owner correctly reports Original PASS and live SKIP, with no failures: android/temp/route_regression_cases/run_20260926_222338_760/summary.json (linux-navigation-owner.log)
- Injected exit 7 and a sleeping native child exercise failure and timeout paths through the real runners. Both child processes are gone, staged data/user/mission directories are removed, and diagnostic logs remain. Absolute spaced build paths work when invoked outside the checkout (linux-navigation-faults.log); temporary executable fixtures were removed
- Native assertions and repeated-result comparisons were not changed. Actual Windows execution and the missing Maximum case remain unverified
- Scoped quality passes (linux-navigation-quality.log), git diff --check passes and all sessions from this tranche are terminal. No new native build or dependency download was needed; outstanding_bugs.md remains unchanged

### Executable test catalog for parity accounting

- Previous goal turn made progress: Original/live native navigation passed retail repeats and injected failure/timeout cleanup through portable data discovery and supervision
- Add a read-only master catalog export from the actual discovery path, before profile/filter selection and environment probes. Include every top-level execution entry plus support ownership; expose declared infrastructure and manual status without claiming host capability or execution success
- Ensure catalog mode neither creates/prunes reports nor registers cancellation handlers/provisions infrastructure. Test completeness against on-disk PS1/JSONC discovery and test normal/extended replay variants; include the fixture in tooling CI
- Use this export to quantify remaining coverage gaps. A catalog is not a parity pass report; per-test Windows/Linux execution and exception evidence remains required
- Actual default export contains 236 top-level entries: 75 no-master-infrastructure, 150 single-emulator, four extraction, one server and six dual-emulator; six entries are manual. It also lists 324 owned PS1/JSONC support scripts (android/temp/linux-test-catalog.json). These are scheduling counts, not execution/capability claims
- Initial catalog integration passes against actual discovery, including both replay matrices and every discovered PS1/JSONC file. Expanded coverage now also seeds six old reports to verify listing preserves them; tests are part of the portable tooling smoke profile
- Final extended Linux tooling smoke passes all 15 checks, including catalog completeness and old-report preservation: android/temp/tooling_smoke/run_f6505aaf5c4743f790967c7d51f60cdb/summary.json (linux-test-catalog-smoke.log)
- Scoped quality passes (linux-test-catalog-quality.log), git diff --check passes, and all sessions from this tranche are terminal. outstanding_bugs.md remains unchanged
- Remaining accounting work: retain compact per-test execution evidence across report pruning and join it to this catalog. Older full-suite Markdown reports have aged out under generation retention, so their historical plan entries cannot substitute for current per-test evidence. Device/media constraints and Windows execution still require explicit accounting

### Host classification and explicit Windows capability skips

- Previous goal turn made progress: read-only master catalog and all 15 extended tooling checks passed
- Catalog review found seven host checks still defaulting to the emulator tier: explicit replay-path discovery, replay build guard, repository artifact policy, mission ZIP publication/recovery, Windows Job Object lifetime, and Windows PowerShell 5.1 compatibility
- Classify these as host checks, preserve Windows-specific coverage, run portable compatibility helper assertions on every host, and replace success-on-skip with the master's explicit SKIP/2 contract
- Bound Windows process-lifetime fixture generations and clean its owned temporary output after completion/failure. Validate corrected host checks directly and through the master without starting infrastructure
- Direct Linux sweep passes five newly classified portable checks plus catalog export; the two Windows-specific checks produce explicit SKIP/2. PowerShell compatibility helper assertions pass before its unavailable-5.1 skip (android/temp/host_classification/run_49b2a7cf6653492887393b5c6b89b598/summary.json; linux-host-classification.log)
- The additional full catalog validator exposed missing suite coverage policy entries for SDK inventory/cleanup. Added both to fixed core and added full catalog validation to tooling smoke. Corrected validator passes: 76 standalone JSON, 156 standalone PS1 and 324 support scripts (linux-host-catalog-validation.log)
- Master HostOnly reports the PowerShell 5.1 test as Tier 0 and SKIP after portable helper PASS, without device setup (temp/test_reports/report_20260926_223318.md; linux-host-windows-skip.log). The initial nine-check sweep retains its catalog failure as evidence; the corrected validator is recorded separately
- Master HostOnly catalog filter passes both inventory and full ownership/coverage validators (2 PASS, zero failures/timeouts; soundfont device test correctly excluded): temp/test_reports/report_20260926_223417.md and linux-host-catalog-master.log. Refreshed inventory has 82 no-master-infrastructure entries, 143 single-emulator, four extract, one server and six dual-emulator
- Scoped quality passes (linux-host-classification-quality.log), git diff --check passes, and all sessions from this tranche are terminal. outstanding_bugs.md remains unchanged. The new Windows lifetime fixture cleanup still requires native Windows execution; latest full extended smoke remains the prior 15-check run, with the added validator validated directly and through the master here

### Fingerprint host tests

- Previous goal turn made progress: seven host classifications, explicit Windows-only skips, SDK suite coverage entries and master catalog validation were corrected and verified
- Classify six fingerprint policy/native checks as host-side; resolve the threshold matcher through shared host helpers and replace Windows-only publication-fixture shims with host-specific launchers
- Supervise native audio enumeration and matcher children. Replace fixed publication scratch storage with unique retained/locked runs; retain normal finally cleanup and avoid new builds/downloads where current extraction tools suffice
- Run real enumeration/threshold tests and synthetic publication/identity/budget/build-policy checks. Preserve publication failure assertions and report any production-workflow Linux gaps exposed by the fixtures
- All six fingerprint checks pass through the master as Tier 0, with zero failures/timeouts/skips. Revalidated after production default-path fixes: temp/test_reports/report_20260926_223846.md (android/temp/linux-fingerprint-master.log)
- Production CD, mission-ZIP and music-pack commands also had Windows-only default executable paths. They now resolve shared host filenames after building and use cached CMake with two build workers; the music-pack build guard still requires successful build before artifact admission
- Real default-tool integration passes: synthetic data-only CD track publishes a fingerprint manifest; mission ZIP containing invalid audio reaches the native decoder, fails, and publishes no sidecar. Both use existing targets and all temporary fixture data is removed (linux-fingerprint-default-tools.log)
- Music-pack default setup resolves verified Linux 7zz, builds/reuses fingerprint_audio and selects the Linux executable. It then rejects the deliberately nonexistent album selector, as expected; this is tool-admission evidence, not a real album fingerprint run (linux-fingerprint-music-pack-default.log)
- Both catalog validators pass through the master after the six host classifications (temp/test_reports/report_20260926_224028.md; linux-fingerprint-catalog.log). Native/fixture temporary run directories are empty after successful cleanup
- Scoped quality passes (linux-fingerprint-quality.log), git diff --check passes, all sessions are terminal and outstanding_bugs.md remains unchanged. No AcoustID requests or dependency downloads were performed. Windows runtime verification and real music-album/mission audio corpus runs remain outstanding

### Extraction policy and fixture isolation

- Previous goal turn made progress: six fingerprint checks, production Linux tool admission and native synthetic fingerprinting passed without new dependency downloads
- Classify eight inspected host-only extraction checks: CD workflow planning, CD/GOG batch policy, spec generation, mocked device preflight, regression workflow, cache provenance and publication
- Replace shared scratch-directory deletion with unique retained run directories and producer locks; retain existing finally removal so parallel test invocations cannot delete each other's work
- Validate all eight through the master runner, then catalog/coverage validators. These fixtures prove host planning, provenance and publication behavior, not Android extraction or complete proprietary-media coverage
- Six extraction checks pass through the master with zero failures/timeouts/skips (temp/test_reports/report_20260926_224210.md; linux-extraction-policy-master.log)
- The CD runner exposed a missing game_data/disc_track_manifest.ps1 used by the production hash publisher; its broad ignore rule also hid the missing source from normal additions. Restored complete CUE/track/SHA-1 validation, normalized ordered hash records, a shared CUE reader with fingerprint publication, and a Git visibility exception/assertion
- Spec-generation fixture omitted Get-DepPlatform.ps1 and host helper dependencies now imported by its copied extraction/JSON helpers. Added those actual source dependencies; the fixture now passes without changing its generation assertions
- CD runner and spec generation both pass through master Tier 0 (report_20260926_224509.md and report_20260926_224527.md; linux-cd-runner-master.log, linux-spec-generation-master.log). Fingerprint publication integration also passes after sharing the CUE parser (linux-extraction-fingerprint-publication.log)
- Two actual concurrent spec-generation children passed with distinct run directories (linux-spec-generation-parallel.log). The first temporary concurrency harness used a variable name that collided with the pool callback scope; corrected the probe variable and reran successfully. No production process-pool change was needed
- Formatting exposed an entry-point bug: the documented space-separated -Paths invocation bound only its first path; Get-Item omitted hidden files, so a .gitignore-first scope became empty and ran all formatters. Stopped the verified formatter PID after the stale-formatter helper failed to recognize its relative invocation, then restored only eight unrelated formatting-only files that were clean before this turn
- Repaired run-code-quality.ps1 to collect remaining positional paths, resolve hidden files with Force, and reject missing/empty explicit scopes before invoking tools. Default unscoped behavior is preserved. Extended the existing code-quality test with an isolated real entry point and recording tool stubs; multi-path/hidden-path, invalid/empty-scope and default-mode checks pass (linux-quality-scope-test.log)
- Re-ran quality using an explicit array of all 39 currently changed/new files, covering previous work as well as this tranche. The log lists that scope, reports no C++/Kotlin files, and all checks pass (linux-extraction-policy-quality.log; linux-quality-changed-paths.json). Unrelated formatter changes and protected outstanding_bugs.md have zero diff
- Final extended Linux smoke passes all 17 checks, including SDK interruption recovery, formatter scope, full catalog/coverage validation, installer recovery and bounded Python/extraction: android/temp/tooling_smoke/run_f02d7c34b0ae4dc6adb90503ef69a08d/summary.json (linux-extraction-policy-smoke.log)
- Master catalog filter also passes both validators (temp/test_reports/report_20260926_225215.md; linux-extraction-policy-catalog.log). All sessions are terminal, git diff --check passes, unrelated formatter changes have zero diff, and outstanding_bugs.md remains unchanged
- Actual Windows execution, device extraction and remaining media/capability gaps remain open. The stale-formatter helper's inability to recognize relative command-line invocations is a separate remaining cleanup gap

### Formatter process discovery and cleanup

- Previous goal turn made progress: extraction policy checks, missing manifest source repair, concurrent fixture isolation, formatter scope regression and all 17 extended tooling checks passed
- Replace duplicated formatter process enumeration with shared host inventory. Resolve relative Linux PowerShell invocations using native argv/cwd; use formatter lock PID/start identity for relative Windows invocations without readable cwd
- Preserve checkout boundaries, exclude the cleanup process and its ancestors, and recheck live process start identity before stopping a matched tree
- Validate with isolated sleeping formatter fixtures and an unrelated sibling checkout, covering preview, relative/absolute invocation, stale identity and owned descendant termination without running real formatters
- Scoped formatting/lint passes for all eight changed PowerShell files (android/temp/linux-formatter-cleanup-quality.log). The initial fixture exposed empty procfs command lines; fixed trailing-NUL handling using an explicit length/character check
- Final extended Linux smoke passes all 18 checks, including actual formatter parent/child termination, sibling checkout isolation, shared process cleanup, SDK interruption recovery, catalog validation and installer/Python/extraction bounds: android/temp/tooling_smoke/run_0091b5792399425db89d55cac56774fa/summary.json (linux-formatter-cleanup-smoke.log)
- Latest remote tooling run 36297181507 at b3cda916 passes Ubuntu 24.04 and Windows 2022. This does not validate the current uncommitted formatter changes on Windows; actual Windows execution remains pending
- Documented explicit formatter preview/stop behavior in CLEANUP.md. No new dependency downloads or native builds were needed; free space is 8.6 GiB. All invoked sessions exited, git diff --check passes, and outstanding_bugs.md is unchanged

### Remaining dependency and D2X-XL host checks

- Previous turn made progress: formatter cleanup integration and 18 extended Linux tooling checks passed
- Dependency verification and D2X-XL sound contracts already pass directly on Linux but were incorrectly scheduled behind an emulator; classify these and the texture checks as host tests and assert the catalog declarations
- Separate portable production TGA pixel decoding from System.Drawing allocation. Add Linux/Windows coverage for 24/32-bit pixels, all four origins, alpha, key masks and malformed inputs; preserve the existing bitmap/archive test for Windows with an explicit Linux skip
- Give dependency and texture fixtures retained unique run directories, producer locks and finally cleanup. Add portable dependency/sound/pixel checks to the cross-platform CI smoke entry point
- Full texture conversion still uses System.Drawing and Windows tool defaults; this tranche enables real decoder coverage and truthful scheduling, not full Linux texture conversion
- Master D2X-XL run: two PASS (sound/pixels), one explicit Linux SKIP (bitmap/archive), zero failures/timeouts; temp/test_reports/report_20260926_231114.md and android/temp/linux-d2xxl-master.log
- Dependency verification passes through master Tier 0: temp/test_reports/report_20260926_231148.md and android/temp/linux-download-verification-master.log
- Scoped quality passes for all eight changed PowerShell sources (android/temp/linux-d2xxl-quality.log). Final extended smoke passes all 21 checks: android/temp/tooling_smoke/run_9e0c9168ac3a49948407e455903e603c/summary.json and android/temp/linux-d2xxl-smoke.log
- Actual Windows bitmap execution remains unverified here. No dependency downloads or native rebuilds were required; all invoked sessions exited and outstanding_bugs.md remains unchanged

### Bounded host execution evidence

- Previous turn made progress: portable TGA production decoding, corrected host classifications, retained fixtures and all 21 extended smoke checks passed
- Master reports age out after later filtered runs, losing the evidence needed for Windows/Linux coverage accounting. Add one bounded atomic summary per OS/architecture, merging latest per-test observations across filtered runs
- Record actual start/finish status, requirement, arguments, source hash, Git commit/dirty marker, host runtime, skip reason and diagnostic paths. RUNNING means outcome unknown after interruption, not proof a process is live; owner PASS does not imply all owned child cases ran
- Serialize writers with a bounded file-lock wait, reject corrupt input without overwriting it, limit entries/bytes and report eviction counts. Validate merge, stale updates, failure/skip semantics, interrupted observations, contention and bounds, then exercise the real master runner
- Implemented master start/completion observations and batch publication for infrastructure/manual/not-run skips, separate by OS/architecture. Evidence write errors make the suite fail after its normal infrastructure cleanup
- Integration exposed PowerShell JSON timestamp coercion and mixed dictionary/object sorting differences. Normalize timestamps and sort uniform objects; strengthened retention assertions verify exactly which oldest observation is evicted
- Integration passes merge/status preservation, concurrent writers, stale completion rejection, corrupt-file preservation, entry/byte bounds, interrupted publication scratch recovery, and three real filtered master runs including a Linux capability skip
- Extended Linux smoke passes all 22 checks: android/temp/tooling_smoke/run_a78ae9b11d6e44e495323757cc798fa2/summary.json (linux-evidence-smoke.log). The final sorting correction additionally passes the strengthened direct integration and the master-owned integration after scoped quality (linux-evidence-integration.log; linux-evidence-final-master.log)
- Real HostOnly emulator exclusion records SKIP with its infrastructure reason (linux-evidence-infra-skip.log). The durable Linux summary preserves this alongside filtered PASS observations in chronological order and is currently 2,646 bytes; it does not backfill unverifiable older runs
- Final scoped quality and git diff --check pass; all invoked sessions exited and outstanding_bugs.md is unchanged. Windows execution and complete per-owner/case parity accounting remain open; this summary records actual local top-level observations rather than declaring parity

### Standalone SDK producer retention

- Previous turn made progress: bounded host execution evidence, master integration and 22 Linux tooling checks passed
- Standalone Linux finalize/emulator provisioning bypassed the owned-package cleaner used by get_all. Invoke the same registration/retirement operation after successful validation, releasing the Linux installer lock before the cleaner acquires it
- Preserve full-bootstrap deferral, fail the producer if retention fails, and reject successful sdkmanager exits that leave requested package metadata missing. Current and referenced packages remain protected by the existing cleaner
- Extend actual entry-point fixtures to verify .NET acquisition of the released shell lock, space-containing repository paths, success/failure ordering and deferred full-bootstrap cleanup
- Windows standalone automatic retirement remains explicitly deferred until shell writers participate in the same lock protocol; no claim of cross-host writer parity is made
- Entry-point fixtures pass standalone .NET lock reacquisition, cached success, cleaner failure propagation, missing metadata/license failure suppression, full-bootstrap deferral and explicit Windows standalone deferral. The fixture itself now uses retained unique runs, a held producer lock and signal/finally cleanup
- The shell transaction owner previously ran Linux/flock fixtures indiscriminately through the master despite smoke already excluding them on Windows. It now reports an explicit non-Linux capability skip; portable dependency tests remain independently scheduled
- Real standalone cached get_emulator.sh succeeded with GET_ALL_RUNNING unset, without downloads (linux-sdk-standalone-real.log). All 11 installed SDK package metadata identities remain unchanged against linux-sdk-standalone-before.json, and all five owned current packages remain Installed under ownership schema 1
- Final mixed-language scoped quality passes (linux-sdk-standalone-quality.log). The complete six-fixture dependency installer owner passes through the master in 22 seconds, zero failures/timeouts/skips (temp/test_reports/report_20260926_233045.md; linux-sdk-standalone-tests.log); its result is recorded in the bounded Linux evidence summary
- git diff --check passes; all sessions exited and outstanding_bugs.md is unchanged. No real SDK payload was removed. Windows writer-lock parity, other standalone dependency producers and SDK partial-download recovery remain open

### Shared SDK writer supervision

- Previous turn made progress: standalone Linux SDK retention, cached real-SDK validation and all installer transaction fixtures passed
- Windows shell SDK producers do not participate in the cleaner's file lock; the PowerShell AVD producer is unlocked on both hosts. Introduce a shared writer wrapper, using existing child supervision, bounded waits and the same SDK lock inode
- Windows shell producers re-enter under the wrapper; PowerShell AVD creation uses it on both hosts. Linux shell producers keep their existing inherited flock/recovery protocol
- Defer prompts and retirement until the writer exits and releases its lock; preserve nonzero exit codes and test serialization, contention, timeout and post-success retention with disposable SDK fixtures
- Actual Windows execution remains a separate validation requirement; latest remote run 36297181507 is green at b3cda916 and predates these edits
- Added invoke_sdk_writer.ps1 with exclusive SDK file locking, bounded wait/execution, existing Windows Job Object/Linux descendant supervision, child status propagation and finally release. Windows get_sdk/finalize/get_emulator/create_avd shell entry points route through it; create_light_avds uses a dedicated wrapper process on both hosts
- Successful standalone Windows finalize/emulator calls now run owned package retention after release; bootstrap calls defer it. Interactive shell pauses occur outside the lock. Updated copied fixtures for get_sdk's shared SDK helper dependency
- Native Linux wrapper integration passes serialization, lock contention, exact nonzero child status, timeout/release and PowerShell switch/AVD-name forwarding. The Windows shell branch passes with actual wrapper execution on Linux and an isolated retention fixture; this is not actual Windows validation
- Extended Linux tooling smoke passes all 23 checks (android/temp/tooling_smoke/run_a0725c91121a43f7be6bab198a5666a1/summary.json; linux-sdk-writer-smoke.log), including SDK package recovery and the full installer fixture owner
- Final mixed-language quality passes (linux-sdk-writer-quality.log). Added real PowerShell AVD-entry re-entry coverage with a cached-AVD fixture; the final expanded writer integration passes through the master in eight seconds, with no AVD payload allocation (temp/test_reports/report_20260926_234224.md; linux-sdk-writer-master.log)
- All sessions exited, git diff --check passes, and outstanding_bugs.md is unchanged. Actual Windows execution, Windows installer transaction recovery after forced interruption, other standalone producer retention and partial SDK downloads remain open

### SDK transaction recovery under the shared wrapper

- Previous turn made progress: SDK writer serialization/supervision, cached AVD entry-point integration and 23 Linux tooling checks passed
- The new Windows SDK wrapper holds the shared lock, but shell begin_dependency_install still bypassed transaction recovery on Windows. Enable the existing journal/recovery protocol under a wrapper-provided, exact cmdline-tools parent scope; reject attempts to use that lock for another destination
- PowerShell AVD consumption must also recover an interrupted command-line publication before resolving avdmanager. Run the existing Bash recovery under the held wrapper lock before starting that consumer
- Validate interrupted previous-tree restoration, abandoned workspace removal, scope mismatch rejection and existing installer checks using isolated fixtures; actual Windows filesystem/process execution remains pending
- Supervised SDK operations now use the existing fixed transaction workspace and recovery instead of Windows' prior bypass. The wrapper publishes the exact tools-parent scope; unrelated destinations fail before creating transaction state
- PowerShell AVD consumers run a bounded recovery phase under the already-held SDK lock, restoring tools before resolving executables. Wrapper environment scope is restored on exit
- Expanded writer integration passes interrupted previous-tree restoration/workspace removal, unrelated destination rejection and cached real AVD entry-point re-entry through the master: temp/test_reports/report_20260926_234713.md (linux-sdk-recovery-master.log)
- Windows shell-branch fixture restores an interrupted command-line publication through the real wrapper on Linux. Complete six-fixture installer owner passes in 23 seconds, zero failures/timeouts/skips: temp/test_reports/report_20260926_234728.md (linux-sdk-recovery-installers.log)
- Scoped mixed-language quality and git diff --check pass (linux-sdk-recovery-quality.log). All sessions exited and outstanding_bugs.md is unchanged. No real SDK payloads or AVDs were created or removed
- Actual Windows execution, other Windows dependency transaction recovery, standalone non-SDK producer retention and sdkmanager internal partial-download cleanup remain open

### Windows tooling CI repair at f7258946

- Run 36332568050 passed Linux and failed Windows SDK cleanup and writer-timeout checks
- Fix the fake native sdkmanager's argument capture: PowerShell -File splits a Windows drive colon into a parameter/value pair. Use original process arguments to test the native invocation faithfully
- Exercise command-line SDK use on both hosts and keep cwd-only coverage on Linux, where process inventory exposes it
- Give each Windows pool worker its own job-owning supervisor so timeout and normal exit retire descendants before captured pipes drain; retain runner-wide protection against forced parent exit
- Strengthen descendant/timeout checks and failure diagnostics, run scoped quality and Linux tooling smoke. Native Windows validation requires the next CI run
- Implemented worker-local Windows job supervision with inherited output handles, plus normal-exit/timeout descendant tests in the Windows smoke suite. The SDK timeout fixture now confirms its shell actually started and includes captured failure details
- Scoped quality passes (android/temp/ci-tooling-repair-quality.log). All 23 extended Linux tooling checks pass: android/temp/tooling_smoke/run_89defb96b1ab45798e07a755289d8ccb/summary.json and android/temp/ci-tooling-repair-smoke.log
- The Windows supervisor's payload decoding, inherited stdout/stderr and exit-code forwarding were also exercised directly on Linux; this does not validate Windows job semantics
- Downloaded CI diagnostics were removed after inspection; test scratch cleanup and retained smoke generations remain bounded. git diff --check passes, all invoked sessions exited, and outstanding_bugs.md is unchanged. Changes are local; native Windows CI validation is pending

### Windows supervisor output repair

- Run 36334206740 passes SDK cleanup and the Windows process-lifetime integration, but every Windows artifact log is empty. The pool, SDK writer and catalog checks fail because they depend on captured output
- The prior supervisor started a windowless child without explicitly supplying standard handles. [.NET's Windows process startup](https://github.com/dotnet/runtime/blob/v8.0.0/src/libraries/System.Diagnostics.Process/src/System/Diagnostics/Process.Windows.cs#L462) sets STARTF_USESTDHANDLES only when at least one stream is redirected
- Redirect and immediately close stdin to provide EOF for headless workers while explicitly inheriting stdout/stderr. Keep the existing job lifetime and direct output pipes, avoiding a relay that could wait on surviving descendants
- Extend pool integration with stderr on nonzero exit and 128 KiB on each stream. The direct supervisor probe passes stdin EOF, both complete output streams and exit 37 on Linux; Windows handle semantics still require native CI validation
- Scoped quality passes (android/temp/ci-output-repair-quality.log). All 23 extended Linux smoke checks pass: android/temp/tooling_smoke/run_74d71e648f05432a86534292ebd03937/summary.json and android/temp/ci-output-repair-smoke.log
- Downloaded CI artifacts were removed, all invoked sessions exited, git diff --check passes and outstanding_bugs.md is unchanged. This repair remains local pending native Windows CI
