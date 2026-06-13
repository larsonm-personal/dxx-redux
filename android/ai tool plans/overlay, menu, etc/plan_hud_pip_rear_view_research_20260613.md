# HUD PIP Rear View Research

Goal: determine whether dxx-redux already supports assigning the left HUD picture-in-picture monitor to rear view, whether this is inherited from DXX-Rebirth or only present in D2X-XL, and what would be needed to make it selectable in fullscreen/minimal HUD modes.

- [x] Inspect local D1/D2 cockpit, gauge, rear-view, and guided missile monitor code.
- [x] Inspect local player/config/control persistence for any monitor assignment setting.
- [x] Cross-check upstream docs or source for DXX-Rebirth/DXX-Redux and D2X-XL behavior.
- [x] Summarize current support and likely implementation path.

Summary:
- D2 has built-in Shift-F1 and Shift-F2 controls to cycle left and right camera windows. In dxx-redux, the first cycle from none selects rear view.
- D2 persists the per-window camera mode in `PlayerCfg.Cockpit3DView[2]` in the pilot file.
- D2 draws these camera windows in full cockpit, status bar, and fullscreen modes.
- The Android launcher currently exposes cockpit size, auto-leveling, and related pilot preferences, but not `Cockpit3DView[0/1]`.
