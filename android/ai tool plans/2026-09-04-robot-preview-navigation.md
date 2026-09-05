# Mission robot preview navigation

- [x] Build a stable mission-wide replacement list with each entry's source level
- [x] Switch level context during native preview navigation, including variants of the same robot ID
- [x] Add regression coverage, format changed files, and build/test both games

Validation:
- Android x86_64 debug APK assembled, compiling the shared preview for D1 and D2
- All five RobotPreviewRequestStoreTest tests passed, including identical mission navigation from early/late entries and duplicate robot ID variants
- Scoped code quality checks and diff whitespace checks passed
- Enemy Within emulator smoke test passed against the Rebirth child extracted from ewithin-versions.zip (the full wrapper exceeded available emulator staging space)
- Walked all 39 replacements (35 robot IDs, 21 source levels) forward and backward, including wraparound, then closed cleanly
- D1 base preview smoke test passed: navigation, attack, sound, rotation, and clean closure
