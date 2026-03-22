# Auto-Infrastructure Test Suite Overhaul

## Goal
Make `run_all_tests.ps1` fully self-provisioning: automatically start emulators,
matchmaking server, and Docker NAT containers instead of skipping tests when
infrastructure is missing. Eliminate "26 skipped" from unattended runs.

## Changes

### 1. test_saf_archiver.ps1 -- D2 data auto-sourcing [DONE]
- Added fallback: `game_data/extracted/VERTIGO/DESCENT2.HAM` when the staging
  dir (`game_data_to_copy_to_emulator/data/`) is empty
- Keeps existing device-pull fallback as third option

### 2. test_helpers.ps1 -- Infrastructure management functions [DONE]
Added at end of file (~180 lines):
- `Start-SingleEmulator` -- wraps emu_health.ps1 -Restart -Wait
- `Start-SecondEmulator` -- launches Nexus5X_Light_2, waits for boot (180s)
- `Start-MatchmakingServer` -- cargo build if needed, start, wait for port 9000
- `Start-DockerNat` / `Stop-DockerNat` -- docker compose up/down with NAT env vars
- `Install-ApkOnDevice` -- adb install -r with optional -Serial
- `Push-GameDataToDevice` -- calls push_game_data.sh with ANDROID_SERIAL override

### 3. run_all_tests.ps1 -- Tiered auto-provisioning orchestrator [DONE]
Complete rewrite of execution model:
- Removed `-AutoServer` switch (always automatic now)
- Added `-SkipDocker` switch
- Tests grouped into tiers by infrastructure requirement:
  - Tier 0: no-infra (game_data, cue_iso, server_integration)
  - Tier 1: single emulator (json5 scripts + most ps1 tests)
  - Tier 2: dual emulator + server (mp, lan, bot_client)
- Each tier auto-provisions before running its tests
- APK install + game data push on each newly started emulator
- Cleanup in reverse order (Docker, server, emulators left running)
- Only 3 tests remain as "manual" skips: test_keyboard_manual, test_dual_emu, test_dual_emu_setup
- Infrastructure failures gracefully recorded as "infra-skipped" with reason

### Files changed
- `android/tests/test_saf_archiver.ps1` -- D2 data fallback path
- `android/test_helpers.ps1` -- 9 new infrastructure functions
- `android/run_all_tests.ps1` -- full tiered rewrite

### Verification
- All three files pass PowerShell parse check
- Pre-existing lint issues (clang-format, ktlint) are unrelated
