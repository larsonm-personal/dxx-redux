# Retroid startup brightness investigation

- Capture display brightness, window dimming, and keyboard state during reproduction
- Trace the responsible launcher/game window code and add targeted diagnostics if needed
- Apply the smallest evidence-backed correction and validate on the Retroid
- Preserve existing unrelated edits and document findings

## Evidence and implementation

- Device JYPR42510121028, installed com.dxxredux.app.github versionCode 23760
- Launcher, intro, main menu, pilot-name keyboard shown/hidden all reported physical backlight 47/255, no WindowManager brightness override, and no dialog dim flag for the game input
- SurfaceFlinger reported game buffer format 43 (RGB10_A2), unknown dataspace, alpha 1, and unchanged native display color mode
- A standalone EGL probe on the Retroid reproduced config selection: first match RGB10/10/10 alpha2 visual43; ordinary RGB8/8/8 alpha0 visual2 and RGBA8 visual1 are available
- EGL channel-size attributes are minimum sizes, and deeper color buffers sort first: https://registry.khronos.org/EGL/specs/eglspec.1.5.withchanges.pdf
- Updated shared Android EGL selection to choose exact requested RGB precision, allowing RGB888 when RGB565 is unavailable, and log selected channels/visual through DXX-EGL
- The unintended 10-bit surface is confirmed; its causal relationship to the user's visible brightness transition remains unconfirmed pending visual comparison
- Evidence and original installed APK are retained in android/temp/brightness-investigation

## Validation

- Scoped code-quality checks passed
- Gradle assembleInternal succeeded for arm64-v8a, including CMake compilation of D1 and D2; an existing d2/main/weapon.c abs(fix64) warning remains
- Installed the signed test APK as an in-place update with saves/settings retained
- Confirmed DXX-EGL reports RGB=5/6/5 alpha=0 visual=4 for the current 16-bit setting, and SurfaceFlinger reports actual buffer format 4 instead of 43
- D2 intro, main menu, pilot selection, keyboard entry and keyboard dismissal worked; backlight remained 47/255 and no fatal/EGL failure appeared in the captured log
- The initial probe used RGB888 minimums; the actual saved game setting requests RGB565. Both were previously susceptible to the same deeper-buffer selection issue
- D1 compiled but was not exercised on this device because its assets are not installed
- Left the candidate fix installed for the user's physical-display comparison; no claim that the perceived brightness transition is conclusively fixed

## Follow-up: confirmed focus highlight cause

User reports the brightness jump persists at difficulty selection with the 19:45 GitHub build. Verified the running build still has the experimental RGB565 selection, so the surface-format change was not sufficient.

Reproduced the gray wash on the difficulty, main and pilot-selection menus. Opening pilot-name text entry removes it; dismissing only the IME leaves the input menu dark; closing text entry restores the wash. A temporary native framebuffer probe reports gamma=0, palfx=0, and a black corner pixel while the Android screenshot of the same frame has a gray corner. This localizes the defect above the native renderer.

The full-screen GameSurfaceView is focusable and requests focus on startup and after text entry. Android's default focus highlight paints over this surface. Disabled defaultFocusHighlightEnabled for this view on API 26+, keeping game/controller focus handling intact. MainActivity is shared by both games.

Removed the unsuccessful EGL color-selection change and temporary framebuffer logging. Existing unrelated MainActivity changes remain intact.

### Final validation

- Scoped Kotlin formatting/lint passed; arm64 Internal build completed successfully with both native engines
- Installed the final signed test APK in place, retaining game data
- Repeated pilot-name keyboard entry, keyboard dismissal, then text-entry exit: same corner pixel changed from the faulty build's RGB 77/77/77 to RGB 0/0/0 with the fix
- Repeated New Game starting-level entry through to difficulty selection, including gamepad-source D-pad movement: difficulty screen remains at normal baseline (corner RGB 0/0/0)
- Left the Retroid at difficulty selection with Rookie selected and the final fix installed
- Removed temporary device screenshot, restored log.tag.DXX-DLOG to unset, and removed native diagnostic probe code
- Final product change is the four-line API-guarded focus-highlight setting in MainActivity; EGL source is restored to its original state
