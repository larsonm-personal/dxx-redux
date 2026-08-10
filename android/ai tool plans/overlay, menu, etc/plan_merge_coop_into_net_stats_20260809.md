# Merge cooperative status into net stats

## Goal

Put the cooperative player status panel at the top of the existing toggleable network statistics window and remove the always-on cooperative overlay

## Plan

- [x] Trace cooperative and network statistics overlay ownership, polling, visibility, and layout
- [x] Merge cooperative status rendering and data providers into the network statistics overlay
- [x] Remove the separate always-on cooperative overlay lifecycle from `MainActivity`
- [x] Add or update focused tests for the combined visibility and content behavior
- [x] Run scoped code quality, tests, and the required Windows CMake build

## Progress

- [x] Plan created before implementation
- [x] Scoped code quality checks passed
- [x] `MultiplayerStatsOverlayTest` passed through `:app:testDebugUnitTest`
- [x] `run-windows-build.ps1 -Target both` passed for D1 and D2
