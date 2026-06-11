## Goal
Survey menu, modal, and border drawing paths for stale or mis-drawn border risk on Android.

## Plan
- [x] Trace shared window draw and flip behavior for non-3D UI.
- [x] Inspect newmenu/messagebox/listbox/background handling.
- [x] Inspect game-over, state error, networking error, and other modal callers.
- [x] Summarize likely risk points and smallest fix options.

## Notes
- This is a survey request, so avoid engine behavior changes unless requested.
- Android skips the non-Android post-swap GL color clear, so partial-window redraws can expose stale pixels.
- Ordinary `newmenu` and listbox panels repaint their panel each draw; risk is mostly the surrounding/backing area.
- Dialogs shown after `window_set_visible(Game_wind, 0)` have no full-screen game redraw behind them.
- Death letterbox is the same class: visible game window, but only the middle subcanvas redraws.
