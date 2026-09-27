# Windows/Linux Android tooling and storage scope

Date: 2026-09-26
Status: implementation in progress; see execution record below

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

| Area | Evidence | Required work |
| --- | --- | --- |
| Workspace cleanup | `clean-workspace.ps1` explicitly rejects deletion outside Windows; its process inventory uses CIM | Implement Linux activity/ownership checks and portable deletion safeguards |
| Native Gradle retention | `build.gradle` invokes `powershell.exe` only on Windows | Run the shared retention policy on both hosts before new `.cxx` generations |
| Cleanup tests | `tests/test_clean_workspace.ps1` mocks CIM and creates a Windows junction | Exercise real Linux symlinks, process ownership, and lock semantics alongside Windows cases |
| Dependency lifecycle | `get_ndk.sh` deletes its download only after successful extraction, extracts directly into the install root, and leaves superseded NDK directories | Staged, validated installation; failure cleanup; bounded obsolete-version retention |
| JDK reproducibility | `get_jdk.sh` derives a `latest` URL while validating against configured `JDK_VERSION` | Download the exact configured release and verify its archive; retain existing rollback behavior |
| Environment selection | `helpers/set_vars.sh` chooses the last glob match for JDK/NDK | Resolve configured versions explicitly so pruning and version selection agree |
| Build entry points | `1_build-aab.ps1` calls `gradlew.bat`; `helpers/build.sh` always waits for a key | Shared wrapper resolution and noninteractive operation with reliable exit codes |
| Test/helper drift | Direct `gradlew.bat` calls in dual-emulator tests, crash-report tests, mission archive helpers, warning collection, and metadata regeneration | Route callers through shared Gradle/tool helpers; inspect branches before classifying each occurrence as a defect |
| Host regeneration/tests | Direct Windows build calls in guidebot tests/helpers and metadata runners | Shared host build dispatch, executable lookup, and capability reporting |
| Process lifetime | `helpers/process_lifetime.ps1` enrolls Windows jobs but initialization is a no-op on Linux | Test and implement owned child/process-group cleanup, including parent termination and grandchildren |
| Sanitizer parity | Linux build CLI lacks the Windows sanitizer option; sanitizer/replay guards reference Windows builds | Provide Linux configuration or an explicit tracked capability gap, never silent success |
| Documentation | `.github/copilot-instructions.md` refers to absent `android/run_emulator.sh` | Document `pwsh android/Run-Emulator.ps1` and remove stale paths |
| Continuous validation | Existing `.github/workflows/package-linux.yml` packages desktop games; no Android test workflow in the tracked workflow inventory | Add Windows/Linux checks for the Android tooling and selected integration paths |

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

| Owner | Default lifecycle |
| --- | --- |
| Dependency downloads and extraction | Unique staging; delete archives/staging after success or failure; next-run recovery for interrupted owned stages |
| Managed JDK/NDK/tool versions | Keep configured/referenced versions and active users; prune superseded owned versions after validated replacement; keep rollback until publication succeeds |
| SDK packages and emulator images | Remove obsolete project-owned packages through SDK tooling; preserve packages/AVDs referenced by supported configurations |
| Native `.cxx` configurations and packages | Existing producer retention of three prior generations per family; protect active configuration/ABI users; explicit cleanup may retain fewer |
| Fixed CMake/Gradle/Cargo build trees | Reuse compatible trees; remove stale owned variants by policy, not unconditional clean builds |
| Test reports and large payloads | Bounded generations plus family byte budgets; trim bulky reproducible payloads separately from compact failure evidence |
| Temporary emulator/device data | Clean owned temporary AVDs, snapshots, uploads, and run data on completion/failure; preserve intentional persistent test AVDs and source assets |
| Global/shared caches | Use tool-supported pruning only within declared ownership; do not recursively purge user-wide Gradle, Cargo, SDK, or dependency directories |

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
- The model cleanup already existed behind __ANDROID__ in d2/main/piggy.c. Removed only that guard so host HAM reloads release the previous polygon models too. D1 has no corresponding HAM reload path. This preserves Android behavior and extends its existing cleanup to desktop/headless builds; sanitizer detection remains enabled
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
