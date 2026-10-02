# Reticle rendering correction

- Trace classic asset selection, layout and scaling in D1, D2 and D1-in-D2
- Add Android diagnostics for the screen/asset resolution mismatch before changing the classic renderer
- Reuse the existing asset-aware HUD resolution and scaling for the D2 classic reticle
- Repair Circle/Dot vertex cache sizing in both engines and remove the obsolete 1080p Circle workaround
- Run scoped formatting, both host builds and Android build/runtime checks

Findings: D1 imports populate both D2 gauge tables with the same D1 assets; classic still uses screen-based HIRESMODE. Native D1 already uses asset resolution. Circle/Dot cache their first allocation but subsequently draw a radius-dependent vertex count

Validation so far:

- Both final Windows CMake builds passed (D1 and D2)
- Scoped mixed-language formatting/lint passed; upstream engine files are excluded by the repository formatter
- Diagnostic Android build reproduced screen_hires=1 asset_hires=0 layout_hires=1 with a 9x7 cross on supported registered D1 assets
- Unsupported original 1.0 CD data was rejected by launcher admission before gameplay; switched to the registered Anniversary fixture
- Added -Reticles to test_d1_in_d2_android.ps1 to render all nine styles and assert minimum/maximum size changes; it preserves the runner's isolated app data workflow
- Extended the reticle runner timeout to cover the full style cycle on the emulator

Final validation:

- Android x86_64 debug APK build passed for both native engines; no new warnings in the changed rendering code
- Reticle runner passed 236/236 steps, including all nine styles, 14 size-setting checks, and the corrected screen_hires=1 asset_hires=0 layout_hires=0 diagnostic
- Evidence: temp/d1-launch-runtime-20260929-144037 (automation_result.json, native-logcat.txt, first-strike.png)
- Visually inspected the final Classic screenshot: components are grouped around the aiming point
- The first size assertion used the blank menu row (16); corrected to the actual slider (17) before the successful run
- Physical phone validation remains for the user; the reported asset/display mismatch was reproduced on the emulator
