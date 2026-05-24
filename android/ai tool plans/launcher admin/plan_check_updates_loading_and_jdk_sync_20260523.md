# Plan: check-updates loading lines and JDK sync 2026-05-23

## Goal

- Show a brief `loading <url>...` line for each initial web request in `check-updates.ps1`
- Make JDK install-sync actually reinstall when `jdk-<major>` exists but the installed patch version does not match `JDK_VERSION`

## Local hypotheses

- `check-updates.ps1` currently suppresses PowerShell progress UI but emits no replacement request logging, so the initial search looks idle while `Invoke-WebRequest` fans out across multiple endpoints
- `get_jdk.sh` only checks whether `jdk-$JDK_MAJOR/bin/java` exists, while `check-updates.ps1` detects the installed version from `jdk-$JDK_MAJOR/release`, so patch-version drift is detected in the table but ignored by the installer helper

## Steps

- [x] Inspect the web-fetch helpers and JDK install helper
- [x] Add per-request loading lines to the initial update search
- [x] Validate the new update-search output with a noninteractive `check-updates.ps1` run
- [x] Fix the JDK helper to compare the installed `release` metadata against `JDK_VERSION`
- [x] Validate the JDK helper logic without starting a full download
- [x] Update this plan with the final result

## Result

- `check-updates.ps1 -NoPrompt` now prints one `loading <url>...` line for each real `Invoke-WebRequest` call during the initial dependency scan
- The new loading lines cover Maven metadata, GitHub API and release pages, Google SDK metadata, Adoptium JDK lookups, and the HTML scrapes already used by the script
- `get_jdk.sh` now reads `jdk-$JDK_MAJOR/release`, compares `JAVA_VERSION` to `JDK_VERSION`, and only exits early when they match
- If the canonical `jdk-$JDK_MAJOR` directory exists but is stale or incomplete, `get_jdk.sh` now removes that directory before extracting the new archive so the refreshed version can take over the canonical path
- Validation: `bash -n android/get_deps/get_jdk.sh` passed, and the current environment reports `target=21.0.11 installed=21.0.10`, which now resolves to `reinstall-needed` instead of the old early-exit path
- Not run: a full JDK install-sync, because it would start a real network download and mutate the local tool install