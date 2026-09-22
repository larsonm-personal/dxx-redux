# D1-in-D2 destroyed light border

## Follow-up: imported light behavior

Treat every imported D1 level as having no destructible-light tags. Keep the destroyed-light palette conversion at the user's request, and preserve shootable monitor effects

- [x] Add a regression for D1 versus D2 light hits, illumination, and monitor destruction
- [x] Reject inherited light destruction for direct hits and nearby explosions in D1 missions
- [x] Run the D2 build, tests, and scoped quality checks

The new gameplay regression failed on the unchanged engine at the inherited light destruction assertion before applying the guards

Windows D2 build and all 59 CTest tests passed after the change. Coverage exercises forced blast hits, ordinary D1 weapon hits, unchanged texture orientation and illumination, tagged and untagged D2 lights, shootable monitors, and switching D1/D2/D1. The retained palette conversion, lava, and reactor regressions also pass. Scoped quality, the native fixture's clang-format check, and `git diff --check` passed. No Android build or in-game playtest was performed. Logs are under `temp/d1-light-policy-*`

## Palette conversion

- [x] Trace First Strike light replacements against the original asset tables
- [x] Fix the confirmed texture or palette handling error
- [x] Exercise the affected loader with regression coverage and run the D2 build
- [x] Run scoped code quality and record validation

## Findings

- D1 light textures 281, 282, 285, and 286 convert to D2 textures 291, 293, 296, and 298
- D2 retains shootable-light behavior and replaces those textures with 292, 294, 297, and 299 (`ceil024b`, `ceil025b`, `ceil028b`, and `ceil029b`)
- D1 has no destroyed-light equivalents, so those bitmaps remain D2 assets while First Strike loads `palette.256`
- Decoding the retail GOG assets confirms palette index 154 is gray `(18, 20, 18)` in `groupa.256` but green `(0, 57, 0)` in D1's palette, in the original 6-bit channel values
- The four destroyed images contain 116, 2, 208, and 379 pixels at that index respectively

## Approach

Convert retained destroyed-light bitmaps from the current D2 PIG palette into the active palette while loading D1 bitmap replacements. Store converted copies in the existing replacement arena so normal PIG reload and replacement cleanup retain ownership. Preserve transparency indices and flags, account for RLE size growth, and protect these destinations from generic replacement clones

Save each converted light's original paging metadata and restore it when freeing replacements if the slot still belongs to this conversion. This ensures successive D1 levels reload original D2 pixels even without a PIG switch, and does not overwrite newer PIG or custom replacements

## Validation

- Extended `test_upstream_compat` failed on the unchanged loader at the destroyed-light color assertion, then passed with the fix
- Regression covers raw and RLE images, RLE expansion into a larger encoded buffer, both transparency indices and flags, unchanged source pixels, repeated replacement loads, restoration of original paging metadata, and preservation of newer replacements
- Existing monitor replacement and animation assertions still pass
- `run-windows-build.ps1 -Target d2` passed; final incremental build reported no compiler warnings
- All 59 D2 CTest tests passed
- Scoped mixed-language code quality passed; its C/C++ scope excludes these files, so the native fixture was also formatted and checked with pinned clang-format 20. Engine formatting follows the surrounding source
- `git diff --check` passed
- No in-game visual playtest or Android build was performed

Build, test, and quality logs are under `temp/d1-light-*`
