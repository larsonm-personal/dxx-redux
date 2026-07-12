# Private newmenu rendering-state cleanup

## Goal

Reduce duplicated branch-owned rendering state and orchestration in the
upstream-original D1/D2 `newmenu.c` files now that the shared scaled-render
callback boundary exists. Preserve menu drawing, selection, input, and
hit-testing behavior exactly.

## Guardrails

- Keep private `newmenu` and `listbox` layouts and all item policy local
- Keep selection, reorder, touch hit-testing, drag-scroll, and event ordering
  local unless a smaller natural callback boundary is proven
- Move only rendering mechanism that is identical in D1 and D2
- Reject any adapter or callback table that is larger or less clear than the
  duplicated bodies it replaces
- Preserve unrelated pathing, Kconfig, networking, demo, and playsave work
- Use a normal shared translation unit and compact game-local callbacks

## Work

- [x] Record live upstream diff metrics and compare D1/D2 newmenu/listbox
  residuals after the Kconfig scaled-render extraction
- [x] Identify the smallest private-state snapshot or draw callback boundary
- [x] Add focused coverage for centralized behavior. No separate host fixture is
  needed because no pure state or geometry policy moved; the new unified
  runtime test exercises the actual canvas transaction
- [x] Implement the shared rendering-state seam and compact mirrored adapters
- [x] Re-audit residuals and leave high-coupling input/hit-testing code local
- [x] Run scoped code quality and `git diff --check`
- [x] Build Windows D1/D2 and all configured Android ABIs
- [x] Run focused menu-scale, readability, touch, pause, and listbox coverage
- [x] Record exact before/after metrics and update the campaign/catalog

## Baseline

- Comparison base: `upstream/main`
- D1 `newmenu.c`: `+1354/-118`, 1,472 changed lines
- D2 `newmenu.c`: `+1345/-125`, 1,470 changed lines
- The scaled newmenu and listbox transactions were exact D1/D2 duplicates

## Result

- `android_menu_scale_draw_result` now owns source and render bitmap
  allocation, canvas/font scaling transitions, source and direct-render
  passes, blitting, telemetry publication, restoration, and cleanup
- Each game retains an 8-line background callback, a 39-line private newmenu
  adapter, and a 31-line private listbox adapter
- Selection, reorder, drag-scroll, touch hit-testing, callbacks, and private
  layouts remain local
- D1 `newmenu.c`: `+1301/-119`, 1,420 changed lines
- D2 `newmenu.c`: `+1292/-126`, 1,418 changed lines
- Exact inherited-file reduction: 106 additions and 104 total changed lines
- Net source result for the four implementation files: 38 fewer lines
- `test_newmenu_render_paths_unified.json5` creates a real pilot before
  relaunching and covers scaled listbox, main/options newmenu, and pause menu
  paths in both games
- Windows D1/D2 and the combined arm64-v8a, armeabi-v7a, and x86_64 Android
  build pass
- The unified emulator run passes for D1 and D2 at 1920x1080 with a 1280x960
  logical menu configuration; the scaled listbox, main menu, and pause menu
  assertions all execute against the installed combined build

## Status

- [x] Recovered the prior newmenu extraction guidance and C14 prerequisite
- [x] Live residual audit and implementation complete
- [x] Final combined build and D1/D2 emulator execution complete
