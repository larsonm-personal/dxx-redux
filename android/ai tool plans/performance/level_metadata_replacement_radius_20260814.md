# Level Metadata Replacement Radius

## Goal

Make live route metadata, background analysis, and next-level precomputation use the player radius from the level's active model replacements.

## Plan

- [x] Map all live and headless metadata scan entry points and replacement-loading lifetimes.
- [x] Load D2 level replacements before metadata scanning and use one canonical navigator radius.
- [x] Update the live D2 level-load ordering so its cache fingerprint matches background analysis.
- [x] Add focused regression coverage for stock versus HXM replacement player radii and cache adoption.
  - Real Obsidian level 4 host analysis is the focused replacement-radius regression.
- [x] Run scoped formatting, native tests, Android unit tests, and D1/D2/Android build verification.

## Boundaries

- Preserve gameplay collision behavior and mission replacement semantics.
- Keep D1 behavior and non-Android builds intact.
- Do not alter cache matching rules merely to accept incompatible artifacts.

## Result

- D2 live loading, background analysis, previews, and host metadata generation now load each level's HXM before scanning.
- Navigation metadata uses the active player model radius, with loaded object sizes retained as a fallback.
- Cache generation 2 invalidates stale artifacts and completed-job ledger entries created without replacement-radius awareness.
- Obsidian level 4 reports navigator radius 310313 in the host analyzer, matching its replacement player model.
- D1 and D2 native tests passed, both Windows builds passed, focused Android cache tests passed, and the debug APK built for all configured ABIs.
- The full Android unit suite ran 813 tests with one unrelated SevenZip native RAR initialization failure; both affected cache test classes pass independently.
