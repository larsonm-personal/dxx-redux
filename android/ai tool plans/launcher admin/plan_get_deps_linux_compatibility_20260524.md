# Get Deps Linux Compatibility Plan

Goal: survey `android/get_deps` and define the minimum Linux compatibility work so `check-updates.ps1` can enumerate and download dependencies on Linux while keeping PowerShell scripts as PowerShell

## Phase 1: Survey

- [x] Inventory scripts and dependency outputs
- [x] Identify Windows path and executable assumptions
- [x] Compare `check-updates.ps1` coverage against install scripts
- [x] Check callers of duplicate shell and PowerShell scripts

## Phase 2: Strategy

- [x] Define shared OS and base-directory helpers
- [x] Choose per-dependency Linux download/install behavior
- [x] Identify unsupported or Windows-only image pipeline tools
- [x] Decide which duplicate shell wrappers can be removed later

## Phase 3: Implementation Follow-up

- [x] Add Linux PowerShell bootstrap helper
- [x] Add shared platform detection for PowerShell scripts
- [x] Add shared platform detection for shell scripts
- [x] Teach `check-updates.ps1` to use Linux paths and download targets
- [x] Run formatting, update plan status, and verify enumeration/downloads

## Implementation Progress

Completed in this tranche:

- Added `android/get_deps/platform.sh` for shared shell platform detection and URL token helpers
- Added `android/get_deps/Get-DepPlatform.ps1` for shared PowerShell platform detection, default dep-base handling, and tool lookup
- Updated `resolve_dep_base.sh` and `Get-DepPlatform.ps1` so Linux dep-base paths are created before installers extract into them
- Updated `check-updates.ps1` to auto-create `dependency_base.txt` with the platform default, use Linux-aware URL generation, and mark Linux-manual optional tools instead of trying Windows-only installers
- Updated `tool_versions.conf` installed-version command hooks to use path-safe PowerShell helper functions
- Updated core installers `get_jdk.sh`, `get_sdk.sh`, `get_ndk.sh`, `get_cmake.sh`, and related SDK helpers for Linux-aware downloads
- Added `get_powershell_ubuntu.sh` and taught it to fall back to the pinned GitHub `.deb` when the Microsoft apt repo has no PowerShell package for the Ubuntu release
- Updated `get_fpcalc.ps1` for Linux tarball installs
- Updated `get_7zip.ps1`, `get_imagemagick.ps1`, and `get_unar.sh` so Linux falls back to host/manual tooling instead of Windows-only binaries
- Added a pinned `virtualenv.pyz` fallback in `get_cmake_format.sh` so Linux hosts without `python3-venv`, `pip`, or `ensurepip` can still install cmakelang
- Fixed Linux tool lookup in `run-clang-format.ps1`, `run-ktlint.ps1`, `run-shellcheck.ps1`, and `run-shfmt.ps1`
- Verified `pwsh -File ./android/get_deps/check-updates.ps1 -InstallSelection a` completes on Ubuntu 26.04 and leaves the installable Linux rows up-to-date
- Ran `pwsh -File ./android/run-code-quality.ps1 --fix` to completion after the Linux runner fixes
- Resolved later merge conflicts in `check-updates.ps1` and `tool_versions.conf` by keeping the logged web-request visibility improvements together with the Linux-aware NDK/JDK update queries and host-aware ImageMagick version detection
- Revalidated the merged `check-updates.ps1` and `tool_versions.conf` with conflict-marker scans plus a clean PowerShell parse

Still pending:

- Optional follow-up work on non-`check-updates` Android PowerShell scripts such as build/test/emulator helpers
- Later cleanup decision on `android/1_build-aab.sh`

## Survey Findings

Host state:

- This Linux checkout has no `dependency_base.txt`
- `pwsh` is installed on this host
- `bash` is installed at `/usr/bin/bash`
- Host reports Linux and Ubuntu 26.04

Script shape:

- Most `get_deps/*.sh` scripts already resolve `LOCAL_DIR` through `resolve_dep_base.sh`
- `resolve_dep_base.sh` supports Unix-style paths, but its error text still only recommends `C:\local`
- Several shell scripts already duplicate `_is_windows_target`; this should become one sourced helper
- The PowerShell installers read `dependency_base.txt` directly and assume Windows executable names
- `check-updates.ps1` can enumerate version metadata on Linux once `pwsh` exists, but installed-version probes and generated download URLs are currently Windows-shaped
- No active script references to `android/1_build-aab.sh` were found, only historical plan notes

