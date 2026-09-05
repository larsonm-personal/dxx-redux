# Routing development regression set

## Phase 1: Resolve the campaign inputs

- [x] Confirm the checked-in metadata names for Castaway Redux, Counterstrike, Obsidian, and D1 under D2
- [x] Determine whether D1-in-D2 already participates in GuideBot simulation discovery
- [x] Define one canonical mission list shared by both focused stages

## Phase 2: Add the focused menu category

- [x] Add an unattended category value and an interactive menu item
- [x] Run Windows-native metadata only for the selected missions
- [x] Run headless GuideBot simulations only for the selected missions
- [x] Preserve normal builds, incremental writes, parallelism, logs, and failure reporting

## Phase 3: Verification

- [x] Extend the regression-generator wiring tests
- [x] Verify the focused metadata and simulation selections without changing checked-in JSON
- [x] Run scoped code quality and review the final diff
