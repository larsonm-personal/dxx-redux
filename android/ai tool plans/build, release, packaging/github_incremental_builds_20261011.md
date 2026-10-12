# GitHub incremental release builds

1. Add a clean/unclean choice to the release helper and publishing menu, with an explicit parameter for unattended use and one choice for both editions
2. Preserve native outputs and enable Gradle task caching, Kotlin incremental compilation, daemon reuse and parallel project execution for unclean builds
3. Refresh native identity through a tracked generated header and inspect packaged Kotlin/native identity before accepting either APK; retain signing, source, SDK and version checks
4. Extend release/menu integration coverage, run scoped formatting, and validate real incremental current/legacy builds and cache reuse

Configuration caching remains disabled because custom tasks use execution-time Project access and configuration-time Git/process state. Do not bypass input checks, use offline dependency resolution or weaken compiler cache correctness

## Implemented

- Both entry points ask clean/unclean once, defaulting to unclean; explicit BuildMode supports unattended use and is passed to the recursive legacy build
- Each edition retains its own app outputs and Gradle project task history, while sharing dependencies and Gradle's task-output cache
- Native commit/version identity is generated as a tracked header, avoiding configure-time Git metadata and per-version CMake directory churn
- Fresh APKs are checked against their compiled BuildInfo fields and a commit/version marker exported by both engines for all three ABIs, alongside existing signing/package/version/SDK/source checks

## Validation

- All 81 release orchestration scenarios, five packaged-identity checks and 17 menu scenarios passed on PowerShell 7.6.6 and Windows PowerShell 5.1.26100.9444
- Scoped mixed-language formatting/lint and git diff --check passed
- Real signed current and legacy universal APKs built and passed embedded identity verification; initial Gradle times were 13m 52s and 9m 47s
- Reusing the same version name with versionCode increased from 24960 to 24961 passed verification for both APKs in 400.3 seconds end to end (Gradle: 3m 39s current, 2m 40s legacy)
- An additional unchanged-parameter repeat stopped on a concurrent, unrelated mod_save_compat.cpp:428 compile error (std::min collided with the engine min macro). No successful repeat timing is claimed; that in-progress code was not modified. These local runs also included concurrent source edits, so the times are observations rather than a controlled benchmark
