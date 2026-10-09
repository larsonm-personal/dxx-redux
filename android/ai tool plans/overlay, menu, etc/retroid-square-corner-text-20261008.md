# Retroid square-corner HUD text

- [x] Inspect device corner reports and trace Kotlin/native inset handling
- [x] Preserve reported zero corners through Kotlin and native scaling, retaining fallback only when measurements are unavailable
- [x] Build both Android engines, check square/rounded/asymmetric corner behavior, and verify the installed fix on the Retroid

## Evidence

The attached Retroid Pocket 4 Pro runs API 33 and WindowManager reports radius 0 at all four corners. MainActivity replaces every zero with 5% of screen width; native scaling independently does the same. Both layers must preserve measured zero to correct the layout. Keep measured positive values, half/full/off settings, and pre-API-31 fallback behavior.

Android's WindowInsets.getRoundedCorner contract returns null when a corner is absent or outside the app bounds: https://developer.android.com/reference/android/view/WindowInsets#getRoundedCorner(int)

## Validation

- Scoped Kotlin/C/H formatting and lint passed
- Compiled a temporary comparison probe from the actual before/after native functions and ran it on the Retroid: 31,488 identical positive-radius results across off/half/full settings, four asymmetric radius sets, three render sizes, and every vertical position
- Square corners now produce zero at every vertical position; mixed square/rounded corners preserve independent top/bottom and left/right values; startup fallback matches the previous implementation
- Probe sources and results are in android/temp/retroid-corners
- arm64 assembleInternal succeeded, including CMake builds of D1 and D2; only existing unused-variable warnings in gauges.c appeared
- Installed the signed Internal APK in place on JYPR42510121028 and resumed the existing D2 save; native lifecycle diagnostics confirm surface=1334x750 tl=0 bl=0 tr=0 br=0
- Before/after device captures confirm the top and bottom HUD rows return to their ordinary edge positions; no fatal exception/signal appeared during startup/resume
- Other physical devices were not attached; rounded-corner preservation was checked against the previous native implementation
- Device build command: android/gradlew.bat -p android :app:assembleInternal -PgithubRelease=true -Pandroid.injected.build.abi=arm64-v8a --console=plain (quote dotted properties in PowerShell). The ABI-injected APK is under app/build/intermediates/apk/internal, is test-only, and needs adb install -r -t
