# Code Quality + test_lan Fix Plan
Status: COMPLETE

## Summary
Three changes:
1. PSScriptAnalyzer (PowerShell linter/formatter) added to code quality pipeline
2. ShellCheck + shfmt (bash linter/formatter) added to code quality pipeline
3. test_lan game data provisioning fix (Install-AppAndData now uses Resolve-GameDataDeps)

## Phase 1: PSScriptAnalyzer [DONE]
- Created android/PSScriptAnalyzerSettings.psd1 with exclusions for build/test script patterns
- Created android/run-psscriptanalyzer.ps1 (lint + format, --check mode)
- PSScriptAnalyzer installed via Install-Module (no get_deps script needed)
- Fixed 3 InvalidVariableReferenceWithDrive issues in gather-warnings-msvc.ps1, teardown_docker_nat.ps1
- Auto-formatted ~18 files (indentation, whitespace, braces)

## Phase 2: ShellCheck + shfmt [DONE]
- Added SHELLCHECK_VERSION=0.10.0 and SHFMT_VERSION=3.10.0 to tool_versions.conf
- Created android/get_deps/get_shellcheck.sh (downloads from GitHub releases)
- Created android/get_deps/get_shfmt.sh (downloads from GitHub releases)
- Created android/run-shellcheck.ps1 (lint, excludes SC1091 and SC2317)
- Created android/run-shfmt.ps1 (format, -i 4 -bn matching .editorconfig)
- Fixed CRLF line endings in 8 .sh files
- Fixed 15 shellcheck warnings (SC2155, SC2086, SC2166, SC2231, SC2001, SC2010, SC2034, SC2207, SC2012)
- Auto-formatted 15 .sh files with shfmt

## Phase 3: run-code-quality.ps1 Integration [DONE]
- Added PSScriptAnalyzer, shellcheck, shfmt sections to run-code-quality.ps1
- Full pipeline: clang-format -> ktlint -> PSScriptAnalyzer -> shellcheck -> shfmt
- Both -Fix and check (no flag) modes verified passing

## Phase 4: test_lan game data fix [DONE]
- Root cause: Install-AppAndData called push_game_data.sh which reads from
  game_data_to_copy_to_emulator/data/ -- that directory only has .gitkeep
- Fix: Install-AppAndData now sets $env:ANDROID_SERIAL and calls
  Resolve-GameDataDeps with Get-StandardGameDataDeps (SHA256-based game data index)
- This uses the same proven mechanism as Start-GameWithRetry/Ensure-GameDataOnDevice

## Files created
- android/PSScriptAnalyzerSettings.psd1
- android/run-psscriptanalyzer.ps1
- android/run-shellcheck.ps1
- android/run-shfmt.ps1
- android/get_deps/get_shellcheck.sh
- android/get_deps/get_shfmt.sh

## Files modified
- android/run-code-quality.ps1 (added 3 tool sections)
- android/get_deps/tool_versions.conf (added SHELLCHECK/SHFMT versions)
- android/test_helpers.ps1 (Install-AppAndData: push_game_data.sh -> Resolve-GameDataDeps)
- android/gather-warnings-msvc.ps1 (${name} fix)
- android/teardown_docker_nat.ps1 (${serial} fix)
- android/get_deps/create_avd.sh (SC2155 fix)
- android/get_deps/finalize.sh (SC2086 disable)
- android/get_deps/get_cmake.sh (SC2166/SC2231 fix)
- android/get_deps/get_jdk.sh (SC2166/SC2231 fix)
- android/1_build-aab.sh (SC2001 fix)
- android/push_game_data.sh (SC2010 fix)
- android/run_automation.sh (SC2034/SC2001 fix)
- android/run_emulator.sh (SC2034 fix)
- android/run_test_menu.sh (SC2034/SC2207/SC2012 fix)
- 8 .sh files had CRLF -> LF conversion
- ~18 .ps1 files auto-formatted by PSScriptAnalyzer
- 15 .sh files auto-formatted by shfmt
