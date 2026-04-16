## Goal

Determine why D2 intro movies do not play on Android when MVL files are present, then implement the smallest fix that keeps desktop behavior intact.

## Plan

- [x] Trace movie library resolution in engine and Android launcher/config paths
- [x] Implement the minimal Android-safe fix for movie library selection or imported file naming
- [x] Validate with targeted build/tests and update this plan with results

## Result

- Root cause: the Android `PHYSFSX_init()` path in D2 skips `InitArgs()` and manually seeds only a few `GameArg` defaults, leaving `GameArg.GfxMovieHires` at zero.
- Effect: D2 Android always looked for `*-l.mvl`, even when the imported files only provided `INTRO-H.MVL`, `OTHER-H.MVL`, and `ROBOTS-H.MVL`.
- Fix: set `GameArg.GfxMovieHires = 1` in the Android-only fallback defaults inside `d2/misc/physfsx.c`, matching the normal desktop default unless `-lowresmovies` is explicitly supplied.
- Validation: `android\run-code-quality.ps1 --fix` passed, and `android\gradlew.bat -p .\android bundleDebug testDebugUnitTest` passed.
- Follow-up note: D1 does not define `GfxMovieHires`, so the initial mirrored D1 change was removed after the Android build caught it.