# Controls Editor Device Text Follow-Up Plan

Status: implemented and verified

## Problem
- Real Android devices show the customize weapon/control editor text as tiny, garbled, and unreadable.
- The emulator path looked correct, and on-device scrolling works, so the latest code is present but text rendering quality differs on hardware.

## Plan
1. Inspect the Android kconfig high-resolution render path, especially the offscreen bitmap allocation, draw state, source slice, and final blit.
2. Trace OpenGL bitmap upload/blit behavior for differences that could make real hardware sample or stride the intermediate text incorrectly.
3. Add targeted Android debug logging for the relevant computed sizes and copy path if code inspection does not reveal the fault directly.
4. Implement the smallest fix that preserves the emulator improvement and makes the device path render from a clean high-resolution source.
5. Re-run code quality, Android debug build, and focused kconfig automation.

## Notes
- Keep D1 and D2 kconfig changes mirrored.
- Do not rely on screenshot/OCR for validation; use introspection and logs where possible.
- The final kconfig region blit now uses 1024x1024 nearest-filtered tiles instead of one filtered texture. This avoids high-resolution phones exceeding the Android OpenGL texture cap or stressing the shared texture upload buffer through power-of-two expansion.
- D1 and D2 kconfig now emit bounded `[kconfig-scale]` Game Logs diagnostics with screen, source, destination, render, font scale, scroll, and item count values.
- Game Logs must be enabled/exported on-device to collect the diagnostics.

## Verification
- `android\run-code-quality.ps1 -Fix` passed for the touched C files and this plan.
- `android\gradlew.bat -p android :app:assembleDebug` passed with JDK 21.
- Focused installed emulator automation `test_kconfig_keyboard_stage_d2.json5` passed via durable result file: `PASS`, 42/41 steps, 12089 ms.