## Shared Helper Strategy

Add a small shared shell helper, likely `android/get_deps/platform.sh`, sourced after `tool_versions.conf` and before download selection:

- `get_host_os`: returns `windows`, `linux`, or `macos` from `uname`
- `is_windows_target`: true for Git Bash, MSYS, Cygwin, Windows PowerShell targets, or WSL paths into `/mnt/<drive>`
- `get_dep_base_default`: returns `C:\local` on Windows-like hosts and `$HOME/local` on Linux/macOS
- `resolve_or_create_dep_base`: reads `dependency_base.txt`, or creates it with the platform default when called from bootstrap scripts
- `tool_exe_name`: returns `<name>.exe` on Windows targets and `<name>` elsewhere

Add a matching PowerShell helper, likely `android/get_deps/Get-DepPlatform.ps1`, dot-sourced by `check-updates.ps1` and the PowerShell install helpers:

- `Get-HostPlatform`: returns `Windows`, `Linux`, or `MacOS`
- `Test-WindowsTargetPath`: true for Windows paths and WSL-mounted Windows paths
- `Get-DefaultDependencyBase`: returns `C:\local` on Windows and `~/local` on Linux/macOS after expansion
- `Get-DependencyBase`: reads `dependency_base.txt`, optionally creates it for bootstrap
- `Join-ToolPath`: uses `Join-Path` and picks `.exe`, `.bat`, or extensionless binary by platform
- `Get-PlatformAssetSuffix`: maps dependencies to URL asset fragments without embedding those decisions in every script

Keep `tool_versions.conf` as the single source of version pins. Prefer version variables plus platform-specific URL construction in scripts over duplicating every URL key, unless a package has truly different upstream naming that benefits from explicit keys.

## PowerShell Bootstrap Helper

Add `android/get_deps/get_powershell_ubuntu.sh` or similar:

- Detect Ubuntu with `lsb_release -is` and require apt-based Ubuntu conventions
- Run `sudo apt update`
- Install `wget apt-transport-https software-properties-common`
- Download `https://packages.microsoft.com/config/ubuntu/$(lsb_release -rs)/packages-microsoft-prod.deb`
- Install it with `sudo dpkg -i`
- Run another `sudo apt update`
- Install `powershell`
- Verify with `pwsh -NoProfile -Command '$PSVersionTable.PSVersion.ToString()'`
- Avoid adding this to `get_all.sh`; it is a host bootstrap prerequisite before PowerShell scripts can run

## Per-Script Strategy

- `check-updates.ps1`: dot-source the PowerShell helper, default missing dep base to `~/local` on Linux, use platform-aware installed-version commands, generate platform-specific URLs for target updates, and print `./gradlew` guidance on Linux
- `resolve_dep_base.sh`: keep the Windows path conversion, add Linux default messaging, and optionally create `dependency_base.txt` from `$HOME/local` when called with a bootstrap flag
- `get_all.sh`: keep as bash; source the shared shell helper indirectly through sub-scripts; optionally add preflight checks for `curl unzip tar python3` on Linux
- `get_jdk.sh`: choose Adoptium `os=linux` on Linux and extract `.tar.gz` instead of assuming zip; keep canonical `jdk-$JDK_MAJOR` directory
- `get_sdk.sh`: choose `commandlinetools-linux-<build>_latest.zip` on Linux; existing extraction layout should still work
- `get_ndk.sh`: choose `android-ndk-$NDK_VERSION-linux.zip` on Linux; extraction layout should still work
- `get_cmake.sh`: choose `cmake-$CMAKE_VERSION-linux-x86_64.tar.gz` on Linux and use `tar`; consider whether this standalone CMake is still needed since `finalize.sh` installs SDK CMake
- `finalize.sh`: mostly platform-ready; use shared `sdkmanager` resolver so `.bat` selection is centralized
- `get_emulator.sh`: mostly platform-ready; use shared `sdkmanager` resolver
- `create_avd.sh`: mostly platform-ready; replace hard-coded API 34 image with `EMULATOR_API_LEVEL` and centralize `avdmanager` resolver
- `create_light_avds.ps1`: currently Windows-only paths and `avdmanager.bat`; port with the PowerShell helper, `$HOME/.android`, and extensionless `avdmanager`
- `get_gradle_wrapper.sh`: platform-neutral; no Linux work expected
- `get_clang_format.sh`: already platform-aware; replace local `_is_windows_target` with shared helper
- `get_cmake_format.sh`: already platform-aware; replace local `_is_windows_target` with shared helper and preflight `python3 -m venv`
- `get_ktlint.sh`: platform-neutral; no Linux work expected beyond Java binary resolution in callers
- `get_shellcheck.sh`: already platform-aware; replace local `_is_windows_target` with shared helper
- `get_shfmt.sh`: already platform-aware; replace local `_is_windows_target` with shared helper
- `get_soundfont.sh`: platform-neutral; no Linux work expected
- `get_stuffit.sh`: platform-neutral if Rust is installed; leave as optional and fail with its current cargo guidance
- `get_unar.sh`: Windows asset only today; on Linux prefer system `unar` or `unar`/`lsar` packages if available, otherwise wall it off as optional Mac installer extraction tooling
- `get_dosbox.sh`: Windows asset only today; select a Linux DOSBox-X release asset if upstream provides one, otherwise wall it off as optional DOS installer extraction tooling
- `get_fpcalc.ps1`: choose Linux chromaprint/fpcalc archive if upstream provides it; otherwise either build from `CHROMAPRINT_URL` or mark host fpcalc as optional with a clear manual install hint
- `get_7zip.ps1`: on Linux prefer host `7z` or `7za`, then install via apt guidance; avoid downloading `7zr.exe`; return the resolved extensionless tool path
- `get_imagemagick.ps1`: on Linux prefer host `magick` if installed, otherwise either use a Linux AppImage/static tarball if available or wall off high-quality texture downscaling as optional

