# Plan: Title Music After Skip-All Intro 2026-04-25

Investigate why title screen music does not start when intro movies are skipped
via the launcher "skip all" setting, while still starting after manual movie
skips or when returning from gameplay.

## Investigation Steps

- [x] Trace the startup path for intro-movie skipping and title-screen music in
   D2.
- [x] Verify whether D1 uses the same control flow or a duplicated variant.
- [x] Identify the exact transition that normally starts title music and why it
   is bypassed by the skip-all path.
- [x] Implement the smallest D2/D1 fix that preserves other startup behavior.
- [x] Run targeted validation for the touched path and update this plan.

## Validation

- Added a focused launcher/game regression script:
   `android/game_scripts/test_title_music_skip_pref_unified.json5`
- Added an introspection signal for this path:
   `music.startup_title_requested`
- Kept the code fix in `show_titles()` for D2 and D1 so the skip-all path now
   requests the title song before entering the menu flow
- D1 also needed a declaration-order fix so the skip-pref `goto done` path does
   not jump past `song_playing` initialization
- Final validation completed with:
   - `android\stop-stale-formatters.ps1`
   - `android\run-code-quality.ps1 -Fix`
   - `android\gradlew.bat assembleDebug --no-daemon`
   - `android\run_test.ps1 -Install -ScriptName test_title_music_skip_pref_unified.json5 -Game d2 -TimeoutSeconds 180`
   - `android\run_test.ps1 -ScriptName test_title_music_skip_pref_unified.json5 -Game d1 -TimeoutSeconds 180`
- The current emulator state did not report active title playback even for the
   manual-skip baseline, so the regression validates the startup title-song
   request signal rather than mixer-active state