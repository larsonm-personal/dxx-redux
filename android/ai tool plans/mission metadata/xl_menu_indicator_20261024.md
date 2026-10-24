# XL menu indicator

- Show a small XL label in the screen's upper-right corner while an XL game is
  active, independent of mission name or pilot admission policy
- Keep drawing and runtime state in XL_compat, with guarded calls from standard
  menus, list boxes, pause and control-binding menus. Draw after menu scaling
- Preserve canvas/font state; hide after leaving the game or loading stock data
- Verify opt-in and default-off host builds, Android build and menu activation
  through the existing automation/introspection path

## Verification

- Scoped formatting/lint passes; default-off Windows D2 and opt-in Windows
  game/lifecycle targets build. Existing weapon-order return warnings remain
- Host XL lifecycle test passes (4.37 seconds)
- Android x86_64 pilot build passes; existing cheat-buffer initializer and
  dependency deprecation warnings remain, no indicator warnings
- Existing XL runtime test extended with menu-draw and inactive-state checks:
  36/36 pass in 17415 ms, run 25c14955927848af8d3033a12ff687d2
- Six indicator draws recorded; runtime inactive after game exit and stock load
- Evidence: temp/xl-menu-runtime.log and temp/xl-menu-runtime-result.json
- Tested APK SHA-256:
  89eddf7b82cafa97561cc0819139deaed525a50a05caef916f846e2cdfe002be
- Automation catalog and scheduling checks pass (374 entries, 431 support scripts)
- App stopped after evidence capture; changes remain uncommitted
