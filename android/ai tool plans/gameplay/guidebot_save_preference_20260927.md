# Guidebot routing preference on save load

- Apply the current single-player routing preference after restoring saved guidebot state
- Reset navigation on a mode change while preserving the companion object and position
- Preserve replay policy, co-op session policy, and frozen secret-world restores
- Hide Warp to Me alongside other enhanced-only wheel and touch actions
- Update existing save lifecycle and menu coverage for both switch directions
- Run scoped formatting, Android build/unit tests, host build, and emulator integration coverage

## Completed validation

- Scoped code quality and git diff checks passed
- Android debug APK built for all three configured ABIs; Windows D2 build passed
- All 30 RemainingKeyTouchActionsTest and GuidebotLockedWheelTest cases passed
- Emulator routing lifecycle passed all 42 steps, including loads in both directions and release-state checks
- Emulator wheel visibility passed in both modes and after both save conversions
- Removed the menu runner's assumption that every configured wheel includes Recall; the unit test still verifies Recall remains enabled in Classic
