# Recommended mods popup

- [x] Move recommendations from the inline Mods/Levels section into a scrollable dialog that retains installed entries
- [x] Include all six recommended release packs with short descriptions, compatibility and decimal MB download sizes
- [x] Preserve downloads when closing the popup and expose progress, installed status and retry actions
- [x] Run scoped formatting and compile/package validation, then inspect the popup on an isolated emulator if available

Verification:

- Scoped Kotlin formatting passed (`android/temp/recommended-mods-format.log`)
- All six catalog sizes match the published `mod-packs.json`
- x86_64 debug APK package build passed, including Kotlin and both native engines (`android/temp/recommended-mods-package.log`)
- On disposable emulator-5586, inspected top/bottom entries and scroll/Close behavior at 480x800 and 800x480; screenshots are `android/temp/recommended-mods-top.png`, `recommended-mods-bottom.png` and `recommended-mods-landscape.png`
- Downloaded the menu/font pack from GitHub, closed and reopened the popup during the download, and verified the pack remains listed as Installed. Setup introspection confirms the imported archive is enabled and exactly 1,426,884 bytes (`android/temp/recommended-mods-installed.json`, `recommended-mods-installed.png`)
- Landscape scroll and Close assertions passed (`android/temp/recommended-mods-landscape-result.json`)