## Dependency Strategy

- Android Gradle Plugin, Gradle, Kotlin, Compose, AndroidX, OkHttp, kotlinx serialization, Play Games, commons-compress, JUnit, Play App Update, xcrash: metadata-only Gradle dependencies, no host OS work
- Android SDK command-line tools: platform-specific URL, Linux zip is available
- Android NDK: platform-specific URL, Linux zip is available
- JDK: platform-specific Adoptium URL and archive format, Linux build is available
- CMake: platform-specific archive available, but SDK-managed CMake remains the main Android build path
- Build Tools, platform-tools, emulator, system images: installed through SDK manager and should work once SDK/JDK are correct
- Soundfont and single-file decoder headers: plain downloads, platform-neutral
- clang-format, ShellCheck, shfmt: Linux assets available and existing scripts already nearly support them
- cmakelang: already supports Linux via host `python3` venv
- ktlint: platform-neutral Java runner
- Chromaprint/fpcalc: likely Linux support, but may need asset-name confirmation or source build fallback
- 7-Zip: Linux should use host package or host binary rather than Windows bootstrap exe
- ImageMagick: likely best as host package or optional pipeline dependency on Linux unless a pinned portable Linux asset is chosen
- unar and DOSBox-X: optional extraction helpers; Linux support should be confirmed per upstream asset/package and can be walled off without blocking core Android dependency download
- PowerShell 7: host-managed prerequisite on Linux, installed through Microsoft apt repository helper

## `check-updates.ps1` Linux Download Target

Minimum viable Linux path:

1. Install `pwsh` with the Ubuntu helper
2. Let `check-updates.ps1` resolve missing `dependency_base.txt` to `~/local`
3. Make installed-version commands path-safe by replacing hard-coded `\` paths with `Join-Path` helpers and extension-aware executable lookup
4. Add platform URL helpers for JDK, SDK command-line tools, NDK, CMake, clang-format, ShellCheck, shfmt, Chromaprint/fpcalc, ImageMagick, 7-Zip, and PowerShell
5. Keep unsupported optional dependencies listed, but suppress install sync with a clear status such as `optional-linux-manual` when no Linux installer is implemented
6. Verify `pwsh -File ./check-updates.ps1 -NoPrompt` enumerates fully on Linux without throwing
7. Verify `-InstallSelection` can download all non-walled dependencies into `~/local`

## Duplicate Script Note

`android/1_build-aab.sh` appears unreferenced by active scripts. Later cleanup can remove it after `1_build-aab.ps1` is made Linux-compatible by selecting `./gradlew` instead of `gradlew.bat`, using platform-aware Java paths, and keeping the existing bash wrapper bootstrap behavior available through `get_gradle_wrapper.sh`.