# D1 difficulty menu tap debug plan

## Goal
Investigate unreliable tap selection in the D1 in-game difficulty selection menu, especially tapping the preselected difficulty.

## Steps
- [x] Trace Android touch delivery into D1 menu hit-testing and selection handling.
- [x] Compare D1 and D2 menu/touch paths for relevant differences.
- [x] Fix an obvious input issue if found, keeping source edits narrow.
- [x] Add Android debug instrumentation if no clear fix is available.
- [x] Run scoped formatting/build checks that are practical for the touched files.

## Notes
- The native D1/D2 `newmenu` mouse path already logs `[newmenu-touch]` and uses shared menu scale remapping.
- The in-game Android admin tray difficulty panel populated its row hit rects only during draw. A tap immediately after opening the panel could arrive before the draw pass refreshed the row list.
- `TouchOverlayView` now computes the difficulty panel layout on demand before draw and touch handling, and logs `[admin-difficulty-touch]` open/down/up/select decisions.
- Validation passed with scoped `android\run-code-quality.ps1 -Fix`, `:app:compileDebugKotlin`, and `git diff --check`.
