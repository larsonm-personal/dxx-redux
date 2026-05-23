# Check Updates Dependency Coverage

## Goal
Survey android/get_deps/check-updates.ps1 against android/get_deps/tool_versions.conf
and add update checks for dependencies that are currently missing but have a
reasonable upstream version source.

## Status
- [x] Compare tool_versions.conf keys against check-updates.ps1 coverage
- [x] Add missing dependency checks and update actions
- [x] Run a focused script validation
- [x] Mark this plan complete