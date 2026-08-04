# BR-0513 canonical save metadata labels

## Plan

- [x] Read repository instructions, BR-0513, and related findings
- [x] Trace canonical save metadata and Save Details label construction
- [x] Implement the smallest shared-label fix with focused regression coverage
- [x] Run scoped code quality, focused tests, and Android build verification
- [x] Archive BR-0513 with exact validation evidence

## Result

Save metadata music values and labels now have one documented Kotlin mirror of the paired native constants. Save Details reports music type 2 as CD, and compact rows, detail rows, and resume choices use one save-kind formatter that identifies periodic autosaves and leaves unknown kinds explicit. Bridge decoding preserves unknown music values for honest presentation, while launch policy uses the shared constants. Focused Save Explorer, resume-panel, and music-policy tests, scoped code quality, the three-ABI debug APK build, and `git diff --check` passed.
