# Robot Preview List Density Plan

## Objective

Reduce excess vertical spacing in the base-game robot preview list and add visible scroll
indicators that reflect whether more robots are available above or below.

## Work

- [x] Inspect the robot-list layout and existing shared scroll-indicator components
- [x] Reduce row padding and the local action height from Material's 48 dp default to 28 dp
- [x] Add scroll indicators to the robot list
- [x] Verify initial and scrolled indicator states through emulator UI semantics
- [x] Run scoped quality checks and Android build/tests

## Verification

- Initial D2 list exposes `Scroll down` but not `Scroll up`
- After scrolling, the list exposes both `Scroll up` and `Scroll down`
- Emulator row pitch is 74 px at its configured density, down from approximately 126 px
